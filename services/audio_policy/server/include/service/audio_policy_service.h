/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef ST_AUDIO_POLICY_SERVICE_H
#define ST_AUDIO_POLICY_SERVICE_H

#include <bitset>
#include <list>
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include "audio_group_handle.h"
#include "audio_info.h"
#include "audio_manager_base.h"
#include "audio_policy_manager_factory.h"
#include "audio_stream_collector.h"
#include "ipc_skeleton.h"
#ifdef FEATURE_DTMF_TONE
#include "audio_tone_parser.h"
#endif

#ifdef ACCESSIBILITY_ENABLE
#include "accessibility_config_listener.h"
#else
#include "iaudio_accessibility_config_observer.h"
#endif
#include "device_status_listener.h"
#include "iaudio_policy_interface.h"
#include "iport_observer.h"
#include "parser_factory.h"
#include "audio_effect_manager.h"
#include "audio_volume_config.h"
#include "policy_provider_stub.h"

namespace OHOS {
namespace AudioStandard {
class AudioPolicyService : public IPortObserver, public IDeviceStatusObserver,
    public IAudioAccessibilityConfigObserver, public PolicyProviderStub {
public:
    static AudioPolicyService& GetAudioPolicyService()
    {
        static AudioPolicyService audioPolicyService;
        return audioPolicyService;
    }

    bool Init(void);
    void Deinit(void);
    void InitKVStore();
    bool ConnectServiceAdapter();

    const sptr<IStandardAudioService> GetAudioServerProxy();

    int32_t GetMaxVolumeLevel(AudioVolumeType volumeType) const;

    int32_t GetMinVolumeLevel(AudioVolumeType volumeType) const;

    int32_t SetSystemVolumeLevel(AudioStreamType streamType, int32_t volumeLevel, bool isFromVolumeKey = false);

    int32_t GetSystemVolumeLevel(AudioStreamType streamType, bool isFromVolumeKey = false) const;

    float GetSystemVolumeDb(AudioStreamType streamType) const;

    int32_t SetLowPowerVolume(int32_t streamId, float volume) const;

    float GetLowPowerVolume(int32_t streamId) const;

    float GetSingleStreamVolume(int32_t streamId) const;

    int32_t SetStreamMute(AudioStreamType streamType, bool mute);

    int32_t SetSourceOutputStreamMute(int32_t uid, bool setMute) const;

    bool GetStreamMute(AudioStreamType streamType) const;

    bool IsStreamActive(AudioStreamType streamType) const;

    void NotifyRemoteRenderState(std::string networkId, std::string condition, std::string value);

    int32_t SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors);

    std::string GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType);

    int32_t SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
        std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors);

    std::vector<sptr<AudioDeviceDescriptor>> GetDevices(DeviceFlag deviceFlag);

    int32_t SetWakeUpAudioCapturer(InternalAudioCapturerOptions options);

    int32_t CloseWakeUpAudioCapturer();

    int32_t SetDeviceActive(InternalDeviceType deviceType, bool active);

    bool IsDeviceActive(InternalDeviceType deviceType) const;

    DeviceType GetActiveOutputDevice() const;

    DeviceType GetActiveInputDevice() const;

    int32_t SetRingerMode(AudioRingerMode ringMode);

    bool IsAudioInterruptEnabled() const;

    auto& GetAudioFocusMap() const
    {
        return focusMap_;
    }

    AudioRingerMode GetRingerMode() const;

    int32_t SetMicrophoneMute(bool isMute);

    bool IsMicrophoneMute();

    int32_t SetAudioScene(AudioScene audioScene);

    AudioScene GetAudioScene(bool hasSystemPermission = true) const;

    int32_t GetAudioLatencyFromXml() const;

    uint32_t GetSinkLatencyFromXml() const;

    int32_t SetSystemSoundUri(const std::string &key, const std::string &uri);

    std::string GetSystemSoundUri(const std::string &key);

    bool IsSessionIdValid(int32_t callerUid, int32_t sessionId);

    // Parser callbacks
    void OnXmlParsingCompleted(const std::unordered_map<ClassType, std::list<AudioModuleInfo>> &xmldata);

    void OnVolumeGroupParsed(std::unordered_map<std::string, std::string>& volumeGroupData);

    void OnInterruptGroupParsed(std::unordered_map<std::string, std::string>& interruptGroupData);

    void OnAudioInterruptEnable(bool enable);

    void OnUpdateRouteSupport(bool isSupported);

    int32_t GetDeviceNameFromDataShareHelper(std::string &deviceName);

    void SetDisplayName(const std::string &deviceName, bool isLocalDevice);

#ifdef FEATURE_DTMF_TONE
    std::vector<int32_t> GetSupportedTones();

    std::shared_ptr<ToneInfo> GetToneConfig(int32_t ltonetype);
#endif

    void OnDeviceStatusUpdated(DeviceType devType, bool isConnected,
        const std::string &macAddress, const std::string &deviceName,
        const AudioStreamInfo &streamInfo);

    void OnPnpDeviceStatusUpdated(DeviceType devType, bool isConnected);

    void OnDeviceConfigurationChanged(DeviceType deviceType,
        const std::string &macAddress, const std::string &deviceName,
        const AudioStreamInfo &streamInfo);

    void OnDeviceStatusUpdated(DStatusInfo statusInfo);

    void OnServiceConnected(AudioServiceIndex serviceIndex);

    void OnServiceDisconnected(AudioServiceIndex serviceIndex);

    void OnMonoAudioConfigChanged(bool audioMono);

    void OnAudioBalanceChanged(float audioBalance);

    void LoadEffectLibrary();

    int32_t SetAudioSessionCallback(AudioSessionCallback *callback);

    int32_t SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag, const sptr<IRemoteObject> &object,
        bool hasBTPermission);

    int32_t UnsetDeviceChangeCallback(const int32_t clientId, DeviceFlag flag);

    int32_t SetPreferredOutputDeviceChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object,
        bool hasBTPermission);

    int32_t SetPreferredInputDeviceChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object,
        bool hasBTPermission);

    int32_t UnsetPreferredOutputDeviceChangeCallback(const int32_t clientId);

    int32_t UnsetPreferredInputDeviceChangeCallback(const int32_t clientId);

    int32_t RegisterAudioRendererEventListener(int32_t clientPid, const sptr<IRemoteObject> &object,
        bool hasBTPermission, bool hasSysPermission);

    int32_t UnregisterAudioRendererEventListener(int32_t clientPid);

    int32_t RegisterAudioCapturerEventListener(int32_t clientPid, const sptr<IRemoteObject> &object,
        bool hasBTPermission, bool hasSysPermission);

    int32_t UnregisterAudioCapturerEventListener(int32_t clientPid);

    int32_t RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
        const sptr<IRemoteObject> &object);

    int32_t UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo);

    int32_t GetCurrentRendererChangeInfos(vector<unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos,
        bool hasBTPermission, bool hasSystemPermission);

    int32_t GetCurrentCapturerChangeInfos(vector<unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos,
        bool hasBTPermission, bool hasSystemPermission);

    void RegisteredTrackerClientDied(pid_t pid);

    void RegisteredStreamListenerClientDied(pid_t pid);

    int32_t ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType);

    void OnAudioLatencyParsed(uint64_t latency);

    void OnSinkLatencyParsed(uint32_t latency);

    int32_t UpdateStreamState(int32_t clientUid, StreamSetStateEventInternal &streamSetStateEventInternal);

    DeviceType GetDeviceTypeFromPin(AudioPin pin);

    std::vector<sptr<VolumeGroupInfo>> GetVolumeGroupInfos();

    void SetParameterCallback(const std::shared_ptr<AudioParameterCallback>& callback);

    void RegiestPolicy();

    // override for IPolicyProvider
    int32_t GetProcessDeviceInfo(const AudioProcessConfig &config, DeviceInfo &deviceInfo);

    int32_t InitSharedVolume(std::shared_ptr<AudioSharedMemory> &buffer);

    bool GetSharedVolume(AudioStreamType streamType, DeviceType deviceType, Volume &vol);

    bool SetSharedVolume(AudioStreamType streamType, DeviceType deviceType, Volume vol);

#ifdef BLUETOOTH_ENABLE
    static void BluetoothServiceCrashedCallback(pid_t pid);
#endif

    void RegisterBluetoothListener();

    void UnregisterBluetoothListener();

    void SubscribeAccessibilityConfigObserver();

    std::vector<sptr<AudioDeviceDescriptor>> GetPreferredOutputDeviceDescriptors(AudioRendererInfo &rendererInfo,
        std::string networkId = LOCAL_NETWORK_ID);

    std::vector<sptr<AudioDeviceDescriptor>> GetPreferredInputDeviceDescriptors(AudioCapturerInfo &captureInfo,
        std::string networkId = LOCAL_NETWORK_ID);

    void GetEffectManagerInfo(OriginalEffectConfig& oriEffectConfig, std::vector<Effect>& availableEffects);

    float GetMinStreamVolume(void);

    float GetMaxStreamVolume(void);

    int32_t GetMaxRendererInstances();

    void RegisterDataObserver();

    bool IsVolumeUnadjustable();

    void GetStreamVolumeInfoMap(StreamVolumeInfoMap &streamVolumeInfos);

    float GetSystemVolumeInDb(AudioVolumeType volumeType, int32_t volumeLevel, DeviceType deviceType) const;

    std::string GetLocalDevicesType();

    int32_t QueryEffectManagerSceneMode(SupportedEffectConfig &supportedEffectConfig);

    void UpdateDescWhenNoBTPermission(vector<sptr<AudioDeviceDescriptor>> &desc);

    int32_t SetPlaybackCapturerFilterInfos(const AudioPlaybackCaptureConfig &config);

    void UnloadLoopback();

    void UpdateOutputDeviceSelectedByCalling(DeviceType deviceType);

private:
    AudioPolicyService()
        :audioPolicyManager_(AudioPolicyManagerFactory::GetAudioPolicyManager()),
        configParser_(ParserFactory::GetInstance().CreateParser(*this)),
        streamCollector_(AudioStreamCollector::GetAudioStreamCollector()),
        audioEffectManager_(AudioEffectManager::GetAudioEffectManager())
    {
#ifdef ACCESSIBILITY_ENABLE
        accessibilityConfigListener_ = std::make_shared<AccessibilityConfigListener>(*this);
#endif
        deviceStatusListener_ = std::make_unique<DeviceStatusListener>(*this);
    }

    ~AudioPolicyService();

    std::string GetPortName(InternalDeviceType deviceType);

    int32_t RememberRoutingInfo(sptr<AudioRendererFilter> audioRendererFilter,
        sptr<AudioDeviceDescriptor> deviceDescriptor);

    int32_t MoveToLocalOutputDevice(std::vector<SinkInput> sinkInputIds,
        sptr<AudioDeviceDescriptor> localDeviceDescriptor);

    int32_t MoveToRemoteOutputDevice(std::vector<SinkInput> sinkInputIds,
        sptr<AudioDeviceDescriptor> remoteDeviceDescriptor);

    int32_t MoveToLocalInputDevice(std::vector<uint32_t> sourceOutputIds,
        sptr<AudioDeviceDescriptor> localDeviceDescriptor);

    int32_t MoveToRemoteInputDevice(std::vector<uint32_t> sourceOutputIds,
        sptr<AudioDeviceDescriptor> remoteDeviceDescriptor);

    AudioModuleInfo ConstructRemoteAudioModuleInfo(std::string networkId,
        DeviceRole deviceRole, DeviceType deviceType);

    AudioModuleInfo ConstructWakeUpAudioModuleInfo(int32_t wakeupNo);

    AudioIOHandle GetAudioIOHandle(InternalDeviceType deviceType);

    int32_t OpenRemoteAudioDevice(std::string networkId, DeviceRole deviceRole, DeviceType deviceType,
        sptr<AudioDeviceDescriptor> remoteDeviceDescriptor);

    InternalDeviceType GetDeviceType(const std::string &deviceName);

    std::string GetGroupName(const std::string& deviceName, const GroupType type);

    bool IsDeviceConnected(sptr<AudioDeviceDescriptor> &audioDeviceDescriptors) const;

    int32_t DeviceParamsCheck(DeviceRole targetRole,
        std::vector<sptr<AudioDeviceDescriptor>> &audioDeviceDescriptors) const;

    bool IsInputDevice(DeviceType deviceType) const;

    bool IsOutputDevice(DeviceType deviceType) const;

    DeviceRole GetDeviceRole(DeviceType deviceType) const;

    DeviceRole GetDeviceRole(const std::string &role);

    int32_t SelectNewDevice(DeviceRole deviceRole, DeviceType deviceType);

    int32_t HandleA2dpDevice(DeviceType deviceType);

    int32_t HandleFileDevice(DeviceType deviceType);

    int32_t ActivateNewDevice(DeviceType deviceType, bool isSceneActivation);

    DeviceRole GetDeviceRole(AudioPin pin) const;

    void KeepPortMute(int32_t muteDuration, std::string portName, DeviceType deviceType);

    int32_t ActivateNewDevice(std::string networkId, DeviceType deviceType, bool isRemote);

    DeviceType FetchHighPriorityDevice(bool isOutputDevice);

    void UpdateConnectedDevicesWhenConnecting(const AudioDeviceDescriptor& deviceDescriptor,
        std::vector<sptr<AudioDeviceDescriptor>>& desc);

    void UpdateConnectedDevicesWhenDisconnecting(const AudioDeviceDescriptor& deviceDescriptor,
        std::vector<sptr<AudioDeviceDescriptor>> &desc);

    void TriggerDeviceChangedCallback(const std::vector<sptr<AudioDeviceDescriptor>> &devChangeDesc, bool connection);
 
    std::vector<sptr<AudioDeviceDescriptor>> DeviceFilterByFlag(DeviceFlag flag,
        const std::vector<sptr<AudioDeviceDescriptor>>& desc);

    void WriteDeviceChangedSysEvents(const std::vector<sptr<AudioDeviceDescriptor>> &desc, bool isConnected);

    bool GetActiveDeviceStreamInfo(DeviceType deviceType, AudioStreamInfo &streamInfo);

    bool IsConfigurationUpdated(DeviceType deviceType, const AudioStreamInfo &streamInfo);

    void UpdateInputDeviceInfo(DeviceType deviceType);

    void UpdateStreamChangeDeviceInfo(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo);

    void UpdateTrackerDeviceChange(const vector<sptr<AudioDeviceDescriptor>> &desc);

    void UpdateGroupInfo(GroupType type, std::string groupName, int32_t& groupId, std::string networkId,
        bool connected, int32_t mappingId);

    void AddAudioDevice(AudioModuleInfo& moduleInfo, InternalDeviceType devType);

    void OnPreferredOutputDeviceUpdated(DeviceType devType, std::string networkId);

    void OnPreferredInputDeviceUpdated(DeviceType deviceType, std::string networkId);

    void OnPreferredDeviceUpdated(DeviceType activeOutputDevice, DeviceType activeInputDevice);

    std::vector<sptr<AudioDeviceDescriptor>> GetDevicesForGroup(GroupType type, int32_t groupId);

    void SetEarpieceState();

    void SetVolumeForSwitchDevice(DeviceType deviceType);

    void SetVoiceCallVolume(int32_t volume);

    void RemoveDeviceInRouterMap(std::string networkId);

    void UpdateDisplayName(sptr<AudioDeviceDescriptor> deviceDescriptor);

    void RegisterRemoteDevStatusCallback();

    void UpdateLocalGroupInfo(bool isConnected, const std::string& macAddress,
        const std::string& deviceName, const AudioStreamInfo& streamInfo, AudioDeviceDescriptor& deviceDesc);

    int32_t HandleLocalDeviceConnected(DeviceType devType, const std::string& macAddress, const std::string& deviceName,
        const AudioStreamInfo& streamInfo);

    int32_t HandleLocalDeviceDisconnected(DeviceType devType, const std::string& macAddress);

    void UpdateEffectDefaultSink(DeviceType deviceType);

    void LoadEffectSinks();

    void LoadSinksForCapturer();

    void LoadInnerCapturerSink();

    void LoadReceiverSink();

    void LoadLoopback();

    DeviceType FindConnectedHeadset();

    bool CreateDataShareHelperInstance();

    void RegisterNameMonitorHelper();

    bool interruptEnabled_ = true;
    bool isUpdateRouteSupported_ = true;
    bool isCurrentRemoteRenderer = false;
    bool remoteCapturerSwitch_ = false;
    bool isOpenRemoteDevice = false;
    bool isBtListenerRegistered = false;
    bool isPnpDeviceConnected = false;
    bool hasModulesLoaded = false;
    const int32_t G_UNKNOWN_PID = -1;
    int32_t dAudioClientUid = 3055;
    int32_t maxRendererInstances_ = 16;
    uint64_t audioLatencyInMsec_ = 50;
    uint32_t sinkLatencyInMsec_ {0};

    std::bitset<MIN_SERVICE_COUNT> serviceFlag_;
    std::mutex serviceFlagMutex_;
    DeviceType effectActiveDevice_ = DEVICE_TYPE_NONE;
    DeviceType currentActiveDevice_ = DEVICE_TYPE_NONE;
    DeviceType activeInputDevice_ = DEVICE_TYPE_NONE;
    DeviceType pnpDevice_ = DEVICE_TYPE_NONE;
    std::string localDevicesType_ = "";

    std::mutex routerMapMutex_; // unordered_map is not concurrently-secure
    std::mutex preferredInputMapMutex_;
    std::unordered_map<int32_t, std::pair<std::string, int32_t>> routerMap_;
    IAudioPolicyInterface& audioPolicyManager_;
    Parser& configParser_;
#ifdef FEATURE_DTMF_TONE
    std::unordered_map<int32_t, std::shared_ptr<ToneInfo>> toneDescriptorMap;
#endif
    AudioStreamCollector& streamCollector_;
#ifdef ACCESSIBILITY_ENABLE
    std::shared_ptr<AccessibilityConfigListener> accessibilityConfigListener_;
#endif
    std::unique_ptr<DeviceStatusListener> deviceStatusListener_;
    std::vector<sptr<AudioDeviceDescriptor>> connectedDevices_;
    std::unordered_map<std::string, AudioStreamInfo> connectedA2dpDeviceMap_;
    std::string activeBTDevice_;

    std::map<std::pair<int32_t, DeviceFlag>, sptr<IStandardAudioPolicyManagerListener>> deviceChangeCbsMap_;
    std::unordered_map<int32_t, sptr<IStandardAudioRoutingManagerListener>> preferredOutputDeviceCbsMap_;
    std::unordered_map<int32_t, sptr<IStandardAudioRoutingManagerListener>> preferredInputDeviceCbsMap_;

    AudioScene audioScene_ = AUDIO_SCENE_DEFAULT;
    std::map<std::pair<AudioFocusType, AudioFocusType>, AudioFocusEntry> focusMap_ = {};
    std::unordered_map<ClassType, std::list<AudioModuleInfo>> deviceClassInfo_ = {};

    std::mutex ioHandlesMutex_;
    std::unordered_map<std::string, AudioIOHandle> IOHandles_ = {};

    std::shared_ptr<AudioSharedMemory> policyVolumeMap_ = nullptr;
    volatile Volume *volumeVector_ = nullptr;

    std::vector<DeviceType> outputPriorityList_ = {
        DEVICE_TYPE_BLUETOOTH_SCO,
        DEVICE_TYPE_BLUETOOTH_A2DP,
        DEVICE_TYPE_USB_HEADSET,
        DEVICE_TYPE_WIRED_HEADSET,
        DEVICE_TYPE_SPEAKER
    };
    std::vector<DeviceType> inputPriorityList_ = {
        DEVICE_TYPE_BLUETOOTH_SCO,
        DEVICE_TYPE_BLUETOOTH_A2DP,
        DEVICE_TYPE_USB_HEADSET,
        DEVICE_TYPE_WIRED_HEADSET,
        DEVICE_TYPE_WAKEUP,
        DEVICE_TYPE_MIC
    };

    std::vector<sptr<VolumeGroupInfo>> volumeGroups_;
    std::vector<sptr<InterruptGroupInfo>> interruptGroups_;
    std::unordered_map<std::string, std::string> volumeGroupData_;
    std::unordered_map<std::string, std::string> interruptGroupData_;
    AudioEffectManager& audioEffectManager_;

    std::mutex outputDeviceSelectedByCallingMutex_;
    std::unordered_map<decltype(IPCSkeleton::GetCallingUid()), DeviceType> outputDeviceSelectedByCalling_;

    bool isMicrophoneMute_ = false;

    int wakeupCount_ = 0;
    std::mutex wakeupCountMutex_;

    std::mutex deviceClassInfoMutex_;

    std::shared_mutex deviceStatusUpdateSharedMutex_;
};
} // namespace AudioStandard
} // namespace OHOS

#endif // ST_AUDIO_POLICY_SERVICE_H
