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

#include "audio_manager_listener_proxy.h"
#include "audio_system_manager.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
AudioManagerListenerProxy::AudioManagerListenerProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IStandardAudioServerManagerListener>(impl)
{
    AUDIO_DEBUG_LOG("Instances create");
}

AudioManagerListenerProxy::~AudioManagerListenerProxy()
{
    AUDIO_DEBUG_LOG("~AudioPolicyManagerListenerProxy: Instance destroy");
}

void AudioManagerListenerProxy::WriteParameterEventParams(MessageParcel& data, const std::string networkId,
    const AudioParamKey key, const std::string& condition, const std::string& value)
{
    data.WriteString(static_cast<std::string>(networkId));
    data.WriteInt32(static_cast<std::int32_t>(key));
    data.WriteString(static_cast<std::string>(condition));
    data.WriteString(static_cast<std::string>(value));
}

void AudioManagerListenerProxy::OnAudioParameterChange(const std::string networkId, const AudioParamKey key,
    const std::string& condition, const std::string& value)
{
    AUDIO_DEBUG_LOG("AudioManagerListenerProxy: ON_PARAMETER_CHANGED at listener proxy");

    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyManagerListenerProxy: WriteInterfaceToken failed");
        return;
    }

    data.WriteString(static_cast<std::string>(networkId));
    data.WriteInt32(static_cast<std::int32_t>(key));
    data.WriteString(static_cast<std::string>(condition));
    data.WriteString(static_cast<std::string>(value));
   
    int error = Remote()->SendRequest(ON_PARAMETER_CHANGED, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("ON_PARAMETER_CHANGED failed, error: %{public}d", error);
    }
}

AudioManagerListenerCallback::AudioManagerListenerCallback(const sptr<IStandardAudioServerManagerListener>& listener)
    : listener_(listener)
{
    AUDIO_DEBUG_LOG("AudioManagerListenerCallback: Instance create");
}

AudioManagerListenerCallback::~AudioManagerListenerCallback()
{
    AUDIO_DEBUG_LOG("AudioManagerListenerCallback: Instance destroy");
}

void AudioManagerListenerCallback::OnAudioParameterChange(const std::string networkId, const AudioParamKey key,
    const std::string& condition, const std::string& value)
{
    if (listener_ != nullptr) {
        listener_->OnAudioParameterChange(networkId, key, condition, value);
    }
}
} // namespace AudioStandard
} // namespace OHOS
