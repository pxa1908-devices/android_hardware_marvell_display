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

#include <errno.h>
#include "HWBaselayComposer.h"
#ifdef ENABLE_HWC_GC_PATH
    #include "HWCGCInterface.h"
#endif
#include <cutils/properties.h>
#include <log/log.h>

using namespace android;

//#define HWC_COMPILE_CHECK(expn) typedef char __HWC_ASSERT__[(expn)?1:-1]

// Here we need to check hwc_layer_t and hwc_layer_list size as libHWComposerGC.so is released in binary.
// If you meet compile error here, ask graphics team for a new libHWComposerGC.so. and update value here.
//HWC_COMPILE_CHECK(sizeof(hwc_layer_1_t) == 100);
//HWC_COMPILE_CHECK(sizeof(hwc_display_contents_1_t) == 20);

HWBaselayComposer::HWBaselayComposer()
:mHwc(0)
{

}

HWBaselayComposer::~HWBaselayComposer()
{

}

int HWBaselayComposer::open(const char * name, struct hw_device_t** device)
{
#ifdef ENABLE_HWC_GC_PATH
    char value[PROPERTY_VALUE_MAX];
    property_get( "persist.hwc.gc.disable", value, "0" );
    if( memcmp(value, "1", 1) == 0 )
    {
        return -EINVAL;
    }
    else
    {
        return hwc_device_open_gc(NULL, name, (struct hw_device_t**)&mHwc);
    }
#else
    return -EINVAL;
#endif
}

int HWBaselayComposer::close(struct hw_device_t* dev)
{
#ifdef ENABLE_HWC_GC_PATH
    return hwc_device_close_gc(&mHwc->common);
#else
    return 0;
#endif
}

int HWBaselayComposer::prepare(hwc_composer_device_1_t *dev, size_t numDisplays, hwc_display_contents_1_t** displays)
{
#ifdef ENABLE_HWC_GC_PATH
    return hwc_prepare_gc(mHwc, numDisplays, displays);
#else
    return 0;
#endif
}

int HWBaselayComposer::set(hwc_composer_device_1_t *dev, size_t numDisplays, hwc_display_contents_1_t** displays)
{
#ifdef ENABLE_HWC_GC_PATH
    return hwc_set_gc(mHwc, numDisplays, displays);
#else
    return 0;
#endif
}

int HWBaselayComposer::blank(hwc_composer_device_1_t *dev, int disp, int blk)
{
#ifdef ENABLE_HWC_GC_PATH
    return hwc_blank_gc(mHwc, disp, blk);
#else
    return 0;
#endif
}
