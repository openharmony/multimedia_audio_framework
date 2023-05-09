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

#include "audio_ringermode_update_listener_proxy.h"
#include "audio_system_manager.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
AudioRingerModeUpdateListenerProxy::AudioRingerModeUpdateListenerProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IStandardRingerModeUpdateListener>(impl)
{
    AUDIO_DEBUG_LOG("Instances create");
}

AudioRingerModeUpdateListenerProxy::~AudioRingerModeUpdateListenerProxy()
{
    AUDIO_DEBUG_LOG("~AudioRingerModeUpdateListenerProxy: Instance destroy");
}

void AudioRingerModeUpdateListenerProxy::OnRingerModeUpdated(const AudioRingerMode &ringerMode)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioRingerModeListenerCallback: WriteInterfaceToken failed");
        return;
    }
    data.WriteInt32(static_cast<int32_t>(ringerMode));
    int error = Remote()->SendRequest(ON_RINGERMODE_UPDATE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("OnRingerModeUpdated failed, error: %{public}d", error);
    }
}

AudioRingerModeListenerCallback::AudioRingerModeListenerCallback(
    const sptr<IStandardRingerModeUpdateListener> &listener) : listener_(listener)
{
        AUDIO_DEBUG_LOG("AudioRingerModeListenerCallback: Instance create");
}

AudioRingerModeListenerCallback::~AudioRingerModeListenerCallback()
{
    AUDIO_DEBUG_LOG("AudioRingerModeListenerCallback: Instance destroy");
}

void AudioRingerModeListenerCallback::OnRingerModeUpdated(const AudioRingerMode &ringerMode)
{
    if (listener_ != nullptr) {
        listener_->OnRingerModeUpdated(ringerMode);
    }
}
} // namespace AudioStandard
} // namespace OHOS
