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

#include "audio_manager_proxy.h"

#include <cinttypes>

#include "audio_system_manager.h"
#include "audio_log.h"
#include "i_audio_process.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
AudioManagerProxy::AudioManagerProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IStandardAudioService>(impl)
{
}

int32_t AudioManagerProxy::SetMicrophoneMute(bool isMute)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "AudioManagerProxy: WriteInterfaceToken failed");
    data.WriteBool(isMute);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_MICROPHONE_MUTE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error,
        "SetMicrophoneMute failed, error: %d", error);

    int32_t result = reply.ReadInt32();
    return result;
}

int32_t AudioManagerProxy::SetVoiceVolume(float volume)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    data.WriteFloat(volume);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_VOICE_VOLUME), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false,
        "SetVoiceVolume failed, error: %d", error);

    int32_t result = reply.ReadInt32();
    return result;
}

int32_t AudioManagerProxy::OffloadSetVolume(float volume)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    data.WriteFloat(volume);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::OFFLOAD_SET_VOLUME), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "OffloadSetVolume failed, error: %d", error);

    int32_t result = reply.ReadInt32();
    return result;
}

int32_t AudioManagerProxy::OffloadDrain()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::OFFLOAD_DRAIN), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "OffloadDrain failed, error: %d", error);

    int32_t result = reply.ReadInt32();
    return result;
}

int32_t AudioManagerProxy::GetCapturePresentationPosition(const std::string& deviceClass, uint64_t& frames,
    int64_t& timeSec, int64_t& timeNanoSec)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioManagerProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteString(deviceClass);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::GET_CAPTURE_PRESENTATION_POSITION), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetCapturePresentationPosition failed, error: %d", error);
        return false;
    }

    int32_t result = reply.ReadInt32();
    frames = reply.ReadUint64();
    timeSec = reply.ReadInt64();
    timeNanoSec = reply.ReadInt64();
    AUDIO_DEBUG_LOG("ret %{public}d, frames %{public}" PRIu64 ", sec %{public}" PRId64 ", Nasec %{public}" PRId64,
        result, frames, timeSec, timeNanoSec);
    return result;
}

int32_t AudioManagerProxy::GetRenderPresentationPosition(const std::string& deviceClass, uint64_t& frames,
    int64_t& timeSec, int64_t& timeNanoSec)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioManagerProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteString(deviceClass);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::GET_RENDER_PRESENTATION_POSITION), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetRenderPresentationPosition failed, error: %d", error);
        return false;
    }

    int32_t result = reply.ReadInt32();
    frames = reply.ReadUint64();
    timeSec = reply.ReadInt64();
    timeNanoSec = reply.ReadInt64();
    AUDIO_DEBUG_LOG("ret %{public}d, frames %{public}" PRIu64 ", sec %{public}" PRId64 ", Nasec %{public}" PRId64,
        result, frames, timeSec, timeNanoSec);
    return result;
}

int32_t AudioManagerProxy::OffloadGetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::OFFLOAD_GET_PRESENTATION_POSITION), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "OffloadGetPresentationPosition failed, error: %d", error);

    int32_t result = reply.ReadInt32();
    frames = reply.ReadUint64();
    timeSec = reply.ReadInt64();
    timeNanoSec = reply.ReadInt64();
    AUDIO_DEBUG_LOG("ret %{public}d, frames %{public}" PRIu64 ", sec %{public}" PRId64 ", Nasec %{public}" PRId64,
        result, frames, timeSec, timeNanoSec);
    return result;
}

int32_t AudioManagerProxy::OffloadSetBufferSize(uint32_t sizeMs)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    data.WriteUint32(sizeMs);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::OFFLOAD_SET_BUFFER_SIZE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "OffloadSetBufferSize failed, error: %d", error);

    int32_t result = reply.ReadInt32();
    return result;
}

int32_t AudioManagerProxy::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    data.WriteInt32(static_cast<int32_t>(audioScene));
    data.WriteInt32(static_cast<int32_t>(activeDevice));

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_AUDIO_SCENE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "SetAudioScene failed, error: %d", error);

    int32_t result = reply.ReadInt32();
    return result;
}

const std::string AudioManagerProxy::GetAudioParameter(const std::string &key)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, "", "WriteInterfaceToken failed");
    data.WriteString(static_cast<std::string>(key));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::GET_AUDIO_PARAMETER), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("Get audio parameter failed, error: %d", error);
        const std::string value = "";
        return value;
    }

    const std::string value = reply.ReadString();
    return value;
}

const std::string AudioManagerProxy::GetAudioParameter(const std::string& networkId, const AudioParamKey key,
    const std::string& condition)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, "", "WriteInterfaceToken failed");
    data.WriteString(static_cast<std::string>(networkId));
    data.WriteInt32(static_cast<int32_t>(key));
    data.WriteString(static_cast<std::string>(condition));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::GET_REMOTE_AUDIO_PARAMETER), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("Get audio parameter failed, error: %d", error);
        const std::string value = "";
        return value;
    }

    const std::string value = reply.ReadString();
    return value;
}

const std::vector<std::pair<std::string, std::string>> AudioManagerProxy::GetExtraParameters(
    const std::string &mainKey, const std::vector<std::string> &subKeys)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, {}, "WriteInterfaceToken failed");
    data.WriteString(static_cast<std::string>(mainKey));
    data.WriteInt32(static_cast<int32_t>(subKeys.size()));
    for (std::string subKey : subKeys) {
        data.WriteString(static_cast<std::string>(subKey));
    }
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::GET_EXTRA_AUDIO_PARAMETERS), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("Get extra audio parameters failed, error: %d", error);
        return {};
    }

    std::vector<std::pair<std::string, std::string>> valueMap;
    int32_t num = reply.ReadInt32();
    for (auto i = 0; i < num; i++) {
        const std::string key = reply.ReadString();
        const std::string value = reply.ReadString();
        valueMap.push_back(std::make_pair(key, value));
    }
    return valueMap;
}

void AudioManagerProxy::SetAudioParameter(const std::string &key, const std::string &value)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_LOG(ret, "WriteInterfaceToken failed");
    data.WriteString(static_cast<std::string>(key));
    data.WriteString(static_cast<std::string>(value));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_AUDIO_PARAMETER), data, reply, option);
    CHECK_AND_RETURN_LOG(error == ERR_NONE, "Get audio parameter failed, error: %d", error);
}

void AudioManagerProxy::SetAudioParameter(const std::string& networkId, const AudioParamKey key,
    const std::string& condition, const std::string& value)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_LOG(ret, "WriteInterfaceToken failed");
    data.WriteString(static_cast<std::string>(networkId));
    data.WriteInt32(static_cast<int32_t>(key));
    data.WriteString(static_cast<std::string>(condition));
    data.WriteString(static_cast<std::string>(value));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_REMOTE_AUDIO_PARAMETER), data, reply, option);
    CHECK_AND_RETURN_LOG(error == ERR_NONE, "Get audio parameter failed, error: %d", error);
}

void AudioManagerProxy::SetExtraParameters(const std::string &key,
    const std::vector<std::pair<std::string, std::string>> &kvpairs)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_LOG(ret, "WriteInterfaceToken failed");
    data.WriteString(static_cast<std::string>(key));
    data.WriteInt32(static_cast<int32_t>(kvpairs.size()));
    for (auto it = kvpairs.begin(); it != kvpairs.end(); it++) {
        data.WriteString(static_cast<std::string>(it->first));
        data.WriteString(static_cast<std::string>(it->second));
    }

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_EXTRA_AUDIO_PARAMETERS), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("Set extra audio parameters failed, error: %d", error);
        return;
    }
}

uint64_t AudioManagerProxy::GetTransactionId(DeviceType deviceType, DeviceRole deviceRole)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    uint32_t transactionId = 0;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, transactionId, "WriteInterfaceToken failed");

    data.WriteInt32(static_cast<int32_t>(deviceType));
    data.WriteInt32(static_cast<int32_t>(deviceRole));

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::GET_TRANSACTION_ID), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, transactionId, "get transaction id failed, error: %d", error);

    transactionId = reply.ReadUint64();

    return transactionId;
}

void AudioManagerProxy::NotifyDeviceInfo(std::string networkId, bool connected)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_LOG(ret, "WriteInterfaceToken failed");
    data.WriteString(networkId);
    data.WriteBool(connected);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::NOTIFY_DEVICE_INFO), data, reply, option);
    CHECK_AND_RETURN_LOG(error == ERR_NONE, "Get audio parameter failed, error: %d", error);
}

int32_t AudioManagerProxy::CheckRemoteDeviceState(std::string networkId, DeviceRole deviceRole, bool isStartDevice)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERR_TRANSACTION_FAILED, "WriteInterfaceToken failed");
    data.WriteString(networkId);
    data.WriteInt32(static_cast<int32_t>(deviceRole));
    data.WriteBool(isStartDevice);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::CHECK_REMOTE_DEVICE_STATE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "CheckRemoteDeviceState failed in proxy, error: %d", error);
    return reply.ReadInt32();
}

int32_t AudioManagerProxy::UpdateActiveDeviceRoute(DeviceType type, DeviceFlag flag)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    data.WriteInt32(type);
    data.WriteInt32(flag);

    auto error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::UPDATE_ROUTE_REQ), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "UpdateActiveDeviceRoute failed, error: %{public}d", error);

    auto result = reply.ReadInt32();
    AUDIO_DEBUG_LOG("[UPDATE_ROUTE_REQ] result %{public}d", result);
    return result;
}

int32_t AudioManagerProxy::SetParameterCallback(const sptr<IRemoteObject>& object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_NULL_OBJECT, "object is null");

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_PARAMETER_CALLBACK), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error,
        "SetParameterCallback failed, error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioManagerProxy::RegiestPolicyProvider(const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);

    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_NULL_OBJECT, "object is null");
    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(static_cast<uint32_t>(AudioServerInterfaceCode::REGISET_POLICY_PROVIDER), data,
        reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error,
        "RegiestPolicyProvider failed, error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioManagerProxy::SetWakeupSourceCallback(const sptr<IRemoteObject>& object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_NULL_OBJECT, "object is null");
    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_WAKEUP_CLOSE_CALLBACK), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error,
        "SetWakeupCloseCallback failed, error: %{public}d", error);

    return reply.ReadInt32();
}

void AudioManagerProxy::SetAudioMonoState(bool audioMono)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_LOG(ret, "WriteInterfaceToken failed");
    (void)data.WriteBool(audioMono);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_AUDIO_MONO_STATE), data, reply, option);
    CHECK_AND_RETURN_LOG(error == ERR_NONE, "SetAudioMonoState failed, error: %{public}d", error);
}

void AudioManagerProxy::SetAudioBalanceValue(float audioBalance)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_LOG(ret, "WriteInterfaceToken failed");
    (void)data.WriteFloat(audioBalance);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_AUDIO_BALANCE_VALUE), data, reply, option);
    CHECK_AND_RETURN_LOG(error == ERR_NONE, "SetAudioBalanceValue failed, error: %{public}d", error);
}

sptr<IRemoteObject> AudioManagerProxy::CreateAudioProcess(const AudioProcessConfig &config)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, nullptr, "WriteInterfaceToken failed");
    ProcessConfig::WriteConfigToParcel(config, data);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::CREATE_AUDIOPROCESS), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, nullptr, "CreateAudioProcess failed, error: %{public}d", error);
    sptr<IRemoteObject> process = reply.ReadRemoteObject();
    return process;
}

bool AudioManagerProxy::LoadAudioEffectLibraries(const vector<Library> libraries, const vector<Effect> effects,
    vector<Effect> &successEffects)
{
    int32_t error;
    int32_t i;

    MessageParcel dataParcel;
    MessageParcel replyParcel;
    MessageOption option;
    bool ret = dataParcel.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");

    int32_t countLib = libraries.size();
    int32_t countEff = effects.size();

    dataParcel.WriteInt32(countLib);
    dataParcel.WriteInt32(countEff);

    for (Library x : libraries) {
        dataParcel.WriteString(x.name);
        dataParcel.WriteString(x.path);
    }

    for (Effect x : effects) {
        dataParcel.WriteString(x.name);
        dataParcel.WriteString(x.libraryName);
    }

    error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::LOAD_AUDIO_EFFECT_LIBRARIES), dataParcel, replyParcel, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "LoadAudioEffectLibraries failed, error: %{public}d", error);
        
    int32_t successEffSize = replyParcel.ReadInt32();
    CHECK_AND_RETURN_RET_LOG((successEffSize >= 0) && (successEffSize <= AUDIO_EFFECT_COUNT_UPPER_LIMIT),
        false, "LOAD_AUDIO_EFFECT_LIBRARIES read replyParcel failed");

    for (i = 0; i < successEffSize; i++) {
        string effectName = replyParcel.ReadString();
        string libName = replyParcel.ReadString();
        successEffects.push_back({effectName, libName});
    }

    return true;
}

void AudioManagerProxy::RequestThreadPriority(uint32_t tid, string bundleName)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_LOG(ret, "WriteInterfaceToken failed");
    (void)data.WriteUint32(tid);
    (void)data.WriteString(bundleName);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::REQUEST_THREAD_PRIORITY), data, reply, option);
    CHECK_AND_RETURN_LOG(error == ERR_NONE, "RequestThreadPriority failed, error: %{public}d", error);
}

bool AudioManagerProxy::CreateEffectChainManager(std::vector<EffectChain> &effectChains,
    std::unordered_map<std::string, std::string> &map)
{
    int32_t error;

    MessageParcel dataParcel;
    MessageParcel replyParcel;
    MessageOption option;

    bool ret = dataParcel.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");

    int32_t countEffectChains = effectChains.size();
    std::vector<int32_t> listCountEffects;

    for (EffectChain &effectChain: effectChains) {
        listCountEffects.emplace_back(effectChain.apply.size());
    }

    dataParcel.WriteInt32(countEffectChains);
    for (int32_t countEffects: listCountEffects) {
        dataParcel.WriteInt32(countEffects);
    }

    for (EffectChain &effectChain: effectChains) {
        dataParcel.WriteString(effectChain.name);
        for (std::string applyName: effectChain.apply) {
            dataParcel.WriteString(applyName);
        }
    }

    dataParcel.WriteInt32(map.size());
    for (auto item = map.begin(); item != map.end(); ++item) {
        dataParcel.WriteString(item->first);
        dataParcel.WriteString(item->second);
    }

    error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::CREATE_AUDIO_EFFECT_CHAIN_MANAGER),
        dataParcel, replyParcel, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false,
        "CreateAudioEffectChainManager failed, error: %{public}d", error);
    return true;
}

bool AudioManagerProxy::SetOutputDeviceSink(int32_t deviceType, std::string &sinkName)
{
    int32_t error;

    MessageParcel dataParcel;
    MessageParcel replyParcel;
    MessageOption option;
    bool ret = dataParcel.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    dataParcel.WriteInt32(deviceType);
    dataParcel.WriteString(sinkName);

    error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_OUTPUT_DEVICE_SINK), dataParcel, replyParcel, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false,
        "SetOutputDeviceSink failed, error: %{public}d", error);
    return true;
}

bool AudioManagerProxy::CreatePlaybackCapturerManager()
{
    int32_t error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");

    error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::CREATE_PLAYBACK_CAPTURER_MANAGER), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false,
        "CreatePlaybackCapturerManager failed, error: %{public}d", error);

    return reply.ReadBool();
}

int32_t AudioManagerProxy::SetSupportStreamUsage(std::vector<int32_t> usage)
{
    int32_t error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    int32_t cnt = (int32_t)usage.size();
    data.WriteInt32(cnt);
    for (int32_t i = 0; i < cnt; i++) {
        data.WriteInt32(usage[i]);
    }

    error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::SET_SUPPORT_STREAM_USAGE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error,
        "SetSupportStreamUsage failed, error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioManagerProxy::SetCaptureSilentState(bool state)
{
    int32_t error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    data.WriteInt32(static_cast<int32_t>(state));
    error = Remote()->SendRequest(static_cast<uint32_t>(AudioServerInterfaceCode::SET_CAPTURE_SILENT_STATE),
        data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error,
        "SetCaptureSilentState failed, error: %{public}d", error);
    return reply.ReadInt32();
}

int32_t AudioManagerProxy::NotifyStreamVolumeChanged(AudioStreamType streamType, float volume)
{
    int32_t error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("NotifyStreamVolumeChanged: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(static_cast<int32_t>(streamType));
    data.WriteFloat(volume);
    error = Remote()->SendRequest(static_cast<uint32_t>(AudioServerInterfaceCode::NOTIFY_STREAM_VOLUME_CHANGED),
        data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("NotifyStreamVolumeChanged failed, error: %{public}d", error);
        return error;
    }
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "NotifyStreamVolumeChanged failed, error: %{public}d", error);
    return reply.ReadInt32();
}

int32_t AudioManagerProxy::UpdateSpatializationState(AudioSpatializationState spatializationState)
{
    int32_t error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteBool(spatializationState.spatializationEnabled);
    data.WriteBool(spatializationState.headTrackingEnabled);

    error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioServerInterfaceCode::UPDATE_SPATIALIZATION_STATE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error,
        "UpdateSpatializationState failed, error: %{public}d", error);

    return reply.ReadInt32();
}
} // namespace AudioStandard
} // namespace OHOS
