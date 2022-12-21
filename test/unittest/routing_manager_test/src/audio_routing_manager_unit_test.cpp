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

#include <thread>
#include "audio_errors.h"
#include "audio_info.h"
#include "audio_routing_manager_unit_test.h"

using namespace std;
using namespace testing::ext;

namespace OHOS {
namespace AudioStandard {

void AudioRoutingManagerUnitTest::SetUpTestCase(void) {}
void AudioRoutingManagerUnitTest::TearDownTestCase(void) {}
void AudioRoutingManagerUnitTest::SetUp(void) {}
void AudioRoutingManagerUnitTest::TearDown(void) {}

/**
* @tc.name  : Test Audio_Routing_Manager_SetMicStateChangeCallback_001 via legal state
* @tc.number: Audio_Routing_Manager_SetMicStateChangeCallback_001
* @tc.desc  : Test SetMicStateChangeCallback interface. Returns success.
*/
HWTEST(AudioRoutingManagerUnitTest, Audio_Routing_Manager_SetMicStateChangeCallback_001, TestSize.Level1)
{
    int32_t ret = -1;
    std::shared_ptr<AudioManagerMicStateChangeCallbackTest> callback =
        std::make_shared<AudioManagerMicStateChangeCallbackTest>();
    ret = AudioRoutingManager::GetInstance()->SetMicStateChangeCallback(callback);
    EXPECT_EQ(SUCCESS, ret);
}
} // namespace AudioStandard
} // namespace OHOS