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

#ifndef AUDIO_POLICY_UNIT_TEST_H
#define AUDIO_POLICY_UNIT_TEST_H

#include "gtest/gtest.h"
#include "audio_info.h"
#include "audio_policy_manager.h"
#include "audio_policy_proxy.h"
#include "audio_stream_manager.h"
#include "audio_stream.h"

namespace OHOS {
namespace AudioStandard {
class AudioManagerDeviceChangeCallbackTest : public AudioManagerDeviceChangeCallback {
    virtual void OnDeviceChange(const DeviceChangeAction &deviceChangeAction) {}
};

class AudioRendererStateChangeCallbackTest : public AudioRendererStateChangeCallback {
    virtual void OnRendererStateChange(
        const std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos) {}
};

class AudioCapturerStateChangeCallbackTest : public AudioCapturerStateChangeCallback {
    virtual void OnCapturerStateChange(
        const std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos) {}
};
class AudioPolicyUnitTest : public testing::Test {
public:
    // SetUpTestCase: Called before all test cases
    static void SetUpTestCase(void);
    // TearDownTestCase: Called after all test case
    static void TearDownTestCase(void);
    // SetUp: Called before each test cases
    void SetUp(void);
    // TearDown: Called after each test cases
    void TearDown(void);
    static void InitAudioPolicyProxy(std::shared_ptr<AudioPolicyProxy> &audioPolicyProxy);
    static void InitAudioStream(std::shared_ptr<AudioStream> &audioStream);
    static uint32_t GetSessionId(std::shared_ptr<AudioStream> &audioStream);
    static void GetIRemoteObject(sptr<IRemoteObject> &object);
};
} // namespace AudioStandard
} // namespace OHOS

#endif // AUDIO_POLICY_UNIT_TEST_H
