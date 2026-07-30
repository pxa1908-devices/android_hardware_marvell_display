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

#ifndef __HWC_FENCE_MANAGER_H__
#define __HWC_FENCE_MANAGER_H__

#include <ui/Fence.h>
#include <fcntl.h>
#include <errno.h>

#include <log/log.h> // FIX: Changed from <cutils/log.h>
// FIX: Removed <cutils/atomic.h>

#include <utils/Mutex.h>
#include <utils/Thread.h>
#include <utils/String8.h>
#include <utils/Vector.h>
#include <cutils/properties.h>
#include <hardware/hwcomposer.h>

namespace android{

/*
 * Fence Timer Thread
 * Later, we may poll this thread on VSYNC,
 * so that we can update fence status in time.
 */
class HWCFenceTimerThread : public Thread
{
    friend class HWCFenceManager;
private:
    HWCFenceTimerThread();

    ~HWCFenceTimerThread();

    bool threadLoop();

    int64_t getCurrentStamp() const{
        return m_nCurrentStamp;
    }

    int32_t createFence(int64_t nFenceId);

    void signalFence(int64_t nFenceId);

    void reset();

private:
    int32_t m_nSyncTimeLineFd;

    int64_t m_nCurrentStamp;

    Mutex m_mutexLock;
};

class HWCFenceManager : public RefBase
{
public:
    HWCFenceManager();

    ~HWCFenceManager();

public:
    int32_t createFence(int64_t nFenceId);

    void signalFence(int64_t nFenceId);

    int64_t getCurrentStamp() const{
        return m_pEventThread->getCurrentStamp();
    }

    void reset();

    void dump(String8& result, char* buffer, int size);

private:
    ///< status
    bool m_bRunning;

    ///< the thread counting the current fence time status.
    sp<HWCFenceTimerThread> m_pEventThread;

};


}// end of namespace android

#endif
