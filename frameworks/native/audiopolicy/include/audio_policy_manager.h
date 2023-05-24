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

#ifndef ST_AUDIO_POLICY_MANAGER_H
#define ST_AUDIO_POLICY_MANAGER_H

#include <cstdint>
#include "audio_policy_base.h"
#include "audio_capturer_state_change_listener_stub.h"
#include "audio_client_tracker_callback_stub.h"
#include "audio_info.h"
#include "audio_interrupt_callback.h"
#include "audio_policy_manager_listener_stub.h"
#include "audio_renderer_state_change_listener_stub.h"
#include "audio_ringermode_update_listener_stub.h"
#include "audio_routing_manager.h"
#include "audio_routing_manager_listener_stub.h"
#include "audio_system_manager.h"
#include "audio_volume_key_event_callback_stub.h"
#include "audio_system_manager.h"
#include "i_audio_volume_key_event_callback.h"
#include "i_standard_renderer_state_change_listener.h"
#include "i_standard_capturer_state_change_listener.h"
#include "i_standard_client_tracker.h"

namespace OHOS {
namespace AudioStandard {
using InternalDeviceType = DeviceType;

class AudioPolicyManager {
public:
    static AudioPolicyManager& GetInstance()
    {
        static AudioPolicyManager policyManager;
        return policyManager;
    }

    const sptr<IAudioPolicy> GetAudioPolicyManagerProxy();

    int32_t SetStreamVolume(AudioStreamType streamType, float volume);

    float GetStreamVolume(AudioStreamType streamType);

    int32_t SetLowPowerVolume(int32_t streamId, float volume);

    float GetLowPowerVolume(int32_t streamId);

    float GetSingleStreamVolume(int32_t streamId);

    int32_t SetStreamMute(AudioStreamType streamType, bool mute);

    bool GetStreamMute(AudioStreamType streamType);

    bool IsStreamActive(AudioStreamType streamType);

    int32_t SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors);

    std::string GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType);

    int32_t SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors);

    std::vector<sptr<AudioDeviceDescriptor>> GetDevices(DeviceFlag deviceFlag);

    int32_t SetDeviceActive(InternalDeviceType deviceType, bool active);

    bool IsDeviceActive(InternalDeviceType deviceType);

    DeviceType GetActiveOutputDevice();

    DeviceType GetActiveInputDevice();

    int32_t SetRingerMode(AudioRingerMode ringMode);

    std::vector<int32_t> GetSupportedTones();

    std::shared_ptr<ToneInfo> GetToneConfig(int32_t ltonetype);

    AudioRingerMode GetRingerMode();

    int32_t SetAudioScene(AudioScene scene);

    int32_t SetMicrophoneMute(bool isMute);

    int32_t SetMicrophoneMuteAudioConfig(bool isMute);

    bool IsMicrophoneMute(API_VERSION api_v = API_9);

    AudioScene GetAudioScene();

    int32_t SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag,
        const std::shared_ptr<AudioManagerDeviceChangeCallback> &callback);

    int32_t UnsetDeviceChangeCallback(const int32_t clientId);

    int32_t SetRingerModeCallback(const int32_t clientId,
                                  const std::shared_ptr<AudioRingerModeCallback> &callback);

    int32_t UnsetRingerModeCallback(const int32_t clientId);

    int32_t SetMicStateChangeCallback(const int32_t clientId,
                                  const std::shared_ptr<AudioManagerMicStateChangeCallback> &callback);

    int32_t SetAudioInterruptCallback(const uint32_t sessionID,
                                    const std::shared_ptr<AudioInterruptCallback> &callback);

    int32_t UnsetAudioInterruptCallback(const uint32_t sessionID);

    int32_t ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt);

    int32_t DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt);

    int32_t SetAudioManagerInterruptCallback(const uint32_t clientID,
        const std::shared_ptr<AudioInterruptCallback> &callback);

    int32_t UnsetAudioManagerInterruptCallback(const uint32_t clientID);

    int32_t RequestAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt);

    int32_t AbandonAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt);

    AudioStreamType GetStreamInFocus();

    int32_t GetSessionInfoInFocus(AudioInterrupt &audioInterrupt);

    int32_t SetVolumeKeyEventCallback(const int32_t clientPid, const std::shared_ptr<VolumeKeyEventCallback> &callback);

    int32_t UnsetVolumeKeyEventCallback(const int32_t clientPid);

    bool VerifyClientPermission(const std::string &permissionName, uint32_t appTokenId, int32_t appUid,
        bool privacyFlag, AudioPermissionState state);

    bool getUsingPemissionFromPrivacy(const std::string &permissionName, uint32_t appTokenId,
        AudioPermissionState state);
    
    int32_t ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType);

    int32_t GetAudioLatencyFromXml();

    uint32_t GetSinkLatencyFromXml();

    int32_t RegisterAudioRendererEventListener(const int32_t clientUID,
        const std::shared_ptr<AudioRendererStateChangeCallback> &callback);

    int32_t UnregisterAudioRendererEventListener(const int32_t clientUID);

    int32_t RegisterAudioCapturerEventListener(const int32_t clientUID,
        const std::shared_ptr<AudioCapturerStateChangeCallback> &callback);

    int32_t UnregisterAudioCapturerEventListener(const int32_t clientUID);

    int32_t RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
        const std::shared_ptr<AudioClientTracker> &clientTrackerObj);

    int32_t UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo);

    int32_t GetCurrentRendererChangeInfos(
        std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos);

    int32_t GetCurrentCapturerChangeInfos(
        std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos);

    int32_t UpdateStreamState(const int32_t clientUid, StreamSetState streamSetState,
                                    AudioStreamType audioStreamType);

    std::vector<sptr<VolumeGroupInfo>> GetVolumeGroupInfos();

    bool IsAudioRendererLowLatencySupported(const AudioStreamInfo &audioStreamInfo);

    std::vector<sptr<AudioDeviceDescriptor>> GetActiveOutputDeviceDescriptors();
private:
    AudioPolicyManager() {}
    ~AudioPolicyManager() {}

    void Init();
    sptr<AudioPolicyManagerListenerStub> listenerStub_ = nullptr;
    std::mutex listenerStubMutex_;
    std::mutex volumeCallbackMutex_;
    std::mutex stateChangelistenerStubMutex_;
    std::mutex clientTrackerStubMutex_;
    std::mutex ringerModelistenerStubMutex_;
    sptr<AudioVolumeKeyEventCallbackStub> volumeKeyEventListenerStub_ = nullptr;
    sptr<AudioRingerModeUpdateListenerStub> ringerModelistenerStub_ = nullptr;
    sptr<AudioRendererStateChangeListenerStub> rendererStateChangelistenerStub_ = nullptr;
    sptr<AudioCapturerStateChangeListenerStub> capturerStateChangelistenerStub_ = nullptr;
    sptr<AudioClientTrackerCallbackStub> clientTrackerCbStub_ = nullptr;
    void RegisterAudioPolicyServerDeathRecipient();
    void AudioPolicyServerDied(pid_t pid);
};
} // namespce AudioStandard
} // namespace OHOS

#endif // ST_AUDIO_POLICY_MANAGER_H
