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

#ifndef __FB_OVERLAY_H__
#define __FB_OVERLAY_H__

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <cutils/log.h>
#include <utils/Singleton.h>
#include <utils/Errors.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <utils/Errors.h>
#include <utils/threads.h>
#include <utils/CallStack.h>
#include <log/log.h>
#include <cutils/properties.h>

#include <binder/IPCThreadState.h>
#include <binder/IMemory.h>

#include <ui/DisplayInfo.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>

#include "IDisplayEngine.h"
#include "video/mmp_ioctl.h"

namespace android{

#define FBOVLYLOG(...)                                    \
    do{                                                   \
        char value[PROPERTY_VALUE_MAX];                   \
        property_get("persist.dms.fbovly.log", value, "0");   \
        bool bLog = (atoi(value) == 1);                   \
        if(bLog){                                         \
            ALOGD(__VA_ARGS__);                            \
        }                                                 \
    }while(0)                                             \

#define FBOVLYWRAPPERLOG(...)                                       \
    do{                                                             \
        char buffer[1024];                                          \
        sprintf(buffer, __VA_ARGS__);                               \
        FBOVLYLOG("%s: %s", this->m_strDevName.string(), buffer);   \
    }while(0)                                                       \


class FBOverlayRef : public IDisplayEngine
{
public:
    FBOverlayRef(const char* pDev) : m_fd(-1)
                                   , m_strDevName(pDev)
                                   , m_nFrameCount(0)
                                   , m_releaseFd(-1)
    {
        memset(&m_mmpSurf, 0, sizeof(m_mmpSurf));
    }

    ~FBOverlayRef(){close();}

public:

    status_t getCaps(DisplayEngineCaps& caps) {return NO_ERROR;}
    status_t getMode(DisplayEngineMode& mode) {return NO_ERROR;}
    status_t getLayout(DisplayEngineLayout& layout) {return NO_ERROR;}
    status_t setMode(const DisplayEngineMode& mode) {return NO_ERROR;}
    status_t setLayout(const DisplayEngineLayout& layout) {return NO_ERROR;}

    status_t open()
    {
        const char* ov_device = m_strDevName.string();
        m_fd = ::open(ov_device, O_RDWR | O_NONBLOCK);
        if( m_fd < 0 ) {
            ALOGE("Open overlay device[%s] fail", ov_device);
            return -EIO;
        }

        return NO_ERROR;
    }

    status_t setSrcPitch(uint32_t yPitch, uint32_t uPitch, uint32_t vPitch)
    {
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.yPitch = %d.", yPitch);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.uPitch = %d.", uPitch);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.vPitch = %d.", vPitch);

        m_mmpSurf.win.pitch[0] = yPitch;
        m_mmpSurf.win.pitch[1] = uPitch;
        m_mmpSurf.win.pitch[2] = vPitch;
        return NO_ERROR;
    }

    status_t setSrcResolution(int32_t srcWidth, int32_t srcHeight, int32_t srcFormat)
    {
        uint32_t nColorFmt = ResolveColorFormat(srcFormat);
        FBOVLYWRAPPERLOG("m_overlaySurf.videoMode = %d.", nColorFmt);
        m_mmpSurf.win.pix_fmt = nColorFmt;
        return NO_ERROR;
    }

    status_t setSrcCrop(uint32_t l, uint32_t t, uint32_t r, uint32_t b)
    {
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.srcWidth = %d.", r-l);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.srcHeight = %d.", b-t);

        m_mmpSurf.win.xsrc = r-l;
        m_mmpSurf.win.ysrc = b-t;
        return NO_ERROR;
    }

    status_t setColorKey(int32_t alpha_type, int32_t alpha_value, int32_t ck_type, int32_t ck_r, int32_t ck_g, int32_t ck_b)
    {
        struct mmp_colorkey_alpha colorkey;
        colorkey.mode = ck_type;
        colorkey.alphapath = alpha_type;
        colorkey.config = alpha_value;
        colorkey.y_coloralpha = ck_r;
        colorkey.u_coloralpha = ck_g;
        colorkey.v_coloralpha = ck_b;

        if( ioctl(m_fd, FB_IOCTL_SET_COLORKEYnALPHA, &colorkey) ) {
            ALOGE("OVERLAY_SetConfig: Set color key failed");
            return -EIO;
        }
        return NO_ERROR;
    }

    status_t setDstPosition(int32_t width, int32_t height, int32_t xOffset, int32_t yOffset)
    {
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.zoomXSize = %d.", width);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortInfo.zoomYSize = %d.", height);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortOffset.xOffset = %d.", xOffset);
        FBOVLYWRAPPERLOG("m_overlaySurf.viewPortOffset.yOffset = %d.", yOffset);

        m_mmpSurf.win.xdst = width;
        m_mmpSurf.win.ydst = height;
        m_mmpSurf.win.xpos = xOffset;
        m_mmpSurf.win.ypos = yOffset;
        return NO_ERROR;
    }

    status_t setPartialDisplayRegion(uint32_t l, uint32_t t, uint32_t r, uint32_t b, uint32_t color)
    {
        return NO_ERROR;
    }

    status_t drawImage(void* yAddr, void* uAddr, void* vAddr, int32_t length, uint32_t addrType)
    {
        FBOVLYWRAPPERLOG("m_overlaySurf.videoBufferAddr.frameID     = %d.", m_nFrameCount);
        FBOVLYWRAPPERLOG("m_overlaySurf.videoBufferAddr.startAddr   = %p.", yAddr);
        FBOVLYWRAPPERLOG("m_overlaySurf.videoBufferAddr.length      = %d.", length);
        m_mmpSurf.addr.phys[0] = (uint32_t)yAddr;
        m_mmpSurf.addr.phys[1] = (uint32_t)uAddr;
        m_mmpSurf.addr.phys[2] = (uint32_t)vAddr;

        if( ioctl(m_fd, FB_IOCTL_FLIP_USR_BUF, &m_mmpSurf) ) {
            ALOGE("ioctl flip_vid failed");
            m_releaseFd = -1;
            return -EIO;
        }

        m_nFrameCount++;
        m_releaseFd = m_mmpSurf.fence_fd;
        return NO_ERROR;
    }

    status_t configVdmaStatus(DISPLAY_VDMA& vdma)
    {
        return NO_ERROR;
    }

    status_t waitVSync(DISPLAY_SYNC_PATH path)
    {
        return NO_ERROR;
    }

    status_t setStreamOn(bool bOn)
    {
        int status = bOn ? 1 : 0;
        FBOVLYWRAPPERLOG("Stream %s.", bOn ? "on" : "off");
        if( ioctl(m_fd, FB_IOCTL_ENABLE_DMA, &status) ) {
            ALOGE("ioctl OVERLAY %s FB_IOCTL_SWITCH_VID_OVLY failed", m_strDevName.string());
            return -EIO;
        }

        return NO_ERROR;
    }

    status_t set3dVideoOn(bool bOn, uint32_t mode){return NO_ERROR;}

    status_t getConsumedImages(uint32_t vAddr[], uint32_t& nImgNum)
    {
        return NO_ERROR;
    }

    status_t close()
    {
        setStreamOn(false);
        m_nFrameCount = 0;

        if(m_fd > 0){
            ::close(m_fd);
        }

        return NO_ERROR;
    }

    int32_t getFd() const {return m_fd;}

    int32_t getReleaseFd() const {return m_releaseFd;}

    const char* getName() const{return m_strDevName.string();}

private:

    int32_t m_fd;

    ///< dev name
    String8 m_strDevName;

    ///< frame count
    uint32_t m_nFrameCount;

    ///< mmp_surf in new LCD driver.
    struct mmp_surface m_mmpSurf;

    ///< release fd
    int32_t m_releaseFd;
};

}
#endif

