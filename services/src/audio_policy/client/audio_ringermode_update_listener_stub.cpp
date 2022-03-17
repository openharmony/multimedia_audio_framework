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

#include "audio_ringermode_update_listener_stub.h"

#include "audio_errors.h"
#include "audio_system_manager.h"
#include "media_log.h"

namespace OHOS {
namespace AudioStandard {
AudioRingerModeUpdateListenerStub::AudioRingerModeUpdateListenerStub()
{
    MEDIA_DEBUG_LOG("AudioRingerModeUpdateListenerStub Instance create");
}

AudioRingerModeUpdateListenerStub::~AudioRingerModeUpdateListenerStub()
{
    MEDIA_DEBUG_LOG("AudioRingerModeUpdateListenerStub Instance destroy");
}

int AudioRingerModeUpdateListenerStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    if (data.ReadInterfaceToken() != GetDescriptor()) {
        MEDIA_ERR_LOG("AudioRingerModeUpdateListenerStub: ReadInterfaceToken failed");
        return -1;
    }
    switch (code) {
        case ON_RINGERMODE_UPDATE: {
            AudioRingerMode ringerMode = static_cast<AudioRingerMode>(data.ReadInt32());
            OnRingerModeUpdated(ringerMode);
            return MEDIA_OK;
        }
        default: {
            MEDIA_ERR_LOG("default case, need check AudioListenerStub");
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
}

void AudioRingerModeUpdateListenerStub::OnRingerModeUpdated(const AudioRingerMode &ringerMode)
{
    std::shared_ptr<AudioRingerModeCallback> cb = callback_.lock();
    if (cb != nullptr) {
        cb->OnRingerModeUpdated(ringerMode);
    } else {
        MEDIA_ERR_LOG("AudioRingerModeUpdateListenerStub: callback_ is nullptr");
    }
}

void AudioRingerModeUpdateListenerStub::SetCallback(const std::weak_ptr<AudioRingerModeCallback> &callback)
{
    callback_ = callback;
}
} // namespace AudioStandard
} // namespace OHOS
