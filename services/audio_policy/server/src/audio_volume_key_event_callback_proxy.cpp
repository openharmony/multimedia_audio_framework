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
#include "audio_log.h"
#include "audio_volume_key_event_callback_proxy.h"

namespace OHOS {
namespace AudioStandard {
AudioVolumeKeyEventCallbackProxy::AudioVolumeKeyEventCallbackProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IAudioVolumeKeyEventCallback>(impl) { }

void AudioVolumeKeyEventCallbackProxy::OnVolumeKeyEvent(VolumeEvent volumeEvent)
{
    AUDIO_DEBUG_LOG("AudioVolumeKeyEventCallbackProxy::OnVolumeKeyEvent");
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioVolumeKeyEventCallbackProxy: WriteInterfaceToken failed");
        return;
    }
    data.WriteInt32(static_cast<int32_t>(volumeEvent.volumeType));
    data.WriteInt32(volumeEvent.volume);
    data.WriteBool(volumeEvent.updateUi);
    data.WriteInt32(volumeEvent.volumeGroupId);
    data.WriteString(volumeEvent.networkId);
    int error = Remote()->SendRequest(ON_VOLUME_KEY_EVENT, data, reply, option);
    if (error != 0) {
        AUDIO_DEBUG_LOG("Error while sending volume key event %{public}d", error);
    }
    reply.ReadInt32();
}

VolumeKeyEventCallbackListner::VolumeKeyEventCallbackListner(const sptr<IAudioVolumeKeyEventCallback> &listener)
    : listener_(listener)
{
    AUDIO_DEBUG_LOG("VolumeKeyEventCallbackListner");
}

VolumeKeyEventCallbackListner::~VolumeKeyEventCallbackListner()
{
    AUDIO_DEBUG_LOG("VolumeKeyEventCallbackListner desctrutor");
}

void VolumeKeyEventCallbackListner::OnVolumeKeyEvent(VolumeEvent volumeEvent)
{
    AUDIO_DEBUG_LOG("AudioVolumeKeyEventCallbackProxy VolumeKeyEventCallbackListner");
    if (listener_ != nullptr) {
        listener_->OnVolumeKeyEvent(volumeEvent);
    }
}
} // namespace AudioStandard
} // namespace OHOS
