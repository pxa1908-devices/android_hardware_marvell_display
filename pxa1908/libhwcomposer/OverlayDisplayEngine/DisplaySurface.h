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

#ifndef __DMS_DISPLAY_SURFACE_H__
#define __DMS_DISPLAY_SURFACE_H__

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utils/Errors.h>
#include <log/log.h>
#include <binder/IMemory.h>

#ifdef __cplusplus
extern "C" {
namespace android{
#endif

/**
 *  struct DISPLAYSURFACE comes exactly from
 *
 *  typedef int FBVideoMode;
 *  struct _sOvlySurface {
 *	    FBVideoMode videoMode;
 *	    struct _sViewPortInfo viewPortInfo;
 *	    struct _sViewPortOffset viewPortOffset;
 *	    struct _sVideoBufferAddr videoBufferAddr;
 *  };
 *  in fb_ioctl.h.
 *  For the platform compatibility concern, we define our own structure, and
 *  trans to the real _sOvlySurface by IDisplayEngine.
 *
 */

#define DISP_WFD_GUI_MODE       1
#define DISP_WFD_VIDEO_MODE     2

#define MAX_DISPLAY_QUEUE_NUM   30

typedef int DisplayVideoMode; 

typedef struct DisplayColorKeyNAlpha {
	unsigned int mode;
	unsigned int alphapath;
	unsigned int config;
	unsigned int Y_ColorAlpha;
	unsigned int U_ColorAlpha;
	unsigned int V_ColorAlpha;
}DISPLAYCOLORKEYNALPHA, *PDISPLAYCOLORKEYNALPHA;

typedef struct DisplayViewPortInfo {
    unsigned short srcWidth;        /* video source size */
    unsigned short srcHeight;
    unsigned short zoomXSize;       /* size after zooming */
    unsigned short zoomYSize;
    unsigned short yPitch;
    unsigned short uPitch;
    unsigned short vPitch;
    unsigned int rotation;
    unsigned int yuv_format;
}DISPLAYVIEWPORTINFO, *PDISPLAYVIEWPORTINFO;

typedef struct DisplayViewPortOffset {
    unsigned short xOffset;         /* position on screen */
    unsigned short yOffset;
}DISPLAYVIEWPORTOFFSET, *PDISPLAYVIEWPORTOFFSET;

typedef struct DisplayVideoBufferAddr {
    unsigned char   frameID;        /* which frame wants */
    /* new buffer (PA). 3 addr for YUV planar */
    unsigned char *startAddr[3];
    unsigned char *inputData;       /* input buf address (VA) */
    unsigned int length;            /* input data's length */
}DISPLAYVIDEOBUFFERADDR, *PDISPLAYVIDEOBUFFERADDR;

typedef struct DisplaySurface{
    uint32_t videoMode;
    DisplayViewPortInfo viewPortInfo;
    DisplayViewPortOffset viewPortOffset;
    DisplayVideoBufferAddr videoBufferAddr;
}DISPLAYSURFACE, *PDISPLAYSURFACE;

typedef struct DisplayVdma {
       /* path id, 0->panel, 1->TV, 2->panel2 */
       uint32_t path;
       /* 0: grafhics, 1: video */
       uint32_t layer;
       uint32_t enable;
}DISPLAY_VDMA, *PDISPLAY_VDMA;

typedef enum Display_Sync_Path{
    DISPLAY_SYNC_SELF = 0x0,
    DISPLAY_SYNC_PANEL = 0x1,
    DISPLAY_SYNC_TV = 0x2,
    DISPLAY_SYNC_PANEL_TV = 0x3,
}DISPLAY_SYNC_PATH;


#ifdef __cplusplus
} // end of namespace
} // end of extern "C"
#endif

#endif
