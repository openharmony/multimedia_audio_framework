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

#include "audio_interrupt_callback.h"
#include "audio_policy_manager_stub.h"
#include "audio_policy_service.h"
#include "audio_server_death_recipient.h"
#include "audio_session_callback.h"
#include "i_audio_volume_key_event_callback.h"
#include "iremote_stub.h"
#include "system_ability.h"
#include "audio_service_dump.h"
#include "audio_info.h"
#include "accesstoken_kit.h"
#include "perm_state_change_callback_customize.h"
#include "bundle_mgr_interface.h"
#include "system_ability_definition.h"
#include "iservice_registry.h"
#include "ipc_skeleton.h"
#include "bundle_mgr_proxy.h"

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

    explicit AudioPolicyServer(int32_t systemAbilityId, bool runOnCreate = true);

    virtual ~AudioPolicyServer() = default;

    void OnDump() override;
    void OnStart() override;
    void OnStop() override;

    int32_t SetStreamVolume(AudioStreamType streamType, float volume) override;

    float GetStreamVolume(AudioStreamType streamType) override;

    int32_t SetLowPowerVolume(int32_t streamId, float volume) override;

    float GetLowPowerVolume(int32_t streamId) override;

    float GetSingleStreamVolume(int32_t streamId) override;

    int32_t SetStreamMute(AudioStreamType streamType, bool mute) override;

    bool GetStreamMute(AudioStreamType streamType) override;

    bool IsStreamActive(AudioStreamType streamType) override;

    int32_t SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) override;

    std::string GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType) override;

    int32_t SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) override;

    std::vector<sptr<AudioDeviceDescriptor>> GetDevices(DeviceFlag deviceFlag) override;

    int32_t SetDeviceActive(InternalDeviceType deviceType, bool active) override;

    bool IsDeviceActive(InternalDeviceType deviceType) override;

    InternalDeviceType GetActiveOutputDevice() override;

    InternalDeviceType GetActiveInputDevice() override;

    int32_t SetRingerMode(AudioRingerMode ringMode) override;

    std::vector<int32_t> GetSupportedTones() override;

    std::shared_ptr<ToneInfo> GetToneConfig(int32_t ltonetype) override;

    AudioRingerMode GetRingerMode() override;

    int32_t SetAudioScene(AudioScene audioScene) override;

    int32_t SetMicrophoneMuteCommon(bool isMute, API_VERSION api_v);

    int32_t SetMicrophoneMute(bool isMute) override;

    int32_t SetMicrophoneMuteAudioConfig(bool isMute) override;

    bool IsMicrophoneMute(API_VERSION api_v) override;

    AudioScene GetAudioScene() override;

    int32_t SetRingerModeCallback(const int32_t clientId, const sptr<IRemoteObject> &object) override;

    int32_t UnsetRingerModeCallback(const int32_t clientId) override;

    int32_t SetMicStateChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object) override;

    int32_t SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag, const sptr<IRemoteObject> &object)
        override;

    int32_t UnsetDeviceChangeCallback(const int32_t clientId) override;

    int32_t SetAudioInterruptCallback(const uint32_t sessionID, const sptr<IRemoteObject> &object) override;

    int32_t UnsetAudioInterruptCallback(const uint32_t sessionID) override;

    int32_t ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt) override;

    int32_t DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt) override;

    int32_t SetAudioManagerInterruptCallback(const uint32_t clientID, const sptr<IRemoteObject> &object) override;

    int32_t UnsetAudioManagerInterruptCallback(const uint32_t clientID) override;

    int32_t RequestAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt) override;

    int32_t AbandonAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt) override;

    AudioStreamType GetStreamInFocus() override;

    int32_t GetSessionInfoInFocus(AudioInterrupt &audioInterrupt) override;

    int32_t SetVolumeKeyEventCallback(const int32_t clientPid, const sptr<IRemoteObject> &object) override;

    int32_t UnsetVolumeKeyEventCallback(const int32_t clientPid) override;

    void OnSessionRemoved(const uint32_t sessionID) override;

    int32_t Dump(int32_t fd, const std::vector<std::u16string> &args) override;

    bool VerifyClientPermission(const std::string &permission, uint32_t appTokenId = 0, int32_t appUid = INVALID_UID,
        bool privacyFlag = false, AudioPermissionState state = AUDIO_PERMISSION_START) override;

    bool getUsingPemissionFromPrivacy(const std::string &permissionName, uint32_t appTokenId,
        AudioPermissionState state = AUDIO_PERMISSION_START) override;

    int32_t ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType) override;

    int32_t GetAudioLatencyFromXml() override;

    uint32_t GetSinkLatencyFromXml() override;

    int32_t RegisterAudioRendererEventListener(int32_t clientUID, const sptr<IRemoteObject> &object) override;

    int32_t UnregisterAudioRendererEventListener(int32_t clientUID) override;

    int32_t RegisterAudioCapturerEventListener(int32_t clientUID, const sptr<IRemoteObject> &object) override;

    int32_t UnregisterAudioCapturerEventListener(int32_t clientUID) override;

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

    std::vector<sptr<VolumeGroupInfo>> GetVolumeGroupInfos() override;

    std::vector<sptr<AudioDeviceDescriptor>> GetActiveOutputDeviceDescriptors() override;

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
    void PrintOwnersLists();
    int32_t ProcessFocusEntry(const AudioInterrupt &incomingInterrupt);
    bool ProcessCurActiveInterrupt(std::list<AudioInterrupt>::iterator &iterActive, const AudioInterrupt &incoming);
    bool ProcessPendingInterrupt(std::list<AudioInterrupt>::iterator &iterPending, const AudioInterrupt &incoming);
    void AddToCurActiveList(const AudioInterrupt &audioInterrupt);
    void UnduckCurActiveList(const AudioInterrupt &exitingInterrupt);
    void ResumeUnduckPendingList(const AudioInterrupt &exitingInterrupt);
    void NotifyFocusGranted(const uint32_t clientID, const AudioInterrupt &audioInterrupt);
    int32_t NotifyFocusAbandoned(const uint32_t clientID, const AudioInterrupt &audioInterrupt);
    int32_t SetStreamVolume(AudioStreamType streamType, float volume, bool isUpdateUi);
    void GetPolicyData(PolicyData &policyData);
    void GetDeviceInfo(PolicyData &policyData);
    void GetGroupInfo(PolicyData &policyData);
    void SubscribeKeyEvents();
    void InitKVStore();
    void ConnectServiceAdapter();
    void RegisterBluetoothListener();
    void SubscribeAccessibilityConfigObserver();

    static float MapVolumeToHDI(int32_t volume);
    static int32_t ConvertVolumeToInt(float volume);

    AudioPolicyService& mPolicyService;
    std::unordered_map<int32_t, std::shared_ptr<VolumeKeyEventCallback>> volumeChangeCbsMap_;
    std::mutex ringerModeMutex_;
    std::mutex interruptMutex_;
    std::mutex volumeKeyEventMutex_;
    std::mutex micStateChangeMutex_;
    uint32_t clientOnFocus_;
    std::unique_ptr<AudioInterrupt> focussedAudioInterruptInfo_;

    std::unordered_map<uint32_t, std::shared_ptr<AudioInterruptCallback>> policyListenerCbsMap_;
    std::unordered_map<uint32_t, std::shared_ptr<AudioInterruptCallback>> audioManagerListenerCbsMap_;
    std::list<AudioInterrupt> curActiveOwnersList_;
    std::list<AudioInterrupt> pendingOwnersList_;
    std::unordered_map<AudioStreamType, int32_t> interruptPriorityMap_;
    std::unordered_map<int32_t, std::shared_ptr<AudioRingerModeCallback>> ringerModeListenerCbsMap_;
    std::unordered_map<int32_t, std::shared_ptr<AudioManagerMicStateChangeCallback>> micStateChangeListenerCbsMap_;
    std::vector<pid_t> clientDiedListenerState_;
    static constexpr int32_t MAX_VOLUME_LEVEL = 15;
    static constexpr int32_t MIN_VOLUME_LEVEL = 0;
    static constexpr int32_t CONST_FACTOR = 100;
    static constexpr int32_t VOLUME_CHANGE_FACTOR = 1;
    static constexpr int32_t FIRST_PRIORITY = 1;
    static constexpr int32_t SECOND_PRIORITY = 2;
    static constexpr int32_t THIRD_PRIORITY = 3;
    static constexpr int32_t VOLUME_KEY_DURATION = 0;
    static constexpr int32_t MEDIA_SERVICE_UID = 1013;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // ST_AUDIO_POLICY_SERVER_H
