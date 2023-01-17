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

#ifndef I_AUDIO_POLICY_BASE_H
#define I_AUDIO_POLICY_BASE_H

#include "audio_interrupt_callback.h"
#include "audio_policy_types.h"
#include "i_audio_volume_key_event_callback.h"
#include "ipc_types.h"
#include "iremote_broker.h"
#include "iremote_proxy.h"
#include "iremote_stub.h"
#include "audio_system_manager.h"

namespace OHOS {
namespace AudioStandard {
using InternalDeviceType = DeviceType;

class IAudioPolicy : public IRemoteBroker {
public:

    virtual int32_t SetStreamVolume(AudioStreamType streamType, float volume) = 0;

    virtual float GetStreamVolume(AudioStreamType streamType) = 0;

    virtual int32_t SetLowPowerVolume(int32_t streamId, float volume) = 0;

    virtual float GetLowPowerVolume(int32_t streamId) = 0;

    virtual float GetSingleStreamVolume(int32_t streamId) = 0;

    virtual int32_t SetStreamMute(AudioStreamType streamType, bool mute) = 0;

    virtual bool GetStreamMute(AudioStreamType streamType) = 0;

    virtual bool IsStreamActive(AudioStreamType streamType) = 0;

    virtual std::vector<sptr<AudioDeviceDescriptor>> GetDevices(DeviceFlag deviceFlag) = 0;

    virtual int32_t SetDeviceActive(InternalDeviceType deviceType, bool active) = 0;

    virtual bool IsDeviceActive(InternalDeviceType deviceType) = 0;

    virtual DeviceType GetActiveOutputDevice() = 0;

    virtual DeviceType GetActiveInputDevice() = 0;

    virtual std::shared_ptr<ToneInfo> GetToneConfig(int32_t ltonetype) = 0;

    virtual std::vector<int32_t> GetSupportedTones() = 0;

    virtual int32_t SetRingerMode(AudioRingerMode ringMode) = 0;

    virtual AudioRingerMode GetRingerMode() = 0;

    virtual int32_t SetAudioScene(AudioScene scene) = 0;

    virtual int32_t SetMicrophoneMute(bool isMute) = 0;

    virtual int32_t SetMicrophoneMuteAudioConfig(bool isMute) = 0;

    virtual bool IsMicrophoneMute(API_VERSION api_v = API_9) = 0;

    virtual AudioScene GetAudioScene() = 0;

    virtual int32_t SetRingerModeCallback(const int32_t clientId, const sptr<IRemoteObject> &object) = 0;

    virtual int32_t UnsetRingerModeCallback(const int32_t clientId) = 0;

    virtual int32_t SetMicStateChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object) = 0;

    virtual int32_t SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag,
        const sptr<IRemoteObject> &object) = 0;

    virtual int32_t UnsetDeviceChangeCallback(const int32_t clientId) = 0;

    virtual int32_t SetAudioInterruptCallback(const uint32_t sessionID, const sptr<IRemoteObject> &object) = 0;

    virtual int32_t UnsetAudioInterruptCallback(const uint32_t sessionID) = 0;

    virtual int32_t ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt) = 0;

    virtual int32_t DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt) = 0;

    virtual int32_t SetAudioManagerInterruptCallback(const uint32_t clientID, const sptr<IRemoteObject> &object) = 0;

    virtual int32_t UnsetAudioManagerInterruptCallback(const uint32_t clientID) = 0;

    virtual int32_t RequestAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt) = 0;

    virtual int32_t AbandonAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt) = 0;

    virtual AudioStreamType GetStreamInFocus() = 0;

    virtual int32_t GetSessionInfoInFocus(AudioInterrupt &audioInterrupt) = 0;

    virtual int32_t SetVolumeKeyEventCallback(const int32_t clientPid, const sptr<IRemoteObject> &object) = 0;

    virtual int32_t UnsetVolumeKeyEventCallback(const int32_t clientPid) = 0;

    virtual bool VerifyClientPermission(const std::string &permission, uint32_t appTokenId, int32_t appUid,
        bool privacyFlag, AudioPermissionState state) = 0;

    virtual bool getUsingPemissionFromPrivacy(const std::string &permission, uint32_t appTokenId,
        AudioPermissionState state) = 0;

    virtual int32_t ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType) = 0;

    virtual int32_t GetAudioLatencyFromXml() = 0;

    virtual uint32_t GetSinkLatencyFromXml() = 0;

    virtual int32_t RegisterAudioRendererEventListener(int32_t clientUID, const sptr<IRemoteObject> &object) = 0;

    virtual int32_t UnregisterAudioRendererEventListener(int32_t clientUID) = 0;

    virtual int32_t RegisterAudioCapturerEventListener(int32_t clientUID, const sptr<IRemoteObject> &object) = 0;

    virtual int32_t UnregisterAudioCapturerEventListener(int32_t clientUID) = 0;

    virtual int32_t RegisterTracker(AudioMode &mode,
        AudioStreamChangeInfo &streamChangeInfo, const sptr<IRemoteObject> &object) = 0;

    virtual int32_t UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo) = 0;

    virtual int32_t GetCurrentRendererChangeInfos(
        std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos) = 0;

    virtual int32_t GetCurrentCapturerChangeInfos(
        std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos) = 0;

    virtual int32_t UpdateStreamState(const int32_t clientUid, StreamSetState streamSetState,
                                            AudioStreamType audioStreamType) = 0;

    virtual int32_t SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) = 0;

    virtual std::string GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType) = 0;

    virtual int32_t SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) = 0;

    virtual std::vector<sptr<VolumeGroupInfo>> GetVolumeGroupInfos() = 0;

    virtual bool IsAudioRendererLowLatencySupported(const AudioStreamInfo &audioStreamInfo) = 0;

    virtual std::vector<sptr<AudioDeviceDescriptor>> GetActiveOutputDeviceDescriptors() = 0;
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"IAudioPolicy");
};
} // namespace AudioStandard
} // namespace OHOS
#endif // I_AUDIO_POLICY_BASE_H
