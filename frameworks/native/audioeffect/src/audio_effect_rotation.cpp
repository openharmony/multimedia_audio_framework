/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#ifndef LOG_TAG
#define LOG_TAG "AudioEffectRotation"
#endif

#include "audio_effect_chain_adapter.h"
#include "audio_effect_rotation.h"
#include "audio_effect_log.h"

namespace OHOS {
namespace AudioStandard {
#ifdef WINDOW_MANAGER_ENABLE
AudioEffectRotation::AudioEffectRotation()
{
    AUDIO_DEBUG_LOG("created!");
    rotationState_ = 0;
}

AudioEffectRotation::~AudioEffectRotation()
{
    AUDIO_DEBUG_LOG("destroyed!");
}

std::shared_ptr<AudioEffectRotation> AudioEffectRotation::GetInstance()
{
    static std::shared_ptr<AudioEffectRotation> effectRotation = std::make_shared<AudioEffectRotation>();
    return effectRotation;
}

void AudioEffectRotation::SetRotation(uint32_t rotationState)
{
    rotationState_ = rotationState;
}

uint32_t AudioEffectRotation::GetRotation()
{
    return rotationState_;
}
#endif
}  // namespace AudioStandard
}  // namespace OHOS