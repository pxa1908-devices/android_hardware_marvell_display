/****************************************************************************
*
*    Copyright (c) 2005 - 2012 by Vivante Corp.  All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Vivante Corporation. This is proprietary information owned by
*    Vivante Corporation. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Vivante Corporation.
*
*****************************************************************************/

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "gc_hwc.h"
#include "gc_hwc_debug.h"

#include <hardware/hardware.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <log/log.h>

static int
hwc_prepare(
    hwc_composer_device_t * dev,
    hwc_layer_list_t * list
    );

static int
hwc_set(
    hwc_composer_device_t * dev,
    hwc_display_t dpy,
    hwc_surface_t surf,
    hwc_layer_list_t * list
    );

static int
hwc_device_close(
    struct hw_device_t * dev
    );

static int
hwc_device_open(
    const struct hw_module_t * module,
    const char * name,
    struct hw_device_t ** device
    );

/******************************************************************************/

static struct hw_module_methods_t hwc_module_methods =
{
    .open = hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM =
{
    .common =
    {
        .tag           = HARDWARE_MODULE_TAG,
        .version_major = 2,
        .version_minor = 0,
        .id            = HWC_HARDWARE_MODULE_ID,
        .name          = "Hardware Composer Module",
        .author        = "Vivante Corporation",
        .methods       = &hwc_module_methods,
        .dso           = NULL,
        .reserved      = {0, }
    }
};

static PFNEGLGETRENDERBUFFERANDROIDPROC _eglGetRenderBufferANDROID;

/******************************************************************************/

int
hwc_prepare(
    hwc_composer_device_t * dev,
    hwc_layer_list_t * list
    )
{
    hwcContext * context = (hwcContext *) dev;

    if (context == gcvNULL)
    {
        ALOGE("%s(%d): Invalid device!", __FUNCTION__, __LINE__);
        return HWC_EGL_ERROR;
    }

    if ((list == NULL) || (list->numHwLayers == 0))
    {
        return 0;
    }

    if (gcmIS_ERROR(hwcPrepare(context, list)))
    {
        ALOGE("%s(%d): Failed in prepare", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    return 0;
}

int
hwc_set(
    hwc_composer_device_t * dev,
    hwc_display_t dpy,
    hwc_surface_t surf,
    hwc_layer_list_t * list
    )
{
    gceSTATUS status;
    hwcContext * context = (hwcContext *) dev;
    android_native_buffer_t * backBuffer = NULL;

    if (context == gcvNULL)
    {
        ALOGE("%s(%d): Invalid device!", __FUNCTION__, __LINE__);
        return HWC_EGL_ERROR;
    }

    if ((list == NULL) || (list->numHwLayers == 0))
    {
        return 0;
    }

    if (context->hasComposition)
    {
        backBuffer = (android_native_buffer_t *)
           _eglGetRenderBufferANDROID((EGLDisplay) dpy, (EGLSurface) surf);

        if (backBuffer == NULL)
        {
            ALOGE("%s(%d): Failed to get back buffer", __FUNCTION__, __LINE__);
            return -EINVAL;
        }

        if (context->separated2D)
        {
            gcoHAL_SetHardwareType(context->hal, gcvHARDWARE_2D);
        }
    }

#if DUMP_SET_TIME
    struct timeval last;
    struct timeval curr;
    long   expired;

    gettimeofday(&last, NULL);
#endif

    gcmONERROR(hwcSet(context, backBuffer, list));

    eglSwapBuffers((EGLDisplay) dpy, (EGLSurface) surf);

    gcmVERIFY_OK(gcoHAL_Commit(context->hal, gcvTRUE));

#if DUMP_SET_TIME
    gettimeofday(&curr, NULL);
    expired = (curr.tv_sec - last.tv_sec) * 1000
            + (curr.tv_usec - last.tv_usec) / 1000;
    ALOGD("Set %d layer(s): %ld ms", list->numHwLayers, expired);
#endif

    if (context->separated2D && backBuffer != NULL)
    {
        gcoHAL_SetHardwareType(context->hal, gcvHARDWARE_3D);
    }

    return 0;

OnError:
    ALOGE("%s(%d): Failed in set", __FUNCTION__, __LINE__);

    if (backBuffer != NULL)
    {
        eglSwapBuffers((EGLDisplay) dpy, (EGLSurface) surf);

        if (context->separated2D)
        {
            gcoHAL_SetHardwareType(context->hal, gcvHARDWARE_3D);
        }
    }

    return -EINVAL;
}

int
hwc_device_close(
    struct hw_device_t * dev
    )
{
    hwcContext * context = (hwcContext *) dev;

    if (context == NULL)
    {
        ALOGE("%s(%d): Invalid device!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    gcmVERIFY_OK(gco2D_FreeFilterBuffer(context->engine));
    gcmVERIFY_OK(gcoHAL_Destroy(context->hal));
    gcmVERIFY_OK(gcoOS_Destroy(context->os));

    free(context);
    return 0;
}

int
hwc_device_open(
    const struct hw_module_t * module,
    const char * name,
    struct hw_device_t ** device
    )
{
    gceSTATUS  status    = gcvSTATUS_OK;
    hwcContext * context = gcvNULL;

    ALOGV("%s(%d): Open hwc device", __FUNCTION__, __LINE__);

    if (strcmp(name, HWC_HARDWARE_COMPOSER) != 0)
    {
        ALOGE("%s(%d): Invalid device name!", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    _eglGetRenderBufferANDROID = (PFNEGLGETRENDERBUFFERANDROIDPROC)
                                 eglGetProcAddress("eglGetRenderBufferANDROID");

    if (_eglGetRenderBufferANDROID == NULL)
    {
        ALOGE("EGL_ANDROID_get_render_buffer extension not found for hwcomposer");
        return HWC_EGL_ERROR;
    }

    context = (hwcContext *) malloc(sizeof (hwcContext));
    memset(context, 0, sizeof (hwcContext));

    context->device.common.tag     = HARDWARE_DEVICE_TAG;
    context->device.common.version = 0;
    context->device.common.module  = (hw_module_t *) module;
    context->device.common.close   = hwc_device_close;
    context->device.prepare        = hwc_prepare;
    context->device.set            = hwc_set;

    gcmONERROR(gcoOS_Construct(gcvNULL, &context->os));
    gcmONERROR(gcoHAL_Construct(gcvNULL, context->os, &context->hal));

    context->separated2D =
        gcoHAL_QuerySeparated3D2D(context->hal) == gcvSTATUS_TRUE;

    if (context->separated2D)
    {
        gcmONERROR(gcoHAL_SetHardwareType(context->hal, gcvHARDWARE_2D));
    }

    if (!gcoHAL_IsFeatureAvailable(context->hal, gcvFEATURE_PIPE_2D))
    {
        ALOGE("%s(%d): 2D PIPE not found", __FUNCTION__, __LINE__);
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    gcmONERROR(gcoHAL_Get2DEngine(context->hal, &context->engine));

    context->pe20 = gcoHAL_IsFeatureAvailable(context->hal, gcvFEATURE_2DPE20);
    if (!context->pe20)
    {
        ALOGE("%s(%d): PE20 not supported", __FUNCTION__, __LINE__);
        gcmONERROR(gcvSTATUS_NOT_SUPPORTED);
    }

    context->multiSourceBlt   = gcoHAL_IsFeatureAvailable(context->hal, gcvFEATURE_2D_MULTI_SOURCE_BLT);
    context->multiSourceBltEx = gcoHAL_IsFeatureAvailable(context->hal, gcvFEATURE_2D_MULTI_SOURCE_BLT_EX);
    context->maxSource        = context->multiSourceBltEx ? 8 : 4;
    context->opf              = gcoHAL_IsFeatureAvailable(context->hal, gcvFEATURE_2D_TILING);

    if (context->separated2D)
    {
        gcmONERROR(gcoHAL_SetHardwareType(context->hal, gcvHARDWARE_3D));
    }

    *device = &context->device.common;

    ALOGI("Vivante HWComposer v2.0\n"
         "Device:               %p\n"
         "Separated 2D/3D:      %s\n"
         "2D PE20:              %s\n",
         (void *) context,
         (context->separated2D ? "YES" : "NO"),
         (context->pe20        ? "YES" : "NO"));

    return 0;

OnError:
    if (context != gcvNULL)
    {
        if (context->hal != gcvNULL)
        {
            gcmVERIFY_OK(gcoHAL_Destroy(context->hal));
            gcmVERIFY_OK(gcoOS_Destroy(context->os));
        }
        free(context);
    }

    *device = NULL;
    ALOGE("%s(%d): Failed to initialize hwcomposer!", __FUNCTION__, __LINE__);
    return -EINVAL;
}