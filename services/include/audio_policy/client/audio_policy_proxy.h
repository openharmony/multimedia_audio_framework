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

#ifndef ST_AUDIO_POLICY_PROXY_H
#define ST_AUDIO_POLICY_PROXY_H

#include "iremote_proxy.h"
#include "audio_policy_base.h"

namespace OHOS {
namespace AudioStandard {
class AudioPolicyProxy : public IRemoteProxy<IAudioPolicy> {
public:
    explicit AudioPolicyProxy(const sptr<IRemoteObject> &impl);
    virtual ~AudioPolicyProxy() = default;

    int32_t SetStreamVolume(AudioStreamType streamType, float volume) override;

    float GetStreamVolume(AudioStreamType streamType) override;

    int32_t SetStreamMute(AudioStreamType streamType, bool mute) override;

    bool GetStreamMute(AudioStreamType streamType) override;

    bool IsStreamActive(AudioStreamType streamType) override;

    std::vector<sptr<AudioDeviceDescriptor>> GetDevices(DeviceFlag deviceFlag) override;

    int32_t SetDeviceActive(InternalDeviceType deviceType, bool active) override;

    bool IsDeviceActive(InternalDeviceType deviceType) override;

    int32_t SetRingerMode(AudioRingerMode ringMode) override;

    AudioRingerMode GetRingerMode() override;

    int32_t SetAudioScene(AudioScene scene) override;

    AudioScene GetAudioScene() override;

    int32_t SetRingerModeCallback(const int32_t clientId, const sptr<IRemoteObject> &object) override;

    int32_t UnsetRingerModeCallback(const int32_t clientId) override;

    int32_t SetDeviceChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object) override;

    int32_t UnsetDeviceChangeCallback(const int32_t clientId) override;

    int32_t SetAudioInterruptCallback(const uint32_t sessionID, const sptr<IRemoteObject> &object) override;

    int32_t UnsetAudioInterruptCallback(const uint32_t sessionID) override;

    int32_t ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt) override;

    int32_t DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt) override;

    int32_t SetAudioManagerInterruptCallback(const uint32_t clientID, const sptr<IRemoteObject> &object) override;

    int32_t UnsetAudioManagerInterruptCallback(const uint32_t clientID) override;

    int32_t RequestAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt) override;

    int32_t AbandonAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt) override;

    int32_t SetVolumeKeyEventCallback(const int32_t clientPid, const sptr<IRemoteObject> &object) override;

    int32_t UnsetVolumeKeyEventCallback(const int32_t clientPid) override;

    AudioStreamType GetStreamInFocus() override;

    int32_t GetSessionInfoInFocus(AudioInterrupt &audioInterrupt) override;
private:
    static inline BrokerDelegator<AudioPolicyProxy> mDdelegator;
    void WriteAudioInteruptParams(MessageParcel &parcel, const AudioInterrupt &audioInterrupt);
    void WriteAudioManagerInteruptParams(MessageParcel &parcel, const AudioInterrupt &audioInterrupt);
    void ReadAudioInterruptParams(MessageParcel &reply, AudioInterrupt &audioInterrupt);
};
} // namespace AudioStandard
} // namespace OHOS
#endif // ST_AUDIO_POLICY_PROXY_H
