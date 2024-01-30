/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef AUDIO_POLICY_CLIENT_STUB_IMPL_H
#define AUDIO_POLICY_CLIENT_STUB_IMPL_H

#include "audio_policy_client_stub.h"
#include "audio_device_info.h"
#include "audio_system_manager.h"
#include "audio_interrupt_info.h"
#include "audio_group_manager.h"
#include "audio_routing_manager.h"

namespace OHOS {
namespace AudioStandard {
class AudioPolicyClientStubImpl : public AudioPolicyClientStub {
public:
    int32_t AddVolumeKeyEventCallback(const std::shared_ptr<VolumeKeyEventCallback> &cb);
    int32_t RemoveVolumeKeyEventCallback();
    int32_t AddFocusInfoChangeCallback(const std::shared_ptr<AudioFocusInfoChangeCallback> &cb);
    int32_t RemoveFocusInfoChangeCallback();
    int32_t AddDeviceChangeCallback(const DeviceFlag &flag,
        const std::shared_ptr<AudioManagerDeviceChangeCallback> &cb);
    int32_t RemoveDeviceChangeCallback();
    int32_t AddRingerModeCallback(const std::shared_ptr<AudioRingerModeCallback> &cb);
    int32_t RemoveRingerModeCallback();
    int32_t RemoveRingerModeCallback(const std::shared_ptr<AudioRingerModeCallback> &cb);
    int32_t AddMicStateChangeCallback(const std::shared_ptr<AudioManagerMicStateChangeCallback> &cb);
    int32_t RemoveMicStateChangeCallback();
    int32_t AddPreferredOutputDeviceChangeCallback(
        const std::shared_ptr<AudioPreferredOutputDeviceChangeCallback> &cb);
    int32_t RemovePreferredOutputDeviceChangeCallback();
    int32_t AddPreferredInputDeviceChangeCallback(
        const std::shared_ptr<AudioPreferredInputDeviceChangeCallback> &cb);
    int32_t RemovePreferredInputDeviceChangeCallback();
    int32_t AddRendererStateChangeCallback(const std::shared_ptr<AudioRendererStateChangeCallback> &cb);
    int32_t RemoveRendererStateChangeCallback();
    int32_t AddCapturerStateChangeCallback(const std::shared_ptr<AudioCapturerStateChangeCallback> &cb);
    int32_t RemoveCapturerStateChangeCallback();
    int32_t AddOutputDeviceChangeWithInfoCallback(
    const uint32_t sessionId, const std::shared_ptr<OutputDeviceChangeWithInfoCallback> &cb);
    int32_t RemoveOutputDeviceChangeWithInfoCallback(const uint32_t sessionId);

    void OnVolumeKeyEvent(VolumeEvent volumeEvent) override;
    void OnAudioFocusInfoChange(const std::list<std::pair<AudioInterrupt, AudioFocuState>> &focusInfoList) override;
    void OnAudioFocusRequested(const AudioInterrupt &requestFocus) override;
    void OnAudioFocusAbandoned(const AudioInterrupt &abandonFocus) override;
    void OnDeviceChange(const DeviceChangeAction &deviceChangeAction) override;
    void OnRingerModeUpdated(const AudioRingerMode &ringerMode) override;
    void OnMicStateUpdated(const MicStateChangeEvent &micStateChangeEvent) override;
    void OnPreferredOutputDeviceUpdated(const std::vector<sptr<AudioDeviceDescriptor>> &desc) override;
    void OnPreferredInputDeviceUpdated(const std::vector<sptr<AudioDeviceDescriptor>> &desc) override;
    void OnRendererStateChange(
        std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos) override;
    void OnCapturerStateChange(
        std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos) override;
    void OnRendererDeviceChange(const uint32_t sessionId,
        const DeviceInfo &deviceInfo, const AudioStreamDeviceChangeReason reason) override;

private:
    std::vector<sptr<AudioDeviceDescriptor>> DeviceFilterByFlag(DeviceFlag flag,
        const std::vector<sptr<AudioDeviceDescriptor>>& desc);

    std::vector<std::weak_ptr<VolumeKeyEventCallback>> volumeKeyEventCallbackList_;
    std::vector<std::shared_ptr<AudioFocusInfoChangeCallback>> focusInfoChangeCallbackList_;
    std::list<std::pair<DeviceFlag, std::shared_ptr<AudioManagerDeviceChangeCallback>>> deviceChangeCallbackList_;
    std::vector<std::shared_ptr<AudioRingerModeCallback>> ringerModeCallbackList_;
    std::vector<std::shared_ptr<AudioManagerMicStateChangeCallback>> micStateChangeCallbackList_;
    std::vector<std::shared_ptr<AudioPreferredOutputDeviceChangeCallback>> preferredOutputDeviceCallbackList_;
    std::vector<std::shared_ptr<AudioPreferredInputDeviceChangeCallback>> preferredInputDeviceCallbackList_;
    std::vector<std::shared_ptr<AudioRendererStateChangeCallback>> rendererStateChangeCallbackList_;
    std::vector<std::weak_ptr<AudioCapturerStateChangeCallback>> capturerStateChangeCallbackList_;

    std::unordered_map<uint32_t,
        std::shared_ptr<OutputDeviceChangeWithInfoCallback>> outputDeviceChangeWithInfoCallbackMap_;

    std::mutex volumeKeyEventMutex_;
    std::mutex focusInfoChangeMutex_;
    std::mutex deviceChangeMutex_;
    std::mutex ringerModeMutex_;
    std::mutex micStateChangeMutex_;
    std::mutex pOutputDeviceChangeMutex_;
    std::mutex pInputDeviceChangeMutex_;
    std::mutex rendererStateChangeMutex_;
    std::mutex capturerStateChangeMutex_;
    std::mutex outputDeviceChangeWithInfoCallbackMutex_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_POLICY_CLIENT_STUB_IMPL_H