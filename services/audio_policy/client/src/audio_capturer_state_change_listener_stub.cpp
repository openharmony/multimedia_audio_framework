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

#include "audio_capturer_state_change_listener_stub.h"

#include "audio_errors.h"
#include "audio_system_manager.h"
#include "audio_log.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
AudioCapturerStateChangeListenerStub::AudioCapturerStateChangeListenerStub()
{
    AUDIO_DEBUG_LOG("AudioCapturerStateChangeListenerStub Instance create");
}

AudioCapturerStateChangeListenerStub::~AudioCapturerStateChangeListenerStub()
{
    AUDIO_DEBUG_LOG("AudioCapturerStateChangeListenerStub Instance destroy");
}

void AudioCapturerStateChangeListenerStub::ReadAudioCapturerChangeInfo(MessageParcel &data,
    unique_ptr<AudioCapturerChangeInfo> &capturerChangeInfo)
{
    AUDIO_DEBUG_LOG("AudioCapturerStateChangeListenerStub: ReadAudioCapturerChangeInfo");
    capturerChangeInfo->sessionId = data.ReadInt32();
    capturerChangeInfo->capturerState = static_cast<CapturerState>(data.ReadInt32());
    capturerChangeInfo->clientUID = data.ReadInt32();
    capturerChangeInfo->capturerInfo.sourceType = static_cast<SourceType>(data.ReadInt32());
    capturerChangeInfo->capturerInfo.capturerFlags = data.ReadInt32();

    capturerChangeInfo->inputDeviceInfo.deviceType = static_cast<DeviceType>(data.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.deviceRole = static_cast<DeviceRole>(data.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.deviceId = data.ReadInt32();
    capturerChangeInfo->inputDeviceInfo.channelMasks = data.ReadInt32();
    capturerChangeInfo->inputDeviceInfo.audioStreamInfo.samplingRate
        = static_cast<AudioSamplingRate>(data.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.audioStreamInfo.encoding
        = static_cast<AudioEncodingType>(data.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.audioStreamInfo.format = static_cast<AudioSampleFormat>(data.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.audioStreamInfo.channels = static_cast<AudioChannel>(data.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.deviceName = data.ReadString();
    capturerChangeInfo->inputDeviceInfo.macAddress = data.ReadString();

    AUDIO_DEBUG_LOG("AudioCapturerStateChangeListenerStub, sessionid = %{public}d", capturerChangeInfo->sessionId);
    AUDIO_DEBUG_LOG("AudioCapturerStateChangeListenerStub, capturerState = %{public}d",
        capturerChangeInfo->capturerState);
    AUDIO_DEBUG_LOG("AudioCapturerStateChangeListenerStub, clientUID = %{public}d", capturerChangeInfo->clientUID);
    AUDIO_DEBUG_LOG("AudioCapturerStateChangeListenerStub, sourceType = %{public}d",
        capturerChangeInfo->capturerInfo.sourceType);
    AUDIO_DEBUG_LOG("AudioCapturerStateChangeListenerStub, capturerFlags = %{public}d",
        capturerChangeInfo->capturerInfo.capturerFlags);
}

int AudioCapturerStateChangeListenerStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    if (data.ReadInterfaceToken() != GetDescriptor()) {
        AUDIO_ERR_LOG("AudioCapturerStateChangeListenerStub: ReadInterfaceToken failed");
        return -1;
    }
    switch (code) {
        case ON_CAPTURERSTATE_CHANGE: {
            vector<unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
            int32_t size = data.ReadInt32();
            while (size > 0) {
                unique_ptr<AudioCapturerChangeInfo> capturerChangeInfo = make_unique<AudioCapturerChangeInfo>();
                CHECK_AND_RETURN_RET_LOG(capturerChangeInfo != nullptr, ERR_MEMORY_ALLOC_FAILED, "No memory!!");
                ReadAudioCapturerChangeInfo(data, capturerChangeInfo);
                audioCapturerChangeInfos.push_back(move(capturerChangeInfo));
                size--;
            }
            OnCapturerStateChange(audioCapturerChangeInfos);
            return AUDIO_OK;
        }
        default: {
            AUDIO_ERR_LOG("default case, need check AudioListenerStub");
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
}

void AudioCapturerStateChangeListenerStub::OnCapturerStateChange(
    const vector<unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    shared_ptr<AudioCapturerStateChangeCallback> cb = callback_.lock();
    if (cb == nullptr) {
        AUDIO_ERR_LOG("AudioCapturerStateChangeListenerStub: callback_ is nullptr");
    } else {
        cb->OnCapturerStateChange(audioCapturerChangeInfos);
    }
    return;
}

void AudioCapturerStateChangeListenerStub::SetCallback(const weak_ptr<AudioCapturerStateChangeCallback> &callback)
{
    callback_ = callback;
}
} // namespace AudioStandard
} // namespace OHOS
