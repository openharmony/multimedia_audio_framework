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

#include <iostream>
#include <cstddef>
#include <cstdint>
#include "audio_info.h"
#include "audio_policy_server.h"
#include "message_parcel.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
const std::u16string FORMMGR_INTERFACE_TOKEN = u"IAudioPolicy";
const int32_t SYSTEM_ABILITY_ID = 3009;
const bool RUN_ON_CREATE = false;
const int32_t LIMITSIZE = 4;

void AudioVolumeFuzzTest(const uint8_t *rawData, size_t size)
{
    if (rawData == nullptr || size < LIMITSIZE) {
        return;
    }
    std::shared_ptr<AudioPolicyServer> AudioPolicyServerPtr =
        std::make_shared<AudioPolicyServer>(SYSTEM_ABILITY_ID, RUN_ON_CREATE);

    AudioStreamType streamType = *reinterpret_cast<const AudioStreamType *>(rawData);
    float volume = *reinterpret_cast<const float *>(rawData);
    int32_t streamId = *reinterpret_cast<const int32_t *>(rawData);
    bool mute = *reinterpret_cast<const bool *>(rawData);
    AudioPolicyServerPtr->SetStreamVolume(streamType, volume);
    AudioPolicyServerPtr->GetStreamVolume(streamType);
    AudioPolicyServerPtr->SetLowPowerVolume(streamId, volume);
    AudioPolicyServerPtr->GetLowPowerVolume(streamId);
    AudioPolicyServerPtr->GetSingleStreamVolume(streamId);
    AudioPolicyServerPtr->SetStreamMute(streamType, mute);
    AudioPolicyServerPtr->GetStreamMute(streamType);
    AudioPolicyServerPtr->IsStreamActive(streamType);
}

void AudioDeviceFuzzTest(const uint8_t *rawData, size_t size)
{
    if (rawData == nullptr || size < LIMITSIZE) {
        return;
    }
    std::shared_ptr<AudioPolicyServer> AudioPolicyServerPtr =
        std::make_shared<AudioPolicyServer>(SYSTEM_ABILITY_ID, RUN_ON_CREATE);

    MessageParcel data;
    data.WriteInterfaceToken(FORMMGR_INTERFACE_TOKEN);
    data.WriteBuffer(rawData, size);
    data.RewindRead(0);

    InternalDeviceType deviceType = *reinterpret_cast<const InternalDeviceType *>(rawData);
    bool active = *reinterpret_cast<const bool *>(rawData);
    AudioPolicyServerPtr->SetDeviceActive(deviceType, active);
    AudioPolicyServerPtr->IsDeviceActive(deviceType);

    AudioRingerMode ringMode = *reinterpret_cast<const AudioRingerMode *>(rawData);
    AudioPolicyServerPtr->SetRingerMode(ringMode);

    int32_t ltonetype = *reinterpret_cast<const int32_t *>(rawData);
    AudioPolicyServerPtr->GetToneConfig(ltonetype);

    AudioScene audioScene = *reinterpret_cast<const AudioScene *>(rawData);
    AudioPolicyServerPtr->SetAudioScene(audioScene);

    bool mute = *reinterpret_cast<const bool *>(rawData);
    AudioPolicyServerPtr->SetMicrophoneMute(mute);

    int32_t clientId = *reinterpret_cast<const int32_t *>(rawData);
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    AudioPolicyServerPtr->SetRingerModeCallback(clientId, object);
    AudioPolicyServerPtr->UnsetRingerModeCallback(clientId);

    DeviceFlag flag = *reinterpret_cast<const DeviceFlag *>(rawData);
    AudioPolicyServerPtr->SetDeviceChangeCallback(clientId, flag, object);
    AudioPolicyServerPtr->UnsetDeviceChangeCallback(clientId);
}

void AudioInterruptFuzzTest(const uint8_t *rawData, size_t size)
{
    if (rawData == nullptr || size < LIMITSIZE) {
        return;
    }
    std::shared_ptr<AudioPolicyServer> AudioPolicyServerPtr =
        std::make_shared<AudioPolicyServer>(SYSTEM_ABILITY_ID, RUN_ON_CREATE);

    MessageParcel data;
    data.WriteInterfaceToken(FORMMGR_INTERFACE_TOKEN);
    data.WriteBuffer(rawData, size);
    data.RewindRead(0);

    sptr<IRemoteObject> object = data.ReadRemoteObject();
    uint32_t sessionID = *reinterpret_cast<const uint32_t *>(rawData);
    AudioPolicyServerPtr->SetAudioInterruptCallback(sessionID, object);
    AudioPolicyServerPtr->UnsetAudioInterruptCallback(sessionID);

    uint32_t clientID = *reinterpret_cast<const uint32_t *>(rawData);
    AudioPolicyServerPtr->SetAudioManagerInterruptCallback(clientID, object);

    AudioInterrupt audioInterrupt;
    audioInterrupt.contentType = *reinterpret_cast<const ContentType *>(rawData);
    audioInterrupt.streamUsage = *reinterpret_cast<const StreamUsage *>(rawData);
    audioInterrupt.streamType = *reinterpret_cast<const AudioStreamType *>(rawData);
    AudioPolicyServerPtr->RequestAudioFocus(clientID, audioInterrupt);
    AudioPolicyServerPtr->AbandonAudioFocus(clientID, audioInterrupt);
}

void AudioPolicyFuzzTest(const uint8_t *rawData, size_t size)
{
    if (rawData == nullptr || size < LIMITSIZE) {
        return;
    }
    std::shared_ptr<AudioPolicyServer> AudioPolicyServerPtr =
        std::make_shared<AudioPolicyServer>(SYSTEM_ABILITY_ID, RUN_ON_CREATE);

    MessageParcel data;
    data.WriteInterfaceToken(FORMMGR_INTERFACE_TOKEN);
    data.WriteBuffer(rawData, size);
    data.RewindRead(0);

    sptr<IRemoteObject> object = data.ReadRemoteObject();
    int32_t clientPid = *reinterpret_cast<const int32_t *>(rawData);
    AudioPolicyServerPtr->SetVolumeKeyEventCallback(clientPid, object);
    AudioPolicyServerPtr->UnsetVolumeKeyEventCallback(clientPid);

    uint32_t sessionID = *reinterpret_cast<const uint32_t *>(rawData);
    AudioPolicyServerPtr->OnSessionRemoved(sessionID);

    int32_t clientUID = *reinterpret_cast<const int32_t *>(rawData);
    AudioPolicyServerPtr->RegisterAudioRendererEventListener(clientUID, object);
    AudioPolicyServerPtr->UnregisterAudioRendererEventListener(clientUID);
    AudioPolicyServerPtr->RegisterAudioCapturerEventListener(clientUID, object);
    AudioPolicyServerPtr->UnregisterAudioCapturerEventListener(clientUID);

    AudioPolicyServer::DeathRecipientId id =
        *reinterpret_cast<const AudioPolicyServer::DeathRecipientId *>(rawData);
    AudioPolicyServerPtr->RegisterClientDeathRecipient(object, id);

    int pid = *reinterpret_cast<const int *>(rawData);
    AudioPolicyServerPtr->RegisteredTrackerClientDied(pid);

    AudioStreamInfo audioStreamInfo = {};
    audioStreamInfo.samplingRate = *reinterpret_cast<const AudioSamplingRate *>(rawData);
    audioStreamInfo.channels = *reinterpret_cast<const AudioChannel *>(rawData);
    audioStreamInfo.format = *reinterpret_cast<const AudioSampleFormat *>(rawData);
    audioStreamInfo.encoding = *reinterpret_cast<const AudioEncodingType *>(rawData);
    AudioPolicyServerPtr->IsAudioRendererLowLatencySupported(audioStreamInfo);

    int32_t clientUid = *reinterpret_cast<const int32_t *>(rawData);
    StreamSetState streamSetState = *reinterpret_cast<const StreamSetState *>(rawData);
    AudioStreamType audioStreamType = *reinterpret_cast<const AudioStreamType *>(rawData);
    AudioPolicyServerPtr->UpdateStreamState(clientUid, streamSetState, audioStreamType);
}
} // namespace AudioStandard
} // namesapce OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    OHOS::AudioStandard::AudioVolumeFuzzTest(data, size);
    OHOS::AudioStandard::AudioDeviceFuzzTest(data, size);
    OHOS::AudioStandard::AudioInterruptFuzzTest(data, size);
    OHOS::AudioStandard::AudioPolicyFuzzTest(data, size);
    return 0;
}

