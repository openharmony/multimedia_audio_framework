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

#ifndef AUDIO_POLICY_MANAGER_STUB_H
#define AUDIO_POLICY_MANAGER_STUB_H

#include "audio_policy_base.h"

namespace OHOS {
namespace AudioStandard {
class AudioPolicyManagerStub : public IRemoteStub<IAudioPolicy> {
public:
    virtual int32_t OnRemoteRequest(uint32_t code, MessageParcel &data,
                                MessageParcel &reply, MessageOption &option) override;

private:
    void SetStreamVolumeInternal(MessageParcel &data, MessageParcel &reply);
    void SetRingerModeInternal(MessageParcel &data, MessageParcel &reply);
    void GetToneInfoInternal(MessageParcel &data, MessageParcel &reply);
    void GetSupportedTonesInternal(MessageParcel &data, MessageParcel &reply);
    void GetRingerModeInternal(MessageParcel &data);
    void SetAudioSceneInternal(MessageParcel &data, MessageParcel &reply);
    void GetAudioSceneInternal(MessageParcel &data);
    void SetMicrophoneMuteInternal(MessageParcel &data, MessageParcel &reply);
    void SetMicrophoneMuteAudioConfigInternal(MessageParcel &data, MessageParcel &reply);
    void IsMicrophoneMuteInternal(MessageParcel &data, MessageParcel &reply);
    void GetStreamVolumeInternal(MessageParcel &data, MessageParcel &reply);
    void SetLowPowerVolumeInternal(MessageParcel &data, MessageParcel &reply);
    void GetLowPowerVolumeInternal(MessageParcel &data, MessageParcel &reply);
    void GetSingleStreamVolumeInternal(MessageParcel &data, MessageParcel &reply);
    void SetStreamMuteInternal(MessageParcel &data, MessageParcel &reply);
    void GetStreamMuteInternal(MessageParcel &data, MessageParcel &reply);
    void IsStreamActiveInternal(MessageParcel &data, MessageParcel &reply);
    void SetDeviceActiveInternal(MessageParcel &data, MessageParcel &reply);
    void GetActiveOutputDeviceInternal(MessageParcel &data, MessageParcel &reply);
    void GetActiveInputDeviceInternal(MessageParcel &data, MessageParcel &reply);
    void IsDeviceActiveInternal(MessageParcel &data, MessageParcel &reply);
    void SetRingerModeCallbackInternal(MessageParcel &data, MessageParcel &reply);
    void UnsetRingerModeCallbackInternal(MessageParcel &data, MessageParcel &reply);
    void SetMicStateChangeCallbackInternal(MessageParcel &data, MessageParcel &reply);
    void SetInterruptCallbackInternal(MessageParcel &data, MessageParcel &reply);
    void UnsetInterruptCallbackInternal(MessageParcel &data, MessageParcel &reply);
    void ActivateInterruptInternal(MessageParcel &data, MessageParcel &reply);
    void DeactivateInterruptInternal(MessageParcel &data, MessageParcel &reply);
    void SetAudioManagerInterruptCbInternal(MessageParcel &data, MessageParcel &reply);
    void UnsetAudioManagerInterruptCbInternal(MessageParcel &data, MessageParcel &reply);
    void RequestAudioFocusInternal(MessageParcel &data, MessageParcel &reply);
    void AbandonAudioFocusInternal(MessageParcel &data, MessageParcel &reply);
    void GetStreamInFocusInternal(MessageParcel &reply);
    void GetSessionInfoInFocusInternal(MessageParcel &reply);
    void ReadAudioInterruptParams(MessageParcel &data, AudioInterrupt &audioInterrupt);
    void ReadAudioManagerInterruptParams(MessageParcel &data, AudioInterrupt &audioInterrupt);
    void WriteAudioInteruptParams(MessageParcel &reply, const AudioInterrupt &audioInterrupt);
    void SetVolumeKeyEventCallbackInternal(MessageParcel &data, MessageParcel &reply);
    void UnsetVolumeKeyEventCallbackInternal(MessageParcel &data, MessageParcel &reply);
    void GetDevicesInternal(MessageParcel &data, MessageParcel &reply);
    void SelectOutputDeviceInternal(MessageParcel &data, MessageParcel &reply);
    void GetSelectedDeviceInfoInternal(MessageParcel &data, MessageParcel &reply);
    void SelectInputDeviceInternal(MessageParcel &data, MessageParcel &reply);
    void SetDeviceChangeCallbackInternal(MessageParcel &data, MessageParcel &reply);
    void UnsetDeviceChangeCallbackInternal(MessageParcel &data, MessageParcel &reply);
    void VerifyClientPermissionInternal(MessageParcel &data, MessageParcel &reply);
    void ReconfigureAudioChannelInternal(MessageParcel &data, MessageParcel &reply);
    void GetAudioLatencyFromXmlInternal(MessageParcel &data, MessageParcel &reply);
    void GetSinkLatencyFromXmlInternal(MessageParcel &data, MessageParcel &reply);
    void ReadStreamChangeInfo(MessageParcel &data, const AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo);
    void RegisterAudioRendererEventListenerInternal(MessageParcel &data, MessageParcel &reply);
    void UnregisterAudioRendererEventListenerInternal(MessageParcel &data, MessageParcel &reply);
    void RegisterAudioCapturerEventListenerInternal(MessageParcel &data, MessageParcel &reply);
    void UnregisterAudioCapturerEventListenerInternal(MessageParcel &data, MessageParcel &reply);
    void RegisterTrackerInternal(MessageParcel &data, MessageParcel &reply);
    void UpdateTrackerInternal(MessageParcel &data, MessageParcel &reply);
    void GetRendererChangeInfosInternal(MessageParcel &data, MessageParcel &reply);
    void GetCapturerChangeInfosInternal(MessageParcel &data, MessageParcel &reply);
    void UpdateStreamStateInternal(MessageParcel& data, MessageParcel& reply);
    void GetVolumeGroupInfoInternal(MessageParcel& data, MessageParcel& reply);
    void IsAudioRendererLowLatencySupportedInternal(MessageParcel &data, MessageParcel &reply);
    void getUsingPemissionFromPrivacyInternal(MessageParcel &data, MessageParcel &reply);
    void GetActiveOutputDeviceDescriptorsInternal(MessageParcel &data, MessageParcel &reply);
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_POLICY_MANAGER_STUB_H
