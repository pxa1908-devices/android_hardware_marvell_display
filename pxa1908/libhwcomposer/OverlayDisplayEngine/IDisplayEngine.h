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

#ifndef __INTERFACE_DISPLAYENGINE_H__
#define __INTERFACE_DISPLAYENGINE_H__

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
#include <binder/IPCThreadState.h>
#include <binder/IMemory.h>
#include <ui/DisplayInfo.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>
#include "DisplaySurface.h"

struct fb_var_screeninfo;
struct fb_fix_screeninfo;

#define MAX_BUFFER_NUM 32

namespace android
{

enum DISPLAYENGINETYPE{
    DISP_ENGINE_BASELAYER = 0,
    DISP_ENGINE_OVERLAY   = 1,
};

class DisplayEngineLayout;

class DisplayEngineMode
{
public:
    DisplayEngineMode()
        : m_nBpp(0)
        , m_nActivate(0)
        , m_nPixClock(0)
        , m_nLeftMargin(0)
        , m_nRightMargin(0)
        , m_nUpperMargin(0)
        , m_nLowerMargin(0)
        , m_nHsyncLen(0)
        , m_nVsyncLen(0)
        , m_nSync(0)
        , m_nVMode(0)
    {}

    ~DisplayEngineMode(){}

    status_t from(const fb_var_screeninfo& varInfo);

    ///< friends
    friend class DisplayModel;
    friend void ResolveScreenInfo(struct ::fb_var_screeninfo& varInfo, const DisplayEngineLayout& m_displayLayout, const DisplayEngineMode& caps);

private:
    uint32_t m_nBpp;
    uint32_t m_nActivate;
    uint32_t m_nPixClock;
    uint32_t m_nLeftMargin;
    uint32_t m_nRightMargin;
    uint32_t m_nUpperMargin;
    uint32_t m_nLowerMargin;
    uint32_t m_nHsyncLen;
    uint32_t m_nVsyncLen;
    uint32_t m_nSync;
    uint32_t m_nVMode;
};

class DisplayEngineCaps
{
    //TODO
};

class DisplayEngineLayout
{
public:
    DisplayEngineLayout()
        : m_nBase(0)
        , m_nSize(0)
        , m_nWidth(0)
        , m_nHeight(0)
        , m_nVirtualWidth(0)
        , m_nVirtualHeight(0)
        , m_nOffsetX(0)
        , m_nOffsetY(0)
        , m_nActivate(0)
    {}

    ~DisplayEngineLayout(){}

    status_t from(const fb_var_screeninfo& varInfo, const fb_fix_screeninfo& fixInfo);

    ///< friends
    friend class DisplayModel;
    friend void ResolveScreenInfo(struct ::fb_var_screeninfo& varInfo, const DisplayEngineLayout& m_displayLayout, const DisplayEngineMode& caps);

private:
    uint32_t m_nBase;
    uint32_t m_nSize;
    uint32_t m_nWidth;
    uint32_t m_nHeight;
    uint32_t m_nVirtualWidth;
    uint32_t m_nVirtualHeight;
    uint32_t m_nOffsetX;
    uint32_t m_nOffsetY;
    uint32_t m_nActivate;
};

class IDisplayEngine : public RefBase
{
public:
    IDisplayEngine(){};
    virtual ~IDisplayEngine(){};

public:
    ///< operation start.
    virtual status_t open() = 0;
    virtual status_t close() = 0;

    virtual status_t getCaps(DisplayEngineCaps& caps) = 0;

    ///< set/get mode capabilities.
    virtual status_t getMode(DisplayEngineMode& mode) = 0;
    virtual status_t getLayout(DisplayEngineLayout& layout) = 0;

    virtual status_t setMode(const DisplayEngineMode& mode) = 0;
    virtual status_t setLayout(const DisplayEngineLayout& layout) = 0;

    ///< draw related.
    virtual status_t setSrcPitch(uint32_t yPitch, uint32_t uPitch, uint32_t vPitch) = 0;
    virtual status_t setSrcResolution(int32_t srcWidth, int32_t srcHeight, int32_t srcFormat) = 0;
    virtual status_t setSrcCrop(uint32_t l, uint32_t t, uint32_t r, uint32_t b) = 0;
    virtual status_t setColorKey(int32_t alpha_type, int32_t alpha_value, int32_t ck_type, int32_t ck_r, int32_t ck_g, int32_t ck_b) = 0;
    virtual status_t setDstPosition(int32_t width, int32_t height, int32_t xOffset, int32_t yOffset) = 0;
    virtual status_t setPartialDisplayRegion(uint32_t l, uint32_t t, uint32_t r, uint32_t b, uint32_t color) = 0;
    virtual status_t drawImage(void* yAddr, void* uAddr, void* vAddr, int32_t lenght, uint32_t addrType) = 0;
    virtual status_t getConsumedImages(uint32_t vAddr[], uint32_t& nNumber) = 0;
    virtual status_t configVdmaStatus(DISPLAY_VDMA& vdma) = 0;
    virtual status_t waitVSync(DISPLAY_SYNC_PATH path) = 0;

    ///< dma related.
    virtual status_t setStreamOn(bool bOn) = 0;

    ///< special feature related.
    virtual status_t set3dVideoOn(bool bOn, uint32_t mode) = 0;

    virtual int32_t getFd() const = 0;
    virtual int32_t getReleaseFd() const = 0;
    virtual const char* getName() const = 0;
};

// factory-liked creator.
IDisplayEngine* CreateDisplayEngine(const char* pszName, DISPLAYENGINETYPE type);
IDisplayEngine* CreateBaseLayerEngine(const char* pszName, DISPLAYENGINETYPE type);
//IDisplayEngine* CreateOverlayEngine(uint32_t idx, DMSInternalConfig const * dms_config);

// common functions;
uint32_t ResolveColorFormat(uint32_t dmsFormat);
void  ResolveScreenInfo(struct ::fb_var_screeninfo& varInfo, const DisplayEngineLayout& displayLayout, const DisplayEngineMode& displayMode);

}

#endif
