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

#ifndef __HWC_GCU_ENGINE_H__
#define __HWC_GCU_ENGINE_H__

#include <utils/RefBase.h>
#include <cutils/properties.h>
#include <utils/KeyedVector.h>
#include <utils/SortedVector.h>
#include <utils/String8.h>
#include <utils/Mutex.h>
#include <log/log.h> // FIX: Added missing logging header for ALOGD/ALOGE
#include "gcu.h"

namespace android{

#ifdef __cplusplus
extern "C" {
#endif

#define GPU_BLIT_SRC        1
#define GPU_BLIT_STRETCH    2
#define GPU_BLIT_FILTER     3
#define GPU_BLIT_FILL       4
#define GPU_BLIT_ROP        5

#define DISP_SUPPORT_LIB_GCU 0

#define MAX_HINT_PICS       1

/*
 * Refer to Surface.java(frameworks/base/core/java/android/view/).
 * Use the same definitions in Surface.java because DisplayDevice uses
 * this set of number to indicate rotations. In native level,
 * SurfaceFlinger uses its own Transform to get its own orientation,
 * but since SF concerns not only the orientation of DisplayDevice, we
 * can not simply re-use its Transform here.
 */
enum DISPLAY_SURFACE_ROTATION{
    /**
     * Rotation constant: 0 degree rotation (natural orientation)
     */
    DISPLAY_SURFACE_ROTATION_0   = 0,

    /**
     * Rotation constant: 90 degree rotation.
     */
    DISPLAY_SURFACE_ROTATION_90  = 1,

    /**
     * Rotation constant: 180 degree rotation.
     */
    DISPLAY_SURFACE_ROTATION_180 = 2,

    /**
     * Rotation constant: 270 degree rotation.
     */
    DISPLAY_SURFACE_ROTATION_270 = 3,

    /**
     */
    DISPLAY_SURFACE_ROTATION_INV,
};


typedef struct __overlay_rect
{
    int     l;
    int     t;
    int     r;
    int     b;
}
overlay_rect, *lpoverlay_rect, DISP_RECT;

#define ConstructBlitDataDescription(_blitData, _BlitType, _CoordinateTransformed, _RotationDegree, \
                                     _SrcWidth, _SrcHeight, _SrcFormat, _pSrcRect, \
                                     _SrcAddr, _SrcUAddr, _SrcVAddr,    \
                                     _SrcStride, _SrcUStride, _SrcVStride, \
                                     _DstWidth, _DstHeight, _DstFormat, \
                                     _pDstRect, _pDstSubRect, _RectCount, \
                                     _DstAddr, _DstStride,              \
                                     _FillColor, _FilterType, _FilterSize, _pCoef, _IsSolidFill, _Rop) \
    do {                                                                \
        _blitData.mBlitType       = _BlitType;                          \
        _blitData.mSrcAddr        = _SrcAddr;                           \
        _blitData.mSrcUAddr       = _SrcUAddr;                          \
        _blitData.mSrcVAddr       = _SrcVAddr;                          \
        _blitData.mSrcStride      = _SrcStride;                         \
        _blitData.mSrcUStride     = _SrcUStride;                        \
        _blitData.mSrcVStride     = _SrcVStride;                        \
        _blitData.mSrcFormat      = _SrcFormat;                         \
        _blitData.mSrcWidth       = _SrcWidth;                          \
        _blitData.mSrcHeight      = _SrcHeight;                         \
        _blitData.mRotationDegree = _RotationDegree;                    \
                                                                        \
        _blitData.mDstAddr        = _DstAddr;                           \
        _blitData.mDstStride      = _DstStride;                         \
        _blitData.mDstWidth       = _DstWidth;                          \
        _blitData.mDstHeight      = _DstHeight;                         \
        _blitData.mDstFormat      = _DstFormat;                         \
                                                                        \
        _blitData.mSrcRect        = _pSrcRect;                          \
        _blitData.mDstRect        = _pDstRect;                          \
        _blitData.mDstSubRect     = _pDstSubRect;                       \
        _blitData.mRectCount      = _RectCount;                         \
                                                                        \
        _blitData.mFillColor     = _FillColor;                          \
        _blitData.mFilterType    = _FilterType;                         \
        _blitData.mFilterSize    = _FilterSize;                         \
        _blitData.mCoef          = _pCoef;                              \
        _blitData.mIsSolidFill   = _IsSolidFill;                        \
        _blitData.mRop           = _Rop;                                \
    }while(0);

class BlitDataDescription
{
public:
    BlitDataDescription() :
        mBlitType(0),
        mCoordinateTransformed(false),
        mSrcWidth(0),
        mDstWidth(0),
        mRotationDegree(0),
        mSrcRect(NULL),
        mDstRect(NULL),
        mDstSubRect(NULL),
        mRectCount(0),
        mSrcAddr(0),
        mSrcUAddr(0),
        mSrcVAddr(0),
        mSrcStride(0),
        mSrcUStride(0),
        mSrcVStride(0),
        mDstAddr(0),
        mDstStride(0),
        mFillColor(0),
        mFilterType(0),
        mFilterSize(0),
        mCoef(NULL),
        mIsSolidFill(true),
        mRop(0)
    {}

    ~BlitDataDescription()
    {}

    void dump()
    {
        char value[PROPERTY_VALUE_MAX];
        property_get("hwc.virtual.gcu.log", value, "0");
        bool bLog = (atoi(value) == 1);
        if(bLog){
            ALOGD("--------------------DUMP GPU BLIT DATA--------------------------");
            ALOGD("mBlitType = %d.", mBlitType);
            ALOGD("mCoordinateTransformed = %d.", mCoordinateTransformed);
            ALOGD("mSrcWidth = %d.", mSrcWidth);
            ALOGD("mSrcHeight = %d.", mSrcHeight);
            ALOGD("mSrcFormat = %d.", mSrcFormat);
            ALOGD("mDstWidth = %d.", mDstWidth);
            ALOGD("mDstHeight = %d.", mDstHeight);
            ALOGD("mDstFormat = %d.", mDstFormat);
            ALOGD("mRotationDegree = %d.", mRotationDegree);

            if(NULL != mSrcRect){
                ALOGD("mSrcRect->l(%d),mSrcRect->t(%d),mSrcRect->r(%d),mSrcRect->b(%d).", mSrcRect->l,mSrcRect->t,mSrcRect->r,mSrcRect->b);
            }

            if(NULL != mDstRect){
                ALOGD("mDstRect->l(%d),mDstRect->t(%d),mDstRect->r(%d),mDstRect->b(%d).", mDstRect->l,mDstRect->t,mDstRect->r,mDstRect->b);
            }

            if(NULL != mDstSubRect){
                ALOGD("mDstSubRect->l(%d),mDstSubRect->t(%d),mDstSubRect->r(%d),mDstSubRect->b(%d).", mDstSubRect->l,mDstSubRect->t,mDstSubRect->r,mDstSubRect->b);
            }

            ALOGD("mRectCount = %d.", mRectCount);
            ALOGD("mSrcAddr = 0x%x.", mSrcAddr);
            ALOGD("mSrcUAddr = 0x%x.", mSrcUAddr);
            ALOGD("mSrcVAddr = 0x%x.", mSrcVAddr);
            ALOGD("mSrcStride = %d.", mSrcStride);
            ALOGD("mSrcUStride = %d.", mSrcUStride);
            ALOGD("mSrcVStride = %d.", mSrcVStride);
            ALOGD("mDstAddr = 0x%x.", mDstAddr);
            ALOGD("mDstStride = %d.", mDstStride);
            ALOGD("mFillColor = 0x%x.", mFillColor);
            ALOGD("mFilterType = %d.", mFilterType);
            ALOGD("mFilterSize = %d.", mFilterSize);
            ALOGD("mRop = 0x%x.", mRop);
        }
    }

public:
    int                 mBlitType;
    bool                mCoordinateTransformed;

    uint32_t            mSrcWidth;
    uint32_t            mSrcHeight;
    uint32_t            mSrcFormat;

    uint32_t            mDstWidth;
    uint32_t            mDstHeight;
    uint32_t            mDstFormat;
    uint32_t            mRotationDegree;

    DISP_RECT           *mSrcRect;
    DISP_RECT           *mDstRect;
    DISP_RECT           *mDstSubRect;
    uint32_t            mRectCount;

    uint32_t            mSrcAddr;
    uint32_t            mSrcUAddr;
    uint32_t            mSrcVAddr;

    uint32_t            mSrcStride;
    uint32_t            mSrcUStride;
    uint32_t            mSrcVStride;

    uint32_t            mDstAddr;
    uint32_t            mDstStride;
    uint32_t            mFillColor;

    uint32_t            mFilterType;
    uint32_t            mFilterSize;

    float               *mCoef;
    bool                mIsSolidFill;
    uint32_t            mRop;
};

typedef class BlitDataDescription*  PBlitDataDesc;

class GcuEngine
{
public:
    GcuEngine();
    ~GcuEngine();

    bool    Blit(PBlitDataDesc blitDesc);

    void    Flush();

    bool    LoadHintPic(uint32_t id, const char* fileName);

    bool    BlitHintPic(uint32_t id, PBlitDataDesc blitDesc);

protected:
    bool    FilterBlit(PBlitDataDesc blitDesc);
    bool    SrcBlit(PBlitDataDesc blitDesc);
    bool    FillBlit(PBlitDataDesc blitDesc);
    bool    RopBlit(PBlitDataDesc blitDesc);

private:
    bool    Init();

    GCU_ROTATION   getGCURotation(uint32_t rotationDegree);
    GCU_FORMAT     getGCUFormat(uint32_t halFormat);
    bool           getSurfaces(PBlitDataDesc blitDesc,
                               GCUSurface &pSrcSurface,
                               GCUSurface &pDstSurface);
    void           getRects(PBlitDataDesc blitDesc,
                               GCU_RECT &srcRect,
                               GCU_RECT &dstRect);

    ///< create a tiny buffer to do ROP.
    void           preparePatternSurfaces();

private:

    GCUContext     mGCUContextPtr;
    bool           mFlushAtEnd;
    GCUSurface     mAndPatternPtr;
    GCUSurface     mOrPatternPtr;

    GCUSurface     mHintSurface[MAX_HINT_PICS];

};

#ifdef __cplusplus
}
#endif

}

#endif
