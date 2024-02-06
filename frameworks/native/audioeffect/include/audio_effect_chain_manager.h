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


#ifndef AUDIO_EFFECT_CHAIN_MANAGER_H
#define AUDIO_EFFECT_CHAIN_MANAGER_H

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <set>

#include "v1_0/ieffect_model.h"
#include "audio_effect_chain_adapter.h"
#include "audio_effect.h"

#ifdef SENSOR_ENABLE
#include "audio_head_tracker.h"
#endif

namespace OHOS {
namespace AudioStandard {

const uint32_t NUM_SET_EFFECT_PARAM = 3;
const uint32_t DEFAULT_FRAMELEN = 1440;
const uint32_t DEFAULT_SAMPLE_RATE = 48000;
const uint32_t DEFAULT_NUM_CHANNEL = STEREO;
const uint32_t DEFAULT_MCH_NUM_CHANNEL = CHANNEL_6;
const uint64_t DEFAULT_NUM_CHANNELLAYOUT = CH_LAYOUT_STEREO;
const uint64_t DEFAULT_MCH_NUM_CHANNELLAYOUT = CH_LAYOUT_5POINT1;
const uint32_t FACTOR_TWO = 2;
const uint32_t BASE_TEN = 10;
const std::string DEFAULT_DEVICE_SINK = "Speaker";
const uint32_t SIZE_OF_SPATIALIZATION_STATE = 2;
const uint32_t SEND_HDI_COMMAND_LEN = 20;
const uint32_t GET_HDI_BUFFER_LEN = 10;
const uint32_t HDI_ROOM_MODE_INDEX_TWO = 2;

typedef struct sessionEffectInfo {
    std::string sceneMode;
    uint32_t channels;
    uint64_t channelLayout;
    std::string spatializationEnabled;
} sessionEffectInfo;

const std::vector<AudioChannelLayout> HVS_SUPPORTED_CHANNELLAYOUTS {
    CH_LAYOUT_STEREO,
    CH_LAYOUT_5POINT1_BACK,
    CH_LAYOUT_5POINT1POINT2,
    CH_LAYOUT_7POINT1,
    CH_LAYOUT_5POINT1POINT4,
    CH_LAYOUT_7POINT1POINT2,
    CH_LAYOUT_7POINT1POINT4,
    CH_LAYOUT_9POINT1POINT4,
    CH_LAYOUT_9POINT1POINT6
};

class AudioEffectHdi {
public:
    AudioEffectHdi();
    ~AudioEffectHdi();
    void InitHdi();
    int32_t UpdateHdiState(int8_t *effectHdiInput);
private:
    std::string libName;
    std::string effectId;
    int8_t input[SEND_HDI_COMMAND_LEN];
    int8_t output[GET_HDI_BUFFER_LEN];
    uint32_t replyLen;
    IEffectModel *hdiModel_ = nullptr;
    IEffectControl *hdiControl_ = nullptr;
};

struct AudioEffectProcInfo {
    bool headTrackingEnabled;
    bool offloadEnabled;
};

class AudioEffectChain {
public:
#ifdef SENSOR_ENABLE
    AudioEffectChain(std::string scene, std::shared_ptr<HeadTracker> headTracker);
#else
    AudioEffectChain(std::string scene);
#endif
    ~AudioEffectChain();
    std::string GetEffectMode();
    void SetEffectMode(std::string mode);
    void ReleaseEffectChain();
    void AddEffectHandleBegin();
    void AddEffectHandleEnd();
    void AddEffectHandle(AudioEffectHandle effectHandle, AudioEffectLibrary *libHandle);
    void SetEffectChain(std::vector<AudioEffectHandle> &effHandles, std::vector<AudioEffectLibrary *> &libHandles);
    void ApplyEffectChain(float *bufIn, float *bufOut, uint32_t frameLen, AudioEffectProcInfo procInfo);
    void SetIOBufferConfig(bool isInput, uint32_t samplingRate, uint32_t channels);
    bool IsEmptyEffectHandles();
    void Dump();
    int32_t UpdateMultichannelIoBufferConfig(const uint32_t &channels, const uint64_t &channelLayout);
    void StoreOldEffectChainInfo(std::string &sceneMode, AudioEffectConfig &ioBufferConfig);
    AudioEffectConfig GetIoBufferConfig();
    void InitEffectChain();
    void SetHeadTrackingDisabled();
private:
    std::mutex reloadMutex;
    std::string sceneType;
    std::string effectMode;
    std::vector<AudioEffectHandle> standByEffectHandles;
    std::vector<AudioEffectLibrary*> libHandles;
    AudioEffectConfig ioBufferConfig;
    AudioBuffer audioBufIn;
    AudioBuffer audioBufOut;

#ifdef SENSOR_ENABLE
    std::shared_ptr<HeadTracker> headTracker_;
#endif
};

class AudioEffectChainManager {
public:
    AudioEffectChainManager();
    ~AudioEffectChainManager();
    static AudioEffectChainManager *GetInstance();
    void InitAudioEffectChainManager(std::vector<EffectChain> &effectChains,
        std::unordered_map<std::string, std::string> &map,
        std::vector<std::unique_ptr<AudioEffectLibEntry>> &effectLibraryList);
    bool CheckAndAddSessionID(std::string sessionID);
    int32_t CreateAudioEffectChainDynamic(std::string sceneType);
    int32_t SetAudioEffectChainDynamic(std::string sceneType, std::string effectMode);
    bool CheckAndRemoveSessionID(std::string sessionID);
    int32_t ReleaseAudioEffectChainDynamic(std::string sceneType);
    bool ExistAudioEffectChain(std::string sceneType, std::string effectMode, std::string spatializationEnabled);
    int32_t ApplyAudioEffectChain(std::string sceneType, BufferAttr *bufferAttr);
    int32_t SetOutputDeviceSink(int32_t device, std::string &sinkName);
    std::string GetDeviceTypeName();
    std::string GetDeviceSinkName();
    int32_t GetFrameLen();
    int32_t SetFrameLen(int32_t frameLen);
    bool GetOffloadEnabled();
    void Dump();
    int32_t UpdateMultichannelConfig(const std::string &sceneType);
    int32_t InitAudioEffectChainDynamic(std::string sceneType);
    int32_t UpdateSpatializationState(AudioSpatializationState spatializationState);
    int32_t SetHdiParam(std::string sceneType, std::string effectMode, bool enabled);
    int32_t SessionInfoMapAdd(std::string sceneType, std::string sessionID, sessionEffectInfo info);
    int32_t SessionInfoMapDelete(std::string sceneType, std::string sessionID);
    int32_t ReturnEffectChannelInfo(const std::string &sceneType, uint32_t *channels, uint64_t *channelLayout);
    int32_t ReturnMultiChannelInfo(uint32_t *channels, uint64_t *channelLayout);
    void RegisterEffectChainCountBackupMap(std::string sceneType, std::string operation);
private:
    void UpdateSensorState();
    void DeleteAllChains();
    void RecoverAllChains();
    std::map<std::string, AudioEffectLibEntry*> EffectToLibraryEntryMap_;
    std::map<std::string, std::string> EffectToLibraryNameMap_;
    std::map<std::string, std::vector<std::string>> EffectChainToEffectsMap_;
    std::map<std::string, std::string> SceneTypeAndModeToEffectChainNameMap_;
    std::map<std::string, AudioEffectChain*> SceneTypeToEffectChainMap_;
    std::map<std::string, int32_t> SceneTypeToEffectChainCountMap_;
    std::set<std::string> SessionIDSet_;
    std::map<std::string, std::set<std::string>> SceneTypeToSessionIDMap_;
    std::map<std::string, sessionEffectInfo> SessionIDToEffectInfoMap_;
    std::map<std::string, int32_t> SceneTypeToEffectChainCountBackupMap_;
    uint32_t frameLen_ = DEFAULT_FRAMELEN;
    DeviceType deviceType_ = DEVICE_TYPE_SPEAKER;
    std::string deviceSink_ = DEFAULT_DEVICE_SINK;
    bool isInitialized_ = false;
    std::recursive_mutex dynamicMutex_;
    bool spatializationEnabled_ = false;
    bool headTrackingEnabled_ = false;
    bool offloadEnabled_ = false;
    bool initializedLogFlag_ = true;
#ifdef SENSOR_ENABLE
    std::shared_ptr<HeadTracker> headTracker_;
#endif

    std::shared_ptr<AudioEffectHdi> audioEffectHdi_;
    int8_t effectHdiInput[SEND_HDI_COMMAND_LEN];
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_EFFECT_CHAIN_MANAGER_H
