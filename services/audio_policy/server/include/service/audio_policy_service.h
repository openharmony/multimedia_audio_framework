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

#ifndef ST_AUDIO_POLICY_SERVICE_H
#define ST_AUDIO_POLICY_SERVICE_H

#include <bitset>
#include <list>
#include <string>
#include <unordered_map>
#include <mutex>
#include "audio_group_handle.h"
#include "audio_info.h"
#include "audio_manager_base.h"
#include "audio_policy_manager_factory.h"
#include "audio_stream_collector.h"
#include "audio_tone_parser.h"

#ifdef ACCESSIBILITY_ENABLE
#include "accessibility_config_listener.h"
#else
#include "iaudio_accessibility_config_observer.h"
#endif

#include "device_status_listener.h"
#include "iaudio_policy_interface.h"
#include "iport_observer.h"
#include "parser_factory.h"

namespace OHOS {
namespace AudioStandard {
class AudioPolicyService : public IPortObserver, public IDeviceStatusObserver,
    public IAudioAccessibilityConfigObserver {
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

    const sptr<IStandardAudioService> GetAudioPolicyServiceProxy();

    int32_t SetStreamVolume(AudioStreamType streamType, float volume);

    float GetStreamVolume(AudioStreamType streamType) const;

    int32_t SetLowPowerVolume(int32_t streamId, float volume) const;

    float GetLowPowerVolume(int32_t streamId) const;

    float GetSingleStreamVolume(int32_t streamId) const;

    int32_t SetStreamMute(AudioStreamType streamType, bool mute) const;

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

    AudioScene GetAudioScene() const;

    int32_t GetAudioLatencyFromXml() const;

    uint32_t GetSinkLatencyFromXml() const;

    // Parser callbacks
    void OnXmlParsingCompleted(const std::unordered_map<ClassType, std::list<AudioModuleInfo>> &xmldata);

    void OnVolumeGroupParsed(std::unordered_map<std::string, std::string>& volumeGroupData);

    void OnInterruptGroupParsed(std::unordered_map<std::string, std::string>& interruptGroupData);

    void OnAudioInterruptEnable(bool enable);

    void OnUpdateRouteSupport(bool isSupported);

    std::vector<int32_t> GetSupportedTones();

    std::shared_ptr<ToneInfo> GetToneConfig(int32_t ltonetype);

    void OnDeviceStatusUpdated(DeviceType devType, bool isConnected,
        const std::string &macAddress, const std::string &deviceName,
        const AudioStreamInfo &streamInfo);

    void OnDeviceConfigurationChanged(DeviceType deviceType,
        const std::string &macAddress, const std::string &deviceName,
        const AudioStreamInfo &streamInfo);

    void OnDeviceStatusUpdated(DStatusInfo statusInfo);

    void OnServiceConnected(AudioServiceIndex serviceIndex);

    void OnServiceDisconnected(AudioServiceIndex serviceIndex);

    void OnMonoAudioConfigChanged(bool audioMono);

    void OnAudioBalanceChanged(float audioBalance);

    int32_t SetAudioSessionCallback(AudioSessionCallback *callback);

    int32_t SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag, const sptr<IRemoteObject> &object);

    int32_t UnsetDeviceChangeCallback(const int32_t clientId);

    int32_t RegisterAudioRendererEventListener(int32_t clientUID, const sptr<IRemoteObject> &object,
        bool hasBTPermission);

    int32_t UnregisterAudioRendererEventListener(int32_t clientUID);

    int32_t RegisterAudioCapturerEventListener(int32_t clientUID, const sptr<IRemoteObject> &object,
        bool hasBTPermission);

    int32_t UnregisterAudioCapturerEventListener(int32_t clientUID);

    int32_t RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
        const sptr<IRemoteObject> &object);

    int32_t UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo);

    int32_t GetCurrentRendererChangeInfos(vector<unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos,
        bool hasBTPermission);

    int32_t GetCurrentCapturerChangeInfos(vector<unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos,
        bool hasBTPermission);

    void RegisteredTrackerClientDied(pid_t pid);

    void RegisteredStreamListenerClientDied(pid_t pid);

    int32_t ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType);

    void OnAudioLatencyParsed(uint64_t latency);

    void OnSinkLatencyParsed(uint32_t latency);

    int32_t UpdateStreamState(int32_t clientUid, StreamSetStateEventInternal &streamSetStateEventInternal);

    DeviceType GetDeviceTypeFromPin(AudioPin pin);

    std::vector<sptr<VolumeGroupInfo>> GetVolumeGroupInfos();

    void SetParameterCallback(const std::shared_ptr<AudioParameterCallback>& callback);

    void RegisterBluetoothListener();

    void UnregisterBluetoothListener();

    void SubscribeAccessibilityConfigObserver();

    std::vector<sptr<AudioDeviceDescriptor>> GetActiveOutputDeviceDescriptors();

private:
    AudioPolicyService()
        : audioPolicyManager_(AudioPolicyManagerFactory::GetAudioPolicyManager()),
          configParser_(ParserFactory::GetInstance().CreateParser(*this)),
          streamCollector_(AudioStreamCollector::GetAudioStreamCollector())
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

    AudioIOHandle GetAudioIOHandle(InternalDeviceType deviceType);

    int32_t OpenRemoteAudioDevice(std::string networkId, DeviceRole deviceRole, DeviceType deviceType,
        sptr<AudioDeviceDescriptor> remoteDeviceDescriptor);

    InternalDeviceType GetDeviceType(const std::string &deviceName);

    std::string GetGroupName(const std::string& deviceName, const GroupType type);

    InternalDeviceType GetCurrentActiveDevice(DeviceRole role) const;

    DeviceRole GetDeviceRole(DeviceType deviceType) const;

    DeviceRole GetDeviceRole(const std::string &role);

    int32_t SelectNewDevice(DeviceRole deviceRole, DeviceType deviceType);

    int32_t ActivateNewDevice(DeviceType deviceType, bool isSceneActivation);

    DeviceRole GetDeviceRole(AudioPin pin) const;

    int32_t ActivateNewDevice(std::string networkId, DeviceType deviceType, bool isRemote);

    DeviceType FetchHighPriorityDevice(bool isOutputDevice);

    void UpdateConnectedDevices(const AudioDeviceDescriptor& deviceDescriptor,
        std::vector<sptr<AudioDeviceDescriptor>>& desc, bool status);

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

    std::vector<sptr<AudioDeviceDescriptor>> GetDevicesForGroup(GroupType type, int32_t groupId);

    bool interruptEnabled_ = true;
    bool isUpdateRouteSupported_ = true;
    bool isCurrentRemoteRenderer = false;
    bool remoteCapturerSwitch = false;
    bool isOpenRemoteDevice = false;
    bool isBtListenerRegistered = false;
    const int32_t G_UNKNOWN_PID = -1;
    int32_t dAudioClientUid = 3055;
    int32_t switchVolumeDelay_ = 500000; // us
    uint64_t audioLatencyInMsec_ = 50;
    uint32_t sinkLatencyInMsec_ {0};
    std::bitset<MIN_SERVICE_COUNT> serviceFlag_;
    std::mutex serviceFlagMutex_;
    DeviceType currentActiveDevice_ = DEVICE_TYPE_NONE;
    DeviceType activeInputDevice_ = DEVICE_TYPE_NONE;
    std::unordered_map<int32_t, std::pair<std::string, int32_t>> routerMap_;
    IAudioPolicyInterface& audioPolicyManager_;
    Parser& configParser_;
    std::unordered_map<int32_t, std::shared_ptr<ToneInfo>> toneDescriptorMap;
    AudioStreamCollector& streamCollector_;
#ifdef ACCESSIBILITY_ENABLE
    std::shared_ptr<AccessibilityConfigListener> accessibilityConfigListener_;
#endif
    std::unique_ptr<DeviceStatusListener> deviceStatusListener_;
    std::vector<sptr<AudioDeviceDescriptor>> connectedDevices_;
    std::unordered_map<std::string, AudioStreamInfo> connectedA2dpDeviceMap_;
    std::string activeBTDevice_;

    std::unordered_map<int32_t, std::pair<DeviceFlag, sptr<IStandardAudioPolicyManagerListener>>>
        deviceChangeCallbackMap_;
    AudioScene audioScene_ = AUDIO_SCENE_DEFAULT;
    std::map<std::pair<AudioStreamType, AudioStreamType>, AudioFocusEntry> focusMap_ = {};
    std::unordered_map<ClassType, std::list<AudioModuleInfo>> deviceClassInfo_ = {};
    std::unordered_map<std::string, AudioIOHandle> IOHandles_ = {};
    std::vector<DeviceType> ioDeviceList = {
        DEVICE_TYPE_BLUETOOTH_A2DP,
        DEVICE_TYPE_BLUETOOTH_SCO,
        DEVICE_TYPE_USB_HEADSET,
        DEVICE_TYPE_WIRED_HEADSET
    };
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
        DEVICE_TYPE_MIC
    };

    std::vector<sptr<VolumeGroupInfo>> volumeGroups_;
    std::vector<sptr<InterruptGroupInfo>> interruptGroups_;
    std::unordered_map<std::string, std::string> volumeGroupData_;
    std::unordered_map<std::string, std::string> interruptGroupData_;
};
} // namespace AudioStandard
} // namespace OHOS

#endif // ST_AUDIO_POLICY_SERVICE_H
