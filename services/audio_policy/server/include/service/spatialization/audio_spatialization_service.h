/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef ST_AUDIO_SPATIALIZATION_SERVICE_H
#define ST_AUDIO_SPATIALIZATION_SERVICE_H

#include <bitset>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include "audio_group_handle.h"
#include "audio_info.h"
#include "audio_manager_base.h"
#include "audio_policy_manager_factory.h"
#include "ipc_skeleton.h"

#include "iaudio_policy_interface.h"
#include "iport_observer.h"
#include "audio_policy_server_handler.h"

namespace OHOS {
namespace AudioStandard {
class AudioSpatializationService {
public:
    static AudioSpatializationService& GetAudioSpatializationService()
    {
        static AudioSpatializationService audioSpatializationService;
        return audioSpatializationService;
    }

    void Init(const std::vector<EffectChain> &effectChains);
    void Deinit(void);

    const sptr<IStandardAudioService> GetAudioServerProxy();
    bool IsSpatializationEnabled();
    bool IsSpatializationEnabled(const std::string address);
    int32_t SetSpatializationEnabled(const bool enable);
    int32_t SetSpatializationEnabled(const sptr<AudioDeviceDescriptor> &selectedAudioDevice, const bool enable);
    bool IsHeadTrackingEnabled();
    bool IsHeadTrackingEnabled(const std::string address);
    int32_t SetHeadTrackingEnabled(const bool enable);
    int32_t SetHeadTrackingEnabled(const sptr<AudioDeviceDescriptor> &selectedAudioDevice, const bool enable);
    void HandleSpatializationEnabledChange(const bool &enabled);
    void HandleSpatializationEnabledChange(const sptr<AudioDeviceDescriptor> &selectedAudioDevice, const bool &enabled);
    void HandleHeadTrackingEnabledChange(const bool &enabled);
    void HandleHeadTrackingEnabledChange(const sptr<AudioDeviceDescriptor> &selectedAudioDevice, const bool &enabled);
    AudioSpatializationState GetSpatializationState(const StreamUsage streamUsage = StreamUsage::STREAM_USAGE_UNKNOWN);
    bool IsSpatializationSupported();
    bool IsSpatializationSupportedForDevice(const std::string address);
    bool IsHeadTrackingSupported();
    bool IsHeadTrackingSupportedForDevice(const std::string address);
    int32_t UpdateSpatialDeviceState(const AudioSpatialDeviceState audioSpatialDeviceState);
    int32_t RegisterSpatializationStateEventListener(const uint32_t sessionID, const StreamUsage streamUsage,
        const sptr<IRemoteObject> &object);
    int32_t UnregisterSpatializationStateEventListener(const uint32_t sessionID);
    void UpdateCurrentDevice(const std::string macAddress);
    AudioSpatializationSceneType GetSpatializationSceneType();
    int32_t SetSpatializationSceneType(const AudioSpatializationSceneType spatializationSceneType);
    bool IsHeadTrackingDataRequested(const std::string &macAddress);
    void UpdateRendererInfo(const std::vector<std::unique_ptr<AudioRendererChangeInfo>> &rendererChangeInfo);
    void InitSpatializationState();
    std::string GetCurrentDeviceAddress() const;
private:
    AudioSpatializationService()
        :audioPolicyServerHandler_(DelayedSingleton<AudioPolicyServerHandler>::GetInstance())
    {}

    ~AudioSpatializationService();

    enum WriteToDbOperation {
        WRITE_SPATIALIZATION_STATE = 0,
        WRITE_SPATIALIZATION_SCENE = 1,
        WRITE_DEVICESPATIAL_INFO = 2,
    };
    static constexpr uint32_t MAX_DEVICE_NUM = 10;
    int32_t UpdateSpatializationStateReal(bool outputDeviceChange, std::string preDeviceAddress = "");
    int32_t UpdateSpatializationState();
    int32_t UpdateSpatializationSceneType();
    void UpdateSpatialDeviceType(AudioSpatialDeviceType audioSpatialDeviceType);
    void HandleSpatializationStateChange(bool outputDeviceChange);
    void WriteSpatializationStateToDb(WriteToDbOperation operation, std::string address = "");
    bool IsHeadTrackingDataRequestedForCurrentDevice();
    void UpdateHeadTrackingDeviceState(bool outputDeviceChange, std::string preDeviceAddress = "");
    void HandleHeadTrackingDeviceChange(const std::unordered_map<std::string, bool> &changeInfo);
    void UpdateDeviceSpatialInfo(const uint32_t deviceID, const std::string deviceSpatialInfo);
    void UpdateDeviceSpatialMapInfo(const std::string address, const std::string deviceSpatialInfo);
    std::string RemoveOldestDevice();
    std::string GetCurrTimestamp();
    std::string EncapsulateDeviceInfo(const std::string address);
    std::string ExtractTimestamp(const std::string deviceSpatialInfo);
    std::string GetSha256EncryptAddress(const std::string& address);
    std::map<std::string, std::string> addressToDeviceSpatialInfoMap_;
    std::map<std::string, uint32_t> addressToDeviceIDMap_;
    std::shared_ptr<AudioPolicyServerHandler> audioPolicyServerHandler_;
    std::string currentDeviceAddress_ = "";
    std::string preSettingSpatialAddress_ = "NO_PREVIOUS_SET_DEVICE";
    bool isSpatializationSupported_ = false;
    bool isHeadTrackingSupported_ = false;
    bool spatializationEnabledReal_ = false;
    bool headTrackingEnabledReal_ = false;
    bool isHeadTrackingDataRequested_ = false;
    bool isLoadedfromDb_ = false;
    AudioSpatializationState spatializationStateFlag_ = {false};
    std::unordered_map<std::string, AudioSpatializationState> addressToSpatialEnabledMap_;
    AudioSpatializationSceneType spatializationSceneType_ = SPATIALIZATION_SCENE_TYPE_DEFAULT;
    AudioSpatialDeviceType currSpatialDeviceType_{ EARPHONE_TYPE_OTHERS };
    std::vector<AudioRendererInfoForSpatialization> spatializationRendererInfoList_;
    std::mutex spatializationServiceMutex_;
    std::mutex spatializationSupportedMutex_;
    std::mutex spatializationStateChangeListnerMutex_;
    std::mutex rendererInfoChangingMutex_;
    std::unordered_map<uint32_t, std::pair<std::shared_ptr<AudioSpatializationStateChangeCallback>, StreamUsage>>
        spatializationStateCBMap_;
    std::map<std::string, AudioSpatialDeviceState> addressToSpatialDeviceStateMap_;
};
} // namespace AudioStandard
} // namespace OHOS

#endif // ST_AUDIO_SPATIALIZATION_SERVICE_H
