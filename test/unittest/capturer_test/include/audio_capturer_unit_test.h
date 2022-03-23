/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef AUDIO_CAPTURER_UNIT_TEST_H
#define AUDIO_CAPTURER_UNIT_TEST_H

#include "gtest/gtest.h"
#include "audio_capturer.h"

namespace OHOS {
namespace AudioStandard {
class CapturerPositionCallbackTest : public CapturerPositionCallback {
public:
    void OnMarkReached(const int64_t &framePosition) override {}
};

class CapturerPeriodPositionCallbackTest : public CapturerPeriodPositionCallback {
public:
    void OnPeriodReached(const int64_t &frameNumber) override {}
};

class AudioCapturerCallbackTest : public AudioCapturerCallback {
public:
    void OnStateChange(const CapturerState state) override {}
};

class AudioCapturerUnitTest : public testing::Test {
public:
    // SetUpTestCase: Called before all test cases
    static void SetUpTestCase(void);
    // TearDownTestCase: Called after all test case
    static void TearDownTestCase(void);
    // SetUp: Called before each test cases
    void SetUp(void);
    // TearDown: Called after each test cases
    void TearDown(void);
    // Init Capturer
    static int32_t InitializeCapturer(std::unique_ptr<AudioCapturer> &audioCapturer);
    // Init Capturer Options
    static void InitializeCapturerOptions(AudioCapturerOptions &capturerOptions);
};
} // namespace AudioStandard
} // namespace OHOS

#endif // AUDIO_CAPTURER_UNIT_TEST_H
