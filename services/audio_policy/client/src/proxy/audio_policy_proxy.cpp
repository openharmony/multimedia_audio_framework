/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#define LOG_TAG "AudioPolicyProxy"
#endif


#include "audio_policy_log.h"
#include "audio_policy_proxy.h"


namespace {
constexpr int MAX_PID_COUNT = 1000;
}

namespace OHOS {
namespace AudioStandard {
using namespace std;

AudioPolicyProxy::AudioPolicyProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IAudioPolicy>(impl)
{
}

void AudioPolicyProxy::WriteStreamChangeInfo(MessageParcel &data,
    const AudioMode &mode, const AudioStreamChangeInfo &streamChangeInfo)
{
    if (mode == AUDIO_MODE_PLAYBACK) {
        streamChangeInfo.audioRendererChangeInfo.Marshalling(data);
    } else {
        streamChangeInfo.audioCapturerChangeInfo.Marshalling(data);
    }
}

int32_t AudioPolicyProxy::SetRingerModeLegacy(AudioRingerMode ringMode)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int>(ringMode));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_RINGER_MODE_LEGACY), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "set SetRingerModeLegacy failed, error: %d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetRingerMode(AudioRingerMode ringMode)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int>(ringMode));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_RINGER_MODE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "set ringermode failed, error: %d", error);

    return reply.ReadInt32();
}

#ifdef FEATURE_DTMF_TONE
std::vector<int32_t> AudioPolicyProxy::GetSupportedTones()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int32_t lListSize = 0;
    AUDIO_DEBUG_LOG("get GetSupportedTones,");
    std::vector<int> lSupportedToneList = {};
    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, lSupportedToneList, "WriteInterfaceToken failed");
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SUPPORTED_TONES), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get ringermode failed, error: %d", error);
    }
    lListSize = reply.ReadInt32();
    for (int i = 0; i < lListSize; i++) {
        lSupportedToneList.push_back(reply.ReadInt32());
    }
    AUDIO_DEBUG_LOG("get GetSupportedTones, %{public}d", lListSize);
    return lSupportedToneList;
}

std::shared_ptr<ToneInfo> AudioPolicyProxy::GetToneConfig(int32_t ltonetype)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::shared_ptr<ToneInfo> spToneInfo =  std::make_shared<ToneInfo>();
    if (spToneInfo == nullptr) {
        return nullptr;
    }
    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, spToneInfo, "WriteInterfaceToken failed");
    data.WriteInt32(ltonetype);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_TONEINFO), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get toneinfo failed, error: %d", error);
    }

    spToneInfo->Unmarshalling(reply);
    AUDIO_DEBUG_LOG("get rGetToneConfig returned,");
    return spToneInfo;
}
#endif

bool AudioPolicyProxy::GetPersistentMicMuteState()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MICROPHONE_MUTE_PERSISTENT), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "get persistent microphoneMute state failed, error: %d", error);

    return reply.ReadBool();
}

AudioRingerMode AudioPolicyProxy::GetRingerMode()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, RINGER_MODE_NORMAL, "WriteInterfaceToken failed");
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_RINGER_MODE), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get ringermode failed, error: %d", error);
    }
    return static_cast<AudioRingerMode>(reply.ReadInt32());
}

int32_t AudioPolicyProxy::SetAudioScene(AudioScene scene)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, RINGER_MODE_NORMAL, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int>(scene));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_AUDIO_SCENE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "set audio scene failed, error: %d", error);

    return reply.ReadInt32();
}

AudioScene AudioPolicyProxy::GetAudioScene()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, AUDIO_SCENE_DEFAULT, "WriteInterfaceToken failed");
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AUDIO_SCENE), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get audio scene failed, error: %d", error);
    }
    return static_cast<AudioScene>(reply.ReadInt32());
}

int32_t AudioPolicyProxy::SetStreamMuteLegacy(AudioVolumeType volumeType, bool mute)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(volumeType));
    data.WriteBool(mute);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_STREAM_MUTE_LEGACY), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "set mute failed, error: %d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetStreamMute(AudioVolumeType volumeType, bool mute)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(volumeType));
    data.WriteBool(mute);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_STREAM_MUTE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "set mute failed, error: %d", error);
    return reply.ReadInt32();
}

bool AudioPolicyProxy::GetStreamMute(AudioVolumeType volumeType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(volumeType));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_STREAM_MUTE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "get mute failed, error: %d", error);
    return reply.ReadBool();
}

bool AudioPolicyProxy::IsStreamActive(AudioVolumeType volumeType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(volumeType));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_STREAM_ACTIVE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "isStreamActive failed, error: %d", error);
    return reply.ReadBool();
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyProxy::GetDevices(DeviceFlag deviceFlag)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, deviceInfo, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(deviceFlag));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_DEVICES), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, deviceInfo, "Get devices failed, error: %d", error);

    int32_t size = reply.ReadInt32();
    for (int32_t i = 0; i < size; i++) {
        deviceInfo.push_back(AudioDeviceDescriptor::Unmarshalling(reply));
    }

    return deviceInfo;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyProxy::GetDevicesInner(DeviceFlag deviceFlag)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, deviceInfo, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(deviceFlag));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_DEVICES_INNER), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, deviceInfo, "Get devices failed, error: %d", error);

    int32_t size = reply.ReadInt32();
    for (int32_t i = 0; i < size; i++) {
        deviceInfo.push_back(AudioDeviceDescriptor::Unmarshalling(reply));
    }

    return deviceInfo;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyProxy::GetPreferredOutputDeviceDescriptors(
    AudioRendererInfo &rendererInfo)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, deviceInfo, "WriteInterfaceToken failed");

    bool res = rendererInfo.Marshalling(data);
    CHECK_AND_RETURN_RET_LOG(res, deviceInfo, "AudioRendererInfo Marshalling() failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_ACTIVE_OUTPUT_DEVICE_DESCRIPTORS), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, deviceInfo, "Get out devices failed, error: %d", error);

    int32_t size = reply.ReadInt32();
    for (int32_t i = 0; i < size; i++) {
        deviceInfo.push_back(AudioDeviceDescriptor::Unmarshalling(reply));
    }

    return deviceInfo;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyProxy::GetPreferredInputDeviceDescriptors(
    AudioCapturerInfo &captureInfo)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, deviceInfo, "WriteInterfaceToken failed");

    bool res = captureInfo.Marshalling(data);
    CHECK_AND_RETURN_RET_LOG(res, deviceInfo, "AudioCapturerInfo Marshalling() failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_PREFERRED_INTPUT_DEVICE_DESCRIPTORS), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, deviceInfo, "Get preferred input devices failed, error: %d", error);

    int32_t size = reply.ReadInt32();
    for (int32_t i = 0; i < size; i++) {
        deviceInfo.push_back(AudioDeviceDescriptor::Unmarshalling(reply));
    }

    return deviceInfo;
}

int32_t AudioPolicyProxy::SetDeviceActive(InternalDeviceType deviceType, bool active)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(deviceType));
    data.WriteBool(active);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_DEVICE_ACTIVE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "set device active failed, error: %d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::LoadSplitModule(const std::string &splitArgs, const std::string &networkId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteString(splitArgs);
    data.WriteString(networkId);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::LOAD_SPLIT_MODULE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "load split module failed, error: %{public}d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetVoiceRingtoneMute(bool isMute)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteBool(isMute);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_VOICE_RINGTONE_MUTE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "SetVoiceRingtoneMute failed, error: %{public}d", error);
    return reply.ReadInt32();
}

bool AudioPolicyProxy::IsDeviceActive(InternalDeviceType deviceType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(deviceType));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_DEVICE_ACTIVE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "is device active failed, error: %d", error);
    return reply.ReadBool();
}

DeviceType AudioPolicyProxy::GetActiveOutputDevice()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, DEVICE_TYPE_INVALID, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_ACTIVE_OUTPUT_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, DEVICE_TYPE_INVALID,
        "get active output device failed, error: %d", error);

    return static_cast<DeviceType>(reply.ReadInt32());
}

DeviceType AudioPolicyProxy::GetActiveInputDevice()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, DEVICE_TYPE_INVALID, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_ACTIVE_INPUT_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, DEVICE_TYPE_INVALID,
        "get active input device failed, error: %d", error);

    return static_cast<DeviceType>(reply.ReadInt32());
}

int32_t AudioPolicyProxy::SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    bool tmp = audioRendererFilter->Marshalling(data);
    CHECK_AND_RETURN_RET_LOG(tmp, -1, "AudioRendererFilter Marshalling() failed");

    uint32_t size = audioDeviceDescriptors.size();
    uint32_t validSize = 20; // Use 20 as limit.
    CHECK_AND_RETURN_RET_LOG(size > 0 && size <= validSize, -1, "SelectOutputDevice get invalid device size.");
    data.WriteInt32(size);
    for (auto audioDeviceDescriptor : audioDeviceDescriptors) {
        bool audioDeviceTmp = audioDeviceDescriptor->Marshalling(data);
        CHECK_AND_RETURN_RET_LOG(audioDeviceTmp, -1, "AudioDeviceDescriptor Marshalling() failed");
    }
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SELECT_OUTPUT_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "SelectOutputDevice failed, error: %{public}d", error);

    return reply.ReadInt32();
}
std::string AudioPolicyProxy::GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, "", "WriteInterfaceToken failed");
    data.WriteInt32(uid);
    data.WriteInt32(pid);
    data.WriteInt32(static_cast<int32_t>(streamType));
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SELECTED_DEVICE_INFO), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, "", "GetSelectedDeviceInfo failed, error: %{public}d", error);

    return reply.ReadString();
}

int32_t AudioPolicyProxy::SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    bool tmp = audioCapturerFilter->Marshalling(data);
    CHECK_AND_RETURN_RET_LOG(tmp, -1, "AudioCapturerFilter Marshalling() failed");

    uint32_t size = audioDeviceDescriptors.size();
    uint32_t validSize = 20; // Use 20 as limit.
    CHECK_AND_RETURN_RET_LOG(size > 0 && size <= validSize, -1, "SelectOutputDevice get invalid device size.");
    data.WriteInt32(size);
    for (auto audioDeviceDescriptor : audioDeviceDescriptors) {
        bool audioDeviceTmp = audioDeviceDescriptor->Marshalling(data);
        CHECK_AND_RETURN_RET_LOG(audioDeviceTmp, -1, "AudioDeviceDescriptor Marshalling() failed");
    }
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SELECT_INPUT_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "SelectInputDevice failed, error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::ConfigDistributedRoutingRole(const sptr<AudioDeviceDescriptor> descriptor, CastType type)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    CHECK_AND_RETURN_RET_LOG(descriptor != nullptr, -1, "descriptor is null");
    bool res = descriptor->Marshalling(data);
    CHECK_AND_RETURN_RET_LOG(res, -1, "AudioDeviceDescriptor marshalling failed");
    data.WriteInt32(static_cast<int32_t>(type));
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::CONFIG_DISTRIBUTED_ROUTING_ROLE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "failed error : %{public}d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::ActivateAudioSession(const AudioSessionStrategy &strategy)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(strategy.concurrencyMode));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::ACTIVATE_AUDIO_SESSION), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "Failed to activate audio session. Error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::DeactivateAudioSession()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::DEACTIVATE_AUDIO_SESSION), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "Failed to deactivate audio session. Error: %{public}d", error);

    return reply.ReadInt32();
}

bool AudioPolicyProxy::IsAudioSessionActivated()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_AUDIO_SESSION_ACTIVATED), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false,
        "Failed to query IsAudioSessionActivated, error: %{public}d", error);

    return reply.ReadBool();
}

void AudioPolicyProxy::ReadAudioFocusInfo(MessageParcel &reply,
    std::list<std::pair<AudioInterrupt, AudioFocuState>> &focusInfoList)
{
    std::pair<AudioInterrupt, AudioFocuState> focusInfo;
    AudioInterrupt::Unmarshalling(reply, focusInfo.first);
    focusInfo.second = static_cast<AudioFocuState>(reply.ReadInt32());
    focusInfoList.push_back(focusInfo);
}

int32_t AudioPolicyProxy::GetAudioFocusInfoList(std::list<std::pair<AudioInterrupt, AudioFocuState>> &focusInfoList,
    const int32_t zoneID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool res = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(res, ERROR, "WriteInterfaceToken failed");
    data.WriteInt32(zoneID);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AUDIO_FOCUS_INFO_LIST), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "GetAudioFocusInfoList, error: %d", error);
    int32_t ret = reply.ReadInt32();
    int32_t size = reply.ReadInt32();
    focusInfoList = {};
    if (ret < 0) {
        return ret;
    } else {
        for (int32_t i = 0; i < size; i++) {
            ReadAudioFocusInfo(reply, focusInfoList);
        }
        return SUCCESS;
    }
}

int32_t AudioPolicyProxy::ActivateAudioInterrupt(
    const AudioInterrupt &audioInterrupt, const int32_t zoneID, const bool isUpdatedAudioStrategy)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(zoneID);
    data.WriteBool(isUpdatedAudioStrategy);
    AudioInterrupt::Marshalling(data, audioInterrupt);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::ACTIVATE_INTERRUPT), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "activate interrupt failed, error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt, const int32_t zoneID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(zoneID);
    AudioInterrupt::Marshalling(data, audioInterrupt);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::DEACTIVATE_INTERRUPT), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "deactivate interrupt failed, error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::RequestAudioFocus(const int32_t clientId, const AudioInterrupt &audioInterrupt)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    data.WriteInt32(clientId);
    AudioInterrupt::Marshalling(data, audioInterrupt);

    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::REQUEST_AUDIO_FOCUS), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "activate interrupt failed, error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::AbandonAudioFocus(const int32_t clientId, const AudioInterrupt &audioInterrupt)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(clientId);
    AudioInterrupt::Marshalling(data, audioInterrupt);

    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::ABANDON_AUDIO_FOCUS), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "deactivate interrupt failed, error: %{public}d", error);

    return reply.ReadInt32();
}

AudioStreamType AudioPolicyProxy::GetStreamInFocus(const int32_t zoneID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, STREAM_DEFAULT, "WriteInterfaceToken failed");
    data.WriteInt32(zoneID);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_STREAM_IN_FOCUS), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get stream in focus failed, error: %d", error);
    }
    return static_cast<AudioStreamType>(reply.ReadInt32());
}

int32_t AudioPolicyProxy::GetSessionInfoInFocus(AudioInterrupt &audioInterrupt, const int32_t zoneID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(zoneID);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SESSION_INFO_IN_FOCUS), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy::GetSessionInfoInFocus failed, error: %d", error);
    }
    AudioInterrupt::Unmarshalling(reply, audioInterrupt);

    return reply.ReadInt32();
}

bool AudioPolicyProxy::CheckRecordingCreate(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid,
    SourceType sourceType)
{
    AUDIO_DEBUG_LOG("CheckRecordingCreate: [uid : %{public}d]", appUid);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    data.WriteUint32(appTokenId);
    data.WriteUint64(appFullTokenId);
    data.WriteInt32(appUid);
    data.WriteInt32(sourceType);

    int result = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::QUERY_MICROPHONE_PERMISSION), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(result == ERR_NONE, false, "CheckRecordingCreate failed, result: %{public}d", result);

    return reply.ReadBool();
}

bool AudioPolicyProxy::CheckRecordingStateChange(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid,
    AudioPermissionState state)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");

    data.WriteUint32(appTokenId);
    data.WriteUint64(appFullTokenId);
    data.WriteInt32(appUid);
    data.WriteInt32(state);

    int result = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_USING_PEMISSION_FROM_PRIVACY), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(result == ERR_NONE, false, "CheckRecordingStateChange failed, result: %{public}d", result);

    return reply.ReadBool();
}

int32_t AudioPolicyProxy::ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, IPC_PROXY_ERR, "WriteInterfaceToken failed");

    data.WriteUint32(count);
    data.WriteInt32(deviceType);

    int result = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::RECONFIGURE_CHANNEL), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(result == ERR_NONE, ERR_TRANSACTION_FAILED,
        "ReconfigureAudioChannel failed, result: %{public}d", result);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::GetAudioLatencyFromXml()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, IPC_PROXY_ERR, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AUDIO_LATENCY), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERR_TRANSACTION_FAILED,
        "GetAudioLatencyFromXml, error: %d", error);

    return reply.ReadInt32();
}

uint32_t AudioPolicyProxy::GetSinkLatencyFromXml()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, 0, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SINK_LATENCY), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, 0, "GetSinkLatencyFromXml, error: %d", error);

    return reply.ReadUint32();
}

int32_t AudioPolicyProxy::GetPreferredOutputStreamType(AudioRendererInfo &rendererInfo)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, AUDIO_FLAG_INVALID, "WriteInterfaceToken failed");

    ret = rendererInfo.Marshalling(data);
    CHECK_AND_RETURN_RET_LOG(ret, AUDIO_FLAG_INVALID, "Marshalling rendererInfo failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_PREFERRED_OUTPUT_STREAM_TYPE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, AUDIO_FLAG_INVALID, "Failed to send request, error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::GetPreferredInputStreamType(AudioCapturerInfo &capturerInfo)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, AUDIO_FLAG_INVALID, "WriteInterfaceToken failed");

    ret = capturerInfo.Marshalling(data);
    CHECK_AND_RETURN_RET_LOG(ret, AUDIO_FLAG_INVALID, "Marshalling capturerInfo failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_PREFERRED_INPUT_STREAM_TYPE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, AUDIO_FLAG_INVALID, "Failed to send request, error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
    const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");

    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_NULL_OBJECT, "Register Tracker Event object is null");

    data.WriteUint32(mode);
    WriteStreamChangeInfo(data, mode, streamChangeInfo);
    data.WriteRemoteObject(object);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::REGISTER_TRACKER), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "event failed , error: %d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");

    data.WriteUint32(mode);
    WriteStreamChangeInfo(data, mode, streamChangeInfo);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UPDATE_TRACKER), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "UpdateTracker event failed , error: %d", error);

    return reply.ReadInt32();
}

void AudioPolicyProxy::FetchOutputDeviceForTrack(AudioStreamChangeInfo &streamChangeInfo,
    const AudioStreamDeviceChangeReasonExt reason)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    if (!ret) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
    }

    streamChangeInfo.audioRendererChangeInfo.Marshalling(data);

    data.WriteInt32(static_cast<int32_t>(reason));

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::FETCH_OUTPUT_DEVICE_FOR_TRACK), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("event failed , error: %d", error);
    }
    return;
}

void AudioPolicyProxy::FetchInputDeviceForTrack(AudioStreamChangeInfo &streamChangeInfo)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    if (!ret) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
    }

    streamChangeInfo.audioCapturerChangeInfo.Marshalling(data);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::FETCH_INPUT_DEVICE_FOR_TRACK), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("event failed , error: %d", error);
    }
    return;
}

int32_t AudioPolicyProxy::GetCurrentRendererChangeInfos(
    vector<unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_RENDERER_CHANGE_INFOS), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "Get Renderer change info event failed , error: %d", error);

    int32_t size = reply.ReadInt32();
    while (size > 0) {
        unique_ptr<AudioRendererChangeInfo> rendererChangeInfo = make_unique<AudioRendererChangeInfo>();
        CHECK_AND_RETURN_RET_LOG(rendererChangeInfo != nullptr, ERR_MEMORY_ALLOC_FAILED, "No memory!!");
        rendererChangeInfo->Unmarshalling(reply);
        audioRendererChangeInfos.push_back(move(rendererChangeInfo));
        size--;
    }

    return SUCCESS;
}

int32_t AudioPolicyProxy::GetCurrentCapturerChangeInfos(
    vector<unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_DEBUG_LOG("AudioPolicyProxy::GetCurrentCapturerChangeInfos");

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_CAPTURER_CHANGE_INFOS), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "Get capturer change info event failed , error: %d", error);

    int32_t size = reply.ReadInt32();
    while (size > 0) {
        unique_ptr<AudioCapturerChangeInfo> capturerChangeInfo = make_unique<AudioCapturerChangeInfo>();
        CHECK_AND_RETURN_RET_LOG(capturerChangeInfo != nullptr, ERR_MEMORY_ALLOC_FAILED, "No memory!!");
        capturerChangeInfo->Unmarshalling(reply);
        audioCapturerChangeInfos.push_back(move(capturerChangeInfo));
        size--;
    }

    return SUCCESS;
}

int32_t AudioPolicyProxy::UpdateStreamState(const int32_t clientUid, StreamSetState streamSetState,
    StreamUsage streamUsage)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");

    data.WriteInt32(static_cast<int32_t>(clientUid));
    data.WriteInt32(static_cast<int32_t>(streamSetState));
    data.WriteInt32(static_cast<int32_t>(streamUsage));

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UPDATE_STREAM_STATE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR,
        "UPDATE_STREAM_STATE stream changed info event failed , error: %d", error);

    return SUCCESS;
}

int32_t AudioPolicyProxy::GetNetworkIdByGroupId(int32_t groupId, std::string &networkId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool res = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(res, ERROR, "WriteInterfaceToken failed");
    data.WriteInt32(groupId);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_NETWORKID_BY_GROUP_ID), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "GetNetworkIdByGroupId, error: %d", error);

    networkId = reply.ReadString();
    int32_t ret = reply.ReadInt32();
    return ret;
}

bool AudioPolicyProxy::IsAudioRendererLowLatencySupported(const AudioStreamInfo &audioStreamInfo)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, IPC_PROXY_ERR, "WriteInterfaceToken failed");
    audioStreamInfo.Marshalling(data);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_AUDIO_RENDER_LOW_LATENCY_SUPPORTED), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERR_TRANSACTION_FAILED,
        "IsAudioRendererLowLatencySupported, error: %d", error);

    return reply.ReadBool();
}

int32_t AudioPolicyProxy::SetSystemSoundUri(const std::string &key, const std::string &uri)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, IPC_PROXY_ERR, "WriteInterfaceToken failed");
    data.WriteString(key);
    data.WriteString(uri);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_SYSTEM_SOUND_URI), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "SetSystemSoundUri failed, error: %d", error);
    return reply.ReadInt32();
}

std::string AudioPolicyProxy::GetSystemSoundUri(const std::string &key)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, "", "WriteInterfaceToken failed");
    data.WriteString(key);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SYSTEM_SOUND_URI), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, "", "GetSystemSoundUri failed, error: %d", error);
    return reply.ReadString();
}

int32_t AudioPolicyProxy::CheckMaxRendererInstances()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::CHECK_MAX_RENDERER_INSTANCES), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "CheckMaxRendererInstances failed, error: %d", error);
    return reply.ReadInt32();
}

static void PreprocessMode(Stream &stream, MessageParcel &reply, int countMode)
{
    for (int j = 0; j < countMode; j++) {
        StreamEffectMode streamEffectMode;
        streamEffectMode.mode = reply.ReadString();
        int countDev = reply.ReadInt32();
        if (countDev > 0) {
            for (int k = 0; k < countDev; k++) {
                string type = reply.ReadString();
                string chain = reply.ReadString();
                streamEffectMode.devicePort.push_back({type, chain});
            }
        }
        stream.streamEffectMode.push_back(streamEffectMode);
    }
}

static Stream PreprocessProcess(MessageParcel &reply)
{
    Stream stream;
    stream.scene = reply.ReadString();
    int countMode = reply.ReadInt32();
    if (countMode > 0) {
        PreprocessMode(stream, reply, countMode);
    }
    return stream;
}

static void PostprocessMode(Stream &stream, MessageParcel &reply, int countMode)
{
    for (int j = 0; j < countMode; j++) {
        StreamEffectMode streamEffectMode;
        streamEffectMode.mode = reply.ReadString();
        int countDev = reply.ReadInt32();
        if (countDev > 0) {
            for (int k = 0; k < countDev; k++) {
                string type = reply.ReadString();
                string chain = reply.ReadString();
                streamEffectMode.devicePort.push_back({type, chain});
            }
        }
        stream.streamEffectMode.push_back(streamEffectMode);
    }
}

static Stream PostprocessProcess(MessageParcel &reply)
{
    Stream stream;
    stream.scene = reply.ReadString();
    int countMode = reply.ReadInt32();
    if (countMode > 0) {
        PostprocessMode(stream, reply, countMode);
    }
    return stream;
}

static int32_t QueryEffectSceneModeChkReply(uint32_t countPre, uint32_t countPost)
{
    CHECK_AND_RETURN_RET_LOG((countPre >= 0) && (countPre <= AUDIO_EFFECT_COUNT_UPPER_LIMIT), -1,
        "QUERY_EFFECT_SCENEMODE read replyParcel failed");
    CHECK_AND_RETURN_RET_LOG((countPost >= 0) && (countPost <= AUDIO_EFFECT_COUNT_UPPER_LIMIT), -1,
        "QUERY_EFFECT_SCENEMODE read replyParcel failed");
    return 0;
}

int32_t AudioPolicyProxy::QueryEffectSceneMode(SupportedEffectConfig &supportedEffectConfig)
{
    uint32_t i;
    int32_t error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::QUERY_EFFECT_SCENEMODE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "get scene & mode failed, error: %d", error);
    uint32_t countPre = reply.ReadUint32();
    uint32_t countPost = reply.ReadUint32();
    uint32_t countPostMap = reply.ReadUint32();
    error = QueryEffectSceneModeChkReply(countPre, countPost);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "get scene & mode failed, error: %d", error);
    // preprocess
    Stream stream;
    if (countPre > 0) {
        ProcessNew preProcessNew;
        for (i = 0; i < countPre; i++) {
            stream = PreprocessProcess(reply);
            preProcessNew.stream.push_back(stream);
        }
        supportedEffectConfig.preProcessNew = preProcessNew;
    }
    // postprocess
    if (countPost > 0) {
        ProcessNew postProcessNew;
        for (i = 0; i < countPost; i++) {
            stream = PostprocessProcess(reply);
            postProcessNew.stream.push_back(stream);
        }
        supportedEffectConfig.postProcessNew = postProcessNew;
    }
    if (countPostMap > 0) {
        SceneMappingItem item;
        for (i = 0; i < countPostMap; i++) {
            item.name = reply.ReadString();
            item.sceneType = reply.ReadString();
            supportedEffectConfig.postProcessSceneMap.push_back(item);
        }
    }
    return 0;
}

int32_t AudioPolicyProxy::SetPlaybackCapturerFilterInfos(const AudioPlaybackCaptureConfig &config, uint32_t appTokenId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(config.silentCapture));
    size_t ss = config.filterOptions.usages.size();
    data.WriteUint32(ss);
    for (size_t i = 0; i < ss; i++) {
        data.WriteInt32(static_cast<int32_t>(config.filterOptions.usages[i]));
    }
    data.WriteUint32(appTokenId);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_PLAYBACK_CAPTURER_FILTER_INFO), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "SetPlaybackCapturerFilterInfos failed, error: %d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetCaptureSilentState(bool state)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG(" SetCaptureSilentState WriteInterfaceToken failed");
        return ERROR;
    }
    data.WriteBool(state);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_CAPTURER_SILENT_STATE), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("SetCaptureSilentState failed, error: %d", error);
        return ERROR;
    }
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::GetHardwareOutputSamplingRate(const sptr<AudioDeviceDescriptor> &desc)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    bool result = desc->Marshalling(data);
    CHECK_AND_RETURN_RET_LOG(result, -1, "AudioDeviceDescriptor Marshalling() failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_HARDWARE_OUTPUT_SAMPLING_RATE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "event failed , error: %d", error);

    return reply.ReadInt32();
}

bool AudioPolicyProxy::IsVgsVolumeSupported()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_VGS_VOLUME_SUPPORTED), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "IsVgsVolumeSupported failed, error: %d", error);
    return reply.ReadBool();
}

std::vector<std::unique_ptr<AudioDeviceDescriptor>> AudioPolicyProxy::GetAvailableDevices(AudioDeviceUsage usage)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::vector<std::unique_ptr<AudioDeviceDescriptor>> audioDeviceDescriptors;

    CHECK_AND_RETURN_RET_LOG(data.WriteInterfaceToken(GetDescriptor()),
        audioDeviceDescriptors, "WriteInterfaceToken failed");

    bool token = data.WriteInt32(static_cast<int32_t>(usage));
    CHECK_AND_RETURN_RET_LOG(token, audioDeviceDescriptors, "WriteInt32 usage failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AVAILABLE_DESCRIPTORS), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, audioDeviceDescriptors, "GetAvailableDevices failed, error: %d", error);

    int32_t size = reply.ReadInt32();
    for (int32_t i = 0; i < size; i++) {
        std::unique_ptr<AudioDeviceDescriptor> desc =
            std::make_unique<AudioDeviceDescriptor>(AudioDeviceDescriptor::Unmarshalling(reply));
        audioDeviceDescriptors.push_back(move(desc));
    }
    return audioDeviceDescriptors;
}

bool AudioPolicyProxy::IsSpatializationEnabled()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_SPATIALIZATION_ENABLED), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "IsSpatializationEnabled failed, error: %{public}d", error);
    return reply.ReadBool();
}

bool AudioPolicyProxy::IsSpatializationEnabled(const std::string address)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    data.WriteString(address);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_SPATIALIZATION_ENABLED_FOR_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "IsSpatializationEnabled failed, error: %{public}d", error);
    return reply.ReadBool();
}


int32_t AudioPolicyProxy::SetSpatializationEnabled(const bool enable)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");
    data.WriteBool(enable);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_SPATIALIZATION_ENABLED), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "SetSpatializationEnabled failed, error: %{public}d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetSpatializationEnabled(const sptr<AudioDeviceDescriptor> &selectedAudioDevice,
    const bool enable)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");

    bool result = selectedAudioDevice->Marshalling(data);
    CHECK_AND_RETURN_RET_LOG(result, -1, "SelectedAudioDevice Marshalling() failed");

    data.WriteBool(enable);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_SPATIALIZATION_ENABLED_FOR_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "SetSpatializationEnabled failed, error: %{public}d", error);
    return reply.ReadInt32();
}

bool AudioPolicyProxy::IsHeadTrackingEnabled()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_HEAD_TRACKING_ENABLED), data, reply, option);

    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "IsHeadTrackingEnabled failed, error: %{public}d", error);
    return reply.ReadBool();
}

bool AudioPolicyProxy::IsHeadTrackingEnabled(const std::string address)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    data.WriteString(address);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_HEAD_TRACKING_ENABLED_FOR_DEVICE), data, reply, option);

    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "IsHeadTrackingEnabled failed, error: %{public}d", error);
    return reply.ReadBool();
}

int32_t AudioPolicyProxy::SetHeadTrackingEnabled(const bool enable)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");
    data.WriteBool(enable);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_HEAD_TRACKING_ENABLED), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "SetHeadTrackingEnabled failed, error: %{public}d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetHeadTrackingEnabled(const sptr<AudioDeviceDescriptor> &selectedAudioDevice,
    const bool enable)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");

    bool result = selectedAudioDevice->Marshalling(data);
    CHECK_AND_RETURN_RET_LOG(result, -1, "SelectedAudioDevice Marshalling() failed");
    data.WriteBool(enable);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_HEAD_TRACKING_ENABLED_FOR_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "SetHeadTrackingEnabled failed, error: %{public}d", error);
    return reply.ReadInt32();
}

AudioSpatializationState AudioPolicyProxy::GetSpatializationState(const StreamUsage streamUsage)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    AudioSpatializationState spatializationState = {false, false};

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, spatializationState, "WriteInterfaceToken failed");

    data.WriteInt32(static_cast<int32_t>(streamUsage));
    int32_t error = Remote() ->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SPATIALIZATION_STATE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, spatializationState, "SendRequest failed, error: %{public}d", error);

    spatializationState.spatializationEnabled = reply.ReadBool();
    spatializationState.headTrackingEnabled = reply.ReadBool();

    return spatializationState;
}

bool AudioPolicyProxy::IsSpatializationSupported()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_SPATIALIZATION_SUPPORTED), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "IsSpatializationSupported failed, error: %d", error);
    return reply.ReadBool();
}

bool AudioPolicyProxy::IsSpatializationSupportedForDevice(const std::string address)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");
    data.WriteString(address);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_SPATIALIZATION_SUPPORTED_FOR_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "IsSpatializationSupportedForDevice failed, error: %d", error);
    return reply.ReadBool();
}

bool AudioPolicyProxy::IsHeadTrackingSupported()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_HEAD_TRACKING_SUPPORTED), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "IsHeadTrackingSupported failed, error: %d", error);
    return reply.ReadBool();
}

bool AudioPolicyProxy::IsHeadTrackingSupportedForDevice(const std::string address)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");
    data.WriteString(address);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_HEAD_TRACKING_SUPPORTED_FOR_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "IsHeadTrackingSupportedForDevice failed, error: %d", error);
    return reply.ReadBool();
}

int32_t AudioPolicyProxy::UpdateSpatialDeviceState(const AudioSpatialDeviceState audioSpatialDeviceState)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");
    data.WriteString(audioSpatialDeviceState.address);
    data.WriteBool(audioSpatialDeviceState.isSpatializationSupported);
    data.WriteBool(audioSpatialDeviceState.isHeadTrackingSupported);
    data.WriteInt32(static_cast<int32_t>(audioSpatialDeviceState.spatialDeviceType));

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UPDATE_SPATIAL_DEVICE_STATE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "UpdateSpatialDeviceState failed, error: %d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::CreateAudioInterruptZone(const std::set<int32_t> &pids, const int32_t zoneID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    CHECK_AND_RETURN_RET_LOG(data.WriteInterfaceToken(GetDescriptor()), -1, "WriteInterfaceToken failed");

    data.WriteInt32(zoneID);
    data.WriteInt32(pids.size());
    int32_t count = 0;
    for (int32_t pid : pids) {
        data.WriteInt32(pid);
        count++;
        if (count >= MAX_PID_COUNT) {
            break;
        }
    }

    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::CREATE_AUDIO_INTERRUPT_ZONE), data, reply, option);

    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "CreateAudioInterruptZone failed, error: %d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::AddAudioInterruptZonePids(const std::set<int32_t> &pids, const int32_t zoneID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    CHECK_AND_RETURN_RET_LOG(data.WriteInterfaceToken(GetDescriptor()), -1, "WriteInterfaceToken failed");
    data.WriteInt32(zoneID);
    data.WriteInt32(pids.size());
    int32_t count = 0;
    for (int32_t pid : pids) {
        data.WriteInt32(pid);
        count++;
        if (count >= MAX_PID_COUNT) {
            break;
        }
    }

    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::ADD_AUDIO_INTERRUPT_ZONE_PIDS), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "AddAudioInterruptZonePids failed, error: %d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::RemoveAudioInterruptZonePids(const std::set<int32_t> &pids, const int32_t zoneID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    CHECK_AND_RETURN_RET_LOG(data.WriteInterfaceToken(GetDescriptor()), -1, "WriteInterfaceToken failed");
    data.WriteInt32(zoneID);
    data.WriteInt32(pids.size());
    int32_t count = 0;
    for (int32_t pid : pids) {
        data.WriteInt32(pid);
        count++;
        if (count >= MAX_PID_COUNT) {
            break;
        }
    }
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::REMOVE_AUDIO_INTERRUPT_ZONE_PIDS), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "RemoveAudioInterruptZonePids failed, error: %d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::ReleaseAudioInterruptZone(const int32_t zoneID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    CHECK_AND_RETURN_RET_LOG(data.WriteInterfaceToken(GetDescriptor()), -1, "WriteInterfaceToken failed");
    data.WriteInt32(zoneID);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::RELEASE_AUDIO_INTERRUPT_ZONE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "ReleaseAudioInterruptZone failed, error: %d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetCallDeviceActive(InternalDeviceType deviceType, bool active, std::string address)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(deviceType));
    data.WriteBool(active);
    data.WriteString(address);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_CALL_DEVICE_ACTIVE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "SendRequest failed, error: %d", error);
    return reply.ReadInt32();
}

std::unique_ptr<AudioDeviceDescriptor> AudioPolicyProxy::GetActiveBluetoothDevice()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::unique_ptr<AudioDeviceDescriptor> audioDeviceDescriptor;

    CHECK_AND_RETURN_RET_LOG(data.WriteInterfaceToken(GetDescriptor()),
        audioDeviceDescriptor, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_ACTIVE_BLUETOOTH_DESCRIPTOR), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, audioDeviceDescriptor,
        "GetActiveBluetoothDevice failed, error: %d", error);

    std::unique_ptr<AudioDeviceDescriptor> desc =
        std::make_unique<AudioDeviceDescriptor>(AudioDeviceDescriptor::Unmarshalling(reply));
    return desc;
}

int32_t AudioPolicyProxy::NotifyCapturerAdded(AudioCapturerInfo capturerInfo, AudioStreamInfo streamInfo,
    uint32_t sessionId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    capturerInfo.Marshalling(data);
    streamInfo.Marshalling(data);
    data.WriteUint32(sessionId);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_WAKEUP_AUDIOCAPTURER), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, -1, "failed, error: %d", error);
    return reply.ReadInt32();
}

ConverterConfig AudioPolicyProxy::GetConverterConfig()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    ConverterConfig result;

    CHECK_AND_RETURN_RET_LOG(data.WriteInterfaceToken(GetDescriptor()), result, "WriteInterfaceToken failed");
    int32_t error = Remote()->SendRequest(static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AUDIO_CONVERTER_CONFIG),
        data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, result, "failed, error: %d", error);

    result.library = {reply.ReadString(), reply.ReadString()};
    result.outChannelLayout = reply.ReadUint64();
    return result;
}

bool AudioPolicyProxy::IsHighResolutionExist()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_HIGH_RESOLUTION_EXIST), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERR_TRANSACTION_FAILED, "SendRequest failed, error: %d", error);
    bool replyReadBool = reply.ReadBool();
    return replyReadBool;
}

int32_t AudioPolicyProxy::SetHighResolutionExist(bool highResExist)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    data.WriteBool(highResExist);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_HIGH_RESOLUTION_EXIST), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "SendRequest failed, error: %d", error);
    return SUCCESS;
}

AudioSpatializationSceneType AudioPolicyProxy::GetSpatializationSceneType()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, SPATIALIZATION_SCENE_TYPE_DEFAULT, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SPATIALIZATION_SCENE_TYPE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, SPATIALIZATION_SCENE_TYPE_DEFAULT,
        "SendRequest failed, error: %{public}d", error);
    return static_cast<AudioSpatializationSceneType>(reply.ReadInt32());
}

int32_t AudioPolicyProxy::SetSpatializationSceneType(const AudioSpatializationSceneType spatializationSceneType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(spatializationSceneType));

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_SPATIALIZATION_SCENE_TYPE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "SendRequest failed, error: %{public}d", error);
    return reply.ReadInt32();
}

float AudioPolicyProxy::GetMaxAmplitude(const int32_t deviceId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    data.WriteInt32(deviceId);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MAX_AMPLITUDE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "SendRequest failed, error: %{public}d", error);
    return reply.ReadFloat();
}

int32_t AudioPolicyProxy::DisableSafeMediaVolume()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, ERROR, "WriteInterfaceToken failed");

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::DISABLE_SAFE_MEDIA_VOLUME), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, ERROR, "SendRequest failed, error: %{public}d", error);
    return reply.ReadInt32();
}

bool AudioPolicyProxy::IsHeadTrackingDataRequested(const std::string &macAddress)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    data.WriteString(macAddress);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_HEAD_TRACKING_DATA_REQUESTED), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "SendRequest failed, error: %d", error);
    return reply.ReadBool();
}

int32_t AudioPolicyProxy::TriggerFetchDevice(AudioStreamDeviceChangeReasonExt reason)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");

    data.WriteInt32(static_cast<int>(reason));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::TRIGGER_FETCH_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "SendRequest failed, error: %{public}d", error);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::MoveToNewPipe(const uint32_t sessionId, const AudioPipeType pipeType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, false, "WriteInterfaceToken failed");
    data.WriteUint32(sessionId);
    data.WriteInt32(pipeType);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::MOVE_TO_NEW_PIPE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, false, "SendRequest failed, error: %d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::ActivateAudioConcurrency(const AudioPipeType &pipeType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(pipeType);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::ACTIVATE_AUDIO_CONCURRENCY), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "activate concurrency failed, error: %{public}d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::InjectInterruption(const std::string networkId, InterruptEvent &event)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteString(networkId);
    data.WriteInt32(event.eventType);
    data.WriteInt32(event.forceType);
    data.WriteInt32(event.hintType);

    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::INJECT_INTERRUPTION), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "SendRequest failed, error: %{public}d", error);
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetDefaultOutputDevice(const DeviceType deviceType, const uint32_t sessionID,
    const StreamUsage streamUsage, bool isRunning)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    bool ret = data.WriteInterfaceToken(GetDescriptor());
    CHECK_AND_RETURN_RET_LOG(ret, -1, "WriteInterfaceToken failed");
    data.WriteInt32(static_cast<int32_t>(deviceType));
    data.WriteUint32(sessionID);
    data.WriteInt32(static_cast<int32_t>(streamUsage));
    data.WriteBool(isRunning);

    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_DEFAULT_OUTPUT_DEVICE), data, reply, option);
    CHECK_AND_RETURN_RET_LOG(error == ERR_NONE, error, "SendRequest failed, error: %{public}d", error);
    return reply.ReadInt32();
}
} // namespace AudioStandard
} // namespace OHOS
