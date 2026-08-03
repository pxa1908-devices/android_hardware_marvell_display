#
# Copyright (C) 2008 The Android Open Source Project
# Copyright (c) 2010-2012,2014 The Linux Foundation. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


LOCAL_PATH:= $(call my-dir)
# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.board.platform>.so


include $(LOCAL_PATH)/../common.mk
include $(CLEAR_VARS)

BOARD_ENABLE_WFD_OPTIMIZATION ?= true 
BOARD_ENABLE_OVERLAY ?= false

LOCAL_SRC_FILES := \
    hwcomposer.cpp \
    HWCDisplayEventMonitor.cpp

LOCAL_SRC_FILES += \
    HWBaselayComposer.cpp

ifeq ($(BOARD_ENABLE_OVERLAY), true)
LOCAL_SRC_FILES += \
    HWOverlayComposer.cpp \
    OverlayDisplayEngine/IDisplayEngine.cpp \
    OverlayDisplayEngine/IOverlay.cpp \
    OverlayDisplayEngine/V4L2Overlay.cpp \
    HWCFenceManager.cpp

LOCAL_C_INCLUDES := $(common_includes) \
    hardware/libhardware/include \
    hardware/marvell/libprebuilt/pxa1908/libGAL/include \
    system/core/libsync/

ifneq ($(BOARD_ENABLE_WFD_OPTIMIZATION), true)
LOCAL_SRC_FILES += \
    GcuEngine.cpp

LOCAL_C_INCLUDES += \
    frameworks/native/services/
endif
endif

ifeq ($(BOARD_ENABLE_WFD_OPTIMIZATION), true)
LOCAL_SRC_FILES += \
    HWVirtualComposer.cpp \
    GcuEngine.cpp
endif


ifeq ($(BOARD_ENABLE_OVERLAY), true)
LOCAL_C_INCLUDES += \
    hardware/marvell/pxa1908/hwcomposer/OverlayDisplayEngine \
    hardware/marvell/pxa1908/original-kernel-headers/ \
    hardware/marvell/display/pxa1908/libgralloc \
    bionic/libc/kernel/uapi/
endif

ifeq ($(BOARD_ENABLE_WFD_OPTIMIZATION), true)
LOCAL_C_INCLUDES += \
    frameworks/native/services 
endif

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw


LOCAL_SHARED_LIBRARIES := $(common_libs) \
        libgcu \
        libui \
        liblog \
        libEGL \
        libhardware_legacy

ifeq ($(BOARD_ENABLE_OVERLAY), true)
LOCAL_SHARED_LIBRARIES += libbinder \
                          libsync
endif

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)

LOCAL_CFLAGS:= -DLOG_TAG=\"HWComposerMarvell\"

ifeq ($(BOARD_ENABLE_OVERLAY), true)
LOCAL_CFLAGS += -DENABLE_OVERLAY
endif

ifeq ($(BOARD_ENABLE_WFD_OPTIMIZATION), true)
LOCAL_CFLAGS += -DENABLE_WFD_OPTIMIZATION
endif

LOCAL_C_INCLUDES += hardware/marvell/display/pxa1908/libHWComposerGC
LOCAL_SHARED_LIBRARIES += libHWComposerGC
LOCAL_CFLAGS += -DENABLE_HWC_GC_PATH
LOCAL_CFLAGS += -DINTEGRATED_WITH_MARVELL

LOCAL_CFLAGS += -g

ifeq ($(ENABLE_OVERLAY_ONLY_HDMI_CONNECTED), true)
LOCAL_CFLAGS += -DENABLE_OVERLAY_ONLY_HDMI_CONNECTED
endif

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
