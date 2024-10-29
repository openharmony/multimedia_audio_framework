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
#ifndef LOG_TAG
#define LOG_TAG "AudioEffectChainManager"
#endif

#include "audio_effect_chain_manager.h"
#include "audio_effect.h"
#include "audio_errors.h"
#include "audio_effect_log.h"
#include "audio_utils.h"
#include "securec.h"

#define DEVICE_FLAG

namespace OHOS {
namespace AudioStandard {
static int32_t CheckValidEffectLibEntry(const std::shared_ptr<AudioEffectLibEntry> &libEntry, const std::string &effect,
    const std::string &libName)
{
    CHECK_AND_RETURN_RET_LOG(libEntry != nullptr, ERROR, "Effect [%{public}s] in lib [%{public}s] is nullptr",
        effect.c_str(), libName.c_str());

    CHECK_AND_RETURN_RET_LOG(libEntry->audioEffectLibHandle, ERROR,
        "AudioEffectLibHandle of Effect [%{public}s] in lib [%{public}s] is nullptr", effect.c_str(), libName.c_str());

    CHECK_AND_RETURN_RET_LOG(libEntry->audioEffectLibHandle->createEffect, ERROR,
        "CreateEffect function of Effect [%{public}s] in lib [%{public}s] is nullptr", effect.c_str(), libName.c_str());

    CHECK_AND_RETURN_RET_LOG(libEntry->audioEffectLibHandle->releaseEffect, ERROR,
        "ReleaseEffect function of Effect [%{public}s] in lib [%{public}s] is nullptr", effect.c_str(),
        libName.c_str());
    return SUCCESS;
}

static int32_t FindEffectLib(const std::string &effect,
    const std::vector<std::shared_ptr<AudioEffectLibEntry>> &effectLibraryList,
    std::shared_ptr<AudioEffectLibEntry> &libEntry, std::string &libName)
{
    for (const std::shared_ptr<AudioEffectLibEntry> &lib : effectLibraryList) {
        if (lib->libraryName == effect) {
            libName = lib->libraryName;
            libEntry = lib;
            return SUCCESS;
        }
    }
    return ERROR;
}

static bool IsChannelLayoutSupported(const uint64_t channelLayout)
{
    return find(AUDIO_EFFECT_SUPPORTED_CHANNELLAYOUTS.begin(),
        AUDIO_EFFECT_SUPPORTED_CHANNELLAYOUTS.end(), channelLayout) != AUDIO_EFFECT_SUPPORTED_CHANNELLAYOUTS.end();
}

AudioEffectChainManager::AudioEffectChainManager()
{
    effectToLibraryEntryMap_.clear();
    effectToLibraryNameMap_.clear();
    effectChainToEffectsMap_.clear();
    sceneTypeAndModeToEffectChainNameMap_.clear();
    sceneTypeToEffectChainMap_.clear();
    sceneTypeToEffectChainCountMap_.clear();
    sessionIDSet_.clear();
    sceneTypeToSessionIDMap_.clear();
    sessionIDToEffectInfoMap_.clear();
    deviceType_ = DEVICE_TYPE_SPEAKER;
    deviceSink_ = DEFAULT_DEVICE_SINK;
    spatialDeviceType_ = EARPHONE_TYPE_OTHERS;
    isInitialized_ = false;

#ifdef SENSOR_ENABLE
    headTracker_ = std::make_shared<HeadTracker>();
#endif

    audioEffectHdiParam_ = std::make_shared<AudioEffectHdiParam>();
    memset_s(static_cast<void *>(effectHdiInput_), sizeof(effectHdiInput_), 0, sizeof(effectHdiInput_));
    GetSysPara("const.build.product", deviceClass_);
    int32_t flag = 0;
    GetSysPara("persist.multimedia.audioflag.debugarmflag", flag);
    debugArmFlag_ = flag == 1 ? true : false;
}

AudioEffectChainManager::~AudioEffectChainManager()
{
    AUDIO_INFO_LOG("~AudioEffectChainManager()");
}

AudioEffectChainManager *AudioEffectChainManager::GetInstance()
{
    static AudioEffectChainManager audioEffectChainManager;
    return &audioEffectChainManager;
}

int32_t AudioEffectChainManager::UpdateDeviceInfo(int32_t device, const std::string &sinkName)
{
    if (!isInitialized_) {
        deviceType_ = (DeviceType)device;
        deviceSink_ = sinkName;
        AUDIO_INFO_LOG("has not beed initialized");
        return ERROR;
    }

    if (deviceSink_ == sinkName) {
        AUDIO_PRERELEASE_LOGI("Same DeviceSinkName");
    }
    deviceSink_ = sinkName;

    if (deviceType_ == (DeviceType)device) {
        AUDIO_INFO_LOG("DeviceType do not need to be Updated");
        return ERROR;
    }
    // Delete effectChain in AP and store in backup map
    AUDIO_PRERELEASE_LOGI("delete all chains when device type change");
    DeleteAllChains();
    deviceType_ = (DeviceType)device;

    return SUCCESS;
}

void AudioEffectChainManager::SetSpkOffloadState()
{
    int32_t ret;
    if (deviceType_ == DEVICE_TYPE_SPEAKER) {
        if (!spkOffloadEnabled_) {
            effectHdiInput_[0] = HDI_INIT;
            ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_SPEAKER);
            if (ret != SUCCESS || !CheckIfSpkDsp()) {
                AUDIO_WARNING_LOG("set hdi init failed, backup speaker entered");
                spkOffloadEnabled_ = false;
                RecoverAllChains();
            } else {
                AUDIO_INFO_LOG("set hdi init succeeded, normal speaker entered");
                spkOffloadEnabled_ = true;
            }
        }
    } else {
        if (spkOffloadEnabled_) {
            effectHdiInput_[0] = HDI_DESTROY;
            ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_SPEAKER);
            if (ret != SUCCESS) {
                AUDIO_WARNING_LOG("set hdi destroy failed, backup speaker entered");
            }
            spkOffloadEnabled_ = false;
        }

        if (deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP && (!spatializationEnabled_ || btOffloadEnabled_)) {
            return;
        }
 
        AUDIO_INFO_LOG("recover all chains if device type not bt.");
        RecoverAllChains();
    }
}

void AudioEffectChainManager::SetOutputDeviceSink(int32_t device, const std::string &sinkName)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (UpdateDeviceInfo(device, sinkName) != SUCCESS) {
        return;
    }
    // recover effectChain in speaker mode
    SetSpkOffloadState();
    return;
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

bool AudioEffectChainManager::GetOffloadEnabled()
{
    if (deviceType_ == DEVICE_TYPE_SPEAKER) {
        return spkOffloadEnabled_;
    } else {
        return btOffloadEnabled_;
    }
}

void AudioEffectChainManager::InitHdiState()
{
    if (audioEffectHdiParam_ == nullptr) {
        AUDIO_INFO_LOG("audioEffectHdiParam_ is nullptr.");
        return;
    }
    audioEffectHdiParam_->InitHdi();
    effectHdiInput_[0] = HDI_BLUETOOTH_MODE;
    effectHdiInput_[1] = 1;
    AUDIO_INFO_LOG("set hdi bluetooth mode: %{public}d", effectHdiInput_[1]);
    int32_t ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_BLUETOOTH_A2DP);
    if (ret != SUCCESS) {
        AUDIO_WARNING_LOG("set hdi bluetooth mode failed");
    }
    effectHdiInput_[0] = HDI_INIT;
    ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_SPEAKER);
    if (ret != SUCCESS) {
        AUDIO_WARNING_LOG("set hdi init failed, backup speaker entered");
        spkOffloadEnabled_ = false;
    } else {
        AUDIO_INFO_LOG("set hdi init succeeded, normal speaker entered");
        spkOffloadEnabled_ = true;
    }
}

// Boot initialize
void AudioEffectChainManager::InitAudioEffectChainManager(std::vector<EffectChain> &effectChains,
    const EffectChainManagerParam &effectChainManagerParam,
    std::vector<std::shared_ptr<AudioEffectLibEntry>> &effectLibraryList)
{
    const std::unordered_map<std::string, std::string> &map = effectChainManagerParam.sceneTypeToChainNameMap;
    maxEffectChainCount_ = effectChainManagerParam.maxExtraNum + 1;
    priorSceneList_ = effectChainManagerParam.priorSceneList;
    std::set<std::string> effectSet;
    for (EffectChain efc: effectChains) {
        for (std::string effect: efc.apply) {
            effectSet.insert(effect);
        }
    }

    // Construct EffectToLibraryEntryMap that stores libEntry for each effect name
    std::shared_ptr<AudioEffectLibEntry> libEntry = nullptr;
    std::string libName;
    for (std::string effect: effectSet) {
        int32_t ret = FindEffectLib(effect, effectLibraryList, libEntry, libName);
        CHECK_AND_CONTINUE_LOG(ret != ERROR, "Couldn't find libEntry of effect %{public}s", effect.c_str());
        ret = CheckValidEffectLibEntry(libEntry, effect, libName);
        CHECK_AND_CONTINUE_LOG(ret != ERROR, "Invalid libEntry of effect %{public}s", effect.c_str());
        effectToLibraryEntryMap_[effect] = libEntry;
        effectToLibraryNameMap_[effect] = libName;
    }
    // Construct EffectChainToEffectsMap that stores all effect names of each effect chain
    for (EffectChain efc: effectChains) {
        std::string key = efc.name;
        std::vector <std::string> effects;
        for (std::string effectName: efc.apply) {
            effects.emplace_back(effectName);
        }
        effectChainToEffectsMap_[key] = effects;
    }

    // Constrcut SceneTypeAndModeToEffectChainNameMap that stores effectMode associated with the effectChainName
    for (auto item = map.begin(); item != map.end(); ++item) {
        sceneTypeAndModeToEffectChainNameMap_[item->first] = item->second;
        if (item->first.substr(0, effectChainManagerParam.defaultSceneName.size()) ==
            effectChainManagerParam.defaultSceneName) {
            sceneTypeAndModeToEffectChainNameMap_[DEFAULT_SCENE_TYPE + item->first.substr(
                effectChainManagerParam.defaultSceneName.size())] = item->second;
        }
    }

    AUDIO_INFO_LOG("EffectToLibraryEntryMap size %{public}zu", effectToLibraryEntryMap_.size());
    AUDIO_DEBUG_LOG("EffectChainToEffectsMap size %{public}zu, SceneTypeAndModeToEffectChainNameMap size %{public}zu",
        effectChainToEffectsMap_.size(), sceneTypeAndModeToEffectChainNameMap_.size());
    InitHdiState();
#ifdef WINDOW_MANAGER_ENABLE
    AUDIO_DEBUG_LOG("Call RegisterDisplayListener.");
#endif
    isInitialized_ = true;
}

bool AudioEffectChainManager::CheckAndAddSessionID(const std::string &sessionID)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (sessionIDSet_.count(sessionID)) {
        return false;
    }
    sessionIDSet_.insert(sessionID);
    return true;
}

int32_t AudioEffectChainManager::CreateAudioEffectChainDynamic(const std::string &sceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    CHECK_AND_RETURN_RET_LOG(isInitialized_, ERROR, "has not been initialized");
    CHECK_AND_RETURN_RET_LOG(sceneType != "", ERROR, "null sceneType");

    std::shared_ptr<AudioEffectChain> audioEffectChain = nullptr;
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    std::string defaultSceneTypeAndDeviceKey = DEFAULT_SCENE_TYPE + "_&_" + GetDeviceTypeName();

    if (sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        if (sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] == nullptr) {
            sceneTypeToEffectChainMap_.erase(sceneTypeAndDeviceKey);
            sceneTypeToEffectChainCountMap_.erase(sceneTypeAndDeviceKey);
            AUDIO_WARNING_LOG("scene type %{public}s has null effect chain", sceneTypeAndDeviceKey.c_str());
        } else {
            sceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey]++;
            if (isDefaultEffectChainExisted_ && sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] ==
                sceneTypeToEffectChainMap_[defaultSceneTypeAndDeviceKey]) {
                defaultEffectChainCount_++;
            }
            AUDIO_INFO_LOG("effect chain already exist, current count: %{public}d, default count: %{public}d",
                sceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey], defaultEffectChainCount_);
            return SUCCESS;
        }
    }
    bool isPriorScene = std::find(priorSceneList_.begin(), priorSceneList_.end(), sceneType) != priorSceneList_.end();
    audioEffectChain = CreateAudioEffectChain(sceneType, isPriorScene);

    sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] = audioEffectChain;
    sceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey] = 1;
    if (!AUDIO_SUPPORTED_SCENE_MODES.count(EFFECT_DEFAULT)) {
        return ERROR;
    }
    std::string effectMode = AUDIO_SUPPORTED_SCENE_MODES.find(EFFECT_DEFAULT)->second;
    if (!isPriorScene && !sceneTypeToSpecialEffectSet_.count(sceneType) && defaultEffectChainCount_ > 1) {
        return SUCCESS;
    }
    std::string createSceneType = (isPriorScene || sceneTypeToSpecialEffectSet_.count(sceneType) > 0) ?
        sceneType : DEFAULT_SCENE_TYPE;
    if (SetAudioEffectChainDynamic(createSceneType, effectMode) != SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::SetAudioEffectChainDynamic(const std::string &sceneType, const std::string &effectMode)
{
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    CHECK_AND_RETURN_RET_LOG(sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey), ERROR,
        "SceneType [%{public}s] does not exist, failed to set", sceneType.c_str());

    std::shared_ptr<AudioEffectChain> audioEffectChain = sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];

    std::string effectChain;
    std::string effectChainKey = sceneType + "_&_" + effectMode + "_&_" + GetDeviceTypeName();
    std::string effectNone = AUDIO_SUPPORTED_SCENE_MODES.find(EFFECT_NONE)->second;
    if (!sceneTypeAndModeToEffectChainNameMap_.count(effectChainKey)) {
        AUDIO_ERR_LOG("EffectChain key [%{public}s] does not exist, auto set to %{public}s",
            effectChainKey.c_str(), effectNone.c_str());
        effectChain = effectNone;
    } else {
        effectChain = sceneTypeAndModeToEffectChainNameMap_[effectChainKey];
    }

    if (effectChain != effectNone && !effectChainToEffectsMap_.count(effectChain)) {
        AUDIO_ERR_LOG("EffectChain name [%{public}s] does not exist, auto set to %{public}s",
            effectChain.c_str(), effectNone.c_str());
        effectChain = effectNone;
    }

    audioEffectChain->SetEffectMode(effectMode);
    audioEffectChain->SetExtraSceneType(extraSceneType_);
    audioEffectChain->SetSpatialDeviceType(spatialDeviceType_);
    audioEffectChain->SetSpatializationSceneType(spatializationSceneType_);
    audioEffectChain->SetSpatializationEnabled(spatializationEnabled_);
    for (std::string effect: effectChainToEffectsMap_[effectChain]) {
        AudioEffectHandle handle = nullptr;
        AudioEffectDescriptor descriptor;
        descriptor.libraryName = effectToLibraryNameMap_[effect];
        descriptor.effectName = effect;
        int32_t ret = effectToLibraryEntryMap_[effect]->audioEffectLibHandle->createEffect(descriptor, &handle);
        CHECK_AND_CONTINUE_LOG(ret == 0, "EffectToLibraryEntryMap[%{public}s] createEffect fail", effect.c_str());

        AUDIO_INFO_LOG("createEffect, EffectToLibraryEntryMap [%{public}s], effectChainKey [%{public}s]",
            effect.c_str(), effectChainKey.c_str());
        AudioEffectScene currSceneType;
        if (!spatializationEnabled_ || (GetDeviceTypeName() != "DEVICE_TYPE_BLUETOOTH_A2DP")) {
            currSceneType = static_cast<AudioEffectScene>(GetKeyFromValue(
                AUDIO_SUPPORTED_SCENE_TYPES, sceneType == DEFAULT_SCENE_TYPE ? DEFAULT_PRESET_SCENE : sceneType));
        } else {
            currSceneType = GetSceneTypeFromSpatializationSceneType(static_cast<AudioEffectScene>(GetKeyFromValue(
                AUDIO_SUPPORTED_SCENE_TYPES, sceneType == DEFAULT_SCENE_TYPE ? DEFAULT_PRESET_SCENE : sceneType)));
        }
        audioEffectChain->AddEffectHandle(handle, effectToLibraryEntryMap_[effect]->audioEffectLibHandle,
            currSceneType);
    }
    audioEffectChain->ResetIoBufferConfig();

    if (audioEffectChain->IsEmptyEffectHandles()) {
        AUDIO_PRERELEASE_LOGI("Effectchain is empty, copy bufIn to bufOut like EFFECT_NONE mode");
    }

    AUDIO_INFO_LOG("SceneType %{public}s delay %{public}u", sceneType.c_str(), audioEffectChain->GetLatency());
    return SUCCESS;
}

bool AudioEffectChainManager::CheckAndRemoveSessionID(const std::string &sessionID)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (!sessionIDSet_.count(sessionID)) {
        return false;
    }
    sessionIDSet_.erase(sessionID);
    return true;
}

int32_t AudioEffectChainManager::ReleaseAudioEffectChainDynamic(const std::string &sceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    CHECK_AND_RETURN_RET_LOG(isInitialized_, ERROR, "has not been initialized");
    CHECK_AND_RETURN_RET_LOG(sceneType != "", ERROR, "null sceneType");

    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    std::string defaultSceneTypeAndDeviceKey = DEFAULT_SCENE_TYPE + "_&_" + GetDeviceTypeName();
    if (!sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        sceneTypeToEffectChainCountMap_.erase(sceneTypeAndDeviceKey);
        return SUCCESS;
    } else if (sceneTypeToEffectChainCountMap_.count(sceneTypeAndDeviceKey) &&
        sceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey] > 1) {
        sceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey]--;
        if (sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] ==
            sceneTypeToEffectChainMap_[defaultSceneTypeAndDeviceKey]) {
            defaultEffectChainCount_--;
        }
        AUDIO_INFO_LOG("effect chain still exist, current count: %{public}d, default count: %{public}d",
            sceneTypeToEffectChainCountMap_[sceneTypeAndDeviceKey], defaultEffectChainCount_);
        return SUCCESS;
    }

    sceneTypeToSpecialEffectSet_.erase(sceneType);
    sceneTypeToEffectChainCountMap_.erase(sceneTypeAndDeviceKey);
    CheckAndReleaseCommonEffectChain(sceneType);
    sceneTypeToEffectChainMap_.erase(sceneTypeAndDeviceKey);

    if (debugArmFlag_ && !spkOffloadEnabled_ && CheckIfSpkDsp()) {
        effectHdiInput_[0] = HDI_INIT;
        int32_t ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_SPEAKER);
        if (ret == SUCCESS) {
            AUDIO_INFO_LOG("set hdi init succeeded, normal speaker entered");
            spkOffloadEnabled_ = true;
            DeleteAllChains();
        }
    }
    if (!sceneTypeToEffectChainMap_.count(defaultSceneTypeAndDeviceKey)) {
        isDefaultEffectChainExisted_ = false;
    }
    AUDIO_DEBUG_LOG("releaseEffect, sceneTypeAndDeviceKey [%{public}s]", sceneTypeAndDeviceKey.c_str());
    return SUCCESS;
}

bool AudioEffectChainManager::ExistAudioEffectChain(const std::string &sceneType, const std::string &effectMode,
    const std::string &spatializationEnabled)
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
    CHECK_AND_RETURN_RET(sceneType != "", false);
    CHECK_AND_RETURN_RET_LOG(GetDeviceTypeName() != "", false, "null deviceType");

#ifndef DEVICE_FLAG
    if (deviceType_ != DEVICE_TYPE_SPEAKER) {
        return false;
    }
#endif
    if ((deviceType_ == DEVICE_TYPE_SPEAKER) && (spkOffloadEnabled_)) {
        return false;
    }

    if ((deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) && (btOffloadEnabled_ || (spatializationEnabled == "0"))) {
        return false;
    }

    std::string effectChainKey = sceneType + "_&_" + effectMode + "_&_" + GetDeviceTypeName();
    if (!sceneTypeAndModeToEffectChainNameMap_.count(effectChainKey)) {
        return false;
    }
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    // if the effectChain exist, see if it is empty
    if (!sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey) ||
        sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] == nullptr) {
        return false;
    }
    auto audioEffectChain = sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
    return !audioEffectChain->IsEmptyEffectHandles();
}

int32_t AudioEffectChainManager::ApplyAudioEffectChain(const std::string &sceneType,
    const std::unique_ptr<EffectBufferAttr> &bufferAttr)
{
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    size_t totLen = static_cast<size_t>(bufferAttr->frameLen * bufferAttr->numChans * sizeof(float));
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    auto it = sceneTypeToEffectChainMap_.find(sceneTypeAndDeviceKey);
#ifdef DEVICE_FLAG
    if (it == sceneTypeToEffectChainMap_.end() || it->second == nullptr) {
        CHECK_AND_RETURN_RET_LOG(memcpy_s(bufferAttr->bufOut, totLen, bufferAttr->bufIn, totLen) == 0, ERROR,
            "memcpy error when no effect applied");
        return ERROR;
    }
#else
    if (deviceType_ != DEVICE_TYPE_SPEAKER || it == sceneTypeToEffectChainMap_.end()
            || it->second == nullptr) {
        CHECK_AND_RETURN_RET_LOG(memcpy_s(bufferAttr->bufOut, totLen, bufferAttr->bufIn, totLen) == 0, ERROR,
            "memcpy error when no effect applied");
        return SUCCESS;
    }
#endif
    auto audioEffectChain = it->second;
    AudioEffectProcInfo procInfo = {headTrackingEnabled_, btOffloadEnabled_};
    audioEffectChain->ApplyEffectChain(bufferAttr->bufIn, bufferAttr->bufOut, bufferAttr->frameLen, procInfo);
    return SUCCESS;
}

void AudioEffectChainManager::Dump()
{
    AUDIO_INFO_LOG("Dump START");
    for (auto item : sceneTypeToEffectChainMap_) {
        std::shared_ptr<AudioEffectChain> audioEffectChain = item.second;
        audioEffectChain->Dump();
    }
}

int32_t AudioEffectChainManager::EffectDspVolumeUpdate(std::shared_ptr<AudioEffectVolume> audioEffectVolume)
{
    AUDIO_INFO_LOG("send volume to dsp.");
    CHECK_AND_RETURN_RET_LOG(audioEffectVolume != nullptr, ERROR, "null audioEffectVolume");
    float volumeMax = 0;
    for (auto it = sceneTypeToSessionIDMap_.begin(); it != sceneTypeToSessionIDMap_.end(); it++) {
        std::set<std::string> sessions = sceneTypeToSessionIDMap_[it->first];
        for (auto s = sessions.begin(); s != sessions.end(); s++) {
            float streamVolumeTemp = audioEffectVolume->GetStreamVolume(*s);
            float systemVolumeTemp = audioEffectVolume->GetSystemVolume(it->first);
            volumeMax = (streamVolumeTemp * systemVolumeTemp) > volumeMax ?
                (streamVolumeTemp * systemVolumeTemp) : volumeMax;
        }
    }
    if (static_cast<int32_t>(audioEffectVolume->GetDspVolume() * MAX_UINT_VOLUME_NUM) !=
        static_cast<int32_t>(volumeMax * MAX_UINT_VOLUME_NUM)) {
        audioEffectVolume->SetDspVolume(volumeMax);
        effectHdiInput_[0] = HDI_VOLUME;
        AUDIO_INFO_LOG("finalVolume change to %{public}f", volumeMax);
        int32_t dspVolumeMax = static_cast<int32_t>(volumeMax * MAX_UINT_DSP_VOLUME);
        int32_t ret = memcpy_s(&effectHdiInput_[1], SEND_HDI_COMMAND_LEN - sizeof(int8_t),
            &dspVolumeMax, sizeof(int32_t));
        CHECK_AND_RETURN_RET_LOG(ret == 0, ERROR, "memcpy volume failed");
        AUDIO_INFO_LOG("set hdi volume: %{public}u", *(reinterpret_cast<uint32_t *>(&effectHdiInput_[1])));
        ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_);
        CHECK_AND_RETURN_RET_LOG(ret == 0, ERROR, "set hdi volume failed");
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::EffectApVolumeUpdate(std::shared_ptr<AudioEffectVolume> audioEffectVolume)
{
    AUDIO_INFO_LOG("send volume to ap.");
    CHECK_AND_RETURN_RET_LOG(audioEffectVolume != nullptr, ERROR, "null audioEffectVolume");
    for (auto it = sceneTypeToSessionIDMap_.begin(); it != sceneTypeToSessionIDMap_.end(); it++) {
        float volumeMax = 0;
        std::set<std::string> sessions = it->second;
        for (auto s = sessions.begin(); s != sessions.end(); s++) {
            float streamVolumeTemp = audioEffectVolume->GetStreamVolume(*s);
            float systemVolumeTemp = audioEffectVolume->GetSystemVolume(it->first);
            volumeMax = (streamVolumeTemp * systemVolumeTemp) > volumeMax ?
                (streamVolumeTemp * systemVolumeTemp) : volumeMax;
        }
        std::string sceneTypeAndDeviceKey = it->first + "_&_" + GetDeviceTypeName();
        CHECK_AND_RETURN_RET_LOG(sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey) > 0 &&
            sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] != nullptr, ERROR, "null audioEffectChain");
        auto audioEffectChain = sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
        if (static_cast<int32_t>(audioEffectChain->GetFinalVolume() * MAX_UINT_VOLUME_NUM) !=
            static_cast<int32_t>(volumeMax * MAX_UINT_VOLUME_NUM)) {
            audioEffectChain->SetFinalVolume(volumeMax);
            int32_t ret = audioEffectChain->UpdateEffectParam();
            CHECK_AND_RETURN_RET_LOG(ret == 0, ERROR, "set ap volume failed");
            AUDIO_INFO_LOG("The delay of SceneType %{public}s is %{public}u, finalVolume changed to %{public}f",
                it->first.c_str(), audioEffectChain->GetLatency(), volumeMax);
        }
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::EffectVolumeUpdate(std::shared_ptr<AudioEffectVolume> audioEffectVolume)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    int32_t ret;
    if (((deviceType_ == DEVICE_TYPE_SPEAKER) && (spkOffloadEnabled_)) ||
        ((deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) && (btOffloadEnabled_))) {
        ret = EffectDspVolumeUpdate(audioEffectVolume);
    } else {
        ret = EffectApVolumeUpdate(audioEffectVolume);
    }
    return ret;
}

int32_t AudioEffectChainManager::StreamVolumeUpdate(const std::string sessionIDString, const float streamVolume)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    // update streamVolume
    std::shared_ptr<AudioEffectVolume> audioEffectVolume = AudioEffectVolume::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectVolume != nullptr, ERROR, "null audioEffectVolume");
    audioEffectVolume->SetStreamVolume(sessionIDString, streamVolume);
    int32_t ret;
    AUDIO_INFO_LOG("streamVolume is %{public}f", audioEffectVolume->GetStreamVolume(sessionIDString));
    ret = EffectVolumeUpdate(audioEffectVolume);
    return ret;
}

int32_t AudioEffectChainManager::SetSceneTypeSystemVolume(const std::string sceneType, const float systemVolume)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    // set systemVolume by sceneType
    std::shared_ptr<AudioEffectVolume> audioEffectVolume = AudioEffectVolume::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectVolume != nullptr, ERROR, "null audioEffectVolume");
    audioEffectVolume->SetSystemVolume(sceneType, systemVolume);
    AUDIO_INFO_LOG("systemVolume is %{public}f", audioEffectVolume->GetSystemVolume(sceneType));

    return SUCCESS;
}

#ifdef WINDOW_MANAGER_ENABLE
int32_t AudioEffectChainManager::EffectDspRotationUpdate(std::shared_ptr<AudioEffectRotation> audioEffectRotation,
    const uint32_t rotationState)
{
    // send rotation to dsp
    AUDIO_DEBUG_LOG("send rotation to dsp.");
    CHECK_AND_RETURN_RET_LOG(audioEffectRotation != nullptr, ERROR, "null audioEffectRotation");
    AUDIO_DEBUG_LOG("rotationState change, new state: %{public}d, previous state: %{public}d",
        rotationState, audioEffectRotation->GetRotation());
    effectHdiInput_[0] = HDI_ROTATION;
    effectHdiInput_[1] = rotationState;
    AUDIO_INFO_LOG("set hdi rotation: %{public}d", effectHdiInput_[1]);
    int32_t ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_);
    CHECK_AND_RETURN_RET_LOG(ret == 0, ERROR, "set hdi rotation failed");

    return SUCCESS;
}

int32_t AudioEffectChainManager::EffectApRotationUpdate(std::shared_ptr<AudioEffectRotation> audioEffectRotation,
    const uint32_t rotationState)
{
    // send rotation to ap
    AUDIO_DEBUG_LOG("send rotation to ap.");
    CHECK_AND_RETURN_RET_LOG(audioEffectRotation != nullptr, ERROR, "null audioEffectRotation");
    AUDIO_DEBUG_LOG("rotationState change, new state: %{public}d, previous state: %{public}d",
        rotationState, audioEffectRotation->GetRotation());
    for (auto it = sceneTypeToSessionIDMap_.begin(); it != sceneTypeToSessionIDMap_.end(); it++) {
        std::string sceneTypeAndDeviceKey = it->first + "_&_" + GetDeviceTypeName();
        if (!sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
            return ERROR;
        }
        auto audioEffectChain = sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
        if (audioEffectChain == nullptr) {
            return ERROR;
        }
        int32_t ret = audioEffectChain->UpdateEffectParam();
        CHECK_AND_RETURN_RET_LOG(ret == 0, ERROR, "set ap rotation failed");
        AUDIO_INFO_LOG("The delay of SceneType %{public}s is %{public}u, rotation changed to %{public}u",
            it->first.c_str(), audioEffectChain->GetLatency(), rotationState);
        }
    
    return SUCCESS;
}

int32_t AudioEffectChainManager::EffectRotationUpdate(const uint32_t rotationState)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    std::shared_ptr<AudioEffectRotation> audioEffectRotation = AudioEffectRotation::GetInstance();
    AUDIO_INFO_LOG("rotation update to %{public}u", rotationState);
    if (audioEffectRotation->GetRotation() != rotationState) {
        audioEffectRotation->SetRotation(rotationState);
        EffectDspRotationUpdate(audioEffectRotation, rotationState);
        EffectApRotationUpdate(audioEffectRotation, rotationState);
    }

    return SUCCESS;
}
#endif

int32_t AudioEffectChainManager::UpdateMultichannelConfig(const std::string &sceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    if (!sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        return ERROR;
    }
    uint32_t inputChannels = DEFAULT_NUM_CHANNEL;
    uint64_t inputChannelLayout = DEFAULT_NUM_CHANNELLAYOUT;
    ReturnEffectChannelInfo(sceneType, inputChannels, inputChannelLayout);

    auto audioEffectChain = sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
    if (audioEffectChain == nullptr) {
        return ERROR;
    }
    audioEffectChain->UpdateMultichannelIoBufferConfig(inputChannels, inputChannelLayout);
    return SUCCESS;
}

int32_t AudioEffectChainManager::InitAudioEffectChainDynamic(const std::string &sceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    CHECK_AND_RETURN_RET_LOG(isInitialized_, ERROR, "has not been initialized");
    CHECK_AND_RETURN_RET_LOG(sceneType != "", ERROR, "null sceneType");

    std::shared_ptr<AudioEffectChain> audioEffectChain = nullptr;
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    if (!sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        return SUCCESS;
    } else {
        audioEffectChain = sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey];
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
        spatializationEnabled_.load(), headTrackingEnabled_);
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (spatializationEnabled_ != spatializationState.spatializationEnabled) {
        UpdateSpatializationEnabled(spatializationState);
    }
    if (headTrackingEnabled_ != spatializationState.headTrackingEnabled) {
        headTrackingEnabled_ = spatializationState.headTrackingEnabled;
        UpdateSensorState();
    }

    std::shared_ptr<AudioEffectVolume> audioEffectVolume = AudioEffectVolume::GetInstance();
    CHECK_AND_RETURN_RET_LOG(audioEffectVolume != nullptr, ERROR, "null audioEffectVolume");
    EffectVolumeUpdate(audioEffectVolume);
    AUDIO_INFO_LOG("systemVolume prepare change or no change");

    return SUCCESS;
}

int32_t AudioEffectChainManager::UpdateSpatialDeviceType(AudioSpatialDeviceType spatialDeviceType)
{
    int32_t ret{ SUCCESS };
    spatialDeviceType_ = spatialDeviceType;

    effectHdiInput_[0] = HDI_UPDATE_SPATIAL_DEVICE_TYPE;
    effectHdiInput_[1] = spatialDeviceType_;
    AUDIO_INFO_LOG("set hdi spatialDeviceType: %{public}d", effectHdiInput_[1]);
    ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_BLUETOOTH_A2DP);
    if (ret != SUCCESS) {
        AUDIO_WARNING_LOG("set hdi update spatial device type failed");
    }

    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    for (const auto& sceneType2EffectChain : sceneTypeToEffectChainMap_) {
        auto audioEffectChain = sceneType2EffectChain.second;
        if (audioEffectChain != nullptr) {
            audioEffectChain->SetSpatialDeviceType(spatialDeviceType_);
            ret = audioEffectChain->UpdateEffectParam();
            CHECK_AND_CONTINUE_LOG(ret == SUCCESS, "UpdateEffectParam failed.");
        }
    }

    return SUCCESS;
}

int32_t AudioEffectChainManager::ReturnEffectChannelInfo(const std::string &sceneType, uint32_t &channels,
    uint64_t &channelLayout)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    if (!sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey)) {
        return ERROR;
    }
    for (auto& scenePair : sceneTypeToSessionIDMap_) {
        std::string pairSceneTypeAndDeviceKey = scenePair.first + "_&_" + GetDeviceTypeName();
        if (sceneTypeToEffectChainMap_.count(pairSceneTypeAndDeviceKey) > 0 &&
            sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] ==
            sceneTypeToEffectChainMap_[pairSceneTypeAndDeviceKey]) {
            std::set<std::string> sessions = scenePair.second;
            FindMaxEffectChannels(scenePair.first, sessions, channels, channelLayout);
        }
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::ReturnMultiChannelInfo(uint32_t *channels, uint64_t *channelLayout)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    for (auto it = sceneTypeToSessionIDMap_.begin(); it != sceneTypeToSessionIDMap_.end(); it++) {
        std::set<std::string> sessions = sceneTypeToSessionIDMap_[it->first];
        for (auto s = sessions.begin(); s != sessions.end(); ++s) {
            SessionEffectInfo info = sessionIDToEffectInfoMap_[*s];
            uint32_t tmpChannelCount = DEFAULT_MCH_NUM_CHANNEL;
            uint64_t tmpChannelLayout = DEFAULT_MCH_NUM_CHANNELLAYOUT;
            if (info.channels > DEFAULT_NUM_CHANNEL &&
                info.channels <= DSP_MAX_NUM_CHANNEL &&
                !ExistAudioEffectChain(it->first, info.sceneMode, info.spatializationEnabled) &&
                IsChannelLayoutSupported(info.channelLayout)) {
                tmpChannelLayout = info.channelLayout;
                tmpChannelCount = info.channels;
            }

            if (tmpChannelCount >= *channels) {
                *channels = tmpChannelCount;
                *channelLayout = tmpChannelLayout;
            }
        }
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::SessionInfoMapAdd(const std::string &sessionID, const SessionEffectInfo &info)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    CHECK_AND_RETURN_RET_LOG(sessionID != "", ERROR, "null sessionID");
    if (!sessionIDToEffectInfoMap_.count(sessionID)) {
        sceneTypeToSessionIDMap_[info.sceneType].insert(sessionID);
        sessionIDToEffectInfoMap_[sessionID] = info;
    } else if (sessionIDToEffectInfoMap_[sessionID].sceneMode != info.sceneMode ||
        sessionIDToEffectInfoMap_[sessionID].spatializationEnabled != info.spatializationEnabled) {
        sessionIDToEffectInfoMap_[sessionID] = info;
    } else {
        return ERROR;
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::SessionInfoMapDelete(const std::string &sceneType, const std::string &sessionID)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (!sceneTypeToSessionIDMap_.count(sceneType)) {
        return ERROR;
    }
    if (sceneTypeToSessionIDMap_[sceneType].erase(sessionID)) {
        if (sceneTypeToSessionIDMap_[sceneType].empty()) {
            sceneTypeToSessionIDMap_.erase(sceneType);
        }
    } else {
        return ERROR;
    }
    if (!sessionIDToEffectInfoMap_.erase(sessionID)) {
        return ERROR;
    }
    return SUCCESS;
}

int32_t AudioEffectChainManager::SetHdiParam(const std::string &sceneType, const std::string &effectMode, bool enabled)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (!isInitialized_) {
        if (initializedLogFlag_) {
            AUDIO_ERR_LOG("audioEffectChainManager has not been initialized");
            initializedLogFlag_ = false;
        }
        return ERROR;
    }
    CHECK_AND_RETURN_RET(sceneType != "", ERROR);
    hdiSceneType_ = GetKeyFromValue(AUDIO_SUPPORTED_SCENE_TYPES, sceneType);
    memset_s(static_cast<void *>(effectHdiInput_), sizeof(effectHdiInput_), 0, sizeof(effectHdiInput_));
    effectHdiInput_[0] = HDI_BYPASS;
    effectHdiInput_[1] = enabled == true ? 0 : 1;
    AUDIO_PRERELEASE_LOGI("set hdi bypass: %{public}d", effectHdiInput_[1]);
    int32_t ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_BLUETOOTH_A2DP);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ERROR, "set hdi bypass failed, ret is %{public}d", ret);

    effectHdiInput_[0] = HDI_ROOM_MODE;
    if (!spatializationEnabled_ || (GetDeviceTypeName() != "DEVICE_TYPE_BLUETOOTH_A2DP")) {
        effectHdiInput_[1] = hdiSceneType_;
    } else {
        effectHdiInput_[1] =
            static_cast<int32_t>(GetSceneTypeFromSpatializationSceneType(static_cast<AudioEffectScene>(hdiSceneType_)));
    }
    hdiEffectMode_ = GetKeyFromValue(AUDIO_SUPPORTED_SCENE_MODES, effectMode);
    effectHdiInput_[HDI_ROOM_MODE_INDEX_TWO] = hdiEffectMode_;
    AUDIO_PRERELEASE_LOGI("set hdi room mode sceneType: %{public}d, effectMode: %{public}d", effectHdiInput_[1],
        effectHdiInput_[HDI_ROOM_MODE_INDEX_TWO]);
    ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ERROR, "set hdi room mode failed, ret is %{public}d", ret);
    return SUCCESS;
}

void AudioEffectChainManager::UpdateSensorState()
{
    effectHdiInput_[0] = HDI_HEAD_MODE;
    effectHdiInput_[1] = headTrackingEnabled_ == true ? 1 : 0;
    AUDIO_INFO_LOG("set hdi head mode: %{public}d", effectHdiInput_[1]);
    int32_t ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_BLUETOOTH_A2DP);
    if (ret != SUCCESS) {
        AUDIO_WARNING_LOG("set hdi head mode failed");
    }

    if (headTrackingEnabled_) {
#ifdef SENSOR_ENABLE
        if (btOffloadEnabled_) {
            headTracker_->SensorInit();
            ret = headTracker_->SensorSetConfig(DSP_SPATIALIZER_ENGINE);
        } else {
            headTracker_->SensorInit();
            ret = headTracker_->SensorSetConfig(ARM_SPATIALIZER_ENGINE);
        }

        if (ret != SUCCESS) {
            AUDIO_ERR_LOG("SensorSetConfig error!");
        }

        if (headTracker_->SensorActive() != SUCCESS) {
            AUDIO_ERR_LOG("SensorActive failed");
        }
#endif
        return;
    }

    if (btOffloadEnabled_) {
        return;
    }

#ifdef SENSOR_ENABLE
    if (headTracker_->SensorDeactive() != SUCCESS) {
        AUDIO_ERR_LOG("SensorDeactive failed");
    }
    headTracker_->SensorUnsubscribe();
    HeadPostureData headPostureData = {1, 1.0, 0.0, 0.0, 0.0}; // ori head posturedata
    headTracker_->SetHeadPostureData(headPostureData);
    for (auto it = sceneTypeToEffectChainMap_.begin(); it != sceneTypeToEffectChainMap_.end(); ++it) {
        auto audioEffectChain = it->second;
        if (audioEffectChain == nullptr) {
            continue;
        }
        audioEffectChain->SetHeadTrackingDisabled();
    }
#endif
}

void AudioEffectChainManager::DeleteAllChains()
{
    std::map<std::string, int32_t> sceneTypeToEffectChainCountBackupMap;
    for (auto it = sceneTypeToEffectChainCountMap_.begin(); it != sceneTypeToEffectChainCountMap_.end(); ++it) {
        AUDIO_DEBUG_LOG("sceneTypeAndDeviceKey %{public}s count:%{public}d", it->first.c_str(), it->second);
        sceneTypeToEffectChainCountBackupMap.insert(
            std::make_pair(it->first.substr(0, static_cast<size_t>(it->first.find("_&_"))), it->second));
    }

    for (auto it = sceneTypeToEffectChainCountBackupMap.begin(); it != sceneTypeToEffectChainCountBackupMap.end();
        ++it) {
        for (int32_t k = 0; k < it->second; ++k) {
            ReleaseAudioEffectChainDynamic(it->first);
        }
    }
    return;
}

void AudioEffectChainManager::RecoverAllChains()
{
    for (auto item : sceneTypeCountList_) {
        AUDIO_DEBUG_LOG("sceneType %{public}s count:%{public}d", item.first.c_str(), item.second);
        for (int32_t k = 0; k < item.second; ++k) {
            CreateAudioEffectChainDynamic(item.first);
        }
        UpdateMultichannelConfig(item.first);
    }
}

uint32_t AudioEffectChainManager::GetLatency(const std::string &sessionId)
{
    if (((deviceType_ == DEVICE_TYPE_SPEAKER) && (spkOffloadEnabled_)) ||
        ((deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) && (btOffloadEnabled_))) {
        AUDIO_DEBUG_LOG("offload enabled, return 0");
        return 0;
    }
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    CHECK_AND_RETURN_RET(sessionIDToEffectInfoMap_.count(sessionId), 0);
    if (sessionIDToEffectInfoMap_[sessionId].sceneMode == "" ||
        sessionIDToEffectInfoMap_[sessionId].sceneMode == "None") {
        AUDIO_DEBUG_LOG("seceneMode is None, return 0");
        return 0;
    }
    if (sessionIDToEffectInfoMap_[sessionId].spatializationEnabled == "0" &&
        GetDeviceTypeName() == "DEVICE_TYPE_BLUETOOTH_A2DP") {
        return 0;
    }
    std::string sceneTypeAndDeviceKey = sessionIDToEffectInfoMap_[sessionId].sceneType + "_&_" + GetDeviceTypeName();
    CHECK_AND_RETURN_RET(sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey) &&
        sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] != nullptr, 0);
    return sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey]->GetLatency();
}

int32_t AudioEffectChainManager::SetSpatializationSceneType(AudioSpatializationSceneType spatializationSceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    AUDIO_INFO_LOG("spatialization scene type is set to be %{public}d", spatializationSceneType);
    spatializationSceneType_ = spatializationSceneType;

    if (!spatializationEnabled_ || (GetDeviceTypeName() != "DEVICE_TYPE_BLUETOOTH_A2DP")) {
        return SUCCESS;
    }

    effectHdiInput_[0] = HDI_SPATIALIZATION_SCENE_TYPE;
    effectHdiInput_[1] = static_cast<int32_t>(spatializationSceneType_);
    if (audioEffectHdiParam_->UpdateHdiState(effectHdiInput_) != SUCCESS) {
        AUDIO_WARNING_LOG("set hdi spatialization scene type failed");
    }

    SetSpatializationSceneTypeToChains();

    return SUCCESS;
}

void AudioEffectChainManager::UpdateExtraSceneType(const std::string &mainkey, const std::string &subkey,
    const std::string &extraSceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (mainkey == "audio_effect" && subkey == "update_audio_effect_type") {
        AUDIO_INFO_LOG("Set scene type: %{public}s to hdi", extraSceneType.c_str());
        int32_t ret{ SUCCESS };
        effectHdiInput_[0] = HDI_EXTRA_SCENE_TYPE;
        effectHdiInput_[1] = static_cast<int32_t>(std::stoi(extraSceneType));
        ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_SPEAKER);
        if (ret != SUCCESS) {
            AUDIO_WARNING_LOG("set hdi update rss scene type failed");
        }
        AUDIO_INFO_LOG("Set scene type: %{public}s to arm", extraSceneType.c_str());
        extraSceneType_ = extraSceneType;
        for (auto it = sceneTypeToEffectChainMap_.begin(); it != sceneTypeToEffectChainMap_.end(); ++it) {
            auto audioEffectChain = it->second;
            if (audioEffectChain == nullptr) {
                continue;
            }
            audioEffectChain->SetExtraSceneType(extraSceneType);
            if (audioEffectChain->UpdateEffectParam() != SUCCESS) {
                AUDIO_WARNING_LOG("Update scene type to effect chain failed");
                continue;
            }
        }
    } else {
        AUDIO_INFO_LOG("UpdateExtraSceneType failed, mainkey is %{public}s, subkey is %{public}s, "
            "extraSceneType is %{public}s", mainkey.c_str(), subkey.c_str(), extraSceneType.c_str());
        return;
    }
}

void AudioEffectChainManager::SetSpatializationSceneTypeToChains()
{
    for (auto it = sceneTypeToEffectChainMap_.begin(); it != sceneTypeToEffectChainMap_.end(); ++it) {
        auto audioEffectChain = it->second;
        if (audioEffectChain == nullptr) {
            continue;
        }
        audioEffectChain->SetSpatializationSceneType(spatializationSceneType_);
        if (audioEffectChain->UpdateEffectParam() != SUCCESS) {
            AUDIO_WARNING_LOG("Update param to effect chain failed");
            continue;
        }
    }
}

void AudioEffectChainManager::SetSpatializationEnabledToChains()
{
    for (auto it = sceneTypeToEffectChainMap_.begin(); it != sceneTypeToEffectChainMap_.end(); ++it) {
        auto audioEffectChain = it->second;
        if (audioEffectChain == nullptr) {
            continue;
        }
        audioEffectChain->SetSpatializationEnabled(spatializationEnabled_);
        if (audioEffectChain->UpdateEffectParam() != SUCCESS) {
            AUDIO_WARNING_LOG("Update param to effect chain failed");
            continue;
        }
    }
}

bool AudioEffectChainManager::GetCurSpatializationEnabled()
{
    return spatializationEnabled_;
}

void AudioEffectChainManager::ResetEffectBuffer()
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    for (const auto &[sceneType, effectChain] : sceneTypeToEffectChainMap_) {
        if (effectChain == nullptr) continue;
        effectChain->InitEffectChain();
    }
}

void AudioEffectChainManager::ResetInfo()
{
    effectToLibraryEntryMap_.clear();
    effectToLibraryNameMap_.clear();
    effectChainToEffectsMap_.clear();
    sceneTypeAndModeToEffectChainNameMap_.clear();
    sceneTypeToEffectChainMap_.clear();
    sceneTypeToEffectChainCountMap_.clear();
    sessionIDSet_.clear();
    sceneTypeToSessionIDMap_.clear();
    sessionIDToEffectInfoMap_.clear();
    sceneTypeToSpecialEffectSet_.clear();
    deviceType_ = DEVICE_TYPE_SPEAKER;
    deviceSink_ = DEFAULT_DEVICE_SINK;
    isInitialized_ = false;
    spatializationEnabled_ = false;
    headTrackingEnabled_ = false;
    btOffloadEnabled_ = false;
    spkOffloadEnabled_ = false;
    initializedLogFlag_ = true;
    spatializationSceneType_ = SPATIALIZATION_SCENE_TYPE_DEFAULT;
    hdiSceneType_ = 0;
    hdiEffectMode_ = 0;
    isDefaultEffectChainExisted_ = false;
}

void AudioEffectChainManager::UpdateRealAudioEffect()
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    uint32_t maxSessionID = 0;
    std::string sceneType = "";
    for (auto& scenePair : sceneTypeToSessionIDMap_) {
        if (!sceneTypeToSpecialEffectSet_.count(scenePair.first) &&
            std::find(priorSceneList_.begin(), priorSceneList_.end(), sceneType) == priorSceneList_.end()) {
            std::set<std::string> &sessions = scenePair.second;
            FindMaxSessionID(maxSessionID, sceneType, scenePair.first, sessions);
        }
    }
    AUDIO_INFO_LOG("newest stream, sessionID: %{public}u, sceneType: %{public}s", maxSessionID, sceneType.c_str());
    std::string key = sceneType + "_&_" + GetDeviceTypeName();
    if (!sceneType.empty() && sceneTypeToEffectChainMap_.count(key) && sceneTypeToEffectChainMap_[key] != nullptr) {
        std::shared_ptr<AudioEffectChain> audioEffectChain = sceneTypeToEffectChainMap_[key];
        AudioEffectScene currSceneType;
        UpdateCurrSceneType(currSceneType, sceneType);
        audioEffectChain->SetEffectCurrSceneType(currSceneType);
        audioEffectChain->UpdateEffectParam();
    }
}

bool AudioEffectChainManager::CheckSceneTypeMatch(const std::string &sinkSceneType, const std::string &sceneType)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    std::string sinkSceneTypeAndDeviceKey = sinkSceneType + "_&_" + GetDeviceTypeName();
    std::string defaultSceneTypeAndDeviceKey = DEFAULT_SCENE_TYPE + "_&_" + GetDeviceTypeName();
    if (!sceneTypeToEffectChainMap_.count(sceneTypeAndDeviceKey) ||
        !sceneTypeToEffectChainMap_.count(sinkSceneTypeAndDeviceKey)) {
        return false;
    }
    if (sceneType == sinkSceneType && (sceneTypeToSpecialEffectSet_.count(sinkSceneType) ||
        std::find(priorSceneList_.begin(), priorSceneList_.end(), sceneType) != priorSceneList_.end())) {
        return true;
    }
    if (sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey] ==
        sceneTypeToEffectChainMap_[sinkSceneTypeAndDeviceKey]) {
        return sceneTypeAndDeviceKey == defaultSceneTypeAndDeviceKey;
    }
    return false;
}

void AudioEffectChainManager::UpdateCurrSceneType(AudioEffectScene &currSceneType, std::string &sceneType)
{
    currSceneType = static_cast<AudioEffectScene>(GetKeyFromValue(AUDIO_SUPPORTED_SCENE_TYPES, sceneType));
}

void AudioEffectChainManager::FindMaxEffectChannels(const std::string &sceneType,
    const std::set<std::string> &sessions, uint32_t &channels, uint64_t &channelLayout)
{
    for (auto s = sessions.begin(); s != sessions.end(); ++s) {
        SessionEffectInfo info = sessionIDToEffectInfoMap_[*s];
        uint32_t tmpChannelCount;
        uint64_t tmpChannelLayout;
        std::string deviceType = GetDeviceTypeName();
        if (((deviceType == "DEVICE_TYPE_BLUETOOTH_A2DP") || (deviceType == "DEVICE_TYPE_SPEAKER"))
            && ExistAudioEffectChain(sceneType, info.sceneMode, info.spatializationEnabled)
            && IsChannelLayoutSupported(info.channelLayout)) {
            tmpChannelLayout = info.channelLayout;
            tmpChannelCount = info.channels;
        } else {
            tmpChannelCount = DEFAULT_NUM_CHANNEL;
            tmpChannelLayout = DEFAULT_NUM_CHANNELLAYOUT;
        }

        if (tmpChannelCount >= channels) {
            channels = tmpChannelCount;
            channelLayout = tmpChannelLayout;
        }
    }
}

std::shared_ptr<AudioEffectChain> AudioEffectChainManager::CreateAudioEffectChain(const std::string &sceneType,
    bool isPriorScene)
{
    std::shared_ptr<AudioEffectChain> audioEffectChain = nullptr;
    std::string defaultSceneTypeAndDeviceKey = DEFAULT_SCENE_TYPE + "_&_" + GetDeviceTypeName();

    if (isPriorScene) {
        AUDIO_INFO_LOG("create prior effect chain: %{public}s", sceneType.c_str());
#ifdef SENSOR_ENABLE
        audioEffectChain = std::make_shared<AudioEffectChain>(sceneType, headTracker_);
#else
        audioEffectChain = std::make_shared<AudioEffectChain>(sceneType);
#endif
        return audioEffectChain;
    }
    if ((maxEffectChainCount_ - static_cast<int32_t>(sceneTypeToSpecialEffectSet_.size())) > 1) {
        AUDIO_INFO_LOG("max audio effect chain count not reached, create special effect chain: %{public}s",
            sceneType.c_str());
        sceneTypeToSpecialEffectSet_.insert(sceneType);
#ifdef SENSOR_ENABLE
        audioEffectChain = std::make_shared<AudioEffectChain>(sceneType, headTracker_);
#else
        audioEffectChain = std::make_shared<AudioEffectChain>(sceneType);
#endif
    } else {
        if (!isDefaultEffectChainExisted_) {
            AUDIO_INFO_LOG("max audio effect chain count reached, create current and default effect chain");
#ifdef SENSOR_ENABLE
            audioEffectChain = std::make_shared<AudioEffectChain>(DEFAULT_SCENE_TYPE, headTracker_);
#else
            audioEffectChain = std::make_shared<AudioEffectChain>(DEFAULT_SCENE_TYPE);
#endif
            sceneTypeToEffectChainMap_[defaultSceneTypeAndDeviceKey] = audioEffectChain;
            defaultEffectChainCount_ = 1;
            isDefaultEffectChainExisted_ = true;
        } else {
            audioEffectChain = sceneTypeToEffectChainMap_[defaultSceneTypeAndDeviceKey];
            defaultEffectChainCount_++;
            AUDIO_INFO_LOG("max audio effect chain count reached and default effect chain already exist: %{public}d",
                defaultEffectChainCount_);
        }
    }
    return audioEffectChain;
}

void AudioEffectChainManager::CheckAndReleaseCommonEffectChain(const std::string &sceneType)
{
    AUDIO_INFO_LOG("release effect chain for scene type: %{public}s", sceneType.c_str());
    std::string sceneTypeAndDeviceKey = sceneType + "_&_" + GetDeviceTypeName();
    std::string defaultSceneTypeAndDeviceKey = DEFAULT_SCENE_TYPE + "_&_" + GetDeviceTypeName();
    if (!isDefaultEffectChainExisted_) {
        return;
    }
    if (sceneTypeToEffectChainMap_[defaultSceneTypeAndDeviceKey] == sceneTypeToEffectChainMap_[sceneTypeAndDeviceKey]) {
        if (defaultEffectChainCount_ <= 1) {
            sceneTypeToEffectChainMap_.erase(defaultSceneTypeAndDeviceKey);
            defaultEffectChainCount_= 0;
            isDefaultEffectChainExisted_ = false;
            AUDIO_INFO_LOG("default effect chain is released");
        } else {
            defaultEffectChainCount_--;
            AUDIO_INFO_LOG("default effect chain still exist, count is %{public}d", defaultEffectChainCount_);
        }
    }
}

void AudioEffectChainManager::UpdateSpatializationEnabled(AudioSpatializationState spatializationState)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    spatializationEnabled_ = spatializationState.spatializationEnabled;

    memset_s(static_cast<void *>(effectHdiInput_), sizeof(effectHdiInput_), 0, sizeof(effectHdiInput_));
    if (spatializationEnabled_) {
        if ((deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) && (!btOffloadSupported_)) {
            AUDIO_INFO_LOG("A2dp-hal, enter ARM processing");
            btOffloadEnabled_ = false;
            RecoverAllChains();
            SetSpatializationEnabledToChains();
            return;
        }
        effectHdiInput_[0] = HDI_INIT;
        int32_t ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_BLUETOOTH_A2DP);
        if (ret != SUCCESS) {
            AUDIO_ERR_LOG("set hdi init failed, enter route of escape in ARM");
            btOffloadEnabled_ = false;
            RecoverAllChains();
        } else {
            AUDIO_INFO_LOG("set hdi init succeeded, normal spatialization entered");
            btOffloadEnabled_ = true;
        }
    } else {
        effectHdiInput_[0] = HDI_DESTROY;
        AUDIO_INFO_LOG("set hdi destroy.");
        int32_t ret = audioEffectHdiParam_->UpdateHdiState(effectHdiInput_, DEVICE_TYPE_BLUETOOTH_A2DP);
        if (ret != SUCCESS) {
            AUDIO_ERR_LOG("set hdi destroy failed");
        }
        if (deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
            AUDIO_INFO_LOG("delete all chains if device type is bt.");
            DeleteAllChains();
        }
        btOffloadEnabled_ = false;
    }
    SetSpatializationEnabledToChains();
}
// for AISS temporarily
bool AudioEffectChainManager::CheckIfSpkDsp()
{
    if (deviceType_ != DEVICE_TYPE_SPEAKER) {
        return false;
    }
    if (debugArmFlag_) {
        for (auto &[key, count] : sceneTypeToEffectChainCountMap_) {
            std::string sceneType = key.substr(0, static_cast<size_t>(key.find("_&_")));
            if (sceneType == "SCENE_MOVIE" && count > 0) {
                return false;
            }
        }
    }
    return true;
}

void AudioEffectChainManager::UpdateEffectBtOffloadSupported(const bool &isSupported)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (isSupported == btOffloadSupported_) {
        return;
    }
    if (!isSupported) {
        btOffloadSupported_ = isSupported;
        AUDIO_INFO_LOG("btOffloadSupported_ off, device disconnect from %{public}d", deviceType_);
        return;
    }

    if (!spatializationEnabled_) {
        btOffloadSupported_ = isSupported;
        AUDIO_INFO_LOG("btOffloadSupported_ on, but spatialization is off, do nothing");
        return;
    }
    // Release ARM, try offload to DSP
    AUDIO_INFO_LOG("btOffloadSupported_ on, try offload effect on device %{public}d", deviceType_);
    AudioSpatializationState oldState = {spatializationEnabled_, headTrackingEnabled_};
    AudioSpatializationState offState = {false, false};
    UpdateSpatializationState(offState);
    btOffloadSupported_ = isSupported;
    UpdateSpatializationState(oldState);
    return;
}

void AudioEffectChainManager::UpdateSceneTypeList(const std::string &sceneType, SceneTypeOperation operation)
{
    std::lock_guard<std::recursive_mutex> lock(dynamicMutex_);
    if (operation == ADD_SCENE_TYPE) {
        auto it = std::find_if(sceneTypeCountList_.begin(), sceneTypeCountList_.end(),
            [sceneType](const std::pair<std::string, int32_t> &element) {
                return element.first == sceneType;
            });
        if (it == sceneTypeCountList_.end()) {
            sceneTypeCountList_.push_back(std::make_pair(sceneType, 1));
            AUDIO_INFO_LOG("scene Type %{public}s is added", sceneType.c_str());
        } else {
            it->second++;
            AUDIO_INFO_LOG("scene Type %{public}s count is increased to %{public}d", sceneType.c_str(), it->second);
        }
    } else if (operation == REMOVE_SCENE_TYPE) {
        auto it = std::find_if(sceneTypeCountList_.begin(), sceneTypeCountList_.end(),
            [sceneType](const std::pair<std::string, int32_t> &element) {
                return element.first == sceneType;
            });
        if (it == sceneTypeCountList_.end()) {
            AUDIO_WARNING_LOG("scene type %{public}s to be removed is not found", sceneType.c_str());
            return;
        }
        if (it->second <= 1) {
            sceneTypeCountList_.erase(it);
            AUDIO_INFO_LOG("scene Type %{public}s is removed", sceneType.c_str());
        } else {
            it->second--;
            AUDIO_INFO_LOG("scene Type %{public}s count is decreased to %{public}d", sceneType.c_str(), it->second);
        }
    } else {
        AUDIO_ERR_LOG("Wrong operation to SceneTypeToEffectChainCountBackupMap.");
    }
}

void AudioEffectChainManager::FindMaxSessionID(uint32_t &maxSessionID, std::string &sceneType,
    const std::string &scenePairType, std::set<std::string> &sessions)
{
    for (auto &sessionID : sessions) {
        if (sessionIDToEffectInfoMap_[sessionID].sceneMode == "EFFECT_NONE") {
            continue;
        }
        uint32_t sessionIDInt = static_cast<uint32_t>(std::stoul(sessionID));
        if (sessionIDInt > maxSessionID) {
            maxSessionID = sessionIDInt;
            sceneType = scenePairType;
        }
    }
}
} // namespace AudioStandard
} // namespace OHOS
