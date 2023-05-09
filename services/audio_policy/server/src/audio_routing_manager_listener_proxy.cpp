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

#include "audio_routing_manager_listener_proxy.h"
#include "audio_routing_manager.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
AudioRoutingManagerListenerProxy::AudioRoutingManagerListenerProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IStandardAudioRoutingManagerListener>(impl)
{
    AUDIO_DEBUG_LOG("Instances create");
}

AudioRoutingManagerListenerProxy::~AudioRoutingManagerListenerProxy()
{
    AUDIO_DEBUG_LOG("~AudioRoutingManagerListenerProxy: Instance destroy");
}

void AudioRoutingManagerListenerProxy::OnMicStateUpdated(const MicStateChangeEvent &micStateChangeEvent)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyManagerListenerProxy: WriteInterfaceToken failed");
        return;
    }

    data.WriteBool(micStateChangeEvent.mute);
    int error = Remote()->SendRequest(ON_MIC_STATE_UPDATED, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("OnMicStateUpdated failed, error: %{public}d", error);
    }
}

AudioRoutingManagerListenerCallback::AudioRoutingManagerListenerCallback(
    const sptr<IStandardAudioRoutingManagerListener> &listener) : listener_(listener)
{
        AUDIO_DEBUG_LOG("AudioRoutingManagerListenerCallback: Instance create");
}

AudioRoutingManagerListenerCallback::~AudioRoutingManagerListenerCallback()
{
    AUDIO_DEBUG_LOG("AudioRoutingManagerListenerCallback: Instance destroy");
}

void AudioRoutingManagerListenerCallback::OnMicStateUpdated(const MicStateChangeEvent &micStateChangeEvent)
{
    if (listener_ != nullptr) {
        listener_->OnMicStateUpdated(micStateChangeEvent);
    }
}
} // namespace AudioStandard
} // namespace OHOS
