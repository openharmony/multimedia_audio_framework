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

#ifndef AUDIO_POLICY_MANAGER_LISTENER_PROXY_H
#define AUDIO_POLICY_MANAGER_LISTENER_PROXY_H

#include "audio_interrupt_callback.h"
#include "audio_system_manager.h"
#include "i_standard_audio_policy_manager_listener.h"

namespace OHOS {
namespace AudioStandard {
class AudioPolicyManagerListenerProxy : public IRemoteProxy<IStandardAudioPolicyManagerListener> {
public:
    explicit AudioPolicyManagerListenerProxy(const sptr<IRemoteObject> &impl);
    virtual ~AudioPolicyManagerListenerProxy();
    DISALLOW_COPY_AND_MOVE(AudioPolicyManagerListenerProxy);
    void OnInterrupt(const InterruptEventInternal &interruptEvent) override;
    void OnAvailableDeviceChange(const AudioDeviceUsage usage, const DeviceChangeAction &deviceChangeAction) override;
    bool OnQueryClientType(const std::string &bundleName, uint32_t uid) override;

private:
    static inline BrokerDelegator<AudioPolicyManagerListenerProxy> delegator_;
    void WriteInterruptEventParams(MessageParcel &data, const InterruptEventInternal &interruptEvent);
};

class AudioPolicyManagerListenerCallback : public AudioInterruptCallback {
public:
    AudioPolicyManagerListenerCallback(const sptr<IStandardAudioPolicyManagerListener> &listener);
    virtual ~AudioPolicyManagerListenerCallback();
    DISALLOW_COPY_AND_MOVE(AudioPolicyManagerListenerCallback);
    void OnInterrupt(const InterruptEventInternal &interruptEvent) override;
private:
    sptr<IStandardAudioPolicyManagerListener> listener_ = nullptr;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_POLICY_MANAGER_LISTENER_PROXY_H
