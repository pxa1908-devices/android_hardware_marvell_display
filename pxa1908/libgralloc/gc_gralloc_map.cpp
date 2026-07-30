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
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <linux/version.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <log/log.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include "mrvl_pxl_formats.h"
#include "gc_gralloc_gr.h"
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

#if gcdDEBUG
/* Must be included after Vivante headers. */
#include <utils/Timers.h>
#endif

/* We need thsi for now because pmem can't mmap at an offset. */
#define PMEM_HACK       1

/* desktop Linux needs a little help with gettid() */
#if defined(ARCH_X86) && !defined(HAVE_ANDROID_OS)
#define __KERNEL__
# include <linux/unistd.h>
pid_t gettid() { return syscall(__NR_gettid);}
#undef __KERNEL__
#endif

/*******************************************************************************
**
**  gc_gralloc_map
**
**  Map android native buffer to userspace.
**  This is not for client. So only the linear surface is exported since only
**  linear surface can be used for software access.
**  Reference table in gralloc_alloc_buffer(gc_gralloc_alloc.cpp)
**
**  INPUT:
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**      void ** Vaddr
**          Point to save virtual address pointer.
*/
int
gc_gralloc_map(
    buffer_handle_t Handle,
    void** Vaddr
    )
{
    //log_func_entry;
    uint32_t Address;
    void *Memory;
    private_handle_t * hnd = (private_handle_t *) Handle;

    /* Map if non-framebuffer and not mapped. */
    if( hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER )
    {
        *Vaddr = (void*)hnd->base;
        return 0;
    }

    if(hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM)
    {
        void* mem = mvmem_mmap(hnd->master, hnd->size, 0);
        if( mem == (void*)-1 )
        {
            ALOGE("Could not mmap: %s", strerror(errno));
            ALOGE("ION MAP failed (%s), fd=%d, shared fd=%d, size=%d", strerror(errno), hnd->fd, hnd->master, hnd->size);
            ALOGE("Failed to map hnd=%p", hnd);
            return -EINVAL;
        }
        hnd->base = hnd->offset + (int)mem;
        *Vaddr = (void*)hnd->base;
        return 0;
    }

    if( gcoSURF_Lock(hnd->surface, &Address, &Memory) < 0 )
    {
        ALOGE("Failed to map hnd=%p", hnd);
        return -EINVAL;
    }
    hnd->base = (int64_t)Memory;
    hnd->lockAddr = Address;
    *Vaddr = (void*)hnd->base;

    return 0;
}

/*******************************************************************************
**
**  gc_gralloc_unmap
**
**  Unmap mapped android native buffer.
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
**      Nothing.
**
*/
int
gc_gralloc_unmap(
    buffer_handle_t Handle
    )
{
    //log_func_entry;
    private_handle_t * hnd = (private_handle_t *) Handle;

    /* Only unmap non-framebuffer and mapped buffer. */
    if( hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER )
        return 0;

    if( hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM )
    {
        if( mvmem_munmap((void*)hnd->base, hnd->size) )
        {
            ALOGE("Could not unmap: %s", strerror(errno));
        }
        hnd->base = 0;
        return 0;
    }

    if( gcoSURF_Unlock(hnd->surface, 0) < 0 )
    {
        ALOGE("Failed to unmap buffer=%p", hnd);
        return -EINVAL;
    }

    hnd->lockAddr = 0;
    hnd->base = 0;
    return 0;
}

/*******************************************************************************
**
**  gc_gralloc_register_buffer
**
**  Register buffer to current process (in client).
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
**      Nothing.
**
*/
int
gc_gralloc_register_buffer(
    gralloc_module_t const * Module,
    buffer_handle_t Handle
    )
{
    (void*)Module;
    //log_func_entry;

    private_handle_t *hnd = (private_handle_t*)Handle;
    int status;
    gcoSURF surface = NULL;
    void *Vaddr = NULL;
    gctSIGNAL signal = 0;
    gctUINT32 videoMem = 0;

    gceHARDWARE_TYPE hwtype = gcvHARDWARE_3D;
    if( private_handle_t::validate(hnd) )
        return -EINVAL;

    if( gcoOS_ModuleConstructor() < 0 )
    {
ON_ERROR:
        if( Vaddr )
            gc_gralloc_unmap(hnd);
        if( surface )
            gcoSURF_Destroy(surface);
        if( signal )
            gcoOS_DestroySignal(0, hnd->signal);

        hnd->base = 0;
        hnd->surface = NULL;
        hnd->surfaceHigh32Bits = 0;
        hnd->signal = NULL;
        hnd->signalHigh32Bits = 0;
        gcoHAL_SetHardwareType(0, hwtype);
        return -EINVAL;
    }

    gcoHAL_GetHardwareType(0, &hwtype);
    setHwType71D0(hnd->allocUsage);

    if( hnd->surface )
    {
        if( hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM )
        {
            int flags = gcvSURF_BITMAP;
            if( hnd->allocUsage & 4 )
                flags += 0x2000000;

            if( gcoSURF_Construct(0, hnd->width, hnd->height, 1, (gceSURF_TYPE)flags, hnd->surfFormat, hnd->pool, &surface) < 0 )
            {
                goto ON_ERROR;
            }

            status = gc_gralloc_map(hnd, &Vaddr);
            if( status == gcvSTATUS_OK )
            {
                if( gcoSURF_MapUserSurface(surface, 0, (gctPOINTER)hnd->base, -1) < 0 )
                {
                    goto ON_ERROR;
                }
                hnd->surfaceHigh32Bits = 0;
                hnd->surface = surface;
            }
        }
        else
        {
            if( gcoSURF_Construct(0, hnd->width, hnd->height, 1, (gceSURF_TYPE)(hnd->surfType|gcvSURF_NO_VIDMEM), hnd->surfFormat, hnd->pool, &surface) < 0 )
            {
                goto ON_ERROR;
            }

            if( hnd->samples > 0 )
            {
                if( gcoSURF_SetSamples(surface, hnd->samples) < 0)
                {
                    goto ON_ERROR;
                }
            }
            surface->totalSize[22] = hnd->size;
            gcoHAL_ImportVideoMemory(hnd->infoA1, &videoMem);
            surface->totalSize[41] = videoMem;
            surface->totalSize[27] = hnd->pool;
            surface->totalSize[39] = hnd->infoB2;
            if( hnd->infoB1 )
                gcoHAL_ImportVideoMemory(hnd->infoB1, &videoMem);
            surface->totalSize[104] = videoMem;
            surface->totalSize[90]  = hnd->infoA2;
            surface->totalSize[102] = hnd->infoB3;
            if( gcoSURF_Lock(surface, 0, 0) < 0 )
            {
                goto ON_ERROR;
            }

            if( gcoSURF_SetFlags(surface, gcvSURF_FLAG_CONTENT_YINVERTED, gcvTRUE) < 0 )
            {
                goto ON_ERROR;
            }

            hnd->surface = surface;
            hnd->surfaceHigh32Bits = 0;
            status = 0;
        }
        if( !(hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM) )
            status = gc_gralloc_map(hnd, &Vaddr);
        if( status )
            goto ON_ERROR;
    }

    if( hnd->signal )
    {
        if( gcoOS_MapSignal(hnd->signal, &signal) < 0 )
        {
            goto ON_ERROR;
        }
        hnd->signal = signal;
        hnd->signalHigh32Bits = 0;
    }

    if( hnd->shAddr )
        gcoSURF_BindShBuffer(surface, hnd->shAddr);

    gcoHAL_SetHardwareType(0, hwtype);

    return 0;
}

/*******************************************************************************
**
**  gc_gralloc_unregister_buffer
**
**  Unregister buffer in current process (in client).
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
**      Nothing.
**
*/
int
gc_gralloc_unregister_buffer(
    gralloc_module_t const * Module,
    buffer_handle_t Handle
    )
{
    //log_func_entry;
    gceHARDWARE_TYPE hwtype = gcvHARDWARE_3D;

    (void*)Module;

    if (private_handle_t::validate(Handle) < 0)
    {
        return -EINVAL;
    }

    private_handle_t* hnd = (private_handle_t*)Handle;

    gcoHAL_GetHardwareType(0, &hwtype);
    setHwType71D0(hnd->allocUsage);
    if( hnd->base )
        gc_gralloc_unmap(hnd);

    if( hnd->signal )
    {
        if( gcoOS_UnmapSignal(hnd->signal) < 0 )
        {
            gcoHAL_SetHardwareType(0, hwtype);
            return -EINVAL;
        }
        hnd->signal = 0;
        hnd->signalHigh32Bits = 0;
    }

    if( hnd->surface == NULL || gcoSURF_Destroy(hnd->surface) >= 0 )
    {
        hnd->clientPID = 0;
        if( hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM )
        {
            if( gcoHAL_Commit(0, gcvTRUE) < 0 )
            {
                ALOGE("Failed to unregister buffer=%p", hnd);
                gcoHAL_SetHardwareType(0, hwtype);
                return -EINVAL;
            }
        }
        else
        {
            if( gcoHAL_Commit(0, gcvFALSE) < 0 )
            {
                ALOGE("Failed to unregister buffer=%p", hnd);
                gcoHAL_SetHardwareType(0, hwtype);
                return -EINVAL;
            }
        }
        gcoHAL_SetHardwareType(0, hwtype);
        return 0;
    }

    ALOGE("Failed to unregister buffer=%p", hnd);
    gcoHAL_SetHardwareType(0, hwtype);
    return -EINVAL;
}

/*******************************************************************************
**
**  gc_gralloc_lock
**
**  Lock android native buffer and get address.
**
**  INPUT:
**
**      gralloc_module_t const * Module
**          Specified gralloc module.
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**      int Usage
**          Usage for lock.
**
**      int Left, Top, Width, Height
**          Lock area.
**
**  OUTPUT:
**
**      void ** Vaddr
**          Point to save virtual address pointer.
*/
int
gc_gralloc_lock(
    gralloc_module_t const* Module,
    buffer_handle_t Handle,
    int Usage,
    int Left,
    int Top,
    int Width,
    int Height,
    void ** Vaddr
    )
{
    //log_func_entry;

    gceHARDWARE_TYPE hwtype = gcvHARDWARE_3D;
    private_handle_t *hnd = (private_handle_t*)Handle;

    if( private_handle_t::validate(hnd) )
        return -EINVAL;

    if( hnd->surface == NULL )
        return -EINVAL;

    if( (Usage & hnd->allocUsage) != Usage )
        ALOGW("Invalid access to buffer=%p: lockUsage=0x%08x, allocUsage=0x%08x", hnd, Usage, hnd->allocUsage);

    *Vaddr = (void*)hnd->base;
    hnd->lockAddr = Usage;
    gcoHAL_GetHardwareType(0, &hwtype);
    setHwType71D0(hnd->allocUsage);

    if( (hnd->flags & (private_handle_t::PRIV_FLAGS_USES_PMEM_ADSP|private_handle_t::PRIV_FLAGS_USES_PMEM)) == private_handle_t::PRIV_FLAGS_USES_PMEM
      && !(Usage & GRALLOC_USAGE_PRIVATE_2) )
    {
        hnd->flags |= private_handle_t::PRIV_FLAGS_NEEDS_FLUSH;
    }
    if( hnd->signal != NULL )
        gcoOS_WaitSignal(0, hnd->signal, gcvINFINITE);

    gcoHAL_SetHardwareType(0, hwtype);
    return 0;
}

/*******************************************************************************
**
**  gc_gralloc_ycbcr
**
*/
int
gc_gralloc_lock_ycbcr(
    gralloc_module_t const * Module,
    buffer_handle_t Handle,
    int Usage,
    int Left,
    int Top,
    int Width,
    int Height,
    android_ycbcr *ycbcr
    )
{
    //log_func_entry;

    int res;
    int stride;
    int colors[13];
    private_handle_t *hnd = (private_handle_t*)Handle;
    if( private_handle_t::validate(hnd) )
        return -EINVAL;

    if( hnd->surface == NULL )
    {
        ycbcr->cr = 0;
        ycbcr->cb = 0;
        ycbcr->y  = 0;
        return -EINVAL;
    }

    if( hnd->format != HAL_PIXEL_FORMAT_YCbCr_420_888 )
        return -EINVAL;

    res = gc_gralloc_lock(Module, Handle, Usage, Left, Top, Width, Height, (void**)colors);
    if( res == 0 )
    {
        gcoSURF_Lock(hnd->surface, 0, (gctPOINTER*)colors);
        gcoSURF_Unlock(hnd->surface, 0);
        ycbcr->cb = (void*)colors[1];
        ycbcr->cr = (void*)(colors[1]+1);
        ycbcr->y  = (void*)colors[0];
        gcoSURF_GetAlignedSize(hnd->surface, 0, 0, &stride);
        ycbcr->chroma_step = 2;
        ycbcr->ystride = stride;
        ycbcr->cstride = stride;
    }

    return res;
}

/*******************************************************************************
**
**  gc_gralloc_unlock
**
**  Unlock android native buffer.
**  For 3D composition, it will resolve linear to tile for SW surfaces.
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
**      Nothing.
**
*/
int
gc_gralloc_unlock(
    gralloc_module_t const * Module,
    buffer_handle_t Handle
)
{
    //log_func_entry;
    private_handle_t *hnd = (private_handle_t*)Handle;

    (void*)Module;

    if (private_handle_t::validate(hnd) < 0)
        return -EINVAL;

    if( hnd->surface == NULL )
        return -EINVAL;

    gcoSURF_UpdateTimeStamp(hnd->surface);
    gcoSURF_PushSharedInfo(hnd->surface);
    if( hnd->lockUsage & GRALLOC_USAGE_SW_WRITE_MASK )
        gcoSURF_CPUCacheOperation(hnd->surface, gcvCACHE_CLEAN);

    if( (hnd->flags & (private_handle_t::PRIV_FLAGS_NEEDS_FLUSH|
                       private_handle_t::PRIV_FLAGS_USES_PMEM|
                       private_handle_t::PRIV_FLAGS_USES_PMEM_ADSP))
        == (private_handle_t::PRIV_FLAGS_NEEDS_FLUSH|private_handle_t::PRIV_FLAGS_USES_PMEM)
    )
    {
        gc_gralloc_flush(hnd, private_handle_t::PRIV_FLAGS_NEEDS_FLUSH|private_handle_t::PRIV_FLAGS_USES_PMEM);
        hnd->flags &= ~private_handle_t::PRIV_FLAGS_NEEDS_FLUSH;
    }

    hnd->lockUsage = 0;

    return 0;
}

/*******************************************************************************
**
**  gc_gralloc_flush
**
**  Flush cache to memory.
**
**  INPUT:
**
**      buffer_handle_t Handle
**          Specified buffer handle.
**
**  OUTPUT:
**
**      Nothing.
*/

int
gc_gralloc_flush(
    buffer_handle_t Handle,
    uint32_t flags
    )
{
    //log_func_entry;
    private_handle_t * hnd = (private_handle_t *) Handle;

    mvmem_sync(hnd->master, flags);
    return 0;
}
