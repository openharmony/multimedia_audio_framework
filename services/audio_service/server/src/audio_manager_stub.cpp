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

#include "audio_manager_base.h"
#include "audio_system_manager.h"
#include "audio_log.h"
#include "i_audio_process.h"
#include "audio_effect_server.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
static void LoadEffectLibrariesReadData(vector<Library>& libList, vector<Effect>& effectList, MessageParcel &data,
    int32_t countLib, int32_t countEff)
{
    int32_t i;
    for (i = 0; i < countLib; i++) {
        string libName = data.ReadString();
        string libPath = data.ReadString();
        libList.push_back({libName, libPath});
    }
    for (i = 0; i < countEff; i++) {
        string effectName = data.ReadString();
        string libName = data.ReadString();
        effectList.push_back({effectName, libName});
    }
}

static void LoadEffectLibrariesWriteReply(const vector<Effect>& successEffectList, MessageParcel &reply)
{
    reply.WriteInt32(successEffectList.size());
    for (Effect effect: successEffectList) {
        reply.WriteString(effect.name);
        reply.WriteString(effect.libraryName);
    }
}

int AudioManagerStub::HandleGetAudioParameter(MessageParcel &data, MessageParcel &reply)
{
    const std::string key = data.ReadString();
    const std::string value = GetAudioParameter(key);
    reply.WriteString(value);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetAudioParameter(MessageParcel &data, MessageParcel &reply)
{
    const std::string key = data.ReadString();
    const std::string value = data.ReadString();
    SetAudioParameter(key, value);
    return AUDIO_OK;
}

int AudioManagerStub::HandleGetExtraAudioParameters(MessageParcel &data, MessageParcel &reply)
{
    const std::string mainKey = data.ReadString();
    int32_t num = data.ReadInt32();
    std::vector<std::string> subKeys = {};
    for (int32_t i = 0; i < num; i++) {
        std::string subKey = data.ReadString();
        subKeys.push_back(subKey);
    }

    const std::vector<std::pair<std::string, std::string>> value = GetExtraParameters(mainKey, subKeys);
    reply.WriteInt32(static_cast<int32_t>(value.size()));
    for (auto it = value.begin(); it != value.end(); it++) {
        reply.WriteString(static_cast<std::string>(it->first));
        reply.WriteString(static_cast<std::string>(it->second));
    }
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetExtraAudioParameters(MessageParcel &data, MessageParcel &reply)
{
    const std::string mainKey = data.ReadString();
    std::vector<std::pair<std::string, std::string>> audioParametersSubKVPairs;
    int32_t mapSize = data.ReadInt32();
    for (int32_t i = 0; i < mapSize; i++) {
        std::string subKey = data.ReadString();
        std::string value = data.ReadString();
        audioParametersSubKVPairs.push_back(std::make_pair(subKey, value));
    }
    SetExtraParameters(mainKey, audioParametersSubKVPairs);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetMicrophoneMute(MessageParcel &data, MessageParcel &reply)
{
    bool isMute = data.ReadBool();
    int32_t result = SetMicrophoneMute(isMute);
    reply.WriteInt32(result);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetAudioScene(MessageParcel &data, MessageParcel &reply)
{
    AudioScene audioScene = (static_cast<AudioScene>(data.ReadInt32()));
    DeviceType activeDevice = (static_cast<DeviceType>(data.ReadInt32()));
    int32_t result = SetAudioScene(audioScene, activeDevice);
    reply.WriteInt32(result);
    return AUDIO_OK;
}

int AudioManagerStub::HandleUpdateActiveDeviceRoute(MessageParcel &data, MessageParcel &reply)
{
    DeviceType type = static_cast<DeviceType>(data.ReadInt32());
    DeviceFlag flag = static_cast<DeviceFlag>(data.ReadInt32());
    int32_t ret = UpdateActiveDeviceRoute(type, flag);
    reply.WriteInt32(ret);
    return AUDIO_OK;
}

int AudioManagerStub::HandleGetTransactionId(MessageParcel &data, MessageParcel &reply)
{
    DeviceType deviceType = (static_cast<DeviceType>(data.ReadInt32()));
    DeviceRole deviceRole = (static_cast<DeviceRole>(data.ReadInt32()));
    uint64_t transactionId = GetTransactionId(deviceType, deviceRole);
    reply.WriteUint64(transactionId);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetParameterCallback(MessageParcel &data, MessageParcel &reply)
{
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    CHECK_AND_RETURN_RET_LOG(object != nullptr, AUDIO_ERR, "SET_PARAMETER_CALLBACK obj is null");
    int32_t result = SetParameterCallback(object);
    reply.WriteInt32(result);
    return AUDIO_OK;
}

int AudioManagerStub::HandleGetRemoteAudioParameter(MessageParcel &data, MessageParcel &reply)
{
    const std::string networkId = data.ReadString();
    AudioParamKey key = static_cast<AudioParamKey>(data.ReadInt32());
    const std::string condition = data.ReadString();
    const std::string value = GetAudioParameter(networkId, key, condition);
    reply.WriteString(value);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetRemoteAudioParameter(MessageParcel &data, MessageParcel &reply)
{
    const std::string networkId = data.ReadString();
    AudioParamKey key = static_cast<AudioParamKey>(data.ReadInt32());
    const std::string condtion = data.ReadString();
    const std::string value = data.ReadString();
    SetAudioParameter(networkId, key, condtion, value);
    return AUDIO_OK;
}

int AudioManagerStub::HandleNotifyDeviceInfo(MessageParcel &data, MessageParcel &reply)
{
    const std::string networkId = data.ReadString();
    const bool connected = data.ReadBool();
    NotifyDeviceInfo(networkId, connected);
    return AUDIO_OK;
}

int AudioManagerStub::HandleCheckRemoteDeviceState(MessageParcel &data, MessageParcel &reply)
{
    std::string networkId = data.ReadString();
    DeviceRole deviceRole = static_cast<DeviceRole>(data.ReadInt32());
    bool isStartDevice = data.ReadBool();
    int32_t result = CheckRemoteDeviceState(networkId, deviceRole, isStartDevice);
    reply.WriteInt32(result);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetVoiceVolume(MessageParcel &data, MessageParcel &reply)
{
    const float volume = data.ReadFloat();
    int32_t result = SetVoiceVolume(volume);
    reply.WriteInt32(result);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetAudioMonoState(MessageParcel &data, MessageParcel &reply)
{
    bool audioMonoState = data.ReadBool();
    SetAudioMonoState(audioMonoState);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetAudioBalanceValue(MessageParcel &data, MessageParcel &reply)
{
    float audioBalanceValue = data.ReadFloat();
    SetAudioBalanceValue(audioBalanceValue);
    return AUDIO_OK;
}

int AudioManagerStub::HandleCreateAudioProcess(MessageParcel &data, MessageParcel &reply)
{
    AudioProcessConfig config;
    ProcessConfig::ReadConfigFromParcel(config, data);
    sptr<IRemoteObject> process = CreateAudioProcess(config);
    CHECK_AND_RETURN_RET_LOG(process != nullptr, AUDIO_ERR,
        "CREATE_AUDIOPROCESS AudioManagerStub CreateAudioProcess failed");
    reply.WriteRemoteObject(process);
    return AUDIO_OK;
}

int AudioManagerStub::HandleLoadAudioEffectLibraries(MessageParcel &data, MessageParcel &reply)
{
    vector<Library> libList = {};
    vector<Effect> effectList = {};
    int32_t countLib = data.ReadInt32();
    int32_t countEff = data.ReadInt32();
    CHECK_AND_RETURN_RET_LOG((countLib >= 0) && (countLib <= AUDIO_EFFECT_COUNT_UPPER_LIMIT) &&
        (countEff >= 0) && (countEff <= AUDIO_EFFECT_COUNT_UPPER_LIMIT), AUDIO_ERR,
        "LOAD_AUDIO_EFFECT_LIBRARIES read data failed");
    LoadEffectLibrariesReadData(libList, effectList, data, countLib, countEff);
    if (countLib > 0) {
        //load lib and reply success list
        vector<Effect> successEffectList = {};
        bool loadSuccess = LoadAudioEffectLibraries(libList, effectList, successEffectList);
        CHECK_AND_RETURN_RET_LOG(loadSuccess, AUDIO_ERR, "Load audio effect libraries failed, please check log");
        LoadEffectLibrariesWriteReply(successEffectList, reply);
    }
    return AUDIO_OK;
}

int AudioManagerStub::HandleRequestThreadPriority(MessageParcel &data, MessageParcel &reply)
{
    uint32_t tid = data.ReadUint32();
    string bundleName = data.ReadString();
    RequestThreadPriority(tid, bundleName);
    return AUDIO_OK;
}

int AudioManagerStub::HandleCreateAudioEffectChainManager(MessageParcel &data, MessageParcel &reply)
{
    int32_t i;
    vector<EffectChain> effectChains = {};
    vector<int32_t> countEffect = {};
    int32_t countChains = data.ReadInt32();
    CHECK_AND_RETURN_RET_LOG(countChains >= 0 && countChains <= AUDIO_EFFECT_CHAIN_COUNT_UPPER_LIMIT,
        AUDIO_ERR, "Create audio effect chain manager failed, please check log");
    for (i = 0; i < countChains; i++) {
        int32_t count = data.ReadInt32();
        CHECK_AND_RETURN_RET_LOG(count >= 0 && count <= AUDIO_EFFECT_COUNT_PER_CHAIN_UPPER_LIMIT,
            AUDIO_ERR, "Create audio effect chain manager failed, please check log");
        countEffect.emplace_back(count);
    }

    for (int32_t count: countEffect) {
        EffectChain effectChain;
        effectChain.name = data.ReadString();
        for (i = 0; i < count; i++) {
            effectChain.apply.emplace_back(data.ReadString());
        }
        effectChains.emplace_back(effectChain);
    }

    unordered_map<string, string> sceneTypeToEffectChainNameMap;
    int32_t mapSize = data.ReadInt32();
    CHECK_AND_RETURN_RET_LOG(mapSize >= 0 && mapSize <= AUDIO_EFFECT_CHAIN_CONFIG_UPPER_LIMIT,
        AUDIO_ERR, "Create audio effect chain manager failed, please check log");
    for (i = 0; i < mapSize; i++) {
        string key = data.ReadString();
        string value = data.ReadString();
        sceneTypeToEffectChainNameMap[key] = value;
    }

    bool createSuccess = CreateEffectChainManager(effectChains, sceneTypeToEffectChainNameMap);
    CHECK_AND_RETURN_RET_LOG(createSuccess, AUDIO_ERR,
        "Create audio effect chain manager failed, please check log");
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetOutputDeviceSink(MessageParcel &data, MessageParcel &reply)
{
    int32_t deviceType = data.ReadInt32();
    CHECK_AND_RETURN_RET_LOG(deviceType >= DEVICE_TYPE_NONE && deviceType <= DEVICE_TYPE_MAX, AUDIO_ERR,
        "Set output device sink failed, please check log");
    std::string sinkName = data.ReadString();
    bool ret = SetOutputDeviceSink(deviceType, sinkName);
    CHECK_AND_RETURN_RET_LOG(ret, AUDIO_ERR, "Set output device sink failed, please check log");
    return AUDIO_OK;
}

int AudioManagerStub::HandleCreatePlaybackCapturerManager(MessageParcel &data, MessageParcel &reply)
{
    bool ret = CreatePlaybackCapturerManager();
    reply.WriteBool(ret);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetSupportStreamUsage(MessageParcel &data, MessageParcel &reply)
{
    vector<int32_t> usage;
    size_t cnt = static_cast<size_t>(data.ReadInt32());
    CHECK_AND_RETURN_RET_LOG(cnt <= AUDIO_SUPPORTED_STREAM_USAGES.size(), AUDIO_ERR,
        "Set support stream usage failed, please check");
    for (size_t i = 0; i < cnt; i++) {
        int32_t tmp_usage = data.ReadInt32();
        if (find(AUDIO_SUPPORTED_STREAM_USAGES.begin(), AUDIO_SUPPORTED_STREAM_USAGES.end(), tmp_usage) ==
            AUDIO_SUPPORTED_STREAM_USAGES.end()) {
            continue;
        }
        usage.emplace_back(tmp_usage);
    }
    int32_t ret = SetSupportStreamUsage(usage);
    reply.WriteInt32(ret);
    return AUDIO_OK;
}

int AudioManagerStub::HandleRegiestPolicyProvider(MessageParcel &data, MessageParcel &reply)
{
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    CHECK_AND_RETURN_RET_LOG(object != nullptr, AUDIO_ERR, "obj is null");
    int32_t result = RegiestPolicyProvider(object);
    reply.WriteInt32(result);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetWakeupSourceCallback(MessageParcel &data, MessageParcel &reply)
{
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    CHECK_AND_RETURN_RET_LOG(object != nullptr, AUDIO_ERR,
        "SET_WAKEUP_CLOSE_CALLBACK obj is null");
    int32_t result = SetWakeupSourceCallback(object);
    reply.WriteInt32(result);
    return AUDIO_OK;
}

int AudioManagerStub::HandleSetCaptureSilentState(MessageParcel &data, MessageParcel &reply)
{
    bool state = false;
    int32_t flag = data.ReadInt32();
    if (flag == 1) {
        state = true;
    }
    int32_t ret = SetCaptureSilentState(state);
    reply.WriteInt32(ret);
    return AUDIO_OK;
}

int AudioManagerStub::HandleUpdateSpatializationState(MessageParcel &data, MessageParcel &reply)
{
    AudioSpatializationState spatializationState;
    spatializationState.spatializationEnabled = data.ReadBool();
    spatializationState.headTrackingEnabled = data.ReadBool();
    int32_t ret = UpdateSpatializationState(spatializationState);
    reply.WriteInt32(ret);
    return AUDIO_OK;
}

int AudioManagerStub::HandleGetCapturePresentationPosition(MessageParcel &data, MessageParcel &reply)
{
    const std::string deviceClass = data.ReadString();
    uint64_t frames;
    int64_t timeSec;
    int64_t timeNanoSec;
    int32_t result = GetCapturePresentationPosition(deviceClass, frames, timeSec, timeNanoSec);
    reply.WriteInt32(result);
    reply.WriteUint64(frames);
    reply.WriteInt64(timeSec);
    reply.WriteInt64(timeNanoSec);

    return AUDIO_OK;
}

int AudioManagerStub::HandleGetRenderPresentationPosition(MessageParcel &data, MessageParcel &reply)
{
    const std::string deviceClass = data.ReadString();
    uint64_t frames;
    int64_t timeSec;
    int64_t timeNanoSec;
    int32_t result = GetRenderPresentationPosition(deviceClass, frames, timeSec, timeNanoSec);
    reply.WriteInt32(result);
    reply.WriteUint64(frames);
    reply.WriteInt64(timeSec);
    reply.WriteInt64(timeNanoSec);

    return AUDIO_OK;
}

int AudioManagerStub::HandleOffloadSetVolume(MessageParcel &data, MessageParcel &reply)
{
    const float volume = data.ReadFloat();
    int32_t result = OffloadSetVolume(volume);
    reply.WriteInt32(result);
    return AUDIO_OK;
}

int AudioManagerStub::HandleOffloadDrain(MessageParcel &data, MessageParcel &reply)
{
    int32_t result = OffloadDrain();
    reply.WriteInt32(result);
    return AUDIO_OK;
}

int AudioManagerStub::HandleOffloadGetPresentationPosition(MessageParcel &data, MessageParcel &reply)
{
    uint64_t frames;
    int64_t timeSec;
    int64_t timeNanoSec;
    int32_t result = OffloadGetPresentationPosition(frames, timeSec, timeNanoSec);
    reply.WriteInt32(result);
    reply.WriteUint64(frames);
    reply.WriteInt64(timeSec);
    reply.WriteInt64(timeNanoSec);

    return AUDIO_OK;
}

int AudioManagerStub::HandleOffloadSetBufferSize(MessageParcel &data, MessageParcel &reply)
{
    uint32_t sizeMs = data.ReadUint32();
    int32_t result = OffloadSetBufferSize(sizeMs);
    reply.WriteInt32(result);
    return AUDIO_OK;
}

int AudioManagerStub::HandleNotifyStreamVolumeChanged(MessageParcel &data, MessageParcel &reply)
{
    AudioStreamType type = static_cast<AudioStreamType>(data.ReadInt32());
    float volume = data.ReadFloat();
    int32_t ret = NotifyStreamVolumeChanged(type, volume);
    reply.WriteInt32(ret);

    return AUDIO_OK;
}

int AudioManagerStub::OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    CHECK_AND_RETURN_RET_LOG(data.ReadInterfaceToken() == GetDescriptor(),
        -1, "ReadInterfaceToken failed");

    CHECK_AND_RETURN_RET(code > static_cast<uint32_t>(AudioServerInterfaceCode::AUDIO_SERVER_CODE_MAX),
        (this->*handlers[code])(data, reply));
    AUDIO_ERR_LOG("default case, need check AudioManagerStub");
    return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
}
} // namespace AudioStandard
} // namespace OHOS
