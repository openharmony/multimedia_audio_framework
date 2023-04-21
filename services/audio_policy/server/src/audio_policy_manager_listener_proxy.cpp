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

#include "audio_policy_manager_listener_proxy.h"
#include "audio_system_manager.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
AudioPolicyManagerListenerProxy::AudioPolicyManagerListenerProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IStandardAudioPolicyManagerListener>(impl)
{
    AUDIO_DEBUG_LOG("Instances create");
}

AudioPolicyManagerListenerProxy::~AudioPolicyManagerListenerProxy()
{
    AUDIO_DEBUG_LOG("~AudioPolicyManagerListenerProxy: Instance destroy");
}

void AudioPolicyManagerListenerProxy::WriteInterruptEventParams(MessageParcel &data,
                                                                const InterruptEventInternal &interruptEvent)
{
    data.WriteInt32(static_cast<int32_t>(interruptEvent.eventType));
    data.WriteInt32(static_cast<int32_t>(interruptEvent.forceType));
    data.WriteInt32(static_cast<int32_t>(interruptEvent.hintType));
    data.WriteFloat(interruptEvent.duckVolume);
}

void AudioPolicyManagerListenerProxy::OnInterrupt(const InterruptEventInternal &interruptEvent)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyManagerListenerProxy: WriteInterfaceToken failed");
        return;
    }

    WriteInterruptEventParams(data, interruptEvent);
    int error = Remote()->SendRequest(ON_INTERRUPT, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("OnInterrupt failed, error: %{public}d", error);
    }
}

void AudioPolicyManagerListenerProxy::OnDeviceChange(const DeviceChangeAction &deviceChangeAction)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerListenerProxy: OnDeviceChange at listener proxy");

    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyManagerListenerProxy: WriteInterfaceToken failed");
        return;
    }

    auto devices = deviceChangeAction.deviceDescriptors;
    size_t size = deviceChangeAction.deviceDescriptors.size();

    data.WriteInt32(deviceChangeAction.type);
    data.WriteInt32(static_cast<int32_t>(size));

    for (size_t i = 0; i < size; i++) {
        devices[i]->Marshalling(data);
    }

    int error = Remote()->SendRequest(ON_DEVICE_CHANGED, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("OnDeviceChange failed, error: %{public}d", error);
    }
}

AudioPolicyManagerListenerCallback::AudioPolicyManagerListenerCallback(
    const sptr<IStandardAudioPolicyManagerListener> &listener) : listener_(listener)
{
        AUDIO_DEBUG_LOG("AudioPolicyManagerListenerCallback: Instance create");
}

AudioPolicyManagerListenerCallback::~AudioPolicyManagerListenerCallback()
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerListenerCallback: Instance destroy");
}

void AudioPolicyManagerListenerCallback::OnInterrupt(const InterruptEventInternal &interruptEvent)
{
    if (listener_ != nullptr) {
        listener_->OnInterrupt(interruptEvent);
    }
}
} // namespace AudioStandard
} // namespace OHOS
