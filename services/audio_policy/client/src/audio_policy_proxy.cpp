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

#include "audio_policy_manager.h"
#include "audio_log.h"
#include "audio_policy_proxy.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;

AudioPolicyProxy::AudioPolicyProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IAudioPolicy>(impl)
{
}

void AudioPolicyProxy::WriteAudioInteruptParams(MessageParcel &data, const AudioInterrupt &audioInterrupt)
{
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.streamUsage));
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.contentType));
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.streamType));
    data.WriteUint32(audioInterrupt.sessionID);
}

void AudioPolicyProxy::WriteAudioManagerInteruptParams(MessageParcel &data, const AudioInterrupt &audioInterrupt)
{
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.streamUsage));
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.contentType));
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.streamType));
    data.WriteBool(audioInterrupt.pauseWhenDucked);
}

void AudioPolicyProxy::ReadAudioInterruptParams(MessageParcel &reply, AudioInterrupt &audioInterrupt)
{
    audioInterrupt.streamUsage = static_cast<StreamUsage>(reply.ReadInt32());
    audioInterrupt.contentType = static_cast<ContentType>(reply.ReadInt32());
    audioInterrupt.streamType = static_cast<AudioStreamType>(reply.ReadInt32());
    audioInterrupt.sessionID = reply.ReadUint32();
}

void AudioPolicyProxy::WriteStreamChangeInfo(MessageParcel &data,
    const AudioMode &mode, const AudioStreamChangeInfo &streamChangeInfo)
{
    if (mode == AUDIO_MODE_PLAYBACK) {
        data.WriteInt32(streamChangeInfo.audioRendererChangeInfo.sessionId);
        data.WriteInt32(streamChangeInfo.audioRendererChangeInfo.rendererState);
        data.WriteInt32(streamChangeInfo.audioRendererChangeInfo.clientUID);
        data.WriteInt32(streamChangeInfo.audioRendererChangeInfo.rendererInfo.contentType);
        data.WriteInt32(streamChangeInfo.audioRendererChangeInfo.rendererInfo.streamUsage);
        data.WriteInt32(streamChangeInfo.audioRendererChangeInfo.rendererInfo.rendererFlags);
    } else {
        data.WriteInt32(streamChangeInfo.audioCapturerChangeInfo.sessionId);
        data.WriteInt32(streamChangeInfo.audioCapturerChangeInfo.capturerState);
        data.WriteInt32(streamChangeInfo.audioCapturerChangeInfo.clientUID);
        data.WriteInt32(streamChangeInfo.audioCapturerChangeInfo.capturerInfo.sourceType);
        data.WriteInt32(streamChangeInfo.audioCapturerChangeInfo.capturerInfo.capturerFlags);
    }
}

void AudioPolicyProxy::WriteAudioStreamInfoParams(MessageParcel &data, const AudioStreamInfo &audioStreamInfo)
{
    data.WriteInt32(static_cast<int32_t>(audioStreamInfo.samplingRate));
    data.WriteInt32(static_cast<int32_t>(audioStreamInfo.channels));
    data.WriteInt32(static_cast<int32_t>(audioStreamInfo.format));
    data.WriteInt32(static_cast<int32_t>(audioStreamInfo.encoding));
}

int32_t AudioPolicyProxy::SetStreamVolume(AudioStreamType streamType, float volume)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(static_cast<int32_t>(streamType));
    data.WriteFloat(volume);
    int32_t error = Remote()->SendRequest(SET_STREAM_VOLUME, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set volume failed, error: %d", error);
        return error;
    }
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetRingerMode(AudioRingerMode ringMode)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int>(ringMode));
    int32_t error = Remote()->SendRequest(SET_RINGER_MODE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set ringermode failed, error: %d", error);
        return error;
    }

    return reply.ReadInt32();
}

std::vector<int32_t> AudioPolicyProxy::GetSupportedTones()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int32_t lListSize = 0;
    AUDIO_DEBUG_LOG("get GetSupportedTones,");
    std::vector<int> lSupportedToneList = {};
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return lSupportedToneList;
    }
    int32_t error = Remote()->SendRequest(GET_SUPPORTED_TONES, data, reply, option);
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
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return spToneInfo;
    }
    data.WriteInt32(ltonetype);
    int32_t error = Remote()->SendRequest(GET_TONEINFO, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get toneinfo failed, error: %d", error);
    }

    spToneInfo->segmentCnt = reply.ReadUint32();
    spToneInfo->repeatCnt = reply.ReadUint32();
    spToneInfo->repeatSegment = reply.ReadUint32();
    AUDIO_INFO_LOG("segmentCnt: %{public}d, repeatCnt: %{public}d, repeatSegment: %{public}d",
        spToneInfo->segmentCnt, spToneInfo->repeatCnt, spToneInfo->repeatSegment);
    for (uint32_t i = 0; i<spToneInfo->segmentCnt; i++) {
        spToneInfo->segments[i].duration = reply.ReadUint32();
        spToneInfo->segments[i].loopCnt = reply.ReadUint16();
        spToneInfo->segments[i].loopIndx = reply.ReadUint16();
        AUDIO_INFO_LOG("seg[%{public}d].duration: %{public}d, seg[%{public}d].loopCnt: %{public}d, seg[%{public}d].loopIndex: %{public}d",
            i, spToneInfo->segments[i].duration, i, spToneInfo->segments[i].loopCnt, i, spToneInfo->segments[i].loopIndx);
        for (uint32_t j = 0; j < TONEINFO_MAX_WAVES+1; j++) {
            spToneInfo->segments[i].waveFreq[j] = reply.ReadUint16();
            AUDIO_INFO_LOG("wave[%{public}d]: %{public}d", j, spToneInfo->segments[i].waveFreq[j]);
        }
    }
    AUDIO_DEBUG_LOG("get rGetToneConfig returned,");
    return spToneInfo;
}

int32_t AudioPolicyProxy::SetMicrophoneMute(bool isMute)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteBool(isMute);
    int32_t error = Remote()->SendRequest(SET_MICROPHONE_MUTE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set microphoneMute failed, error: %d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetMicrophoneMuteAudioConfig(bool isMute)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteBool(isMute);
    int32_t error = Remote()->SendRequest(SET_MICROPHONE_MUTE_AUDIO_CONFIG, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set microphoneMute failed, error: %d", error);
        return error;
    }

    return reply.ReadInt32();
}

bool AudioPolicyProxy::IsMicrophoneMute(API_VERSION api_v)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int32_t>(api_v));
    int32_t error = Remote()->SendRequest(IS_MICROPHONE_MUTE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set microphoneMute failed, error: %d", error);
        return error;
    }

    return reply.ReadBool();
}

AudioRingerMode AudioPolicyProxy::GetRingerMode()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return RINGER_MODE_NORMAL;
    }
    int32_t error = Remote()->SendRequest(GET_RINGER_MODE, data, reply, option);
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

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int>(scene));
    int32_t error = Remote()->SendRequest(SET_AUDIO_SCENE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set audio scene failed, error: %d", error);
        return error;
    }

    return reply.ReadInt32();
}

AudioScene AudioPolicyProxy::GetAudioScene()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return AUDIO_SCENE_DEFAULT;
    }
    int32_t error = Remote()->SendRequest(GET_AUDIO_SCENE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get audio scene failed, error: %d", error);
    }
    return static_cast<AudioScene>(reply.ReadInt32());
}

float AudioPolicyProxy::GetStreamVolume(AudioStreamType streamType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int32_t>(streamType));
    int32_t error = Remote()->SendRequest(GET_STREAM_VOLUME, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get volume failed, error: %d", error);
        return error;
    }
    return reply.ReadFloat();
}

int32_t AudioPolicyProxy::SetLowPowerVolume(int32_t streamId, float volume)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(streamId);
    data.WriteFloat(volume);
    int32_t error = Remote()->SendRequest(SET_LOW_POWER_STREM_VOLUME, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set low power stream volume failed, error: %d", error);
        return error;
    }
    return reply.ReadInt32();
}

float AudioPolicyProxy::GetLowPowerVolume(int32_t streamId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(streamId);
    int32_t error = Remote()->SendRequest(GET_LOW_POWRR_STREM_VOLUME, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get low power stream volume failed, error: %d", error);
        return error;
    }
    return reply.ReadFloat();
}

float AudioPolicyProxy::GetSingleStreamVolume(int32_t streamId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(streamId);
    int32_t error = Remote()->SendRequest(GET_SINGLE_STREAM_VOLUME, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get single stream volume failed, error: %d", error);
        return error;
    }
    return reply.ReadFloat();
}

int32_t AudioPolicyProxy::SetStreamMute(AudioStreamType streamType, bool mute)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int32_t>(streamType));
    data.WriteBool(mute);
    int32_t error = Remote()->SendRequest(SET_STREAM_MUTE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set mute failed, error: %d", error);
        return error;
    }
    return reply.ReadInt32();
}

bool AudioPolicyProxy::GetStreamMute(AudioStreamType streamType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return false;
    }
    data.WriteInt32(static_cast<int32_t>(streamType));
    int32_t error = Remote()->SendRequest(GET_STREAM_MUTE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get mute failed, error: %d", error);
        return false;
    }
    return reply.ReadBool();
}

bool AudioPolicyProxy::IsStreamActive(AudioStreamType streamType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return false;
    }
    data.WriteInt32(static_cast<int32_t>(streamType));
    int32_t error = Remote()->SendRequest(IS_STREAM_ACTIVE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("isStreamActive failed, error: %d", error);
        return false;
    }
    return reply.ReadBool();
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyProxy::GetDevices(DeviceFlag deviceFlag)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return deviceInfo;
    }
    data.WriteInt32(static_cast<int32_t>(deviceFlag));
    int32_t error = Remote()->SendRequest(GET_DEVICES, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("Get devices failed, error: %d", error);
        return deviceInfo;
    }

    int32_t size = reply.ReadInt32();
    for (int32_t i = 0; i < size; i++) {
        deviceInfo.push_back(AudioDeviceDescriptor::Unmarshalling(reply));
    }

    return deviceInfo;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyProxy::GetActiveOutputDeviceDescriptors()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return deviceInfo;
    }
    int32_t error = Remote()->SendRequest(GET_ACTIVE_OUTPUT_DEVICE_DESCRIPTORS, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("Get out devices failed, error: %d", error);
        return deviceInfo;
    }

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

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int32_t>(deviceType));
    data.WriteBool(active);
    int32_t error = Remote()->SendRequest(SET_DEVICE_ACTIVE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set device active failed, error: %d", error);
        return error;
    }
    return reply.ReadInt32();
}

bool AudioPolicyProxy::IsDeviceActive(InternalDeviceType deviceType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return false;
    }
    data.WriteInt32(static_cast<int32_t>(deviceType));
    int32_t error = Remote()->SendRequest(IS_DEVICE_ACTIVE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("is device active failed, error: %d", error);
        return false;
    }
    return reply.ReadBool();
}

DeviceType AudioPolicyProxy::GetActiveOutputDevice()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return DEVICE_TYPE_INVALID;
    }

    int32_t error = Remote()->SendRequest(GET_ACTIVE_OUTPUT_DEVICE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get active output device failed, error: %d", error);
        return DEVICE_TYPE_INVALID;
    }

    return static_cast<DeviceType>(reply.ReadInt32());
}

DeviceType AudioPolicyProxy::GetActiveInputDevice()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return DEVICE_TYPE_INVALID;
    }

    int32_t error = Remote()->SendRequest(GET_ACTIVE_INPUT_DEVICE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get active input device failed, error: %d", error);
        return DEVICE_TYPE_INVALID;
    }

    return static_cast<DeviceType>(reply.ReadInt32());
}

int32_t AudioPolicyProxy::SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    if (!audioRendererFilter->Marshalling(data)) {
        AUDIO_ERR_LOG("AudioRendererFilter Marshalling() failed");
        return -1;
    }
    int size = audioDeviceDescriptors.size();
    int validSize = 20; // Use 20 as limit.
    if (size <= 0 || size > validSize) {
        AUDIO_ERR_LOG("SelectOutputDevice get invalid device size.");
        return -1;
    }
    data.WriteInt32(size);
    for (auto audioDeviceDescriptor : audioDeviceDescriptors) {
        if (!audioDeviceDescriptor->Marshalling(data)) {
            AUDIO_ERR_LOG("AudioDeviceDescriptor Marshalling() failed");
            return -1;
        }
    }
    int error = Remote()->SendRequest(SELECT_OUTPUT_DEVICE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("SelectOutputDevice failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}
std::string AudioPolicyProxy::GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return "";
    }
    data.WriteInt32(uid);
    data.WriteInt32(pid);
    data.WriteInt32(static_cast<int32_t>(streamType));
    int error = Remote()->SendRequest(GET_SELECTED_DEVICE_INFO, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetSelectedDeviceInfo failed, error: %{public}d", error);
        return "";
    }

    return reply.ReadString();
}

int32_t AudioPolicyProxy::SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    if (!audioCapturerFilter->Marshalling(data)) {
        AUDIO_ERR_LOG("AudioCapturerFilter Marshalling() failed");
        return -1;
    }
    int size = audioDeviceDescriptors.size();
    int validSize = 20; // Use 20 as limit.
    if (size <= 0 || size > validSize) {
        AUDIO_ERR_LOG("SelectInputDevice get invalid device size.");
        return -1;
    }
    data.WriteInt32(size);
    for (auto audioDeviceDescriptor : audioDeviceDescriptors) {
        if (!audioDeviceDescriptor->Marshalling(data)) {
            AUDIO_ERR_LOG("AudioDeviceDescriptor Marshalling() failed");
            return -1;
        }
    }
    int error = Remote()->SendRequest(SELECT_INPUT_DEVICE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("SelectInputDevice failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetRingerModeCallback(const int32_t clientId, const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyProxy: SetRingerModeCallback object is null");
        return ERR_NULL_OBJECT;
    }
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(clientId);
    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(SET_RINGERMODE_CALLBACK, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: set ringermode callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnsetRingerModeCallback(const int32_t clientId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(clientId);
    int error = Remote()->SendRequest(UNSET_RINGERMODE_CALLBACK, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: unset ringermode callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetMicStateChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyProxy: SetMicStateChangeCallback object is null");
        return ERR_NULL_OBJECT;
    }
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(clientId);
    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(SET_MIC_STATE_CHANGE_CALLBACK, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: SetMicStateChangeCallback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag,
    const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyProxy: SetDeviceChangeCallback object is null");
        return ERR_NULL_OBJECT;
    }
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(clientId);
    data.WriteInt32(flag);
    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(SET_DEVICE_CHANGE_CALLBACK, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: SetDeviceChangeCallback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnsetDeviceChangeCallback(const int32_t clientId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(clientId);
    int error = Remote()->SendRequest(UNSET_DEVICE_CHANGE_CALLBACK, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: unset device change callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetAudioInterruptCallback(const uint32_t sessionID, const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyProxy: SetAudioInterruptCallback object is null");
        return ERR_NULL_OBJECT;
    }
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteUint32(sessionID);
    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(SET_CALLBACK, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: set callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnsetAudioInterruptCallback(const uint32_t sessionID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteUint32(sessionID);
    int error = Remote()->SendRequest(UNSET_CALLBACK, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: unset callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    WriteAudioInteruptParams(data, audioInterrupt);
    int error = Remote()->SendRequest(ACTIVATE_INTERRUPT, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: activate interrupt failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    WriteAudioInteruptParams(data, audioInterrupt);
    int error = Remote()->SendRequest(DEACTIVATE_INTERRUPT, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: deactivate interrupt failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetAudioManagerInterruptCallback(const uint32_t clientID, const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyProxy: SetAudioManagerInterruptCallback object is null");
        return ERR_NULL_OBJECT;
    }
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteUint32(clientID);
    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(SET_INTERRUPT_CALLBACK, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: set callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnsetAudioManagerInterruptCallback(const uint32_t clientID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteUint32(clientID);

    int error = Remote()->SendRequest(UNSET_INTERRUPT_CALLBACK, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: unset callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::RequestAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteUint32(clientID);
    WriteAudioManagerInteruptParams(data, audioInterrupt);

    int error = Remote()->SendRequest(REQUEST_AUDIO_FOCUS, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: activate interrupt failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::AbandonAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteUint32(clientID);
    WriteAudioManagerInteruptParams(data, audioInterrupt);

    int error = Remote()->SendRequest(ABANDON_AUDIO_FOCUS, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: deactivate interrupt failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

AudioStreamType AudioPolicyProxy::GetStreamInFocus()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return STREAM_DEFAULT;
    }
    int32_t error = Remote()->SendRequest(GET_STREAM_IN_FOCUS, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get stream in focus failed, error: %d", error);
    }
    return static_cast<AudioStreamType>(reply.ReadInt32());
}

int32_t AudioPolicyProxy::GetSessionInfoInFocus(AudioInterrupt &audioInterrupt)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    int32_t error = Remote()->SendRequest(GET_SESSION_INFO_IN_FOCUS, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy::GetSessionInfoInFocus failed, error: %d", error);
    }
    ReadAudioInterruptParams(reply, audioInterrupt);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetVolumeKeyEventCallback(const int32_t clientPid, const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    if (object == nullptr) {
        AUDIO_ERR_LOG("VolumeKeyEventCallback object is null");
        return ERR_NULL_OBJECT;
    }

    data.WriteInt32(clientPid);
    data.WriteRemoteObject(object);
    int result = Remote()->SendRequest(SET_VOLUME_KEY_EVENT_CALLBACK, data, reply, option);
    if (result != ERR_NONE) {
        AUDIO_ERR_LOG("SetAudioVolumeKeyEventCallback failed, result: %{public}d", result);
        return result;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnsetVolumeKeyEventCallback(const int32_t clientPid)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(clientPid);
    int result = Remote()->SendRequest(UNSET_VOLUME_KEY_EVENT_CALLBACK, data, reply, option);
    if (result != ERR_NONE) {
        AUDIO_ERR_LOG("UnsetVolumeKeyEventCallback failed, result: %{public}d", result);
        return result;
    }

    return reply.ReadInt32();
}

bool AudioPolicyProxy::VerifyClientPermission(const std::string &permissionName, uint32_t appTokenId, int32_t appUid,
    bool privacyFlag, AudioPermissionState state)
{
    AUDIO_DEBUG_LOG("Proxy [permission : %{public}s] | [tid : %{public}d]", permissionName.c_str(), appTokenId);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("VerifyClientPermission: WriteInterfaceToken failed");
        return false;
    }

    data.WriteString(permissionName);
    data.WriteUint32(appTokenId);
    data.WriteInt32(appUid);
    data.WriteBool(privacyFlag);
    data.WriteInt32(state);

    int result = Remote()->SendRequest(QUERY_PERMISSION, data, reply, option);
    if (result != ERR_NONE) {
        AUDIO_ERR_LOG("VerifyClientPermission failed, result: %{public}d", result);
        return false;
    }

    return reply.ReadBool();
}

bool AudioPolicyProxy::getUsingPemissionFromPrivacy(const std::string &permissionName, uint32_t appTokenId,
    AudioPermissionState state)
{
    AUDIO_DEBUG_LOG("Proxy [permission : %{public}s] | [tid : %{public}d]", permissionName.c_str(), appTokenId);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("getUsingPemissionFromPrivacy: WriteInterfaceToken failed");
        return false;
    }

    data.WriteString(permissionName);
    data.WriteUint32(appTokenId);
    data.WriteInt32(state);

    int result = Remote()->SendRequest(GET_USING_PEMISSION_FROM_PRIVACY, data, reply, option);
    if (result != ERR_NONE) {
        AUDIO_ERR_LOG("getUsingPemissionFromPrivacy failed, result: %{public}d", result);
        return false;
    }

    return reply.ReadBool();
}

int32_t AudioPolicyProxy::ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType)
{
    AUDIO_ERR_LOG("ReconfigureAudioChannel proxy %{public}d, %{public}d", count, deviceType);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("ReconfigureAudioChannel: WriteInterfaceToken failed");
        return IPC_PROXY_ERR;
    }

    data.WriteUint32(count);
    data.WriteInt32(deviceType);

    int result = Remote()->SendRequest(RECONFIGURE_CHANNEL, data, reply, option);
    if (result != ERR_NONE) {
        AUDIO_ERR_LOG("ReconfigureAudioChannel failed, result: %{public}d", result);
        return ERR_TRANSACTION_FAILED;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::RegisterAudioRendererEventListener(const int32_t clientUID, const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_INFO_LOG("AudioPolicyProxy::RegisterAudioRendererEventListener");
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("RegisterAudioRendererEventListener: WriteInterfaceToken failed");
        return ERROR;
    }
    if (object == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioRendererEventListener Event object is null");
        return ERR_NULL_OBJECT;
    }

    data.WriteInt32(clientUID);
    data.WriteRemoteObject(object);
    int32_t error = Remote() ->SendRequest(REGISTER_PLAYBACK_EVENT, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("RegisterAudioRendererEventListener register playback event failed , error: %d", error);
        return ERROR;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnregisterAudioRendererEventListener(const int32_t clientUID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_INFO_LOG("AudioPolicyProxy::UnregisterAudioRendererEventListener");
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("UnregisterAudioRendererEventListener WriteInterfaceToken failed");
        return ERROR;
    }

    data.WriteInt32(clientUID);
    int32_t error = Remote() ->SendRequest(UNREGISTER_PLAYBACK_EVENT, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("UnregisterAudioRendererEventListener unregister playback event failed , error: %d", error);
        return ERROR;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::GetAudioLatencyFromXml()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: GetAudioLatencyFromXml WriteInterfaceToken failed");
        return IPC_PROXY_ERR;
    }

    int32_t error = Remote()->SendRequest(GET_AUDIO_LATENCY, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetAudioLatencyFromXml, error: %d", error);
        return ERR_TRANSACTION_FAILED;
    }

    return reply.ReadInt32();
}

uint32_t AudioPolicyProxy::GetSinkLatencyFromXml()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: GetSinkLatencyFromXml WriteInterfaceToken failed");
        return 0;
    }

    int32_t error = Remote()->SendRequest(GET_SINK_LATENCY, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetSinkLatencyFromXml, error: %d", error);
        return 0;
    }

    return reply.ReadUint32();
}

int32_t AudioPolicyProxy::RegisterAudioCapturerEventListener(const int32_t clientUID, const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_INFO_LOG("AudioPolicyProxy::RegisterAudioCapturerEventListener");

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("RegisterAudioCapturerEventListener:: WriteInterfaceToken failed");
        return ERROR;
    }
    if (object == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioCapturerEventListener Event object is null");
        return ERR_NULL_OBJECT;
    }

    data.WriteInt32(clientUID);
    data.WriteRemoteObject(object);
    int32_t error = Remote() ->SendRequest(REGISTER_RECORDING_EVENT, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("RegisterAudioCapturerEventListener recording event failed , error: %d", error);
        return ERROR;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnregisterAudioCapturerEventListener(const int32_t clientUID)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_INFO_LOG("AudioPolicyProxy::UnregisterAudioCapturerEventListener");

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy:: WriteInterfaceToken failed");
        return ERROR;
    }

    data.WriteInt32(clientUID);
    int32_t error = Remote() ->SendRequest(UNREGISTER_RECORDING_EVENT, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("UnregisterAudioCapturerEventListener recording event failed , error: %d", error);
        return ERROR;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
    const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_INFO_LOG("AudioPolicyProxy::RegisterTracker");

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("RegisterTracker WriteInterfaceToken failed");
        return ERROR;
    }

    if (object == nullptr) {
        AUDIO_ERR_LOG("Register Tracker Event object is null");
        return ERR_NULL_OBJECT;
    }

    data.WriteUint32(mode);
    WriteStreamChangeInfo(data, mode, streamChangeInfo);
    data.WriteRemoteObject(object);

    int32_t error = Remote()->SendRequest(REGISTER_TRACKER, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("RegisterTracker event failed , error: %d", error);
        return ERROR;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_INFO_LOG("AudioPolicyProxy::UpdateTracker");

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("UpdateTracker: WriteInterfaceToken failed");
        return ERROR;
    }

    data.WriteUint32(mode);
    WriteStreamChangeInfo(data, mode, streamChangeInfo);

    int32_t error = Remote()->SendRequest(UPDATE_TRACKER, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("UpdateTracker event failed , error: %d", error);
        return ERROR;
    }

    return reply.ReadInt32();
}

void AudioPolicyProxy::ReadAudioRendererChangeInfo(MessageParcel &reply,
    unique_ptr<AudioRendererChangeInfo> &rendererChangeInfo)
{
    rendererChangeInfo->sessionId = reply.ReadInt32();
    rendererChangeInfo->rendererState = static_cast<RendererState>(reply.ReadInt32());
    rendererChangeInfo->clientUID = reply.ReadInt32();
    rendererChangeInfo->rendererInfo.contentType = static_cast<ContentType>(reply.ReadInt32());
    rendererChangeInfo->rendererInfo.streamUsage = static_cast<StreamUsage>(reply.ReadInt32());
    rendererChangeInfo->rendererInfo.rendererFlags = reply.ReadInt32();

    rendererChangeInfo->outputDeviceInfo.deviceType = static_cast<DeviceType>(reply.ReadInt32());
    rendererChangeInfo->outputDeviceInfo.deviceRole = static_cast<DeviceRole>(reply.ReadInt32());
    rendererChangeInfo->outputDeviceInfo.deviceId = reply.ReadInt32();
    rendererChangeInfo->outputDeviceInfo.channelMasks = reply.ReadInt32();
    rendererChangeInfo->outputDeviceInfo.audioStreamInfo.samplingRate
        = static_cast<AudioSamplingRate>(reply.ReadInt32());
    rendererChangeInfo->outputDeviceInfo.audioStreamInfo.encoding
        = static_cast<AudioEncodingType>(reply.ReadInt32());
    rendererChangeInfo->outputDeviceInfo.audioStreamInfo.format = static_cast<AudioSampleFormat>(reply.ReadInt32());
    rendererChangeInfo->outputDeviceInfo.audioStreamInfo.channels = static_cast<AudioChannel>(reply.ReadInt32());
    rendererChangeInfo->outputDeviceInfo.deviceName = reply.ReadString();
    rendererChangeInfo->outputDeviceInfo.macAddress = reply.ReadString();
}

void AudioPolicyProxy::ReadAudioCapturerChangeInfo(MessageParcel &reply,
    unique_ptr<AudioCapturerChangeInfo> &capturerChangeInfo)
{
    capturerChangeInfo->sessionId = reply.ReadInt32();
    capturerChangeInfo->capturerState = static_cast<CapturerState>(reply.ReadInt32());
    capturerChangeInfo->clientUID = reply.ReadInt32();
    capturerChangeInfo->capturerInfo.sourceType = static_cast<SourceType>(reply.ReadInt32());
    capturerChangeInfo->capturerInfo.capturerFlags = reply.ReadInt32();

    capturerChangeInfo->inputDeviceInfo.deviceType = static_cast<DeviceType>(reply.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.deviceRole = static_cast<DeviceRole>(reply.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.deviceId = reply.ReadInt32();
    capturerChangeInfo->inputDeviceInfo.channelMasks = reply.ReadInt32();
    capturerChangeInfo->inputDeviceInfo.audioStreamInfo.samplingRate
        = static_cast<AudioSamplingRate>(reply.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.audioStreamInfo.encoding
        = static_cast<AudioEncodingType>(reply.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.audioStreamInfo.format = static_cast<AudioSampleFormat>(reply.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.audioStreamInfo.channels = static_cast<AudioChannel>(reply.ReadInt32());
    capturerChangeInfo->inputDeviceInfo.deviceName = reply.ReadString();
    capturerChangeInfo->inputDeviceInfo.macAddress = reply.ReadString();
}

int32_t AudioPolicyProxy::GetCurrentRendererChangeInfos(
    vector<unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_DEBUG_LOG("AudioPolicyProxy::GetCurrentRendererChangeInfos");

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("GetCurrentRendererChangeInfo: WriteInterfaceToken failed");
        return ERROR;
    }

    int32_t error = Remote()->SendRequest(GET_RENDERER_CHANGE_INFOS, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("Get Renderer change info event failed , error: %d", error);
        return ERROR;
    }

    int32_t size = reply.ReadInt32();
    while (size > 0) {
        unique_ptr<AudioRendererChangeInfo> rendererChangeInfo = make_unique<AudioRendererChangeInfo>();
        CHECK_AND_RETURN_RET_LOG(rendererChangeInfo != nullptr, ERR_MEMORY_ALLOC_FAILED, "No memory!!");
        ReadAudioRendererChangeInfo(reply, rendererChangeInfo);
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

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("GetCurrentCapturerChangeInfos: WriteInterfaceToken failed");
        return ERROR;
    }

    int32_t error = Remote()->SendRequest(GET_CAPTURER_CHANGE_INFOS, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("Get capturer change info event failed , error: %d", error);
        return ERROR;
    }

    int32_t size = reply.ReadInt32();
    while (size > 0) {
        unique_ptr<AudioCapturerChangeInfo> capturerChangeInfo = make_unique<AudioCapturerChangeInfo>();
        CHECK_AND_RETURN_RET_LOG(capturerChangeInfo != nullptr, ERR_MEMORY_ALLOC_FAILED, "No memory!!");
        ReadAudioCapturerChangeInfo(reply, capturerChangeInfo);
        audioCapturerChangeInfos.push_back(move(capturerChangeInfo));
        size--;
    }

    return SUCCESS;
}

int32_t AudioPolicyProxy::UpdateStreamState(const int32_t clientUid, StreamSetState streamSetState,
    AudioStreamType audioStreamType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_DEBUG_LOG("AudioPolicyProxy::UpdateStreamState");

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("UpdateStreamState: WriteInterfaceToken failed");
        return ERROR;
    }

    data.WriteInt32(static_cast<int32_t>(clientUid));
    data.WriteInt32(static_cast<int32_t>(streamSetState));
    data.WriteInt32(static_cast<int32_t>(audioStreamType));

    int32_t error = Remote()->SendRequest(UPDATE_STREAM_STATE, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("UPDATE_STREAM_STATE stream changed info event failed , error: %d", error);
        return ERROR;
    }

    return SUCCESS;
}

std::vector<sptr<VolumeGroupInfo>> AudioPolicyProxy::GetVolumeGroupInfos()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    std::vector<sptr<VolumeGroupInfo>> infos;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: GetVolumeGroupById WriteInterfaceToken failed");
        return infos;
    }

    int32_t error = Remote()->SendRequest(GET_VOLUME_GROUP_INFO, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetVolumeGroupInfo, error: %d", error);
        return infos;
    }

    int32_t size = reply.ReadInt32();
    for (int32_t i = 0; i < size; i++) {
        infos.push_back(VolumeGroupInfo::Unmarshalling(reply));
    }

    return infos;
}

bool AudioPolicyProxy::IsAudioRendererLowLatencySupported(const AudioStreamInfo &audioStreamInfo)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("IsAudioRendererLowLatencySupported WriteInterfaceToken failed");
        return IPC_PROXY_ERR;
    }
    WriteAudioStreamInfoParams(data, audioStreamInfo);
    int32_t error = Remote()->SendRequest(IS_AUDIO_RENDER_LOW_LATENCY_SUPPORTED, data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("IsAudioRendererLowLatencySupported, error: %d", error);
        return ERR_TRANSACTION_FAILED;
    }

    return reply.ReadBool();
}
} // namespace AudioStandard
} // namespace OHOS
