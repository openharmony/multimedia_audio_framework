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
#include "media_log.h"
#include "audio_volume_key_event_callback_proxy.h"

namespace OHOS {
namespace AudioStandard {
AudioVolumeKeyEventCallbackProxy::AudioVolumeKeyEventCallbackProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IAudioVolumeKeyEventCallback>(impl) { }

void AudioVolumeKeyEventCallbackProxy::OnVolumeKeyEvent(AudioStreamType streamType, int32_t volumeLevel,
    bool isUpdateUi)
{
    MEDIA_DEBUG_LOG("AudioVolumeKeyEventCallbackProxy::OnVolumeKeyEvent");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioVolumeKeyEventCallbackProxy: WriteInterfaceToken failed");
        return;
    }
    data.WriteInt32(static_cast<int32_t>(streamType));
    data.WriteInt32(volumeLevel);
    data.WriteBool(isUpdateUi);
    int error = Remote()->SendRequest(ON_VOLUME_KEY_EVENT, data, reply, option);
    if (error != 0) {
        MEDIA_DEBUG_LOG("Error while sending volume key event %{public}d", error);
    }
    reply.ReadInt32();
}

VolumeKeyEventCallbackListner::VolumeKeyEventCallbackListner(const sptr<IAudioVolumeKeyEventCallback> &listener)
    : listener_(listener)
{
    MEDIA_DEBUG_LOG("VolumeKeyEventCallbackListner");
}

VolumeKeyEventCallbackListner::~VolumeKeyEventCallbackListner()
{
    MEDIA_DEBUG_LOG("VolumeKeyEventCallbackListner desctrutor");
}

void VolumeKeyEventCallbackListner::OnVolumeKeyEvent(AudioStreamType streamType, int32_t volumeLevel, bool isUpdateUi)
{
    MEDIA_DEBUG_LOG("AudioVolumeKeyEventCallbackProxy VolumeKeyEventCallbackListner");
    if (listener_ != nullptr) {
        listener_->OnVolumeKeyEvent(streamType, volumeLevel, isUpdateUi);
    }
}
} // namespace AudioStandard
} // namespace OHOS
