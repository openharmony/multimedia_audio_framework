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
#include "audio_info.h"
#include "audio_errors.h"

namespace OHOS {
namespace AudioStandard {
class AudioPolicyProxy : public IRemoteProxy<IAudioPolicy> {
public:
    explicit AudioPolicyProxy(const sptr<IRemoteObject> &impl);
    virtual ~AudioPolicyProxy() = default;

    int32_t SetStreamVolume(AudioStreamType streamType, float volume) override;

    float GetStreamVolume(AudioStreamType streamType) override;

    int32_t SetLowPowerVolume(int32_t streamId, float volume) override;

    float GetLowPowerVolume(int32_t streamId) override;

    float GetSingleStreamVolume(int32_t streamId) override;

    int32_t SetStreamMute(AudioStreamType streamType, bool mute) override;

    bool GetStreamMute(AudioStreamType streamType) override;

    bool IsStreamActive(AudioStreamType streamType) override;

    std::vector<sptr<AudioDeviceDescriptor>> GetDevices(DeviceFlag deviceFlag) override;

    int32_t SetDeviceActive(InternalDeviceType deviceType, bool active) override;

    bool IsDeviceActive(InternalDeviceType deviceType) override;

    DeviceType GetActiveOutputDevice() override;

    DeviceType GetActiveInputDevice() override;

    int32_t SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) override;

    std::string GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType) override;

    int32_t SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) override;

    int32_t SetRingerMode(AudioRingerMode ringMode) override;

    std::vector<int32_t> GetSupportedTones() override;

    std::shared_ptr<ToneInfo> GetToneConfig(int32_t ltonetype) override;

    AudioRingerMode GetRingerMode() override;

    int32_t SetAudioScene(AudioScene scene) override;

    int32_t SetMicrophoneMute(bool isMute) override;

    int32_t SetMicrophoneMuteAudioConfig(bool isMute) override;

    bool IsMicrophoneMute(API_VERSION api_v) override;

    AudioScene GetAudioScene() override;

    int32_t SetRingerModeCallback(const int32_t clientId, const sptr<IRemoteObject> &object) override;

    int32_t UnsetRingerModeCallback(const int32_t clientId) override;

    int32_t SetMicStateChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object) override;

    int32_t SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag,
        const sptr<IRemoteObject>& object) override;

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

    bool VerifyClientPermission(const std::string &permissionName, uint32_t appTokenId, int32_t appUid,
        bool privacyFlag, AudioPermissionState state) override;

    bool getUsingPemissionFromPrivacy(const std::string &permissionName, uint32_t appTokenId,
        AudioPermissionState state) override;

    int32_t ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType) override;

    int32_t GetAudioLatencyFromXml() override;

    uint32_t GetSinkLatencyFromXml() override;

    int32_t RegisterAudioRendererEventListener(int32_t clientUID, const sptr<IRemoteObject> &object) override;

    int32_t UnregisterAudioRendererEventListener(int32_t clientUID) override;

    int32_t RegisterAudioCapturerEventListener(int32_t clientUID, const sptr<IRemoteObject> &object) override;

    int32_t UnregisterAudioCapturerEventListener(int32_t clientUID) override;

    int32_t RegisterTracker(AudioMode &mode,
        AudioStreamChangeInfo &streamChangeInfo, const sptr<IRemoteObject> &object) override;

    int32_t UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo) override;

    int32_t GetCurrentRendererChangeInfos(
        std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos) override;

    int32_t GetCurrentCapturerChangeInfos(
        std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos) override;

    int32_t UpdateStreamState(const int32_t clientUid, StreamSetState streamSetState,
        AudioStreamType audioStreamType) override;

    std::vector<sptr<VolumeGroupInfo>> GetVolumeGroupInfos() override;

    bool IsAudioRendererLowLatencySupported(const AudioStreamInfo &audioStreamInfo) override;

    std::vector<sptr<AudioDeviceDescriptor>> GetActiveOutputDeviceDescriptors() override;

private:
    static inline BrokerDelegator<AudioPolicyProxy> mDdelegator;
    void WriteAudioInteruptParams(MessageParcel &parcel, const AudioInterrupt &audioInterrupt);
    void WriteAudioManagerInteruptParams(MessageParcel &parcel, const AudioInterrupt &audioInterrupt);
    void ReadAudioInterruptParams(MessageParcel &reply, AudioInterrupt &audioInterrupt);
    void WriteStreamChangeInfo(MessageParcel &data, const AudioMode &mode,
        const AudioStreamChangeInfo &streamChangeInfo);
    void ReadAudioRendererChangeInfo(MessageParcel &reply,
        std::unique_ptr<AudioRendererChangeInfo> &rendererChangeInfo);
    void ReadAudioCapturerChangeInfo(MessageParcel &reply,
        std::unique_ptr<AudioCapturerChangeInfo> &capturerChangeInfo);
    void WriteAudioStreamInfoParams(MessageParcel &parcel, const AudioStreamInfo &audioStreamInfo);
};
} // namespace AudioStandard
} // namespace OHOS
#endif // ST_AUDIO_POLICY_PROXY_H
