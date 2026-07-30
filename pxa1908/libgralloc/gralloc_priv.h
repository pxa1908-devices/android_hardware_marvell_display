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

#ifndef GRALLOC_PRIV_H_
#define GRALLOC_PRIV_H_

#include <stdint.h>
#include <limits.h>
#include <sys/cdefs.h>
#include <hardware/gralloc.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#include <cutils/native_handle.h>
#include <log/log.h>

#include <linux/fb.h>

#include <libGAL.h>

#include "mrvl_pxl_formats.h"

struct private_module_t;
struct private_handle_t;

/* always use two FDs */
//#define PRIVATE_HANDLE_INT_COUNT      14
//#define PRIVATE_HANDLE_FD_COUNT        2
#define PRIVATE_HANDLE_INT_COUNT      47
#define PRIVATE_HANDLE_FD_COUNT        2
#define GC_PRIVATE_HANDLE_INT_COUNT    47
#define GC_PRIVATE_HANDLE_FD_COUNT    2

#define GRALLOC_USAGE_MRVL_PRIVATE_1 0x400000

class __DEBUG_CLASS_LOG__
{
    const char* _func;
    public:
        __DEBUG_CLASS_LOG__(int line, const char* func):_func(func)
        {
            ALOGE("Entering %s at line %d", _func, line);
        }

        ~__DEBUG_CLASS_LOG__()
        {
            ALOGE("Exiting %s", _func);
        }
};

#define log_func_entry         __DEBUG_CLASS_LOG__ __debug_obj__(__LINE__, __func__)
#define log_func_line(fmt,...) ALOGE("%s at %d: " fmt, __func__, __LINE__, ## __VA_ARGS__)


struct private_module_t
{
    gralloc_module_t base;

    struct private_handle_t* framebuffer;
    uint32_t flags;
    uint32_t numBuffers;
    uint32_t bufferMask;
    pthread_mutex_t lock;
    buffer_handle_t currentBuffer;

    struct fb_var_screeninfo info;
    struct fb_fix_screeninfo finfo;
    float xdpi;
    float ydpi;
    float fps;
    char field_1B8[460];
};

#ifdef __cplusplus
struct private_handle_t : public native_handle {

    enum {
        PRIV_FLAGS_FRAMEBUFFER      = 0x00000001,
        PRIV_FLAGS_USES_PMEM        = 0x00000002,
        PRIV_FLAGS_USES_PMEM_ADSP   = 0x00000004,
        PRIV_FLAGS_NEEDS_FLUSH      = 0x00000008,
        PRIV_FLAGS_NEEDS_INVALIDATE = 0x00000010
    };

#else
struct private_handle_t {
    native_handle_t nativeHandle;
#endif /* __cplusplus */
    /* ion buffer fd */
    int     master;
    /* client fd */
    int fd;
    /* ints */
    int magic;
    int flags;
    int32_t size;
    int64_t offset;
    int format;
    int width;
    int height;
    int mem_xstride;
    int mem_ystride;
    int field_3C; // int     needUnregister;
    gcoSURF surface; // its a pointer, looks like gal pointers are 64b
    int surfaceHigh32Bits; // High 32b of surface
    gctSIGNAL signal; // its a pointer, looks like gal pointers are 64b
    int signalHigh32Bits; // High 32b of signal
    int field_50;
    int field_54;
    int field_58;
    int field_5C;
    gctSHBUF shAddr; // its a pointer, looks like gal pointers are 64b
    int shAddrHight32Bits; // High 32b of field_64
    int lockAddr;
    int stride;
    gceSURF_FORMAT surfFormat;
    gceSURF_TYPE surfType;
    int samples;
    int infoA1;
    gcePOOL pool;
    int infoB2;
    int infoB1;
    int infoA2;
    int infoB3;
    int lockUsage;
    int allocUsage;
    int clientPID;
    int field_A0;
    int field_A4;
    int fenceFd;
    int field_AC;
    int field_B0;
    int field_B4;
    int field_B8;
    int field_BC;
    int64_t base;
    int pid;
    int physAddr;
#ifdef __cplusplus
    static const int sNumInts = PRIVATE_HANDLE_INT_COUNT;
    static const int sNumFds = PRIVATE_HANDLE_FD_COUNT;
    static const int sMagic = 0x3141592;

    private_handle_t(int fd, int size, int flags) :
        fd(fd),
        magic(sMagic),
        flags(flags),
        size(size),
        offset(0),
        format(0),
        width(0),
        height(0),
        mem_xstride(0),
        mem_ystride(0),
        base(0),
        pid(getpid())
    {
        version = sizeof(native_handle);
        numInts = sNumInts;
        numFds = sNumFds;
    }

    ~private_handle_t()
    {
        magic = 0;
    }

    static int validate(const native_handle* h)
    {
        const private_handle_t* hnd = (const private_handle_t*)h;

        /* Here we always check gc_private_handle_t's numInts and numFds */
        if (!h || h->version != sizeof(native_handle) ||
                h->numInts != GC_PRIVATE_HANDLE_INT_COUNT || h->numFds != GC_PRIVATE_HANDLE_FD_COUNT ||
                hnd->magic != sMagic)
        {
            ALOGE("invalid gralloc handle (at %p)", h);
            return -EINVAL;
        }
        return 0;
    }

    static private_handle_t* dynamicCast(const native_handle_t* in) {
        if (validate(in) == 0) {
            return (private_handle_t*) in;
        }
        return NULL;
    }
#endif
};

#endif /* GRALLOC_PRIV_H_ */

