/*
 * Copyright (C) 2016 The CyanogenMod Project
 *               2017 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <cutils/ashmem.h>
#include <log/log.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <binder/IPCThreadState.h>

#include "gc_gralloc_gr.h"
#include "gralloc_priv.h"
#include "mrvl_pxl_formats.h"

//#include <gc_hal_user.h>
//#include <gc_hal_base.h>

#if HAVE_ANDROID_OS
#if (MRVL_VIDEO_MEMORY_USE_TYPE == gcdMEM_TYPE_ION)
#include <linux/ion.h>
#include <linux/pxa_ion.h>
#else
#include <linux/android_pmem.h>
#endif
#endif

#if MRVL_SUPPORT_DISPLAY_MODEL
#include <dms_if.h>
extern android::sp<android::IDisplayModel> displayModel;
#endif

using namespace android;

static struct {
    int android_format;
    gceSURF_FORMAT hal_format;
} HAL_PIXEL_FORMAT_TABLE[] =
{
    { HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, gcvSURF_A8B8G8R8},
    { HAL_PIXEL_FORMAT_RGBA_8888             , gcvSURF_A8B8G8R8},
    { HAL_PIXEL_FORMAT_RGBX_8888             , gcvSURF_X8B8G8R8},
    { HAL_PIXEL_FORMAT_RGB_888               , gcvSURF_UNKNOWN },
    { HAL_PIXEL_FORMAT_RGB_565               , gcvSURF_R5G6B5  },
    { HAL_PIXEL_FORMAT_BGRA_8888             , gcvSURF_A8R8G8B8},
    { HAL_PIXEL_FORMAT_YCbCr_420_888         , gcvSURF_NV12    },
    { HAL_PIXEL_FORMAT_YV12                  , gcvSURF_YV12    }, // YCrCb 4:2:0 Planar
    { HAL_PIXEL_FORMAT_YCbCr_422_SP          , gcvSURF_NV16    }, // NV16
    { HAL_PIXEL_FORMAT_YCrCb_420_SP          , gcvSURF_NV21    }, // NV21
    { HAL_PIXEL_FORMAT_YCbCr_420_P           , gcvSURF_I420    },
    { HAL_PIXEL_FORMAT_YCbCr_422_I           , gcvSURF_YUY2    }, // YUY2
    { HAL_PIXEL_FORMAT_CbYCrY_422_I          , gcvSURF_UYVY    },
    { HAL_PIXEL_FORMAT_YCbCr_420_SP_MRVL     , gcvSURF_NV12    },
    { 0x00                                   , gcvSURF_UNKNOWN },
};

/*******************************************************************************
**
**  _ConvertAndroid2HALFormat
**
**  Convert android pixel format to Vivante HAL pixel format.
**
**  INPUT:
**
**      int Format
**          Specified android pixel format.
**
**  OUTPUT:
**
**      gceSURF_FORMAT HalFormat
**          Pointer to hold hal pixel format.
*/
gceSTATUS
_ConvertAndroid2HALFormat(
    int Format,
    gceSURF_FORMAT * HalFormat
    )
{
    *HalFormat = gcvSURF_UNKNOWN;
    for( int i = 0; ; ++i )
    {
        if( HAL_PIXEL_FORMAT_TABLE[i].android_format == 0 )
        {
            ALOGE("Unknown ANDROID format: %d", format);
            return gcvSTATUS_INVALID_ARGUMENT;
        }

        if( Format == HAL_PIXEL_FORMAT_TABLE[i].android_format )
        {
            *HalFormat = HAL_PIXEL_FORMAT_TABLE[i].hal_format;
            if( *HalFormat == gcvSURF_UNKNOWN )
            {
                ALOGE("Not supported ANDROID format: %d", format);
                return gcvSTATUS_INVALID_ARGUMENT;
            }
            return gcvSTATUS_OK;
        }
    }
}

int _ConvertFormatToSurfaceInfo(
    int Format,
    int Width,
    int Height,
    size_t *Xstride,
    size_t *Ystride,
    size_t *Size
    )
{
    //log_func_entry;
    size_t xstride = 0;
    size_t ystride = 0;
    size_t size = 0;

    switch(Format)
    {
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_MRVL:
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        xstride = _ALIGN(Width, 64);
        ystride = _ALIGN(Height, 64);
        size = xstride * ystride * 3 / 2;
        break;

    case HAL_PIXEL_FORMAT_RGB_888:
        xstride = _ALIGN(Width, 16);
        ystride = _ALIGN(Height, 4);
        size = xstride * ystride * 3;
        break;

    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        xstride = _ALIGN(Width, 16);
        ystride = _ALIGN(Height, 4);
        size = xstride * ystride * 4;
        break;

    case HAL_PIXEL_FORMAT_RGB_565:
        xstride = _ALIGN(Width, 16);
        ystride = _ALIGN(Height, 4);
        size = xstride * ystride * 2;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
        xstride = _ALIGN(Width, 64);
        ystride = _ALIGN(Height, 64);
        size = xstride * ystride * 2;
        break;

    case HAL_PIXEL_FORMAT_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        xstride = _ALIGN(Width, 16);
        ystride = _ALIGN(Height, 32);
        size = xstride * ystride * 2;
        break;

    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
        xstride = _ALIGN(Width, 16);
        ystride = _ALIGN(Height, 4);
        size = xstride * ystride * 4;
        break;

    default:
        return -EINVAL;
    }

    *Xstride = xstride;
    *Ystride = ystride;
    *Size = size;

    return 0;
}

/*******************************************************************************
**
**  _MapBuffer
**
**  Map android native buffer (call gc_gralloc_map).
**
**  INPUT:
**
**      gralloc_module_t const * Module
**          Specified gralloc module.
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**  OUTPUT:
**
**      void ** Vaddr
**          Point to save virtual address pointer.
*/
int
_MapBuffer(
    gralloc_module_t const * Module,
    private_handle_t * Handle,
    void **Vaddr)
{
    //log_func_entry;
    (void)Module;
    int retCode = 0;

    retCode = gc_gralloc_map(Handle, Vaddr);
    if( retCode >= 0 && (Handle->surfFormat - gcvSURF_YUY2) > 0x63 &&
       (Handle->flags & (private_handle_t::PRIV_FLAGS_USES_PMEM_ADSP|private_handle_t::PRIV_FLAGS_USES_PMEM)) == private_handle_t::PRIV_FLAGS_USES_PMEM )
    {
        gc_gralloc_flush(Handle, Handle->flags);
    }

    return retCode;
}

/*******************************************************************************
**
**  gc_gralloc_alloc_buffer
**
**  Allocate android native buffer.
**  Will allocate surface types as follows,
**
**  +------------------------------------------------------------------------+
**  | To Allocate Surface(s)        | Linear(surface) | Tile(resolveSurface) |
**  |------------------------------------------------------------------------|
**  |Enable              |  CPU app |      yes        |                      |
**  |LINEAR OUTPUT       |  3D app  |      yes        |                      |
**  |------------------------------------------------------------------------|
**  |Disable             |  CPU app |      yes        |                      |
**  |LINEAR OUTPUT       |  3D app  |                 |         yes          |
**  +------------------------------------------------------------------------+
**
**
**  INPUT:
**
**      alloc_device_t * Dev
**          alloc device handle.
**
**      int Width
**          Specified target buffer width.
**
**      int Hieght
**          Specified target buffer height.
**
**      int Format
**          Specified target buffer format.
**
**      int Usage
**          Specified target buffer usage.
**
**  OUTPUT:
**
**      buffer_handle_t * Handle
**          Pointer to hold the allocated buffer handle.
**
**      int * Stride
**          Pointer to hold buffer stride.
*/

#define gcoOS_DebugStatus2Name(...)  -1
#define gcoOS_DebugTrace(...)

extern int gc_gralloc_alloc(alloc_device_t *Dev, int Width, int Height, int Format, int Usage, buffer_handle_t *Handle, int *Stride)
{
    android::IPCThreadState *ipc; // r0
    signed int isPmemAlloc; // r8
    int32_t status; // r0 MAPDST
    unsigned int height; // r8
    int dirtyWidthAligned16; // r4
    unsigned int v24; // r11
    int samples; // r11
    gceSURF_TYPE surfType; // r9
    int v27; // r0
    int xstride; // r7
    int ystride; // r6
    int v33; // r9 MAPDST
    signed int isGcAlloc; // r5
    signed int v36; // r0
    int32_t v42; // r6
    private_handle_t *handle; // r0 MAPDST
    int32_t size; // r3
    int v64; // r1
    int v68; // r0
    int v70; // r0
    int master; // [sp+18h] [bp-90h]
    unsigned int size_rounded_to_page; // [sp+1Ch] [bp-8Ch]
    int dirtyHeight; // [sp+20h] [bp-88h]
    signed int fd; // [sp+24h] [bp-84h] MAPDST
    int dirtyWidth; // [sp+28h] [bp-80h]
    hw_module_t *dev; // [sp+2Ch] [bp-7Ch]
    int buffFlags; // [sp+30h] [bp-78h] MAPDST
    int callingPID; // [sp+34h] [bp-74h] MAPDST
    gceHARDWARE_TYPE hwtype; // [sp+3Ch] [bp-6Ch]
    gceSURF_FORMAT format; // [sp+44h] [bp-64h] MAPDST
    gcePOOL resolvePool;
    gcoSURF Surface; // [sp+4Ch] [bp-5Ch] MAPDST
    int name; // [sp+50h] [bp-58h]
    int v94; // [sp+54h] [bp-54h]
    int shAddr; // [sp+58h] [bp-50h] MAPDST
    gctUINT32 pixelPipes; // [sp+5Ch] [bp-4Ch]
    int stride; // [sp+60h] [bp-48h] MAPDST
    int alignedHeight; // [sp+64h] [bp-44h]
    gctUINT32 resolveVidNode;
    gctSIZE_T resolveAdjustedSize;
    uint32_t physAddr; // [sp+70h] [bp-38h]
    void *Vaddr; // [sp+74h] [bp-34h] MAPDST
    int alignedWidth2; // [sp+78h] [bp-30h]
    int alignedHeight2; // [sp+7Ch] [bp-2Ch] MAPDST

    dirtyHeight = Height;
    dirtyWidth = Width;
    hwtype = (gceHARDWARE_TYPE)1;
    if ( !Handle || !Stride )
        return -EINVAL;

    gcoHAL_GetHardwareType(0, &hwtype);
    setHwType71D0(Usage);
    fd = open("/dev/null", 0, 0);
    stride = 0;
    Surface = 0;
    shAddr = 0;
    stride = 0;
    alignedHeight = 0;
    resolveVidNode = 0;
    resolveAdjustedSize = 0;
    physAddr = 0;
    ipc = android::IPCThreadState::self();
    callingPID = ipc->getCallingPid();
    dev = Dev->common.module;
    handle = NULL;

    _ConvertAndroid2HALFormat(Format, &format);
    if ( format == gcvSURF_UNKNOWN )
    {
        ALOGE("error: HAL_PIXEL_FORMAT %x\n", Format);
        master = -1;
        goto OnError;
    }

    status = gcoHAL_QueryPixelPipesInfo(&pixelPipes, 0, 0);
    if ( status < 0 )
    {
      master = -1;
      goto OnError;
    }
    if ( (Usage & 0x10200) != GRALLOC_USAGE_HW_RENDER )
    {
        isPmemAlloc = 1;

        buffFlags = private_handle_t::PRIV_FLAGS_USES_PMEM;
      if ( Usage & GRALLOC_USAGE_HW_VIDEO_ENCODER )
        buffFlags |= private_handle_t::PRIV_FLAGS_USES_PMEM_ADSP;
      height =  _ALIGN(dirtyHeight, 4*pixelPipes);
      dirtyWidthAligned16 = _ALIGN(dirtyWidth, 16);
      v24 = ((unsigned int)Usage >> 2) & 1;
      resolvePool = gcvPOOL_USER;

      _ConvertFormatToSurfaceInfo(Format, Width, Height,
                                 (size_t*)&xstride, (size_t*)&ystride, (size_t*)&size);

          size_rounded_to_page = _ALIGN(size, 4096);
          v33 = errno;

          if ( Usage < 0 )
          {
            master = mvmem_alloc(size_rounded_to_page, 0x30001, 0x1000);
            if ( master >= 0 )
            {
              isGcAlloc = 1;
              goto LABEL_78;
            }
            ALOGW("WARNING: continuous ion memory alloc failed. Try to alloc non-continuous ion memory.");
          }
          master = mvmem_alloc(size_rounded_to_page, 0x30002, 0x1000);
          if ( master < 0 )
          {
            v33 = -v33;
            ALOGE("Failed to allocate memory from ION");
            master = v33;
          }
          else
          {
              isGcAlloc = 0;
    LABEL_78:
              mvmem_set_name(master, "gralloc");
              if ( isGcAlloc )
              {
                v33 = mvmem_get_phys(master, (int*)&physAddr);
                if ( v33 < 0 )
                {
                    mvmem_free(master);
                    ALOGE("Failed to allocate memory from ION");
                    master = v33;
                }
              }
              else
              {
                physAddr = 0;
              }
          }
          // TODO, reverse from here now
          v36 = 0x2000006;
          if ( !v24 )
            v36 = 6;
          status = gcoSURF_Construct(0, dirtyWidthAligned16, height, 1, (gceSURF_TYPE)v36, format, gcvPOOL_USER, &Surface);
          if ( status < 0 )
          {
            goto OnError;
          }
          status = gcoSURF_GetAlignedSize(Surface, (uint32_t*)&stride, (uint32_t*)&alignedHeight, 0);
          if ( status < 0 )
          {
            goto OnError;
          }
          status = gcoSURF_QueryVidMemNode(Surface, (_gcuVIDMEM_NODE**)&resolveVidNode, &resolvePool, (gctUINT_PTR)&resolveAdjustedSize);
          if ( status < 0 )
          {
            goto OnError;
          }
          if ( dirtyWidthAligned16 != dirtyWidth || height != dirtyHeight )
          {
            status = gcoSURF_SetRect(Surface, dirtyWidth, dirtyHeight);
            if ( status < 0 )
            {
              goto OnError;
            }
          }
          dirtyHeight = height;
          samples = 0;
          dirtyWidth = dirtyWidthAligned16;
          surfType = gcvSURF_BITMAP;
          goto AllocBuffer;
    }
    isPmemAlloc = 0;
    samples = (uint8_t)Usage;
    if ( Usage & (GRALLOC_USAGE_SW_READ_MASK|GRALLOC_USAGE_SW_WRITE_MASK) )
    {
      if ( (Usage & GRALLOC_USAGE_SW_WRITE_OFTEN) == GRALLOC_USAGE_SW_WRITE_OFTEN
        || (Usage & GRALLOC_USAGE_SW_READ_OFTEN) == GRALLOC_USAGE_SW_READ_OFTEN )
      {
        samples = 0;
        resolvePool = gcvPOOL_CONTIGUOUS;
        surfType = gcvSURF_CACHEABLE_BITMAP;
      }
      else
      {
        samples = 0;
        resolvePool = gcvPOOL_DEFAULT;
        surfType = gcvSURF_BITMAP;
      }
    }
    else
    {
        v27 = Usage & 0x700000;
        if ( v27 == GRALLOC_USAGE_RENDERSCRIPT )
        {
          resolvePool = gcvPOOL_DEFAULT;
          surfType = gcvSURF_RENDER_TARGET;
        }
        else
        {
            switch ( v27 )
            {
              case GRALLOC_USAGE_FOREIGN_BUFFERS:
                resolvePool = gcvPOOL_DEFAULT;
                surfType = gcvSURF_RENDER_TARGET_NO_TILE_STATUS;
                break;

              case GRALLOC_USAGE_FOREIGN_BUFFERS|GRALLOC_USAGE_RENDERSCRIPT:
                resolvePool = gcvPOOL_DEFAULT;
                surfType = (gceSURF_TYPE)0x40004;
                break;

              case GRALLOC_USAGE_MRVL_PRIVATE_1:
                resolvePool = gcvPOOL_DEFAULT;
                samples = 4;
                surfType = gcvSURF_RENDER_TARGET;
                break;

              default:
                if ( v27 != 0x500000 && (v27 == 0x600000 || Usage & 0xA00 || !(Usage & GRALLOC_USAGE_HW_TEXTURE)) )
                {
                  samples = 0;
                  resolvePool = gcvPOOL_DEFAULT;
                  surfType = gcvSURF_BITMAP;
                  break;
                }
                samples = 0;
                resolvePool = gcvPOOL_DEFAULT;
                surfType = gcvSURF_TEXTURE;
                break;
            }
        }
    }

    master = open("/dev/null", 0, 0);

     if( surfType == gcvSURF_RENDER_TARGET )
    {
        switch( format )
        {
            case gcvSURF_R5G5B5A1:
                format = gcvSURF_A1R5G5B5;
                break;

            case gcvSURF_R4G4B4A4:
                format = gcvSURF_A4R4G4B4;
                break;

            case gcvSURF_X8B8G8R8:
                format = gcvSURF_X8R8G8B8;
                break;

            case gcvSURF_YUY2:
            case gcvSURF_UYVY:
            case gcvSURF_YV12:
            case gcvSURF_I420:
            case gcvSURF_NV12:
            case gcvSURF_NV21:
            case gcvSURF_NV16:
            case gcvSURF_NV61:
                format = gcvSURF_YUY2;
                break;

            case gcvSURF_A8B8G8R8:
                format = gcvSURF_A8R8G8B8;
                break;

            case gcvSURF_A8R8G8B8:
                format = gcvSURF_A8R8G8B8;
                break;

            case gcvSURF_R5G6B5:
                break;

            default:
                format = gcvSURF_A8R8G8B8;
        }
    }
    else if( surfType == gcvSURF_TEXTURE )
    {
        if( gcoTEXTURE_GetClosestFormat(gcvNULL, format, &format) < 0 )
        {
            handle = 0;
            isPmemAlloc = 0;
            goto OnError;
        }
    }

    if ( Usage & GRALLOC_USAGE_PROTECTED )
      *(int32_t*)&surfType += 0x8000u;

    status = gcoSURF_Construct(0, Width, Height, 1, surfType, format, resolvePool, &Surface);
    if ( status >= 0
      || resolvePool == gcvPOOL_CONTIGUOUS
      && (resolvePool = gcvPOOL_VIRTUAL,
          status = gcoSURF_Construct(0, Width, Height, 1, surfType, format, gcvPOOL_VIRTUAL, &Surface),
          status >= 0) )
    {
      if ( (Usage & 0x700000) == 0x600000 )
      {
        status = gcoSURF_Unlock(Surface, 0);
        if ( status < 0 )
        {
            handle = 0;
            isPmemAlloc = 0;
            goto OnError;
        }
        setHwType71D0(0);
        status = gcoSURF_Lock(Surface, 0, 0);
        if ( status < 0 )
        {
            handle = 0;
            isPmemAlloc = 0;
            goto OnError;
        }
        setHwType71D0(Usage);
      }
      if ( (unsigned int)samples <= 1 || (status = gcoSURF_SetSamples(Surface, 4), status >= 0) )
      {
        status = gcoSURF_SetFlags(Surface, gcvSURF_FLAG_CONTENT_YINVERTED, gcvTRUE);
        if ( status >= 0 )
        {
          status = gcoSURF_GetAlignedSize(Surface, (gctUINT*)&stride, 0, 0);
          if ( status >= 0 )
          {
            xstride = 0;
            ystride = 0;
            isPmemAlloc = 0;
            size_rounded_to_page = 0;
            Surface->totalSize[50] = 1;
            buffFlags = 0;
            goto AllocBuffer;
          }
        }
      }
    }
    else
    {
      ALOGE("Failed to construct surface");
    }
    handle = 0;
    isPmemAlloc = 0;
    goto OnError;

AllocBuffer:
    status = gcoSURF_AllocShBuffer(Surface, (void**)&shAddr);
    if ( status < 0 )
    {
        goto OnError;
    }
    size = stride * dirtyHeight;
    handle = new private_handle_t(fd, size, buffFlags);
    handle->format = Format;
    handle->surfType = surfType;
    handle->samples = samples;
    handle->height = dirtyHeight;
    handle->width = dirtyWidth;
    handle->surface = Surface;
    handle->surfaceHigh32Bits = 0;
    handle->surfFormat = format;
    handle->size = Surface->totalSize[22];
    handle->clientPID = callingPID;
    handle->allocUsage = Usage;
    handle->signal = 0;
    handle->signalHigh32Bits = 0;
    handle->lockUsage = 0;
    handle->shAddr = (gctSHBUF)shAddr;
    handle->shAddrHight32Bits = 0;
    handle->master = master;
    if ( isPmemAlloc )
    {
        handle->lockAddr = physAddr;
        handle->pool = (gcePOOL)Surface->totalSize[27];
        handle->infoB2 = Surface->totalSize[39];
        handle->infoA2 = Surface->totalSize[90];
        v64 = Surface->totalSize[102];
        handle->mem_ystride = ystride;
        handle->infoB3 = v64;
        handle->size = size_rounded_to_page;
        handle->mem_xstride = xstride;
        v42 = _MapBuffer((gralloc_module_t*)dev, handle, &Vaddr);
        if ( v42 < 0 )
        {
            ALOGE("gc_gralloc_map memory error");
            fd = -1;
            goto OnError;
        }
        status = gcoSURF_MapUserSurface(Surface, 0, (gctPOINTER)handle->base, -1);
        if ( status < 0 )
        {
            goto OnError;
        }
    }
    else
    {
        v68 = Surface->totalSize[41];
        alignedWidth2 = 0;
        alignedHeight2 = 0;
        name = v68;
        gcoHAL_NameVideoMemory(v68, (gctUINT32*)&name);
        handle->infoA1 = name;
        handle->pool = (gcePOOL)Surface->totalSize[27];
        handle->infoB2 = Surface->totalSize[39];
        v70 = Surface->totalSize[104];
        v94 = v70;
        if ( v70 )
            gcoHAL_NameVideoMemory(v70, (gctUINT32*)&v94);

        handle->infoB1 = v94;
        handle->infoA2 = Surface->totalSize[90];
        handle->infoB3 = Surface->totalSize[102];
        status = gcoSURF_GetAlignedSize(Surface, (gctUINT*)&alignedWidth2, (gctUINT*)&alignedHeight2, 0);
        if ( status < 0 )
        {
            fd = -1;
            goto OnError;
        }
        handle->mem_xstride = alignedWidth2;
        handle->mem_ystride = alignedHeight2;
        v42 = _MapBuffer((gralloc_module_t*)dev, handle, &Vaddr);
    }
    if ( !v42 )
    {
        *Handle = handle;
        *Stride = stride;
        if ( Vaddr && !(Usage & GRALLOC_USAGE_HW_RENDER) )
        memset(Vaddr, 0, handle->size);

        gcoHAL_SetHardwareType(0, hwtype);
        return 0;
    }
    status = -13;
    fd = -1;

OnError:
  // End of the part if pixel format is not recognized
  if ( Surface )
    gcoSURF_Destroy(Surface);

  if ( handle )
  {
    handle->magic = 0;
    operator delete(handle);
  }
  if ( fd > 0 )
    close(fd);
  if ( master >= 0 )
  {
    if ( isPmemAlloc )
      mvmem_free(master);
    else
      close(master);
  }
  ALOGE("Failed to allocate, status=%d", status);
  gcoHAL_SetHardwareType(0, hwtype);
  return -EINVAL;
}

/*******************************************************************************
**
**  gc_gralloc_alloc
**
**  General buffer alloc.
**
**  INPUT:
**
**      alloc_device_t * Dev
**          alloc device handle.
**
**      int Width
**          Specified target buffer width.
**
**      int Height
**          Specified target buffer height.
**
**      int Format
**          Specified target buffer format.
**
**      int Usage
**          Specified target buffer usage.
**
**  OUTPUT:
**
**      buffer_handle_t * Handle
**          Pointer to hold the allocated buffer handle.
**
**      int * Stride
**          Pointer to hold buffer stride.
*/

/*
int
gc_gralloc_alloc(
    alloc_device_t * Dev,
    int Width,
    int Height,
    int Format,
    int Usage,
    buffer_handle_t * Handle,
    int * Stride)
{
    //log_func_entry;

    int ret;
    gceHARDWARE_TYPE hwtype = gcvHARDWARE_3D;

    if (!Handle || !Stride)
    {
        return -EINVAL;
    }

    // Must be set current hardwareType to 3D, due to surfaceflinger need tile
    // surface.
    gcoHAL_GetHardwareType(0, &hwtype);
    setHwType71D0(Usage);

    ret = gc_gralloc_alloc_buffer(Dev, Width, Height, Format, Usage, Handle, Stride);

    gcoHAL_SetHardwareType(0, hwtype);

    return ret;
}
*/
int setHwType71D0(int AllocUsage)
{
    //log_func_entry;
    static gceHARDWARE_TYPE hwtype = gcvHARDWARE_INVALID;
    int buffer[80];

    if( hwtype == gcvHARDWARE_INVALID )
    {
        buffer[0] = 39;
        gcoOS_DeviceControl(0, 30000, buffer, sizeof(buffer), buffer, sizeof(buffer));
        for ( int i = 0; i < buffer[8]; ++i )
        {
            switch ( buffer[i+9] )
            {
                case gcvHARDWARE_3D:
                    hwtype = gcvHARDWARE_3D;
                    break;
                case gcvHARDWARE_3D2D:
                    hwtype = gcvHARDWARE_3D2D;
                    break;
                case gcvHARDWARE_2D:
                    if ( !(hwtype & ~gcvHARDWARE_VG) )
                        hwtype = gcvHARDWARE_2D;
                    break;
                case gcvHARDWARE_VG:
                    if ( hwtype == gcvHARDWARE_INVALID )
                        hwtype = gcvHARDWARE_VG;
                    break;
                default:
                    continue;
            }
        }
        if ( !hwtype )
            ALOGE("Failed to get hardware types");
    }
    if( (AllocUsage & (GRALLOC_USAGE_RENDERSCRIPT|GRALLOC_USAGE_FOREIGN_BUFFERS|GRALLOC_USAGE_MRVL_PRIVATE_1)) == (GRALLOC_USAGE_FOREIGN_BUFFERS|GRALLOC_USAGE_MRVL_PRIVATE_1) )
        return gcoHAL_SetHardwareType(0, gcvHARDWARE_VG);
    else
        return gcoHAL_SetHardwareType(0, hwtype);
}

/*******************************************************************************
**
**  gc_gralloc_free
**
**  General buffer free.
**
**  INPUT:
**
**      alloc_device_t * Dev
**          alloc device handle.
**
**      buffer_handle_t Handle
**          Specified target buffer to free.
**
**  OUTPUT:
**
**      Nothing.
*/
int
gc_gralloc_free(
    alloc_device_t * Dev,
    buffer_handle_t Handle
    )
{
    //log_func_entry;
    (void)Dev;
    gceHARDWARE_TYPE hwtype = gcvHARDWARE_3D;

    if( private_handle_t::validate(Handle) )
    {
        return -EINVAL;
    }

    /* Cast private buffer handle. */
    private_handle_t * hnd = (private_handle_t*)Handle;

    gcoHAL_GetHardwareType(0, &hwtype);
    setHwType71D0(hnd->allocUsage);
    if( hnd->base )
        gc_gralloc_unmap(hnd);

    if( hnd->surface )
    {
        if( (hnd->allocUsage & (GRALLOC_USAGE_MRVL_PRIVATE_1|GRALLOC_USAGE_FOREIGN_BUFFERS|GRALLOC_USAGE_RENDERSCRIPT)) == (GRALLOC_USAGE_MRVL_PRIVATE_1|GRALLOC_USAGE_FOREIGN_BUFFERS) )
        {
            setHwType71D0(GRALLOC_USAGE_SW_READ_NEVER);
            gcoSURF_Unlock(hnd->surface, 0);
            setHwType71D0(hnd->allocUsage);
        }
        gcoSURF_Destroy(hnd->surface);
    }
    if( hnd->signal )
    {
        gcoOS_DestroySignal(0, hnd->signal);
        hnd->signal = NULL;
        hnd->signalHigh32Bits = 0;
    }
    hnd->clientPID = 0;
    gcoHAL_Commit(0,gcvFALSE);
    if( hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM )
    {
        mvmem_free(hnd->master);
        if( hnd->fd >= 0 )
            close(hnd->fd);
    }
    else
    {
        close(hnd->fd);
        close(hnd->master);
    }

    hnd->magic = 0;
    delete hnd;
    gcoHAL_SetHardwareType(0, hwtype);

    return 0;
}

int gc_gralloc_notify_change(buffer_handle_t Handle)
{
    //log_func_entry;
    private_handle_t *hnd = (private_handle_t*)Handle;
    if( private_handle_t::validate(hnd) )
        return -EINVAL;

    if( hnd->surface == NULL )
        return -EINVAL;

    gcoSURF_UpdateTimeStamp(hnd->surface);
    gcoSURF_PushSharedInfo(hnd->surface);

    return 0;
}

int setHwType71D4()
{
    //log_func_entry;
    static gceHARDWARE_TYPE hwtype = gcvHARDWARE_INVALID;
    int buffer[80];

    if( hwtype == 0 )
    {
        buffer[0] = 39;
        gcoOS_DeviceControl(0, 30000, buffer, sizeof(buffer), buffer, sizeof(buffer));
        for( int i = 0; i < buffer[8]; ++i )
        {
            switch( buffer[i+9] )
            {
                case gcvHARDWARE_3D: hwtype = gcvHARDWARE_3D; break;
                case gcvHARDWARE_3D2D: hwtype = gcvHARDWARE_3D2D; break;
                case gcvHARDWARE_2D:
                    if( (hwtype & ~gcvHARDWARE_VG) == 0 )
                        hwtype = gcvHARDWARE_2D;
                    break;
                case gcvHARDWARE_VG:
                    if( hwtype == gcvHARDWARE_INVALID )
                        hwtype = gcvHARDWARE_VG;
                    break;
            }
        }
        if( hwtype == gcvHARDWARE_INVALID )
            ALOGE("Failed to get hardware types");
    }
    return gcoHAL_SetHardwareType(0, hwtype);
}

int
gc_gralloc_unwrap(buffer_handle_t Handle)
{
    //log_func_entry;
    gceHARDWARE_TYPE hwtype = gcvHARDWARE_3D;
    private_handle_t *hnd = (private_handle_t*)Handle;
    if( private_handle_t::validate(hnd) )
        return -EINVAL;

    gcoHAL_GetHardwareType(0, &hwtype);
    setHwType71D4();
    gcoSURF_Unlock(hnd->surface, 0);
    gcoSURF_Destroy(hnd->surface);
    gcoHAL_Commit(0, gcvTRUE);
    memset(&hnd->surface, 0, 108);
    gcoHAL_SetHardwareType(0, hwtype);
    return 0;
}

int
gc_gralloc_wrap(buffer_handle_t Handle, int w, int h, int format, int stride, int offset, void *vaddr)
{
   //log_func_entry;
    private_handle_t *hnd = (private_handle_t*)Handle;
    int status;
    gcoSURF surface = 0;
    gceSURF_FORMAT surfFormat;
    gceHARDWARE_TYPE hwtype = gcvHARDWARE_3D;
    gctUINT32 Address;
    gctUINT32 physical = offset;
    gctUINT32 base_addr;
    gctSHBUF shbuf;
    gcsSURF_FORMAT_INFO_PTR formatInfo;

    if( private_handle_t::validate(hnd) )
        return -EINVAL;

    if( vaddr == NULL)
    {
        ALOGE("%s Invalid virtual address", __FUNCTION__);
        return -EINVAL;
    }

    if( _ConvertAndroid2HALFormat(format, &surfFormat) != gcvSTATUS_OK )
    {
        return -EINVAL;
    }

    gcoHAL_GetHardwareType(0, &hwtype);
    status = gcoSURF_QueryFormat(surfFormat, &formatInfo);
    if( status >= 0 )
    {
        if( (surfFormat-gcvSURF_YUY2) <= 9 )
        {
            if( (1 << (surfFormat+12)) & 0x303 )
            {
                stride *= 2;
            }
            else if( (1 << (surfFormat+12)) & 0xFC )
            {
            }
            else
            {
                stride *= formatInfo->interleaved >> 3;
            }
        }
        else
        {
            stride *= formatInfo->interleaved >> 3;
        }
        memset(&hnd->surface, 0, 108);
        setHwType71D4();
        if( offset != -1 )
        {
            status = gcoOS_GetBaseAddress(0, &base_addr);
            if( status < 0 )
            {
                if( surface )
                    gcoSURF_Destroy(surface);
                gcoHAL_SetHardwareType(0, hwtype);
                return -EFAULT;
            }
            physical = offset - base_addr;
            if( (signed int)(physical) < 0 )
            {
                physical = -1;
            }
            else if( (signed int)(physical + h * stride - 1) < 0 )
            {
                physical = -1;
            }
        }
        status = gcoSURF_Construct(0, w, h, 1, gcvSURF_BITMAP, surfFormat, gcvPOOL_USER, &surface);
        if( status >= 0 )
        {
            status = gcoSURF_SetBuffer(surface, gcvSURF_BITMAP, surfFormat, stride, vaddr, physical);
            if( status >= 0 )
            {
                status = gcoSURF_SetWindow(surface, 0, 0, w, h);
                if( status >= 0 )
                {
                    status = gcoSURF_Lock(surface, &Address, (gctPOINTER*)&base_addr);
                    if( status >= 0 )
                    {
                        status = gcoSURF_SetFlags(surface, gcvSURF_FLAG_CONTENT_YINVERTED, gcvTRUE);
                        if( status >= 0 )
                        {
                            status = gcoSURF_AllocShBuffer(surface, &shbuf);
                            if( status >= 0 )
                            {
                                hnd->stride = stride;
                                hnd->surface = surface;
                                hnd->surfaceHigh32Bits = 0;
                                hnd->lockAddr = Address;
                                hnd->height = h;
                                hnd->width = w;
                                hnd->format = format;
                                hnd->surfFormat = surfFormat;
                                hnd->shAddr = shbuf;
                                hnd->shAddrHight32Bits = 0;
                                gcoHAL_SetHardwareType(0, hwtype);
                                return 0;
                            }
                            else
                                ALOGE("%s: Failed gcoSURF_AllocShBuffer", __FUNCTION__);
                        }
                        else
                            ALOGE("%s: Failed gcoSURF_SetFlags", __FUNCTION__);
                    }
                    else
                        ALOGE("%s: Failed gcoSURF_Lock", __FUNCTION__);
                }
                else
                    ALOGE("%s: Failed gcoSURF_SetWindow", __FUNCTION__);
            }
            else
                ALOGE("%s: Failed gcoSURF_Buffer", __FUNCTION__);
        }
        else
            ALOGE("%s: Failed gcoSURF_Construct", __FUNCTION__);
    }

    ALOGE("failed to wrap handle=%p", hnd);

    if( surface )
        gcoSURF_Destroy(surface);

    gcoHAL_SetHardwareType(0, hwtype);

    return -EFAULT;
}

int
gc_gralloc_register_wrap(private_handle_t *Handle, int32_t offset, void* Vaddr)
{
    //log_func_entry;
    gceHARDWARE_TYPE hwtype = gcvHARDWARE_3D;
    gctUINT32 Memory;
    int status;
    gcoSURF surface = 0;
    gctUINT32 Address;

    if( private_handle_t::validate(Handle) )
        return -EINVAL;

    if( Vaddr == NULL )
    {
        ALOGE("Invalid virtual address");
        return -EINVAL;
    }

    if( Handle->surface == NULL )
        return -EINVAL;

    if( Handle->height == 0 )
        return -EINVAL;
    if( Handle->width == 0 )
        return -EINVAL;

    if( Handle->format == gcvSURF_UNKNOWN )
        return -EINVAL;

    if( Handle->surfFormat == 0 )
        return -EINVAL;

    gcoHAL_GetHardwareType(0, &hwtype);
    setHwType71D4();
    if( offset != -1 )
    {
        status = gcoOS_GetBaseAddress(0, &Memory);
        if( status < 0 )
        {
            gcoHAL_SetHardwareType(0, hwtype);
            return -EFAULT;
        }
        offset -= (uint32_t)Memory;
    }
    status = gcoSURF_Construct(0, Handle->width, Handle->height, 1, gcvSURF_BITMAP, Handle->surfFormat, gcvPOOL_USER, &surface);
    if( status < 0 )
    {
        if( surface )
        {
            gcoSURF_Destroy(surface);
            gcoHAL_Commit(0, gcvFALSE);
        }
        gcoHAL_SetHardwareType(0, hwtype);
        return -EFAULT;
    }

    status = gcoSURF_SetBuffer(surface, gcvSURF_BITMAP, Handle->surfFormat, Handle->stride, Vaddr, offset);
    if( status < 0 )
    {
        gcoSURF_Destroy(surface);
        gcoHAL_Commit(0, gcvFALSE);
        gcoHAL_SetHardwareType(0, hwtype);
        return -EFAULT;
    }

    status = gcoSURF_SetWindow(surface, 0, 0, Handle->width, Handle->height);
    if( status < 0 )
    {
        gcoSURF_Destroy(surface);
        gcoHAL_Commit(0, gcvFALSE);
        gcoHAL_SetHardwareType(0, hwtype);
        return -EFAULT;
    }

    status = gcoSURF_Lock(surface, &Address, (void**)&Memory);
    if( status < 0 )
    {
        gcoSURF_Destroy(surface);
        gcoHAL_Commit(0, gcvFALSE);
        gcoHAL_SetHardwareType(0, hwtype);
        return -EFAULT;
    }

    status = gcoSURF_SetFlags(surface, gcvSURF_FLAG_CONTENT_YINVERTED, gcvTRUE);
    if( status < 0 )
    {
        gcoSURF_Destroy(surface);
        gcoHAL_Commit(0, gcvFALSE);
        gcoHAL_SetHardwareType(0, hwtype);
        return -EFAULT;
    }

    if( Handle->shAddr )
        gcoSURF_BindShBuffer(surface, Handle->shAddr);

    Handle->surfaceHigh32Bits = 0;
    Handle->surface = surface;
    Handle->lockAddr = Address;

    gcoHAL_SetHardwareType(0, hwtype);
    return 0;
}
