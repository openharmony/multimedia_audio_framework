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

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_policy_manager_listener_stub.h"

namespace OHOS {
namespace AudioStandard {
AudioPolicyManagerListenerStub::AudioPolicyManagerListenerStub()
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerLiternerStub Instance create");
}

AudioPolicyManagerListenerStub::~AudioPolicyManagerListenerStub()
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerListenerStub Instance complete");
}

void AudioPolicyManagerListenerStub::ReadInterruptEventParams(MessageParcel &data,
                                                              InterruptEventInternal &interruptEvent)
{
    interruptEvent.eventType = static_cast<InterruptType>(data.ReadInt32());
    interruptEvent.forceType = static_cast<InterruptForceType>(data.ReadInt32());
    interruptEvent.hintType = static_cast<InterruptHint>(data.ReadInt32());
    interruptEvent.duckVolume = data.ReadFloat();
}

void AudioPolicyManagerListenerStub::ReadAudioDeviceChangeData(MessageParcel &data, DeviceChangeAction &devChange)
{
    std::vector<sptr<AudioDeviceDescriptor>> deviceChangeDesc = {};

    int32_t type = data.ReadInt32();
    int32_t size = data.ReadInt32();

    for (int32_t i = 0; i < size; i++) {
        deviceChangeDesc.push_back(AudioDeviceDescriptor::Unmarshalling(data));
    }

    devChange.type = static_cast<DeviceChangeType>(type);
    devChange.deviceDescriptors = deviceChangeDesc;
}

int AudioPolicyManagerListenerStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    if (data.ReadInterfaceToken() != GetDescriptor()) {
        AUDIO_ERR_LOG("AudioPolicyManagerListenerStub: ReadInterfaceToken failed");
        return -1;
    }
    switch (code) {
        case ON_INTERRUPT: {
            InterruptEventInternal interruptEvent = {};
            ReadInterruptEventParams(data, interruptEvent);
            // To be modified by enqueuing the interrupt action scheduler
            OnInterrupt(interruptEvent);
            return AUDIO_OK;
        }
        case ON_DEVICE_CHANGED: {
            AUDIO_INFO_LOG("Device change callback received");
            DeviceChangeAction deviceChangeAction = {};

            ReadAudioDeviceChangeData(data, deviceChangeAction);
            OnDeviceChange(deviceChangeAction);

            return AUDIO_OK;
        }
        default: {
            AUDIO_ERR_LOG("default case, need check AudioListenerStub");
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
}

void AudioPolicyManagerListenerStub::OnInterrupt(const InterruptEventInternal &interruptEvent)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerLiternerStub OnInterrupt start");
    std::shared_ptr<AudioInterruptCallback> cb = callback_.lock();
    if (cb != nullptr) {
        cb->OnInterrupt(interruptEvent);
    } else {
        AUDIO_ERR_LOG("AudioPolicyManagerListenerStub: callback_ is nullptr");
    }
}

void AudioPolicyManagerListenerStub::OnDeviceChange(const DeviceChangeAction &deviceChangeAction)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerLiternerStub OnDeviceChange start");
    std::shared_ptr<AudioManagerDeviceChangeCallback> deviceChangedCallback = deviceChangeCallback_.lock();

    if (deviceChangedCallback == nullptr) {
        AUDIO_ERR_LOG("OnDeviceChange: deviceChangeCallback_ or deviceChangeAction is nullptr");
        return;
    }

    deviceChangedCallback->OnDeviceChange(deviceChangeAction);
}

void AudioPolicyManagerListenerStub::SetInterruptCallback(const std::weak_ptr<AudioInterruptCallback> &callback)
{
    callback_ = callback;
}

void AudioPolicyManagerListenerStub::SetDeviceChangeCallback(const std::weak_ptr<AudioManagerDeviceChangeCallback> &cb)
{
    deviceChangeCallback_ = cb;
}
} // namespace AudioStandard
} // namespace OHOS
