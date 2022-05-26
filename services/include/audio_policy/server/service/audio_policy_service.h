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

#include "audio_info.h"
#include "audio_policy_manager_factory.h"
#include "device_status_listener.h"
#include "iaudio_policy_interface.h"
#include "iport_observer.h"
#include "parser_factory.h"

#include <bitset>
#include <list>
#include <string>
#include <unordered_map>

namespace OHOS {
namespace AudioStandard {
class AudioPolicyService : public IPortObserver, public IDeviceStatusObserver {
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

    int32_t SetStreamVolume(AudioStreamType streamType, float volume) const;

    float GetStreamVolume(AudioStreamType streamType) const;

    int32_t SetStreamMute(AudioStreamType streamType, bool mute) const;

    bool GetStreamMute(AudioStreamType streamType) const;

    bool IsStreamActive(AudioStreamType streamType) const;

    std::vector<sptr<AudioDeviceDescriptor>> GetDevices(DeviceFlag deviceFlag);

    int32_t SetDeviceActive(InternalDeviceType deviceType, bool active);

    bool IsDeviceActive(InternalDeviceType deviceType) const;

    int32_t SetRingerMode(AudioRingerMode ringMode);

    bool IsAudioInterruptEnabled() const;

    auto& GetAudioFocusTable() const
    {
        return focusTable_;
    }

    AudioRingerMode GetRingerMode() const;

    int32_t SetAudioScene(AudioScene audioScene);

    AudioScene GetAudioScene() const;

    // Parser callbacks
    void OnXmlParsingCompleted(const std::unordered_map<ClassType, std::list<AudioModuleInfo>> &xmldata);

    void OnAudioInterruptEnable(bool enable);

    void OnUpdateRouteSupport(bool isSupported);

    void OnDeviceStatusUpdated(DeviceType deviceType, bool connected, void *privData,
        const std::string &macAddress, const AudioStreamInfo &streamInfo);

    void OnDeviceConfigurationChanged(DeviceType deviceType,
        const std::string &macAddress, const AudioStreamInfo &streamInfo);

    void OnServiceConnected(AudioServiceIndex serviceIndex);

    int32_t SetAudioSessionCallback(AudioSessionCallback *callback);

    int32_t SetDeviceChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object);

    int32_t UnsetDeviceChangeCallback(const int32_t clientId);

private:
    AudioPolicyService()
        : mAudioPolicyManager(AudioPolicyManagerFactory::GetAudioPolicyManager()),
          mConfigParser(ParserFactory::GetInstance().CreateParser(*this))
    {
        mDeviceStatusListener = std::make_unique<DeviceStatusListener>(*this);
    }

    ~AudioPolicyService();

    std::string GetPortName(InternalDeviceType deviceType);

    AudioIOHandle GetAudioIOHandle(InternalDeviceType deviceType);

    InternalDeviceType GetDeviceType(const std::string &deviceName);

    InternalDeviceType GetCurrentActiveDevice(DeviceRole role) const;

    DeviceRole GetDeviceRole(DeviceType deviceType) const;

    DeviceRole GetDeviceRole(const std::string &role);

    int32_t ActivateNewDevice(DeviceType deviceType);

    DeviceType FetchHighPriorityDevice();

    void UpdateConnectedDevices(DeviceType deviceType, std::vector<sptr<AudioDeviceDescriptor>> &desc, bool status,
        const std::string &macAddress, const AudioStreamInfo &streamInfo);

    void TriggerDeviceChangedCallback(const std::vector<sptr<AudioDeviceDescriptor>> &devChangeDesc, bool connection);

    bool GetActiveDeviceStreamInfo(DeviceType deviceType, AudioStreamInfo &streamInfo);

    bool IsConfigurationUpdated(DeviceType deviceType, const AudioStreamInfo &streamInfo);

    bool interruptEnabled_ = true;
    bool isUpdateRouteSupported_ = true;
    int32_t mDefaultDeviceCount = 0;
    std::bitset<MIN_SERVICE_COUNT> serviceFlag_;
    DeviceType mCurrentActiveDevice = DEVICE_TYPE_NONE;
    IAudioPolicyInterface& mAudioPolicyManager;
    Parser& mConfigParser;
    std::unique_ptr<DeviceStatusListener> mDeviceStatusListener;
    std::vector<sptr<AudioDeviceDescriptor>> mConnectedDevices;
    std::unordered_map<std::string, AudioStreamInfo> connectedBTDeviceMap_;
    std::string activeBTDevice_;
    std::unordered_map<int32_t, sptr<IStandardAudioPolicyManagerListener>> deviceChangeCallbackMap_;
    AudioScene mAudioScene = AUDIO_SCENE_DEFAULT;
    AudioFocusEntry focusTable_[MAX_NUM_STREAMS][MAX_NUM_STREAMS];
    std::unordered_map<ClassType, std::list<AudioModuleInfo>> deviceClassInfo_ = {};
    std::unordered_map<std::string, AudioIOHandle> mIOHandles = {};
    std::vector<DeviceType> ioDeviceList = {
        DEVICE_TYPE_BLUETOOTH_A2DP,
        DEVICE_TYPE_USB_HEADSET,
        DEVICE_TYPE_WIRED_HEADSET
    };
    std::vector<DeviceType> priorityList = {
        DEVICE_TYPE_BLUETOOTH_A2DP,
        DEVICE_TYPE_USB_HEADSET,
        DEVICE_TYPE_WIRED_HEADSET,
        DEVICE_TYPE_SPEAKER
    };
};
} // namespace AudioStandard
} // namespace OHOS

#endif // ST_AUDIO_POLICY_SERVICE_H
