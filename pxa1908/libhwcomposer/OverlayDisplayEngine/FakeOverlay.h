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

#ifndef __FAKE_OVERLAY_H__
#define __FAKE_OVERLAY_H__

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <log/log.h>
#include <utils/Singleton.h>
#include <utils/Errors.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "IDisplayEngine.h"

namespace android{

class FakeOverlayRef : public IDisplayEngine
{
public:
    FakeOverlayRef(const char* pDev) : m_fd(1)
        , m_strDevName(pDev)
    {
        char buf[16];
        sprintf(buf, "FakeOverlay-%d", m_nCount++);
        m_strDevName = buf;
    }

    ~FakeOverlayRef()
    {
        close();
        m_nCount--;
    }

public:
    status_t open()
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t getCaps(DisplayEngineCaps& caps)
    {
        return NO_ERROR;
    }

    status_t getMode(DisplayEngineMode& mode)
    {
        return NO_ERROR;
    }

    status_t getLayout(DisplayEngineLayout& layout)
    {
        return NO_ERROR;
    }

    status_t setMode(const DisplayEngineMode& mode)
    {
        return NO_ERROR;
    }

    status_t setLayout(const DisplayEngineLayout& layout)
    {
        return NO_ERROR;
    }

    status_t setSrcPitch(uint32_t yPitch, uint32_t uPitch, uint32_t vPitch)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setSrcResolution(int32_t srcWidth, int32_t srcHeight, int32_t srcFormat)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setSrcCrop(uint32_t l, uint32_t t, uint32_t r, uint32_t b)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setColorKey(int32_t alpha_type, int32_t alpha_value, int32_t ck_type, int32_t ck_r, int32_t ck_g, int32_t ck_b)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setDstPosition(int32_t width, int32_t height, int32_t xOffset, int32_t yOffset)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setPartialDisplayRegion(uint32_t l, uint32_t t, uint32_t r, uint32_t b, uint32_t color)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t drawImage(void* yAddr, void* uAddr, void* vAddr, int32_t length, uint32_t addrType)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t configVdmaStatus(DISPLAY_VDMA& vdma)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t waitVSync(DISPLAY_SYNC_PATH path)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t setStreamOn(bool bOn)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t set3dVideoOn(bool bOn, uint32_t mode)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t getConsumedImages(uint32_t vAddr[], uint32_t& nImgNum)
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    status_t close()
    {
        ALOGD("Calling %s.", __func__);
        return NO_ERROR;
    }

    int32_t getFd() const {return m_fd;}

    int32_t getReleaseFd() const {return m_fd;}

    const char* getName() const{return m_strDevName.string();}

private:
    ///< fake fd.
    int32_t m_fd;

    ///< dev name.
    String8 m_strDevName;

    ///< ref count for name difference.
    static uint32_t m_nCount;
};

uint32_t FakeOverlayRef::m_nCount = 0;

}
#endif

