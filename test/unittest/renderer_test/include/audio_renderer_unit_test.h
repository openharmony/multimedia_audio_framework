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

#ifndef AUDIO_RENDERER_UNIT_TEST_H
#define AUDIO_RENDERER_UNIT_TEST_H

#include "gtest/gtest.h"
#include "audio_renderer.h"

namespace OHOS {
namespace AudioStandard {
class AudioRendererCallbackTest : public AudioRendererCallback {
public:
    void OnInterrupt(const InterruptEvent &interruptEvent) override {}
    void OnStateChange(const RendererState state, const StateChangeCmdType cmdType) override {}
};

class RendererPositionCallbackTest : public RendererPositionCallback {
public:
    void OnMarkReached(const int64_t &framePosition) override {}
};

class RendererPeriodPositionCallbackTest : public RendererPeriodPositionCallback {
public:
    void OnPeriodReached(const int64_t &frameNumber) override {}
};

class AudioRenderModeCallbackTest : public AudioRendererWriteCallback {
public:
    void OnWriteData(size_t length) override;
};

class AudioRendererUnitTest : public testing::Test {
public:
    // SetUpTestCase: Called before all test cases
    static void SetUpTestCase(void);
    // TearDownTestCase: Called after all test case
    static void TearDownTestCase(void);
    // SetUp: Called before each test cases
    void SetUp(void);
    // TearDown: Called after each test cases
    void TearDown(void);
    // Init Renderer
    static int32_t InitializeRenderer(std::unique_ptr<AudioRenderer> &audioRenderer);
    // Init Renderer Options
    static void InitializeRendererOptions(AudioRendererOptions &rendererOptions);
};
} // namespace AudioStandard
} // namespace OHOS

#endif // AUDIO_RENDERER_UNIT_TEST_H
