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

#ifndef ST_AUDIO_POLICY_SERVER_H
#define ST_AUDIO_POLICY_SERVER_H

#include <mutex>
#include <pthread.h>

#include "system_ability_definition.h"
#include "ipc_skeleton.h"
#include "system_ability.h"
#include "iservice_registry.h"

#include "accesstoken_kit.h"
#include "perm_state_change_callback_customize.h"

#include "bundle_mgr_interface.h"
#include "bundle_mgr_proxy.h"

#include "audio_info.h"
#include "audio_policy_service.h"
#include "audio_session_callback.h"
#include "audio_interrupt_callback.h"
#include "audio_policy_manager_stub.h"
#include "audio_server_death_recipient.h"
#include "audio_service_dump.h"

namespace OHOS {
namespace AudioStandard {
class AudioPolicyServer : public SystemAbility, public AudioPolicyManagerStub, public AudioSessionCallback {
    DECLARE_SYSTEM_ABILITY(AudioPolicyServer);
public:
    DISALLOW_COPY_AND_MOVE(AudioPolicyServer);

    enum DeathRecipientId {
        TRACKER_CLIENT = 0,
        LISTENER_CLIENT
    };

    const std::vector<AudioStreamType> GET_STREAM_ALL_VOLUME_TYPES {
        STREAM_VOICE_CALL,
        STREAM_RING,
        STREAM_MUSIC,
        STREAM_VOICE_ASSISTANT,
        STREAM_ALARM,
        STREAM_ACCESSIBILITY,
        STREAM_ULTRASONIC
    };

    explicit AudioPolicyServer(int32_t systemAbilityId, bool runOnCreate = true);

    virtual ~AudioPolicyServer() = default;

    void OnDump() override;
    void OnStart() override;
    void OnStop() override;

    int32_t GetMaxVolumeLevel(AudioVolumeType volumeType) override;

    int32_t GetMinVolumeLevel(AudioVolumeType volumeType) override;

    int32_t SetSystemVolumeLevel(AudioStreamType streamType, int32_t volumeLevel, API_VERSION api_v = API_9) override;

    int32_t GetSystemVolumeLevel(AudioStreamType streamType) override;

    int32_t SetLowPowerVolume(int32_t streamId, float volume) override;

    float GetLowPowerVolume(int32_t streamId) override;

    float GetSingleStreamVolume(int32_t streamId) override;

    int32_t SetStreamMute(AudioStreamType streamType, bool mute, API_VERSION api_v = API_9) override;

    bool GetStreamMute(AudioStreamType streamType) override;

    bool IsStreamActive(AudioStreamType streamType) override;

    bool IsVolumeUnadjustable() override;

    int32_t AdjustVolumeByStep(VolumeAdjustType adjustType) override;

    int32_t AdjustSystemVolumeByStep(AudioVolumeType volumeType, VolumeAdjustType adjustType) override;

    float GetSystemVolumeInDb(AudioVolumeType volumeType, int32_t volumeLevel, DeviceType deviceType) override;

    int32_t SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) override;

    std::string GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType) override;

    int32_t SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) override;

    std::vector<sptr<AudioDeviceDescriptor>> GetDevices(DeviceFlag deviceFlag) override;

    int32_t SetWakeUpAudioCapturer(InternalAudioCapturerOptions options) override;

    int32_t CloseWakeUpAudioCapturer() override;

    int32_t SetDeviceActive(InternalDeviceType deviceType, bool active) override;

    bool IsDeviceActive(InternalDeviceType deviceType) override;

    InternalDeviceType GetActiveOutputDevice() override;

    InternalDeviceType GetActiveInputDevice() override;

    int32_t SetRingerMode(AudioRingerMode ringMode, API_VERSION api_v = API_9) override;

#ifdef FEATURE_DTMF_TONE
    std::vector<int32_t> GetSupportedTones() override;

    std::shared_ptr<ToneInfo> GetToneConfig(int32_t ltonetype) override;
#endif

    AudioRingerMode GetRingerMode() override;

    int32_t SetAudioScene(AudioScene audioScene) override;

    int32_t SetMicrophoneMuteCommon(bool isMute, API_VERSION api_v);

    int32_t SetMicrophoneMute(bool isMute) override;

    int32_t SetMicrophoneMuteAudioConfig(bool isMute) override;

    bool IsMicrophoneMute(API_VERSION api_v) override;

    AudioScene GetAudioScene() override;

    int32_t SetRingerModeCallback(const int32_t clientId, const sptr<IRemoteObject> &object,
        API_VERSION api_v = API_9) override;

    int32_t UnsetRingerModeCallback(const int32_t clientId) override;

    int32_t SetMicStateChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object) override;

    int32_t SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag, const sptr<IRemoteObject> &object)
        override;

    int32_t UnsetDeviceChangeCallback(const int32_t clientId, DeviceFlag flag) override;
    
    int32_t SetPreferredOutputDeviceChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object) override;

    int32_t SetPreferredInputDeviceChangeCallback(const sptr<IRemoteObject> &object) override;

    int32_t UnsetPreferredOutputDeviceChangeCallback(const int32_t clientId) override;

    int32_t UnsetPreferredInputDeviceChangeCallback() override;

    int32_t SetAudioInterruptCallback(const uint32_t sessionID, const sptr<IRemoteObject> &object) override;

    int32_t UnsetAudioInterruptCallback(const uint32_t sessionID) override;

    int32_t ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt) override;

    int32_t DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt) override;

    int32_t SetAudioManagerInterruptCallback(const int32_t clientId, const sptr<IRemoteObject> &object) override;

    int32_t UnsetAudioManagerInterruptCallback(const int32_t clientId) override;

    int32_t RequestAudioFocus(const int32_t clientId, const AudioInterrupt &audioInterrupt) override;

    int32_t AbandonAudioFocus(const int32_t clientId, const AudioInterrupt &audioInterrupt) override;

    AudioStreamType GetStreamInFocus() override;

    int32_t GetSessionInfoInFocus(AudioInterrupt &audioInterrupt) override;

    int32_t SetVolumeKeyEventCallback(const int32_t clientId,
        const sptr<IRemoteObject> &object, API_VERSION api_v = API_9) override;

    int32_t UnsetVolumeKeyEventCallback(const int32_t clientId) override;

    void OnSessionRemoved(const uint32_t sessionID) override;

    void OnPlaybackCapturerStop() override;

    void OnWakeupCapturerStop() override;

    int32_t Dump(int32_t fd, const std::vector<std::u16string> &args) override;

    bool CheckRecordingCreate(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid) override;

    bool CheckRecordingStateChange(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid,
        AudioPermissionState state) override;

    int32_t ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType) override;

    int32_t GetAudioLatencyFromXml() override;

    uint32_t GetSinkLatencyFromXml() override;

    int32_t RegisterAudioRendererEventListener(int32_t clientPid, const sptr<IRemoteObject> &object) override;

    int32_t UnregisterAudioRendererEventListener(int32_t clientPid) override;

    int32_t RegisterAudioCapturerEventListener(int32_t clientPid, const sptr<IRemoteObject> &object) override;

    int32_t UnregisterAudioCapturerEventListener(int32_t clientPid) override;

    int32_t RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
        const sptr<IRemoteObject> &object) override;

    int32_t UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo) override;

    int32_t GetCurrentRendererChangeInfos(
        std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos) override;

    int32_t GetCurrentCapturerChangeInfos(
        std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos) override;

    void RegisterClientDeathRecipient(const sptr<IRemoteObject> &object, DeathRecipientId id);

    void RegisteredTrackerClientDied(int pid);

    void RegisteredStreamListenerClientDied(int pid);

    bool IsAudioRendererLowLatencySupported(const AudioStreamInfo &audioStreamInfo) override;

    int32_t UpdateStreamState(const int32_t clientUid, StreamSetState streamSetState,
        AudioStreamType audioStreamType) override;

    int32_t GetVolumeGroupInfos(std::string networkId, std::vector<sptr<VolumeGroupInfo>> &infos) override;

    int32_t GetNetworkIdByGroupId(int32_t groupId, std::string &networkId) override;

    std::vector<sptr<AudioDeviceDescriptor>> GetPreferredOutputDeviceDescriptors(
        AudioRendererInfo &rendererInfo) override;

    std::vector<sptr<AudioDeviceDescriptor>> GetPreferredInputDeviceDescriptors(
        AudioCapturerInfo &captureInfo) override;

    int32_t GetAudioFocusInfoList(std::list<std::pair<AudioInterrupt, AudioFocuState>> &focusInfoList) override;

    int32_t RegisterFocusInfoChangeCallback(const int32_t clientId, const sptr<IRemoteObject>& object) override;

    int32_t UnregisterFocusInfoChangeCallback(const int32_t clientId) override;

    int32_t SetSystemSoundUri(const std::string &key, const std::string &uri) override;

    std::string GetSystemSoundUri(const std::string &key) override;

    float GetMinStreamVolume(void) override;

    float GetMaxStreamVolume(void) override;

    int32_t GetMaxRendererInstances() override;

    void GetStreamVolumeInfoMap(StreamVolumeInfoMap& streamVolumeInfos);

    int32_t QueryEffectSceneMode(SupportedEffectConfig &supportedEffectConfig) override;

    int32_t SetPlaybackCapturerFilterInfos(const AudioPlaybackCaptureConfig &config,
        uint32_t appTokenId) override;

    class RemoteParameterCallback : public AudioParameterCallback {
    public:
        RemoteParameterCallback(sptr<AudioPolicyServer> server);
        // AudioParameterCallback
        void OnAudioParameterChange(const std::string networkId, const AudioParamKey key, const std::string& condition,
            const std::string& value) override;
    private:
        sptr<AudioPolicyServer> server_;
        void VolumeOnChange(const std::string networkId, const std::string& condition);
        void InterruptOnChange(const std::string networkId, const std::string& condition);
        void StateOnChange(const std::string networkId, const std::string& condition, const std::string& value);
    };

    std::shared_ptr<RemoteParameterCallback> remoteParameterCallback_;

    class PerStateChangeCbCustomizeCallback : public Security::AccessToken::PermStateChangeCallbackCustomize {
    public:
        explicit PerStateChangeCbCustomizeCallback(const Security::AccessToken::PermStateChangeScope &scopeInfo,
            sptr<AudioPolicyServer> server) : PermStateChangeCallbackCustomize(scopeInfo),
            ready_(false), server_(server) {}
        ~PerStateChangeCbCustomizeCallback() {}

        void PermStateChangeCallback(Security::AccessToken::PermStateChangeInfo& result);
        int32_t getUidByBundleName(std::string bundle_name, int user_id);

        bool ready_;
    private:
        sptr<AudioPolicyServer> server_;
    };

protected:
    void OnAddSystemAbility(int32_t systemAbilityId, const std::string& deviceId) override;

    void RegisterParamCallback();

    void OnRemoveSystemAbility(int32_t systemAbilityId, const std::string& deviceId) override;

private:
    static constexpr int32_t MAX_VOLUME_LEVEL = 15;
    static constexpr int32_t MIN_VOLUME_LEVEL = 0;
    static constexpr int32_t VOLUME_CHANGE_FACTOR = 1;
    static constexpr int32_t VOLUME_KEY_DURATION = 0;
    static constexpr int32_t VOLUME_MUTE_KEY_DURATION = 1;
    static constexpr int32_t MEDIA_SERVICE_UID = 1013;
    static constexpr int32_t DEFAULT_APP_PID = -1;

    static const std::map<InterruptHint, AudioFocuState> HINTSTATEMAP;
    static const std::list<uid_t> RECORD_ALLOW_BACKGROUND_LIST;
    static const std::list<uid_t> RECORD_PASS_APPINFO_LIST;
    static std::map<InterruptHint, AudioFocuState> CreateStateMap();

    // for audio interrupt
    bool IsSameAppInShareMode(const AudioInterrupt incomingInterrupt, const AudioInterrupt activateInterrupt);
    int32_t ProcessFocusEntry(const AudioInterrupt &incomingInterrupt);
    void ProcessCurrentInterrupt(const AudioInterrupt &incomingInterrupt);
    void ResumeAudioFocusList();
    std::list<std::pair<AudioInterrupt, AudioFocuState>> SimulateFocusEntry();
    void NotifyStateChangedEvent(AudioFocuState oldState, AudioFocuState newState,
        std::list<std::pair<AudioInterrupt, AudioFocuState>>::iterator &iterActive);
    void NotifyFocusGranted(const int32_t clientId, const AudioInterrupt &audioInterrupt);
    int32_t NotifyFocusAbandoned(const int32_t clientId, const AudioInterrupt &audioInterrupt);
    void OnAudioFocusInfoChange();
    void UpdateAudioScene(const AudioInterrupt &audioInterrupt, AudioInterruptChangeType changeType);

    // for audio volume and mute status
    int32_t SetSystemVolumeLevelInternal(AudioStreamType streamType, int32_t volumeLevel, bool isUpdateUi);
    int32_t SetSingleStreamVolume(AudioStreamType streamType, int32_t volumeLevel, bool isUpdateUi);
    int32_t GetSystemVolumeLevelInternal(AudioStreamType streamType, bool isFromVolumeKey);
    float GetSystemVolumeDb(AudioStreamType streamType);
    int32_t SetStreamMuteInternal(AudioStreamType streamType, bool mute, bool isUpdateUi);
    int32_t SetSingleStreamMute(AudioStreamType streamType, bool mute, bool isUpdateUi);
    bool GetStreamMuteInternal(AudioStreamType streamType);
    AudioVolumeType GetVolumeTypeFromStreamType(AudioStreamType streamType);
    bool IsVolumeTypeValid(AudioStreamType streamType);
    bool IsVolumeLevelValid(AudioStreamType streamType, int32_t volumeLevel);

    // Permission and privacy
    bool VerifyPermission(const std::string &permission, uint32_t tokenId = 0, bool isRecording = false);
    bool CheckAppBackgroundPermission(uid_t callingUid, uint64_t targetFullTokenId, uint32_t targetTokenId);
    Security::AccessToken::AccessTokenID GetTargetTokenId(uid_t callingUid, uint32_t callingTokenId,
        uint32_t appTokenId);
    uint64_t GetTargetFullTokenId(uid_t callingUid, uint64_t callingFullTokenId, uint64_t appFullTokenId);
    bool CheckRootCalling(uid_t callingUid, int32_t appUid);
    void NotifyPrivacy(uint32_t targetTokenId, AudioPermissionState state);

    // common
    void GetPolicyData(PolicyData &policyData);
    void GetDeviceInfo(PolicyData &policyData);
    void GetGroupInfo(PolicyData &policyData);

    // externel function call
    bool MaxOrMinVolumeOption(const int32_t &volLevel, const int32_t keyType, const AudioStreamType &streamInFocus);
    void RegisterVolumeKeyEvents(const int32_t keyType);
    void RegisterVolumeKeyMuteEvents();
    void SubscribeVolumeKeyEvents();
    void InitKVStore();
    void ConnectServiceAdapter();
    void LoadEffectLibrary();
    void RegisterBluetoothListener();
    void SubscribeAccessibilityConfigObserver();
    void RegisterDataObserver();

    AudioPolicyService& mPolicyService;
    int32_t clientOnFocus_;
    int32_t volumeStep_;
    std::unique_ptr<AudioInterrupt> focussedAudioInterruptInfo_;
    std::recursive_mutex focussedAudioInterruptInfoMutex_;
    std::list<std::pair<AudioInterrupt, AudioFocuState>> audioFocusInfoList_;
    std::vector<pid_t> clientDiedListenerState_;

    std::unordered_map<int32_t, std::shared_ptr<VolumeKeyEventCallback>> volumeChangeCbsMap_;
    std::unordered_map<uint32_t, std::shared_ptr<AudioInterruptCallback>> interruptCbsMap_;
    std::unordered_map<int32_t, std::shared_ptr<AudioInterruptCallback>> amInterruptCbsMap_;
    std::unordered_map<int32_t, sptr<IStandardAudioPolicyManagerListener>> focusInfoChangeCbsMap_;
    std::unordered_map<int32_t, std::shared_ptr<AudioRingerModeCallback>> ringerModeCbsMap_;
    std::unordered_map<int32_t, std::shared_ptr<AudioManagerMicStateChangeCallback>> micStateChangeCbsMap_;

    std::mutex volumeKeyEventMutex_;
    std::mutex interruptMutex_;
    std::mutex amInterruptMutex_;
    std::mutex focusInfoChangeMutex_;
    std::mutex ringerModeMutex_;
    std::mutex micStateChangeMutex_;
    std::mutex clientDiedListenerStateMutex_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // ST_AUDIO_POLICY_SERVER_H
