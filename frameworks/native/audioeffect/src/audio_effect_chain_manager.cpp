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

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cstddef>
#include <map>
#include <memory>
#include <vector>
#include <set>

#include "securec.h"
#include "audio_effect_chain_adapter.h"
#include "audio_effect_chain_manager.h"
#include "audio_log.h"
#include "audio_errors.h"
#include "audio_effect.h"

#define DEVICE_FLAG

using namespace OHOS::AudioStandard;

static std::map<AudioChannelSet, pa_channel_position> chSetToPaPositionMap = {
    {FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_LEFT}, {FRONT_RIGHT, PA_CHANNEL_POSITION_FRONT_RIGHT},
    {FRONT_CENTER, PA_CHANNEL_POSITION_FRONT_CENTER}, {LOW_FREQUENCY, PA_CHANNEL_POSITION_LFE},
    {SIDE_LEFT, PA_CHANNEL_POSITION_SIDE_LEFT}, {SIDE_RIGHT, PA_CHANNEL_POSITION_SIDE_RIGHT},
    {BACK_LEFT, PA_CHANNEL_POSITION_REAR_LEFT}, {BACK_RIGHT, PA_CHANNEL_POSITION_REAR_RIGHT},
    {FRONT_LEFT_OF_CENTER, PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER},
    {FRONT_RIGHT_OF_CENTER, PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER},
    {BACK_CENTER, PA_CHANNEL_POSITION_REAR_CENTER}, {TOP_CENTER, PA_CHANNEL_POSITION_TOP_CENTER},
    {TOP_FRONT_LEFT, PA_CHANNEL_POSITION_TOP_FRONT_LEFT}, {TOP_FRONT_CENTER, PA_CHANNEL_POSITION_TOP_FRONT_CENTER},
    {TOP_FRONT_RIGHT, PA_CHANNEL_POSITION_TOP_FRONT_RIGHT}, {TOP_BACK_LEFT, PA_CHANNEL_POSITION_TOP_REAR_LEFT},
    {TOP_BACK_CENTER, PA_CHANNEL_POSITION_TOP_REAR_CENTER}, {TOP_BACK_RIGHT, PA_CHANNEL_POSITION_TOP_REAR_RIGHT},
    /** Channel layout positions below do not have precise mapped pulseaudio positions */
    {STEREO_LEFT, PA_CHANNEL_POSITION_FRONT_LEFT}, {STEREO_RIGHT, PA_CHANNEL_POSITION_FRONT_RIGHT},
    {WIDE_LEFT, PA_CHANNEL_POSITION_FRONT_LEFT}, {WIDE_RIGHT, PA_CHANNEL_POSITION_FRONT_RIGHT},
    {SURROUND_DIRECT_LEFT, PA_CHANNEL_POSITION_SIDE_LEFT}, {SURROUND_DIRECT_RIGHT, PA_CHANNEL_POSITION_SIDE_LEFT},
    {BOTTOM_FRONT_CENTER, PA_CHANNEL_POSITION_FRONT_CENTER},
    {BOTTOM_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_LEFT}, {BOTTOM_FRONT_RIGHT, PA_CHANNEL_POSITION_FRONT_RIGHT},
    {TOP_SIDE_LEFT, PA_CHANNEL_POSITION_TOP_REAR_LEFT}, {TOP_SIDE_RIGHT, PA_CHANNEL_POSITION_TOP_REAR_RIGHT},
    {LOW_FREQUENCY_2, PA_CHANNEL_POSITION_LFE},
};


int32_t EffectChainManagerProcess(char *sceneType, BufferAttr *bufferAttr)
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, ERR_INVALID_HANDLE, "null audioEffectChainManager");
    std::string sceneTypeString = "";
    if (sceneType) {
        sceneTypeString = sceneType;
    }
    if (audioEffectChainManager->ApplyAudioEffectChain(sceneTypeString, bufferAttr) != SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

int32_t EffectChainManagerGetFrameLen()
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, DEFAULT_FRAMELEN, "null audioEffectChainManager");
    return audioEffectChainManager->GetFrameLen();
}

bool EffectChainManagerExist(const char *sceneType, const char *effectMode, const char *spatializationEnabled)
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, false, "null audioEffectChainManager");
    std::string sceneTypeString = "";
    if (sceneType) {
        sceneTypeString = sceneType;
    }
    std::string effectModeString = "";
    if (effectMode) {
        effectModeString = effectMode;
    }
    std::string spatializationEnabledString = "";
    if (spatializationEnabled) {
        spatializationEnabledString = spatializationEnabled;
    }
    return audioEffectChainManager->ExistAudioEffectChain(sceneTypeString, effectModeString,
        spatializationEnabledString);
}

int32_t EffectChainManagerCreateCb(const char *sceneType, const char *sessionID)
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, ERR_INVALID_HANDLE, "null audioEffectChainManager");
    std::string sceneTypeString = "";
    std::string sessionIDString = "";
    if (sceneType) {
        sceneTypeString = sceneType;
    }
    if (sessionID) {
        sessionIDString = sessionID;
    }
    if (!audioEffectChainManager->CheckAndAddSessionID(sessionIDString)) {
        return SUCCESS;
    }
    if (audioEffectChainManager->GetOffloadEnabled()) {
        audioEffectChainManager->RegisterEffectChainCountBackupMap(sceneTypeString, "Register");
        return SUCCESS;
    }
    if (audioEffectChainManager->CreateAudioEffectChainDynamic(sceneTypeString) != SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

int32_t EffectChainManagerReleaseCb(const char *sceneType, const char *sessionID)
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, ERR_INVALID_HANDLE, "null audioEffectChainManager");
    std::string sceneTypeString = "";
    std::string sessionIDString = "";
    if (sceneType) {
        sceneTypeString = sceneType;
    }
    if (sessionID) {
        sessionIDString = sessionID;
    }
    if (!audioEffectChainManager->CheckAndRemoveSessionID(sessionIDString)) {
        return SUCCESS;
    }
    if (audioEffectChainManager->GetOffloadEnabled()) {
        audioEffectChainManager->RegisterEffectChainCountBackupMap(sceneTypeString, "Deregister");
        return SUCCESS;
    }
    if (audioEffectChainManager->ReleaseAudioEffectChainDynamic(sceneTypeString) != SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

int32_t EffectChainManagerMultichannelUpdate(const char *sceneType)
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, ERR_INVALID_HANDLE, "null audioEffectChainManager");
    std::string sceneTypeString = "";
    if (sceneType != nullptr && strlen(sceneType)) {
        sceneTypeString = sceneType;
    } else {
        AUDIO_ERR_LOG("Scenetype is null.");
        return ERROR;
    }
    if (audioEffectChainManager->UpdateMultichannelConfig(sceneTypeString) != SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

bool IsChannelLayoutHVSSupported(const uint64_t channelLayout)
{
    return find(HVS_SUPPORTED_CHANNELLAYOUTS.begin(), HVS_SUPPORTED_CHANNELLAYOUTS.end(),
        channelLayout) != HVS_SUPPORTED_CHANNELLAYOUTS.end();
}

uint32_t ConvertChLayoutToPaChMap(const uint64_t channelLayout, pa_channel_map *paMap)
{
    uint32_t channelNum = 0;
    uint64_t mode = (channelLayout & CH_MODE_MASK) >> CH_MODE_OFFSET;
    switch (mode) {
        case 0: {
            for (auto bit = chSetToPaPositionMap.begin(); bit != chSetToPaPositionMap.end(); ++bit) {
                if ((channelLayout & (bit->first)) != 0) {
                    paMap->map[channelNum++] = bit->second;
                }
            }
            break;
        }
        case 1: {
            uint64_t order = (channelLayout & CH_HOA_ORDNUM_MASK) >> CH_HOA_ORDNUM_OFFSET;
            channelNum = (order + 1) * (order + 1);
            for (uint32_t i = 0; i < channelNum; ++i) {
                paMap->map[i] = chSetToPaPositionMap[FRONT_LEFT];
            }
            break;
        }
        default:
            channelNum = 0;
            break;
    }
    return channelNum;
}

int32_t EffectChainManagerSetHdiParam(const char *sceneType, const char *effectMode, bool enabled)
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, ERR_INVALID_HANDLE, "null audioEffectChainManager");
    std::string sceneTypeString = "";
    if (sceneType) {
        sceneTypeString = sceneType;
    }
    std::string effectModeString = "";
    if (effectMode) {
        effectModeString = effectMode;
    }
    return audioEffectChainManager->SetHdiParam(sceneTypeString, effectModeString, enabled);
}

int32_t EffectChainManagerInitCb(const char *sceneType)
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, ERR_INVALID_HANDLE, "null audioEffectChainManager");
    std::string sceneTypeString = "";
    if (sceneType) {
        sceneTypeString = sceneType;
    }
    if (audioEffectChainManager->InitAudioEffectChainDynamic(sceneTypeString) != SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

bool EffectChainManagerCheckA2dpOffload()
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, ERR_INVALID_HANDLE, "null audioEffectChainManager");
    std::string effectChainManagerDeviceType = audioEffectChainManager->GetDeviceTypeName();
    std::string effectChainManagerDeviceSink = audioEffectChainManager->GetDeviceSinkName();
    if ((effectChainManagerDeviceType == "DEVICE_TYPE_BLUETOOTH_A2DP") &&
        (effectChainManagerDeviceSink == "Speaker")) {
        return true;
    }
    return false;
}

int32_t EffectChainManagerAddSessionInfo(const char *sceneType, const char *sessionID, SessionInfoPack pack)
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, ERR_INVALID_HANDLE, "null audioEffectChainManager");

    uint64_t channelLayoutNum = 0;
    std::string sceneTypeString = "";
    std::string sessionIDString = "";
    std::string sceneModeString = "";
    std::string spatializationEnabledString = "";
    if (sceneType && pack.channelLayout && sessionID && pack.sceneMode && pack.spatializationEnabled) {
        sceneTypeString = sceneType;
        channelLayoutNum = std::strtoull(pack.channelLayout, nullptr, BASE_TEN);
        sessionIDString = sessionID;
        sceneModeString = pack.sceneMode;
        spatializationEnabledString = pack.spatializationEnabled;
    } else {
        AUDIO_ERR_LOG("map input parameters missing.");
        return ERROR;
    }

    sessionEffectInfo info;
    info.sceneMode = sceneModeString;
    info.channels = pack.channels;
    info.channelLayout = channelLayoutNum;
    info.spatializationEnabled = spatializationEnabledString;
    return audioEffectChainManager->SessionInfoMapAdd(sceneTypeString, sessionIDString, info);
}

int32_t EffectChainManagerDeleteSessionInfo(const char *sceneType, const char *sessionID)
{
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectChainManager != nullptr, ERR_INVALID_HANDLE, "null audioEffectChainManager");

    std::string sceneTypeString = "";
    std::string sessionIDString = "";
    if (sceneType && sessionID) {
        sceneTypeString = sceneType;
        sessionIDString = sessionID;
    } else {
        AUDIO_ERR_LOG("map unlink parameters missing.");
        return ERROR;
    }

    return audioEffectChainManager->SessionInfoMapDelete(sceneTypeString, sessionIDString);
}

int32_t EffectChainManagerReturnEffectChannelInfo(const char *sceneType, uint32_t *channels, uint64_t *channelLayout)
{
    if (sceneType == nullptr || channels == nullptr || channelLayout == nullptr) {
        return ERROR;
    }
    std::string sceneTypeString = sceneType;
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    return audioEffectChainManager->ReturnEffectChannelInfo(sceneTypeString, channels, channelLayout);
}

int32_t EffectChainManagerReturnMultiChannelInfo(uint32_t *channels, uint64_t *channelLayout)
{
    if (channels == nullptr || channelLayout == nullptr) {
        return ERROR;
    }
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    return audioEffectChainManager->ReturnMultiChannelInfo(channels, channelLayout);
}

namespace OHOS {
namespace AudioStandard {

#ifdef SENSOR_ENABLE
AudioEffectChain::AudioEffectChain(std::string scene, std::shared_ptr<HeadTracker> headTracker)
{
    sceneType = scene;
    effectMode = AUDIO_SUPPORTED_SCENE_MODES.find(EFFECT_DEFAULT)->second;
    audioBufIn.frameLength = 0;
    audioBufOut.frameLength = 0;
    ioBufferConfig.inputCfg.samplingRate = DEFAULT_SAMPLE_RATE;
    ioBufferConfig.inputCfg.channels = DEFAULT_NUM_CHANNEL;
    ioBufferConfig.inputCfg.format = DATA_FORMAT_F32;
    ioBufferConfig.inputCfg.channelLayout = DEFAULT_NUM_CHANNELLAYOUT;
    ioBufferConfig.outputCfg.samplingRate = DEFAULT_SAMPLE_RATE;
    ioBufferConfig.outputCfg.channels = DEFAULT_NUM_CHANNEL;
    ioBufferConfig.outputCfg.format = DATA_FORMAT_F32;
    ioBufferConfig.outputCfg.channelLayout = DEFAULT_NUM_CHANNELLAYOUT;
    headTracker_ = headTracker;
}
#else
AudioEffectChain::AudioEffectChain(std::string scene)
{
    sceneType = scene;
    effectMode = AUDIO_SUPPORTED_SCENE_MODES.find(EFFECT_DEFAULT)->second;
    audioBufIn.frameLength = 0;
    audioBufOut.frameLength = 0;
    ioBufferConfig.inputCfg.samplingRate = DEFAULT_SAMPLE_RATE;
    ioBufferConfig.inputCfg.channels = DEFAULT_NUM_CHANNEL;
    ioBufferConfig.inputCfg.format = DATA_FORMAT_F32;
    ioBufferConfig.inputCfg.channelLayout = DEFAULT_NUM_CHANNELLAYOUT;
    ioBufferConfig.outputCfg.samplingRate = DEFAULT_SAMPLE_RATE;
    ioBufferConfig.outputCfg.channels = DEFAULT_NUM_CHANNEL;
    ioBufferConfig.outputCfg.format = DATA_FORMAT_F32;
    ioBufferConfig.outputCfg.channelLayout = DEFAULT_NUM_CHANNELLAYOUT;
}
#endif

AudioEffectChain::~AudioEffectChain()
{
    std::lock_guard<std::mutex> lock(reloadMutex);
    ReleaseEffectChain();
}

void AudioEffectChain::Dump()
{
    std::lock_guard<std::mutex> lock(reloadMutex);
    for (AudioEffectHandle handle: standByEffectHandles) {
        AUDIO_INFO_LOG("standByEffectHandle for [%{public}s], handle address is %{public}p", sceneType.c_str(),
            handle);
    }
}

std::string AudioEffectChain::GetEffectMode()
{
    return effectMode;
}

void AudioEffectChain::SetEffectMode(std::string mode)
{
    effectMode = mode;
}

template <typename T>
int32_t GetKeyFromValue(const std::unordered_map<T, std::string> &map, const std::string &value)
{
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (it->second == value) {
            return it->first;
        }
    }
    return -1;
}

void AudioEffectChain::ReleaseEffectChain()
{
    for (uint32_t i = 0; i < standByEffectHandles.size() && i < libHandles.size(); ++i) {
        if (!libHandles[i]) {
            continue;
        }
        if (!standByEffectHandles[i]) {
            continue;
        }
        if (!libHandles[i]->releaseEffect) {
            continue;
        }
        libHandles[i]->releaseEffect(standByEffectHandles[i]);
    }
    standByEffectHandles.clear();
    libHandles.clear();
}

void AudioEffectChain::AddEffectHandleBegin()
{
    reloadMutex.lock();
    ReleaseEffectChain();
}

void AudioEffectChain::AddEffectHandle(AudioEffectHandle handle, AudioEffectLibrary *libHandle)
{
    int32_t ret;
    int32_t replyData = 0;
    AudioEffectTransInfo cmdInfo = {sizeof(AudioEffectConfig), &ioBufferConfig};
    AudioEffectTransInfo replyInfo = {sizeof(int32_t), &replyData};
    ret = (*handle)->command(handle, EFFECT_CMD_INIT, &cmdInfo, &replyInfo);
    CHECK_AND_RETURN_LOG(ret == 0, "[%{public}s] with mode [%{public}s], either one of libs EFFECT_CMD_INIT fail",
        sceneType.c_str(), effectMode.c_str());
    ret = (*handle)->command(handle, EFFECT_CMD_ENABLE, &cmdInfo, &replyInfo);
    CHECK_AND_RETURN_LOG(ret == 0, "[%{public}s] with mode [%{public}s], either one of libs EFFECT_CMD_ENABLE fail",
        sceneType.c_str(), effectMode.c_str());
    ret = (*handle)->command(handle, EFFECT_CMD_SET_CONFIG, &cmdInfo, &replyInfo);
    CHECK_AND_RETURN_LOG(ret == 0, "[%{public}s] with mode [%{public}s], either one of libs EFFECT_CMD_SET_CONFIG fail",
        sceneType.c_str(), effectMode.c_str());
    // Set param
    AudioEffectParam *effectParam = new AudioEffectParam[sizeof(AudioEffectParam) +
        NUM_SET_EFFECT_PARAM * sizeof(int32_t)];
    effectParam->status = 0;
    effectParam->paramSize = sizeof(int32_t);
    effectParam->valueSize = 0;
    int32_t *data = &(effectParam->data[0]);
    *data++ = EFFECT_SET_PARAM;
    *data++ = GetKeyFromValue(AUDIO_SUPPORTED_SCENE_TYPES, sceneType);
    *data++ = GetKeyFromValue(AUDIO_SUPPORTED_SCENE_MODES, effectMode);
    cmdInfo = {sizeof(AudioEffectParam) + sizeof(int32_t) * NUM_SET_EFFECT_PARAM, effectParam};
    ret = (*handle)->command(handle, EFFECT_CMD_SET_PARAM, &cmdInfo, &replyInfo);
    delete[] effectParam;
    CHECK_AND_RETURN_LOG(ret == 0, "[%{public}s] with mode [%{public}s], either one of libs EFFECT_CMD_SET_PARAM fail",
        sceneType.c_str(), effectMode.c_str());
    standByEffectHandles.emplace_back(handle);
    libHandles.emplace_back(libHandle);
}

void AudioEffectChain::AddEffectHandleEnd()
{
    reloadMutex.unlock();
}

void AudioEffectChain::SetEffectChain(std::vector<AudioEffectHandle> &effHandles,
    std::vector<AudioEffectLibrary *> &libHandles)
{
    std::lock_guard<std::mutex> lock(reloadMutex);
    AddEffectHandleBegin();
    for (uint32_t i = 0; i < effHandles.size(); i++) {
        AddEffectHandle(effHandles[i], libHandles[i]);
    }
}


void CopyBuffer(const float *bufIn, float *bufOut, uint32_t totalLen)
{
    for (uint32_t i = 0; i < totalLen; ++i) {
        bufOut[i] = bufIn[i];
    }
}

void AudioEffectChain::ApplyEffectChain(float *bufIn, float *bufOut, uint32_t frameLen,
    AudioEffectProcInfo procInfo)
{
    if (IsEmptyEffectHandles()) {
        CopyBuffer(bufIn, bufOut, frameLen * ioBufferConfig.outputCfg.channels);
        return;
    }

#ifdef SENSOR_ENABLE
    int32_t replyData = 0;
    auto imuData = headTracker_->GetHeadPostureData();
    AudioEffectTransInfo cmdInfo = {sizeof(HeadPostureData), &imuData};
    AudioEffectTransInfo replyInfo = {sizeof(int32_t), &replyData};
#endif

    audioBufIn.frameLength = frameLen;
    audioBufOut.frameLength = frameLen;
    int32_t count = 0;
    {
        std::lock_guard<std::mutex> lock(reloadMutex);
        for (AudioEffectHandle handle: standByEffectHandles) {
#ifdef SENSOR_ENABLE
            if ((!procInfo.offloadEnabled) && procInfo.headTrackingEnabled) {
            (*handle)->command(handle, EFFECT_CMD_SET_IMU, &cmdInfo, &replyInfo);
            }
#endif
            if (count % FACTOR_TWO == 0) {
                audioBufIn.raw = bufIn;
                audioBufOut.raw = bufOut;
            } else {
                audioBufOut.raw = bufIn;
                audioBufIn.raw = bufOut;
            }
            int32_t ret = (*handle)->process(handle, &audioBufIn, &audioBufOut);
            CHECK_AND_CONTINUE_LOG(ret == 0, "[%{public}s] with mode [%{public}s], either one of libs process fail",
                sceneType.c_str(), effectMode.c_str());
            count++;
        }
    }

    if (count % FACTOR_TWO == 0) {
        CopyBuffer(bufIn, bufOut, frameLen * ioBufferConfig.outputCfg.channels);
    }
}

void AudioEffectChain::SetIOBufferConfig(bool isInput, uint32_t samplingRate, uint32_t channels)
{
    if (isInput) {
        ioBufferConfig.inputCfg.samplingRate = samplingRate;
        ioBufferConfig.inputCfg.channels = channels;
    } else {
        ioBufferConfig.outputCfg.samplingRate = samplingRate;
        ioBufferConfig.outputCfg.channels = channels;
    }
}

bool AudioEffectChain::IsEmptyEffectHandles()
{
    std::lock_guard<std::mutex> lock(reloadMutex);
    return standByEffectHandles.size() == 0;
}

int32_t AudioEffectChain::UpdateMultichannelIoBufferConfig(const uint32_t &channels, const uint64_t &channelLayout)
{
    std::lock_guard<std::mutex> lock(reloadMutex);
    if (ioBufferConfig.inputCfg.channels == channels && ioBufferConfig.inputCfg.channelLayout == channelLayout) {
        return SUCCESS;
    }
    ioBufferConfig.inputCfg.channels = channels;
    ioBufferConfig.inputCfg.channelLayout = channelLayout;
    int32_t replyData = 0;
    AudioEffectTransInfo cmdInfo = {sizeof(AudioEffectConfig), &ioBufferConfig};
    AudioEffectTransInfo replyInfo = {sizeof(int32_t), &replyData};
    for (AudioEffectHandle handle: standByEffectHandles) {
        int32_t ret = (*handle)->command(handle, EFFECT_CMD_SET_CONFIG, &cmdInfo, &replyInfo);
        CHECK_AND_RETURN_RET_LOG(ret == 0, ERROR, "Multichannel effect chain update EFFECT_CMD_SET_CONFIG fail");
    }
    return SUCCESS;
}

AudioEffectConfig AudioEffectChain::GetIoBufferConfig()
{
    return ioBufferConfig;
}

void AudioEffectChain::StoreOldEffectChainInfo(std::string &sceneMode, AudioEffectConfig &ioBufferConfig)
{
    sceneMode = GetEffectMode();
    ioBufferConfig = GetIoBufferConfig();
    return;
}

#ifdef SENSOR_ENABLE
void AudioEffectChain::SetHeadTrackingDisabled()
{
    if (IsEmptyEffectHandles()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(reloadMutex);
        for (AudioEffectHandle handle: standByEffectHandles) {
            int32_t replyData = 0;
            HeadPostureData imuDataDisabled = {1, 1.0, 0.0, 0.0, 0.0};
            AudioEffectTransInfo cmdInfo = {sizeof(HeadPostureData), &imuDataDisabled};
            AudioEffectTransInfo replyInfo = {sizeof(int32_t), &replyData};
            int32_t ret = (*handle)->command(handle, EFFECT_CMD_SET_IMU, &cmdInfo, &replyInfo);
            if (ret != 0) {
                AUDIO_WARNING_LOG("SetHeadTrackingDisabled failed");
            }
        }
    }
}
#endif

int32_t FindEffectLib(const std::string &effect,
    const std::vector<std::unique_ptr<AudioEffectLibEntry>> &effectLibraryList,
    AudioEffectLibEntry **libEntry, std::string &libName)
{
    for (const std::unique_ptr<AudioEffectLibEntry> &lib : effectLibraryList) {
        if (lib->libraryName == effect) {
            libName = lib->libraryName;
            *libEntry = lib.get();
            return SUCCESS;
        }
    }
    return ERROR;
}

void AudioEffectChain::InitEffectChain()
{
    if (IsEmptyEffectHandles()) {
        return;
    }
    std::lock_guard<std::mutex> lock(reloadMutex);
    for (AudioEffectHandle handle: standByEffectHandles) {
        int32_t replyData = 0;
        AudioEffectTransInfo cmdInfo = {sizeof(int32_t), &replyData};
        AudioEffectTransInfo replyInfo = {sizeof(int32_t), &replyData};
        int32_t ret = (*handle)->command(handle, EFFECT_CMD_ENABLE, &cmdInfo, &replyInfo);
        CHECK_AND_RETURN_LOG(ret == 0, "[%{public}s] with mode [%{public}s], either one of libs EFFECT_CMD_ENABLE fail",
            sceneType.c_str(), effectMode.c_str());
    }
}

int32_t CheckValidEffectLibEntry(AudioEffectLibEntry *libEntry, const std::string &effect, const std::string &libName)
{
    CHECK_AND_RETURN_RET_LOG(libEntry, ERROR, "Effect [%{public}s] in lib [%{public}s] is nullptr",
        effect.c_str(), libName.c_str());

    CHECK_AND_RETURN_RET_LOG(libEntry->audioEffectLibHandle, ERROR,
        "AudioEffectLibHandle of Effect [%{public}s] in lib [%{public}s] is nullptr",
        effect.c_str(), libName.c_str());

    CHECK_AND_RETURN_RET_LOG(libEntry->audioEffectLibHandle->createEffect, ERROR,
        "CreateEffect function of Effect [%{public}s] in lib [%{public}s] is nullptr",
        effect.c_str(), libName.c_str());

    CHECK_AND_RETURN_RET_LOG(libEntry->audioEffectLibHandle->releaseEffect, ERROR,
        "ReleaseEffect function of Effect [%{public}s] in lib [%{public}s] is nullptr",
        effect.c_str(), libName.c_str());
    return SUCCESS;
}

AudioEffectChainManager::AudioEffectChainManager()
{
    EffectToLibraryEntryMap_.clear();
    EffectToLibraryNameMap_.clear();
    EffectChainToEffectsMap_.clear();
    SceneTypeAndModeToEffectChainNameMap_.clear();
    SceneTypeToEffectChainMap_.clear();
    SceneTypeToEffectChainCountMap_.clear();
    SessionIDSet_.clear();
    SceneTypeToSessionIDMap_.clear();
    SessionIDToEffectInfoMap_.clear();
    SceneTypeToEffectChainCountBackupMap_.clear();
    frameLen_ = DEFAULT_FRAMELEN;
    deviceType_ = DEVICE_TYPE_SPEAKER;
    deviceSink_ = DEFAULT_DEVICE_SINK;
    isInitialized_ = false;

#ifdef SENSOR_ENABLE
    headTracker_ = std::make_shared<HeadTracker>();
#endif

    audioEffectHdi_ = std::make_shared<AudioEffectHdi>();
    memset_s(static_cast<void *>(effectHdiInput), sizeof(effectHdiInput), 0, sizeof(effectHdiInput));
}

AudioEffectChainManager::~AudioEffectChainManager()
{
    for (auto effChain = SceneTypeToEffectChainMap_.begin(); effChain != SceneTypeToEffectChainMap_.end(); ++effChain) {
        effChain->second->ReleaseEffectChain();
    }
}

AudioEffectChainManager *AudioEffectChainManager::GetInstance()
{
    static AudioEffectChainManager audioEffectChainManager;
    return &audioEffectChainManager;
}

static int32_t UpdateDeviceInfo(DeviceType &deviceType, std::string &deviceSink, const bool isInitialized,
    int32_t device, std::string &sinkName)
{
    if (!isInitialized) {
        deviceType = (DeviceType)device;
        deviceSink = sinkName;
        AUDIO_INFO_LOG("has not beed initialized");
        return ERROR;
    }

    if (deviceSink == sinkName) {
        AUDIO_INFO_LOG("Same DeviceSinkName");
    }
    deviceSink = sinkName;

    if (deviceType == (DeviceType)device) {
        AUDIO_INFO_LOG("DeviceType do not need to be Updated");
        return ERROR;
    }

    deviceType = (DeviceType)device;
    return SUCCESS;
}

int32_t AudioEffectChainManager::SetOutputDeviceSink(int32_t device, std::string &sinkName)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (UpdateDeviceInfo(deviceType_, deviceSink_, isInitialized_, device, sinkName) != SUCCESS) {
        return SUCCESS;
    }

    std::vector<std::string> keys;
    for (auto it = SceneTypeToEffectChainMap_.begin(); it != SceneTypeToEffectChainMap_.end(); ++it) {
        keys.push_back(it->first);
    }
    std::string deviceName = GetDeviceTypeName();
    for (auto key: keys) {
        std::string sceneType = key.substr(0, static_cast<size_t>(key.find("_&_")));
        std::string sceneTypeAndDeviceKey = sceneType + "_&_" + deviceName;
        if (SceneTypeToEffectChainCountMap_.count(sceneTypeAndDeviceKey)) {
            SceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey] = SceneTypeToEffectChainCountMap_[key];
        } else {
            SceneTypeToEffectChainCountMap_.insert(
                std::make_pair(sceneTypeAndDeviceKey, SceneTypeToEffectChainCountMap_[key]));
        }
        SceneTypeToEffectChainCountMap_.erase(key);

        auto *audioEffectChain = SceneTypeToEffectChainMap_[key];
        std::string sceneMode;
        AudioEffectConfig ioBufferConfig;
        if (audioEffectChain != nullptr) {
            audioEffectChain->StoreOldEffectChainInfo(sceneMode, ioBufferConfig);
            delete audioEffectChain;
        } else {
            sceneMode = AUDIO_SUPPORTED_SCENE_MODES.find(EFFECT_DEFAULT)->second;
        }
        SceneTypeToEffectChainMap_.erase(key);

#ifdef SENSOR_ENABLE
        audioEffectChain = new AudioEffectChain(sceneType, headTracker_);
#else
        audioEffectChain = new AudioEffectChain(sceneType);
#endif

        if (SceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
            SceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] = audioEffectChain;
        } else {
            SceneTypeToEffectChainMap_.insert(std::make_pair(sceneTypeAndDeviceKey, audioEffectChain));
        }

        if (SetAudioEffectChainDynamic(sceneType, sceneMode) != SUCCESS) {
            AUDIO_ERR_LOG("Fail to set effect chain for [%{public}s]", sceneType.c_str());
        }
        audioEffectChain->UpdateMultichannelIoBufferConfig(ioBufferConfig.inputCfg.channels,
            ioBufferConfig.inputCfg.channelLayout);
    }
    return SUCCESS;
}

std::string AudioEffectChainManager::GetDeviceTypeName()
{
    std::string name = "";
    auto device = SUPPORTED_DEVICE_TYPE.find(deviceType_);
    if (device != SUPPORTED_DEVICE_TYPE.end()) {
        name = device->second;
    }
    return name;
}

std::string AudioEffectChainManager::GetDeviceSinkName()
{
    return deviceSink_;
}

int32_t AudioEffectChainManager::SetFrameLen(int32_t frameLength)
{
    frameLen_ = frameLength;
    return SUCCESS;
}

int32_t AudioEffectChainManager::GetFrameLen()
{
    return frameLen_;
}

bool AudioEffectChainManager::GetOffloadEnabled()
{
    return offloadEnabled_;
}

// Boot initialize
void AudioEffectChainManager::InitAudioEffectChainManager(std::vector<EffectChain> &effectChains,
    std::unordered_map<std::string, std::string> &map,
    std::vector<std::unique_ptr<AudioEffectLibEntry>> &effectLibraryList)
{
    std::set<std::string> effectSet;
    for (EffectChain efc: effectChains) {
        for (std::string effect: efc.apply) {
            effectSet.insert(effect);
        }
    }

    // Construct EffectToLibraryEntryMap that stores libEntry for each effect name
    AudioEffectLibEntry *libEntry = nullptr;
    std::string libName;
    for (std::string effect: effectSet) {
        int32_t ret = FindEffectLib(effect, effectLibraryList, &libEntry, libName);
        CHECK_AND_CONTINUE_LOG(ret != ERROR, "Couldn't find libEntry of effect %{public}s", effect.c_str());
        ret = CheckValidEffectLibEntry(libEntry, effect, libName);
        CHECK_AND_CONTINUE_LOG(ret != ERROR, "Invalid libEntry of effect %{public}s", effect.c_str());
        EffectToLibraryEntryMap_[effect] = libEntry;
        EffectToLibraryNameMap_[effect] = libName;
    }
    // Construct EffectChainToEffectsMap that stores all effect names of each effect chain
    for (EffectChain efc: effectChains) {
        std::string key = efc.name;
        std::vector <std::string> effects;
        for (std::string effectName: efc.apply) {
            effects.emplace_back(effectName);
        }
        EffectChainToEffectsMap_[key] = effects;
    }

    // Constrcut SceneTypeAndModeToEffectChainNameMap that stores effectMode associated with the effectChainName
    for (auto item = map.begin(); item != map.end(); ++item) {
        SceneTypeAndModeToEffectChainNameMap_[item->first] = item->second;
    }

    AUDIO_INFO_LOG("EffectToLibraryEntryMap size %{public}zu", EffectToLibraryEntryMap_.size());
    AUDIO_DEBUG_LOG("EffectChainToEffectsMap size %{public}zu", EffectChainToEffectsMap_.size());
    AUDIO_DEBUG_LOG("SceneTypeAndModeToEffectChainNameMap size %{public}zu",
        SceneTypeAndModeToEffectChainNameMap_.size());

    audioEffectHdi_->InitHdi();

    isInitialized_ = true;
}

bool AudioEffectChainManager::CheckAndAddSessionID(std::string sessionID)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (SessionIDSet_.count(sessionID)) {
        return false;
    }
    SessionIDSet_.insert(sessionID);
    return true;
}

int32_t AudioEffectChainManager::CreateAudioEffectChainDynamic(std::string sceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    CHECK_AND_RETURN_RET_LOG(isInitialized_, ERROR, "has not been initialized");
    CHECK_AND_RETURN_RET_LOG(sceneType != "", ERROR, "null sceneType");

    AudioEffectChain *audioEffectChain;
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    if (SceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        if (!SceneTypeToEffectChainCountMap_.count(sceneTypeAndDeviceKey) ||
            SceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey] < 1) {
            audioEffectChain = SceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
            if (audioEffectChain != nullptr) {
                delete audioEffectChain;
            }
            SceneTypeToEffectChainCountMap_.erase(sceneTypeAndDeviceKey);
            SceneTypeToEffectChainMap_.erase(sceneTypeAndDeviceKey);
            return ERROR;
        }
        SceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey]++;
        return SUCCESS;
    } else {
#ifdef SENSOR_ENABLE
        audioEffectChain = new AudioEffectChain(sceneType, headTracker_);
#else
        audioEffectChain = new AudioEffectChain(sceneType);
#endif

        SceneTypeToEffectChainMap_.insert(std::make_pair(sceneTypeAndDeviceKey, audioEffectChain));
        if (!SceneTypeToEffectChainCountMap_.count(sceneTypeAndDeviceKey)) {
            SceneTypeToEffectChainCountMap_.insert(std::make_pair(sceneTypeAndDeviceKey, 1));
        } else {
            SceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey] = 1;
        }
    }
    if (!AUDIO_SUPPORTED_SCENE_MODES.count(EFFECT_DEFAULT)) {
        return ERROR;
    }
    std::string effectMode = AUDIO_SUPPORTED_SCENE_MODES.find(EFFECT_DEFAULT)->second;
    if (SetAudioEffectChainDynamic(sceneType, effectMode) != SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::SetAudioEffectChainDynamic(std::string sceneType, std::string effectMode)
{
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    CHECK_AND_RETURN_RET_LOG(SceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey), ERROR,
        "SceneType [%{public}s] does not exist, failed to set", sceneType.c_str());

    AudioEffectChain *audioEffectChain = SceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];

    std::string effectChain;
    std::string effectChainKey = sceneType + "_&_" + effectMode + "_&_" + GetDeviceTypeName();
    std::string effectNone = AUDIO_SUPPORTED_SCENE_MODES.find(EFFECT_NONE)->second;
    if (!SceneTypeAndModeToEffectChainNameMap_.count(effectChainKey)) {
        AUDIO_ERR_LOG("EffectChain key [%{public}s] does not exist, auto set to %{public}s",
            effectChainKey.c_str(), effectNone.c_str());
        effectChain = effectNone;
    } else {
        effectChain = SceneTypeAndModeToEffectChainNameMap_[effectChainKey];
    }

    if (effectChain != effectNone && !EffectChainToEffectsMap_.count(effectChain)) {
        AUDIO_ERR_LOG("EffectChain name [%{public}s] does not exist, auto set to %{public}s",
            effectChain.c_str(), effectNone.c_str());
        effectChain = effectNone;
    }

    audioEffectChain->SetEffectMode(effectMode);
    for (std::string effect: EffectChainToEffectsMap_[effectChain]) {
        AudioEffectHandle handle = nullptr;
        AudioEffectDescriptor descriptor;
        descriptor.libraryName = EffectToLibraryNameMap_[effect];
        descriptor.effectName = effect;
        int32_t ret = EffectToLibraryEntryMap_[effect]->audioEffectLibHandle->createEffect(descriptor, &handle);
        CHECK_AND_CONTINUE_LOG(ret == 0, "EffectToLibraryEntryMap[%{public}s] createEffect fail", effect.c_str());
        audioEffectChain->AddEffectHandle(handle, EffectToLibraryEntryMap_[effect]->audioEffectLibHandle);
    }

    if (audioEffectChain->IsEmptyEffectHandles()) {
        AUDIO_ERR_LOG("Effectchain is empty, copy bufIn to bufOut like EFFECT_NONE mode");
    }

    return SUCCESS;
}

bool AudioEffectChainManager::CheckAndRemoveSessionID(std::string sessionID)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (!SessionIDSet_.count(sessionID)) {
        return false;
    }
    SessionIDSet_.erase(sessionID);
    return true;
}

int32_t AudioEffectChainManager::ReleaseAudioEffectChainDynamic(std::string sceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    CHECK_AND_RETURN_RET_LOG(isInitialized_, ERROR, "has not been initialized");
    CHECK_AND_RETURN_RET_LOG(sceneType != "", ERROR, "null sceneType");

    AudioEffectChain *audioEffectChain;
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    if (!SceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        SceneTypeToEffectChainCountMap_.erase(sceneTypeAndDeviceKey);
        return SUCCESS;
    } else if (SceneTypeToEffectChainCountMap_.count(sceneTypeAndDeviceKey) &&
        SceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey] > 1) {
        SceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey]--;
        return SUCCESS;
    } else {
        audioEffectChain = SceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
    }
    if (audioEffectChain != nullptr) {
        delete audioEffectChain;
    }

    SceneTypeToEffectChainCountMap_.erase(sceneTypeAndDeviceKey);
    SceneTypeToEffectChainMap_.erase(sceneTypeAndDeviceKey);
    return SUCCESS;
}

bool AudioEffectChainManager::ExistAudioEffectChain(std::string sceneType, std::string effectMode,
    std::string spatializationEnabled)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (!isInitialized_) {
        if (initializedLogFlag_) {
            AUDIO_ERR_LOG("audioEffectChainManager has not been initialized");
            initializedLogFlag_ = false;
        }
        return false;
    }
    initializedLogFlag_ = true;
    CHECK_AND_RETURN_RET_LOG(sceneType != "", false, "null sceneType");
    CHECK_AND_RETURN_RET_LOG(GetDeviceTypeName() != "", false, "null deviceType");

#ifndef DEVICE_FLAG
    if (deviceType_ != DEVICE_TYPE_SPEAKER) {
        return false;
    }
#endif
    if (offloadEnabled_) {
        return false;
    }

    if ((spatializationEnabled == "0") && (GetDeviceTypeName() == "DEVICE_TYPE_BLUETOOTH_A2DP")) {
        return false;
    }
    std::string effectChainKey = sceneType + "_&_" + effectMode + "_&_" + GetDeviceTypeName();
    if (!SceneTypeAndModeToEffectChainNameMap_.count(effectChainKey)) {
        return false;
    }
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    // if the effectChain exist, see if it is empty
    if (!SceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey) ||
        SceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] == nullptr) {
        return false;
    }
    auto *audioEffectChain = SceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
    CHECK_AND_RETURN_RET_LOG(audioEffectChain != nullptr, false, "null SceneTypeToEffectChainMap_[%{public}s]",
        sceneTypeAndDeviceKey.c_str());
    return !audioEffectChain->IsEmptyEffectHandles();
}

int32_t AudioEffectChainManager::ApplyAudioEffectChain(std::string sceneType, BufferAttr *bufferAttr)
{
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
#ifdef DEVICE_FLAG
    if (!SceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        CopyBuffer(bufferAttr->bufIn, bufferAttr->bufOut, bufferAttr->frameLen * bufferAttr->numChanIn);
        return ERROR;
    }
#else
    if (deviceType_ != DEVICE_TYPE_SPEAKER || !SceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        CopyBuffer(bufferAttr->bufIn, bufferAttr->bufOut, bufferAttr->frameLen * bufferAttr->numChanIn);
        return SUCCESS;
    }
#endif

    auto *audioEffectChain = SceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
    AudioEffectProcInfo procInfo = {headTrackingEnabled_, offloadEnabled_};
    audioEffectChain->ApplyEffectChain(bufferAttr->bufIn, bufferAttr->bufOut, bufferAttr->frameLen, procInfo);

    return SUCCESS;
}

void AudioEffectChainManager::Dump()
{
    AUDIO_INFO_LOG("Dump START");
    for (auto item = SceneTypeToEffectChainMap_.begin(); item != SceneTypeToEffectChainMap_.end(); ++item) {
        AudioEffectChain *audioEffectChain = item->second;
        audioEffectChain->Dump();
    }
}

int32_t AudioEffectChainManager::UpdateMultichannelConfig(const std::string &sceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    if (!SceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        return ERROR;
    }
    uint32_t inputChannels = DEFAULT_NUM_CHANNEL;
    uint64_t inputChannelLayout = DEFAULT_NUM_CHANNELLAYOUT;
    ReturnEffectChannelInfo(sceneType, &inputChannels, &inputChannelLayout);

    auto *audioEffectChain = SceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
    if (audioEffectChain == nullptr) {
        return ERROR;
    }
    audioEffectChain->UpdateMultichannelIoBufferConfig(inputChannels, inputChannelLayout);
    return SUCCESS;
}

int32_t AudioEffectChainManager::InitAudioEffectChainDynamic(std::string sceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    CHECK_AND_RETURN_RET_LOG(isInitialized_, ERROR, "has not been initialized");
    CHECK_AND_RETURN_RET_LOG(sceneType != "", ERROR, "null sceneType");

    AudioEffectChain *audioEffectChain = nullptr;
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    if (!SceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        return SUCCESS;
    } else {
        audioEffectChain = SceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
    }
    if (audioEffectChain != nullptr) {
        audioEffectChain->InitEffectChain();
    }

    return SUCCESS;
}

int32_t AudioEffectChainManager::UpdateSpatializationState(AudioSpatializationState spatializationState)
{
    AUDIO_INFO_LOG("UpdateSpatializationState entered, current state: %{public}d and %{public}d, previous state: \
        %{public}d and %{public}d", spatializationState.spatializationEnabled, spatializationState.headTrackingEnabled,
        spatializationEnabled_, headTrackingEnabled_);
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (spatializationEnabled_ != spatializationState.spatializationEnabled) {
        spatializationEnabled_ = spatializationState.spatializationEnabled;
        memset_s(static_cast<void *>(effectHdiInput), sizeof(effectHdiInput), 0, sizeof(effectHdiInput));
        if (spatializationEnabled_) {
            effectHdiInput[0] = HDI_INIT;
            int32_t ret = audioEffectHdi_->UpdateHdiState(effectHdiInput);
            if (ret != 0) {
                AUDIO_ERR_LOG("set hdi init failed, backup spatialization entered");
                offloadEnabled_ = false;
            } else {
                AUDIO_INFO_LOG("set hdi init succeeded, normal spatialization entered");
                offloadEnabled_ = true;
                DeleteAllChains();
            }
        } else {
            effectHdiInput[0] = HDI_DESTROY;
            AUDIO_INFO_LOG("set hdi destroy.");
            int32_t ret = audioEffectHdi_->UpdateHdiState(effectHdiInput);
            if (ret != 0) {
                AUDIO_ERR_LOG("set hdi destroy failed");
            }
            offloadEnabled_ = false;
            RecoverAllChains();
        }
    }
    if (headTrackingEnabled_ != spatializationState.headTrackingEnabled) {
        headTrackingEnabled_ = spatializationState.headTrackingEnabled;
        UpdateSensorState();
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::ReturnEffectChannelInfo(const std::string &sceneType, uint32_t *channels,
    uint64_t *channelLayout)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (!SceneTypeToSessionIDMap_.count(sceneType)) {
        return ERROR;
    }
    std::set<std::string> sessions = SceneTypeToSessionIDMap_[sceneType];
    for (auto s = sessions.begin(); s != sessions.end(); ++s) {
        sessionEffectInfo info = SessionIDToEffectInfoMap_[*s];
        uint32_t TmpChannelCount;
        uint64_t TmpChannelLayout;
        if (GetDeviceTypeName() != "DEVICE_TYPE_BLUETOOTH_A2DP"
            || !ExistAudioEffectChain(sceneType, info.sceneMode, info.spatializationEnabled)
            || !IsChannelLayoutHVSSupported(info.channelLayout)) {
            TmpChannelCount = DEFAULT_NUM_CHANNEL;
            TmpChannelLayout = DEFAULT_NUM_CHANNELLAYOUT;
        } else {
            TmpChannelLayout = info.channelLayout;
            TmpChannelCount = info.channels;
        }

        if (TmpChannelCount >= *channels) {
            *channels = TmpChannelCount;
            *channelLayout = TmpChannelLayout;
        }
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::ReturnMultiChannelInfo(uint32_t *channels, uint64_t *channelLayout)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    for (auto it = SceneTypeToSessionIDMap_.begin(); it != SceneTypeToSessionIDMap_.end(); it++) {
        std::set<std::string> sessions = SceneTypeToSessionIDMap_[it->first];
        for (auto s = sessions.begin(); s != sessions.end(); ++s) {
            sessionEffectInfo info = SessionIDToEffectInfoMap_[*s];
            uint32_t TmpChannelCount = DEFAULT_MCH_NUM_CHANNEL;
            uint64_t TmpChannelLayout = DEFAULT_MCH_NUM_CHANNELLAYOUT;
            if (info.channels > DEFAULT_NUM_CHANNEL &&
                !ExistAudioEffectChain(it->first, info.sceneMode, info.spatializationEnabled) &&
                IsChannelLayoutHVSSupported(info.channelLayout)) {
                TmpChannelLayout = info.channelLayout;
                TmpChannelCount = info.channels;
            }

            if (TmpChannelCount >= *channels) {
                *channels = TmpChannelCount;
                *channelLayout = TmpChannelLayout;
            }
        }
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::SessionInfoMapAdd(std::string sceneType, std::string sessionID, sessionEffectInfo info)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (!SessionIDToEffectInfoMap_.count(sessionID)) {
        SceneTypeToSessionIDMap_[sceneType].insert(sessionID);
        SessionIDToEffectInfoMap_[sessionID] = info;
    } else if (SessionIDToEffectInfoMap_[sessionID].sceneMode != info.sceneMode ||
        SessionIDToEffectInfoMap_[sessionID].spatializationEnabled != info.spatializationEnabled) {
        SessionIDToEffectInfoMap_[sessionID] = info;
    } else {
        return ERROR;
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::SessionInfoMapDelete(std::string sceneType, std::string sessionID)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (!SceneTypeToSessionIDMap_.count(sceneType)) {
        return ERROR;
    }
    if (SceneTypeToSessionIDMap_[sceneType].erase(sessionID)) {
        if (SceneTypeToSessionIDMap_[sceneType].empty()) {
            SceneTypeToSessionIDMap_.erase(sceneType);
        }
    } else {
        return ERROR;
    }
    if (!SessionIDToEffectInfoMap_.erase(sessionID)) {
        return ERROR;
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::SetHdiParam(std::string sceneType, std::string effectMode, bool enabled)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (!isInitialized_) {
        if (initializedLogFlag_) {
            AUDIO_ERR_LOG("audioEffectChainManager has not been initialized");
            initializedLogFlag_ = false;
        }
        return ERROR;
    }
    initializedLogFlag_ = true;
    CHECK_AND_RETURN_RET_LOG(sceneType != "", ERROR, "null sceneType");
    memset_s(static_cast<void *>(effectHdiInput), sizeof(effectHdiInput), 0, sizeof(effectHdiInput));
    effectHdiInput[0] = HDI_BYPASS;
    effectHdiInput[1] = enabled == true ? 0 : 1;
    AUDIO_INFO_LOG("set hdi bypass: %{public}d", effectHdiInput[1]);
    int32_t ret = audioEffectHdi_->UpdateHdiState(effectHdiInput);
    if (ret != 0) {
        AUDIO_WARNING_LOG("set hdi bypass failed");
        return ret;
    }

    effectHdiInput[0] = HDI_ROOM_MODE;
    effectHdiInput[1] = GetKeyFromValue(AUDIO_SUPPORTED_SCENE_TYPES, sceneType);
    effectHdiInput[HDI_ROOM_MODE_INDEX_TWO] = GetKeyFromValue(AUDIO_SUPPORTED_SCENE_MODES, effectMode);
    AUDIO_INFO_LOG("set hdi room mode sceneType: %{public}d, effectMode: %{public}d",
        effectHdiInput[1], effectHdiInput[HDI_ROOM_MODE_INDEX_TWO]);
    ret = audioEffectHdi_->UpdateHdiState(effectHdiInput);
    if (ret != 0) {
        AUDIO_WARNING_LOG("set hdi room mode failed");
        return ret;
    }
    return SUCCESS;
}

AudioEffectHdi::AudioEffectHdi()
{
    AUDIO_INFO_LOG("AudioEffectHdi constructor!");
    memset_s(static_cast<void *>(input), sizeof(input), 0, sizeof(input));
    memset_s(static_cast<void *>(output), sizeof(output), 0, sizeof(output));
    replyLen = GET_HDI_BUFFER_LEN;
}

AudioEffectHdi::~AudioEffectHdi()
{
    AUDIO_INFO_LOG("AudioEffectHdi destructor!");
}

void AudioEffectHdi::InitHdi()
{
    hdiModel_ = IEffectModelGet(false);
    if (hdiModel_ == nullptr) {
        AUDIO_WARNING_LOG("IEffectModelGet failed");
        hdiControl_ = nullptr;
        return;
    }
    libName = strdup("libspatialization_processing_dsp");
    effectId = strdup("aaaabbbb-8888-9999-6666-aabbccdd9966gg");
    EffectInfo info = {
        .libName = &libName[0],
        .effectId = &effectId[0],
        .ioDirection = 1,
    };
    ControllerId controllerId;
    int32_t ret = hdiModel_->CreateEffectController(hdiModel_, &info, &hdiControl_, &controllerId);
    if ((ret != 0) || (hdiControl_ == nullptr)) {
        AUDIO_WARNING_LOG("hdi init failed");
        hdiControl_ = nullptr;
        return;
    }

    uint32_t replyLen = GET_HDI_BUFFER_LEN;
    input[0] = HDI_BLUETOOTH_MODE;
    input[1] = 1;
    AUDIO_INFO_LOG("set hdi bluetooth mode.");
    ret = hdiControl_->SendCommand(hdiControl_, HDI_SET_PATAM, input, SEND_HDI_COMMAND_LEN, output, &replyLen);
    if (ret != 0) {
        AUDIO_WARNING_LOG("set hdi bluetooth mode failed");
        hdiControl_ = nullptr;
        return;
    }
}

int32_t AudioEffectHdi::UpdateHdiState(int8_t *effectHdiInput)
{
    if (hdiControl_ == nullptr) {
        AUDIO_WARNING_LOG("hdiControl_ is nullptr.");
        return ERROR;
    }
    memcpy_s(static_cast<void *>(input), sizeof(input), static_cast<void *>(effectHdiInput), sizeof(input));
    uint32_t replyLen = GET_HDI_BUFFER_LEN;
    int32_t ret = hdiControl_->SendCommand(hdiControl_, HDI_SET_PATAM, input, SEND_HDI_COMMAND_LEN, output, &replyLen);
    if (ret != 0) {
        AUDIO_WARNING_LOG("hdi send command failed");
        return ret;
    }
    return ret;
}

void AudioEffectChainManager::UpdateSensorState()
{
    effectHdiInput[0] = HDI_HEAD_MODE;
    effectHdiInput[1] = headTrackingEnabled_ == true ? 1 : 0;
    AUDIO_INFO_LOG("set hdi head mode: %{public}d", effectHdiInput[1]);
    int32_t ret = audioEffectHdi_->UpdateHdiState(effectHdiInput);
    if (ret != 0) {
        AUDIO_ERR_LOG("set hdi head mode failed");
    }

    if (headTrackingEnabled_) {
#ifdef SENSOR_ENABLE
        if (offloadEnabled_) {
            headTracker_->SensorInit();
            headTracker_->SensorSetConfig(DSP_SPATIALIZER_ENGINE);
        } else {
            headTracker_->SensorInit();
            headTracker_->SensorSetConfig(ARM_SPATIALIZER_ENGINE);
        }

        if (headTracker_->SensorActive() != 0) {
            AUDIO_ERR_LOG("SensorActive failed");
        }
#endif
        return;
    }

    if (offloadEnabled_) {
        return;
    }

#ifdef SENSOR_ENABLE
    if (headTracker_->SensorDeactive() != 0) {
        AUDIO_ERR_LOG("SensorDeactive failed");
    }
    headTracker_->SensorUnsubscribe();
    HeadPostureData headPostureData = {1, 1.0, 0.0, 0.0, 0.0}; // ori head posturedata
    headTracker_->SetHeadPostureData(headPostureData);
    for (auto it = SceneTypeToEffectChainMap_.begin(); it != SceneTypeToEffectChainMap_.end(); ++it) {
        auto *audioEffectChain = it->second;
        if (audioEffectChain == nullptr) {
            continue;
        }
        audioEffectChain->SetHeadTrackingDisabled();
    }
#endif
}

void AudioEffectChainManager::DeleteAllChains()
{
    SceneTypeToEffectChainCountBackupMap_.clear();
    for (auto it = SceneTypeToEffectChainCountMap_.begin(); it != SceneTypeToEffectChainCountMap_.end(); ++it) {
        SceneTypeToEffectChainCountBackupMap_.insert(std::make_pair(it->first, it->second));
    }
    
    for (auto it = SceneTypeToEffectChainCountBackupMap_.begin(); it != SceneTypeToEffectChainCountBackupMap_.end();
        ++it) {
        std::string sceneType = it->first.substr(0, static_cast<size_t>(it->first.find("_&_")));
        for (int32_t k = 0; k < it->second; ++k) {
            ReleaseAudioEffectChainDynamic(sceneType);
        }
    }
    return;
}

void AudioEffectChainManager::RecoverAllChains()
{
    for (auto it = SceneTypeToEffectChainCountBackupMap_.begin(); it != SceneTypeToEffectChainCountBackupMap_.end();
        ++it) {
        std::string sceneType = it->first.substr(0, static_cast<size_t>(it->first.find("_&_")));
        for (int32_t k = 0; k < it->second; ++k) {
            CreateAudioEffectChainDynamic(sceneType);
        }
    }
    SceneTypeToEffectChainCountBackupMap_.clear();
}

void AudioEffectChainManager::RegisterEffectChainCountBackupMap(std::string sceneType, std::string operation)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    if (operation == "Register") {
        if (!SceneTypeToEffectChainCountBackupMap_.count(sceneTypeAndDeviceKey)) {
            SceneTypeToEffectChainCountBackupMap_[sceneTypeAndDeviceKey] = 1;
            return;
        }
        SceneTypeToEffectChainCountBackupMap_[sceneTypeAndDeviceKey]++;
    } else if (operation == "Deregister") {
        if (SceneTypeToEffectChainCountBackupMap_.count(sceneTypeAndDeviceKey) == 1) {
            SceneTypeToEffectChainCountBackupMap_.erase(sceneTypeAndDeviceKey);
            return;
        }
        SceneTypeToEffectChainCountBackupMap_[sceneTypeAndDeviceKey]--;
    } else {
        AUDIO_ERR_LOG("Wrong operation to SceneTypeToEffectChainCountBackupMap.");
    }
    return;
}
}
}