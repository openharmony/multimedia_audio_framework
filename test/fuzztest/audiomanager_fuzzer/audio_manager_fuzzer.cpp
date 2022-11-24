/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <iostream>

#include "audio_errors.h"
#include "audio_info.h"
#include "audio_system_manager.h"
#include "audio_stream_manager.h"
#include "audio_manager_fuzzer.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
void AudioRendererStateCallbackFuzz::OnRendererStateChange(
    const std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos) {}

void AudioCapturerStateCallbackFuzz::OnCapturerStateChange(
    const std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos) {}
const int32_t LIMITSIZE = 4;
void AudioManagerFuzzTest(const uint8_t* data, size_t size)
{
    if ((data == nullptr) || (size < LIMITSIZE)) {
        return;
    }

    AudioVolumeType type = *reinterpret_cast<const AudioVolumeType *>(data);
    int32_t volume = *reinterpret_cast<const int32_t *>(data);
    AudioSystemManager::GetInstance()->SetVolume(type, volume);
    AudioSystemManager::GetInstance()->GetVolume(type);
    AudioSystemManager::GetInstance()->GetMinVolume(type);
    AudioSystemManager::GetInstance()->GetMaxVolume(type);
    AudioSystemManager::GetInstance()->SetMute(type, true);
    AudioSystemManager::GetInstance()->IsStreamMute(type);
    AudioSystemManager::GetInstance()->SetRingerMode(*reinterpret_cast<const AudioRingerMode *>(data));
    AudioSystemManager::GetInstance()->SetAudioScene(*reinterpret_cast<const AudioScene *>(data));

    std::string key(reinterpret_cast<const char*>(data), size);
    std::string value(reinterpret_cast<const char*>(data), size);
    AudioSystemManager::GetInstance()->SetAudioParameter(key, value);
}

void AudioStreamManagerFuzzTest(const uint8_t* data, size_t size)
{
    if ((data == nullptr) || (size < LIMITSIZE)) {
        return;
    }

    int32_t clientUID = *reinterpret_cast<const int32_t *>(data);
    shared_ptr<AudioRendererStateCallbackFuzz> audioRendererStateCallbackFuzz =
        std::make_shared<AudioRendererStateCallbackFuzz>();
    shared_ptr<AudioCapturerStateCallbackFuzz> audioCapturerStateCallbackFuzz =
        std::make_shared<AudioCapturerStateCallbackFuzz>();
    AudioStreamManager::GetInstance()->RegisterAudioRendererEventListener(clientUID, audioRendererStateCallbackFuzz);
    AudioStreamManager::GetInstance()->UnregisterAudioRendererEventListener(clientUID);
    AudioStreamManager::GetInstance()->RegisterAudioCapturerEventListener(clientUID, audioCapturerStateCallbackFuzz);
    AudioStreamManager::GetInstance()->UnregisterAudioCapturerEventListener(clientUID);

    std::vector<std::unique_ptr<AudioRendererChangeInfo>> audioRendererChangeInfos;
    AudioStreamManager::GetInstance()->GetCurrentRendererChangeInfos(audioRendererChangeInfos);

    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
    AudioStreamManager::GetInstance()->GetCurrentCapturerChangeInfos(audioCapturerChangeInfos);
}
} // namespace AudioStandard
} // namesapce OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::AudioStandard::AudioManagerFuzzTest(data, size);
    OHOS::AudioStandard::AudioStreamManagerFuzzTest(data, size);
    return 0;
}
