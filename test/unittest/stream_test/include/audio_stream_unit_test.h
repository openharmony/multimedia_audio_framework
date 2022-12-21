/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AUDIO_STREAM_UNIT_TEST_H
#define AUDIO_STREAM_UNIT_TEST_H

#include "gtest/gtest.h"
#include "audio_info.h"
#include "audio_stream.h"

namespace OHOS {
namespace AudioStandard {
class RenderCallbackTest : public AudioRendererCallbacks {
public:
    void OnSinkDeviceUpdatedCb() const
    {
        AUDIO_INFO_LOG("render callback OnSinkDeviceUpdatedCb");
    }
    virtual void OnStreamStateChangeCb() const
    {
        AUDIO_INFO_LOG("render callback OnStreamStateChangeCb");
    }

    virtual void OnStreamBufferUnderFlowCb() const {}
    virtual void OnStreamBufferOverFlowCb() const {}
    virtual void OnErrorCb(AudioServiceErrorCodes error) const {}
    virtual void OnEventCb(AudioServiceEventTypes error) const {}
};

class CapturterCallbackTest : public AudioCapturerCallbacks {
public:
    void OnSourceDeviceUpdatedCb() const
    {
        AUDIO_INFO_LOG("captuer callback OnSourceDeviceUpdatedCb");
    }
    // Need to check required state changes to update applications
    virtual void OnStreamStateChangeCb() const
    {
        AUDIO_INFO_LOG("captuer callback OnStreamStateChangeCb");
    }

    virtual void OnStreamBufferUnderFlowCb() const {}
    virtual void OnStreamBufferOverFlowCb() const {}
    virtual void OnErrorCb(AudioServiceErrorCodes error) const {}
    virtual void OnEventCb(AudioServiceEventTypes error) const {}
};

class AudioStreamCallbackTest : public AudioStreamCallback {
public:
    virtual ~AudioStreamCallbackTest() = default;

    virtual void OnStateChange(const State state, const StateChangeCmdType cmdType = CMD_FROM_CLIENT) {}
};

class AudioCapturerReadCallbackTest : public AudioCapturerReadCallback {
public:
    virtual ~AudioCapturerReadCallbackTest() = default;

    /**
     * Called when buffer to be enqueued.
     *
     * @param length Indicates requested buffer length.
     */
    virtual void OnReadData(size_t length) {};
};

class CapturerPeriodPositionCallbackTest : public CapturerPeriodPositionCallback {
public:
    virtual ~CapturerPeriodPositionCallbackTest() = default;

    /**
     * Called when the requested frame count is read.
     *
     * @param frameCount requested frame frame count for callback.
     */
    virtual void OnPeriodReached(const int64_t &frameNumber) {};
};

class AudioStreamUnitTest : public testing::Test {
public:
    // SetUpTestCase: Called before all test cases
    static void SetUpTestCase(void);
    // TearDownTestCase: Called after all test case
    static void TearDownTestCase(void);
    // SetUp: Called before each test cases
    void SetUp(void);
    // TearDown: Called after each test cases
    void TearDown(void);
    static void InitAudioStream(std::shared_ptr<AudioStream> &audioStream);
};
} // namespace AudioStandard
} // namespace OHOS

#endif // AUDIO_STREAM_UNIT_TEST_H
