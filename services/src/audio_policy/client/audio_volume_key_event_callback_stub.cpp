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

#include "audio_volume_key_event_callback_stub.h"
#include "media_log.h"

namespace OHOS {
namespace AudioStandard {
AudioVolumeKeyEventCallbackStub::AudioVolumeKeyEventCallbackStub()
{
}

AudioVolumeKeyEventCallbackStub::~AudioVolumeKeyEventCallbackStub()
{
}

int AudioVolumeKeyEventCallbackStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    MEDIA_DEBUG_LOG("AudioVolumeKeyEventCallbackStub::OnRemoteRequest");
    if (data.ReadInterfaceToken() != GetDescriptor()) {
        MEDIA_ERR_LOG("AudioVolumeKeyEventCallbackStub: ReadInterfaceToken failed");
        return -1;
    }
    switch (code) {
        case ON_VOLUME_KEY_EVENT: {
            AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
            int32_t volume = data.ReadInt32();
            bool isUpdateUi = data.ReadBool();
            OnVolumeKeyEvent(streamType, volume, isUpdateUi);
            reply.WriteInt32(0);
            break;
        }
        default: {
            reply.WriteInt32(-1);
            break;
        }
    }
    return 0;
}

void AudioVolumeKeyEventCallbackStub::OnVolumeKeyEvent(AudioStreamType streamType, int32_t volumeLevel, bool isUpdateUi)
{
    MEDIA_DEBUG_LOG("AudioVolumeKeyEventCallbackStub::OnVolumeKeyEvent");
    std::shared_ptr<VolumeKeyEventCallback> cb = callback_.lock();
    if (cb != nullptr) {
        MEDIA_DEBUG_LOG("AudioVolumeKeyEventCallbackStub::OnVolumeKeyEvent CALLBACK NOT NULL");
        cb->OnVolumeKeyEvent(streamType, volumeLevel, isUpdateUi);
    } else {
        MEDIA_DEBUG_LOG("AudioVolumeKeyEventCallbackStub::OnVolumeKeyEvent CALLBACK NULL");
    }
}

void AudioVolumeKeyEventCallbackStub::SetOnVolumeKeyEventCallback(
    const std::weak_ptr<VolumeKeyEventCallback> &callback)
{
    MEDIA_DEBUG_LOG("AudioVolumeKeyEventCallbackStub::SetOnVolumeKeyEventCallback");
    callback_ = callback;
}
} // namespace AudioStandard
} // namespace OHOS
