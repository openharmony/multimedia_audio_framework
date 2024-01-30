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

#ifndef ST_AUDIO_MANAGER_PROXY_H
#define ST_AUDIO_MANAGER_PROXY_H

#include "iremote_proxy.h"
#include "audio_system_manager.h"
#include "audio_manager_base.h"

namespace OHOS {
namespace AudioStandard {
class AudioManagerProxy : public IRemoteProxy<IStandardAudioService> {
public:
    explicit AudioManagerProxy(const sptr<IRemoteObject> &impl);
    virtual ~AudioManagerProxy() = default;
    
    int32_t SetMicrophoneMute(bool isMute) override;
    int32_t SetVoiceVolume(float volume) override;
    int32_t GetCapturePresentationPosition(const std::string& deviceClass, uint64_t& frames, int64_t& timeSec,
        int64_t& timeNanoSec) override;
    int32_t GetRenderPresentationPosition(const std::string& deviceClass, uint64_t& frames, int64_t& timeSec,
        int64_t& timeNanoSec) override;
    int32_t OffloadSetVolume(float volume) override;
    int32_t OffloadDrain() override;
    int32_t OffloadGetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec) override;
    int32_t OffloadSetBufferSize(uint32_t sizeMs) override;
    int32_t SetAudioScene(AudioScene audioScene, DeviceType activeDevice) override;
    const std::string GetAudioParameter(const std::string &key) override;
    const std::string GetAudioParameter(const std::string& networkId, const AudioParamKey key,
        const std::string& condition) override;
    const std::vector<std::pair<std::string, std::string>> GetExtraParameters(const std::string &mainKey,
        const std::vector<std::string> &subKeys) override;
    void SetAudioParameter(const std::string &key, const std::string &value) override;
    void SetAudioParameter(const std::string& networkId, const AudioParamKey key, const std::string& condition,
        const std::string& value) override;
    void SetExtraParameters(const std::string &key,
        const std::vector<std::pair<std::string, std::string>> &kvpairs) override;
    int32_t UpdateActiveDeviceRoute(DeviceType type, DeviceFlag flag) override;
    uint64_t GetTransactionId(DeviceType deviceType, DeviceRole deviceRole) override;
    void NotifyDeviceInfo(std::string networkId, bool connected) override;
    int32_t CheckRemoteDeviceState(std::string networkId, DeviceRole deviceRole, bool isStartDevice) override;
    int32_t SetParameterCallback(const sptr<IRemoteObject>& object) override;
    int32_t SetWakeupSourceCallback(const sptr<IRemoteObject>& object) override;
    void SetAudioMonoState(bool audioMono) override;
    void SetAudioBalanceValue(float audioBalance) override;
    sptr<IRemoteObject> CreateAudioProcess(const AudioProcessConfig &config) override;
    bool LoadAudioEffectLibraries(const std::vector<Library> libraries, const std::vector<Effect> effects,
        std::vector<Effect> &successEffects) override;
    void RequestThreadPriority(uint32_t tid, std::string bundleName) override;
    bool CreateEffectChainManager(std::vector<EffectChain> &effectChains,
        std::unordered_map<std::string, std::string> &map) override;
    bool SetOutputDeviceSink(int32_t deviceType, std::string &sinkName) override;
    bool CreatePlaybackCapturerManager() override;
    int32_t SetSupportStreamUsage(std::vector<int32_t> usage) override;
    int32_t RegiestPolicyProvider(const sptr<IRemoteObject> &object) override;
    int32_t SetCaptureSilentState(bool state) override;
    int32_t UpdateSpatializationState(AudioSpatializationState spatializationState) override;
    int32_t NotifyStreamVolumeChanged(AudioStreamType streamType, float volume) override;
private:
    static inline BrokerDelegator<AudioManagerProxy> delegator_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // ST_AUDIO_MANAGER_PROXY_H
