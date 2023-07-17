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
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.audioFocusType.streamType));
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.audioFocusType.sourceType));
    data.WriteBool(audioInterrupt.audioFocusType.isPlay);
    data.WriteUint32(audioInterrupt.sessionID);
    data.WriteInt32(audioInterrupt.pid);
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.mode));
}

void AudioPolicyProxy::WriteAudioManagerInteruptParams(MessageParcel &data, const AudioInterrupt &audioInterrupt)
{
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.streamUsage));
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.contentType));
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.audioFocusType.streamType));
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.audioFocusType.sourceType));
    data.WriteBool(audioInterrupt.audioFocusType.isPlay);
    data.WriteBool(audioInterrupt.pauseWhenDucked);
    data.WriteInt32(audioInterrupt.pid);
    data.WriteInt32(static_cast<int32_t>(audioInterrupt.mode));
}

void AudioPolicyProxy::ReadAudioInterruptParams(MessageParcel &reply, AudioInterrupt &audioInterrupt)
{
    audioInterrupt.streamUsage = static_cast<StreamUsage>(reply.ReadInt32());
    audioInterrupt.contentType = static_cast<ContentType>(reply.ReadInt32());
    audioInterrupt.audioFocusType.streamType = static_cast<AudioStreamType>(reply.ReadInt32());
    audioInterrupt.audioFocusType.sourceType = static_cast<SourceType>(reply.ReadInt32());
    audioInterrupt.audioFocusType.isPlay = reply.ReadBool();
    audioInterrupt.sessionID = reply.ReadUint32();
    audioInterrupt.pid = reply.ReadInt32();
    audioInterrupt.mode = static_cast<InterruptMode>(reply.ReadInt32());
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

int32_t AudioPolicyProxy::GetMaxVolumeLevel(AudioVolumeType volumeType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("GetMaxVolumeLevel: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int32_t>(volumeType));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MAX_VOLUMELEVEL), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get max volume failed, error: %d", error);
        return error;
    }
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::GetMinVolumeLevel(AudioVolumeType volumeType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("GetMinVolumeLevel: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int32_t>(volumeType));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MIN_VOLUMELEVEL), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get min volume failed, error: %d", error);
        return error;
    }
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetSystemVolumeLevel(AudioStreamType streamType, int32_t volumeLevel, API_VERSION api_v)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(static_cast<int32_t>(streamType));
    data.WriteInt32(volumeLevel);
    data.WriteInt32(static_cast<int32_t>(api_v));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_SYSTEM_VOLUMELEVEL), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set volume failed, error: %d", error);
        return error;
    }
    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetRingerMode(AudioRingerMode ringMode, API_VERSION api_v)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int>(ringMode));
    data.WriteInt32(static_cast<int32_t>(api_v));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_RINGER_MODE), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("set ringermode failed, error: %d", error);
        return error;
    }

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
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return lSupportedToneList;
    }
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
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return spToneInfo;
    }
    data.WriteInt32(ltonetype);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_TONEINFO), data, reply, option);
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
#endif

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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_MICROPHONE_MUTE), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_MICROPHONE_MUTE_AUDIO_CONFIG), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_MICROPHONE_MUTE), data, reply, option);
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

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int>(scene));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_AUDIO_SCENE), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AUDIO_SCENE), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get audio scene failed, error: %d", error);
    }
    return static_cast<AudioScene>(reply.ReadInt32());
}

int32_t AudioPolicyProxy::GetSystemVolumeLevel(AudioStreamType streamType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int32_t>(streamType));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SYSTEM_VOLUMELEVEL), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get volume failed, error: %d", error);
        return error;
    }
    return reply.ReadInt32();
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_LOW_POWER_STREM_VOLUME), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_LOW_POWRR_STREM_VOLUME), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SINGLE_STREAM_VOLUME), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get single stream volume failed, error: %d", error);
        return error;
    }
    return reply.ReadFloat();
}

int32_t AudioPolicyProxy::SetStreamMute(AudioStreamType streamType, bool mute, API_VERSION api_v)
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
    data.WriteInt32(static_cast<int32_t>(api_v));
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_STREAM_MUTE), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_STREAM_MUTE), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_STREAM_ACTIVE), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_DEVICES), data, reply, option);
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

bool AudioPolicyProxy::SetWakeUpAudioCapturer(InternalAudioCapturerOptions options)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return false;
    }
    data.WriteInt32(static_cast<int32_t>(options.streamInfo.samplingRate));
    data.WriteInt32(static_cast<int32_t>(options.streamInfo.encoding));
    data.WriteInt32(static_cast<int32_t>(options.streamInfo.format));
    data.WriteInt32(static_cast<int32_t>(options.streamInfo.channels));

    data.WriteInt32(static_cast<int32_t>(options.capturerInfo.sourceType));
    data.WriteInt32(options.capturerInfo.capturerFlags);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_WAKEUP_AUDIOCAPTURER), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("CreateWakeUpAudioCapturer failed, error: %d", error);
        return false;
    }
    return reply.ReadInt32();
}

bool AudioPolicyProxy::CloseWakeUpAudioCapturer()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return false;
    }
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::CLOSE_WAKEUP_AUDIOCAPTURER), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("CloseWakeUpAudioCapturer failed, error: %d", error);
        return false;
    }
    return reply.ReadInt32();
}
std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyProxy::GetPreferOutputDeviceDescriptors(
    AudioRendererInfo &rendererInfo)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return deviceInfo;
    }
    sptr<AudioRendererFilter> audioRendererFilter = new(std::nothrow) AudioRendererFilter();
    audioRendererFilter->uid = -1;
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_ACTIVE_OUTPUT_DEVICE_DESCRIPTORS), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_DEVICE_ACTIVE), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_DEVICE_ACTIVE), data, reply, option);
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

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_ACTIVE_OUTPUT_DEVICE), data, reply, option);
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

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_ACTIVE_INPUT_DEVICE), data, reply, option);
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
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SELECT_OUTPUT_DEVICE), data, reply, option);
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
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SELECTED_DEVICE_INFO), data, reply, option);
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
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SELECT_INPUT_DEVICE), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("SelectInputDevice failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetRingerModeCallback(const int32_t clientId,
    const sptr<IRemoteObject> &object, API_VERSION api_v)
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
    data.WriteInt32(static_cast<int32_t>(api_v));
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_RINGERMODE_CALLBACK), data, reply, option);
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
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_RINGERMODE_CALLBACK), data, reply, option);
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
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_MIC_STATE_CHANGE_CALLBACK), data, reply, option);
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
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_DEVICE_CHANGE_CALLBACK), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: SetDeviceChangeCallback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnsetDeviceChangeCallback(const int32_t clientId, DeviceFlag flag)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(clientId);
    data.WriteInt32(flag);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_DEVICE_CHANGE_CALLBACK), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: unset device change callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetPreferOutputDeviceChangeCallback(const int32_t clientId,
    const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (object == nullptr) {
        AUDIO_ERR_LOG("SetPreferOutputDeviceChangeCallback object is null");
        return ERR_NULL_OBJECT;
    }
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(clientId);
    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_ACTIVE_OUTPUT_DEVICE_CHANGE_CALLBACK), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("SetPreferOutputDeviceChangeCallback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnsetPreferOutputDeviceChangeCallback(const int32_t clientId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(clientId);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_ACTIVE_OUTPUT_DEVICE_CHANGE_CALLBACK),
        data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("unset active output device change callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

void AudioPolicyProxy::ReadAudioFocusInfo(MessageParcel &reply,
    std::list<std::pair<AudioInterrupt, AudioFocuState>> &focusInfoList)
{
    std::pair<AudioInterrupt, AudioFocuState> focusInfo;

    focusInfo.first.streamUsage = static_cast<StreamUsage>(reply.ReadInt32());
    focusInfo.first.contentType = static_cast<ContentType>(reply.ReadInt32());
    focusInfo.first.audioFocusType.streamType = static_cast<AudioStreamType>(reply.ReadInt32());
    focusInfo.first.audioFocusType.sourceType = static_cast<SourceType>(reply.ReadInt32());
    focusInfo.first.audioFocusType.isPlay = reply.ReadBool();
    focusInfo.first.sessionID = reply.ReadInt32();
    focusInfo.first.pauseWhenDucked = reply.ReadBool();
    focusInfo.first.mode = static_cast<InterruptMode>(reply.ReadInt32());
    focusInfo.second = static_cast<AudioFocuState>(reply.ReadInt32());

    focusInfoList.push_back(focusInfo);
}

int32_t AudioPolicyProxy::GetAudioFocusInfoList(std::list<std::pair<AudioInterrupt, AudioFocuState>> &focusInfoList)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG(" GetAudioFocusInfoList WriteInterfaceToken failed");
        return ERROR;
    }
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AUDIO_FOCUS_INFO_LIST), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetAudioFocusInfoList, error: %d", error);
        return error;
    }
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

int32_t AudioPolicyProxy::RegisterFocusInfoChangeCallback(const int32_t clientId,
    const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (object == nullptr) {
        AUDIO_ERR_LOG("RegisterFocusInfoChangeCallback object is null");
        return ERR_NULL_OBJECT;
    }
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(clientId);
    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::REGISTER_FOCUS_INFO_CHANGE_CALLBACK), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("RegisterFocusInfoChangeCallback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnregisterFocusInfoChangeCallback(const int32_t clientId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(clientId);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UNREGISTER_FOCUS_INFO_CHANGE_CALLBACK),
        data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("unset focus info change callback failed, error: %{public}d", error);
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
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_CALLBACK), data, reply, option);
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
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_CALLBACK), data, reply, option);
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
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::ACTIVATE_INTERRUPT), data, reply, option);
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
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::DEACTIVATE_INTERRUPT), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: deactivate interrupt failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetAudioManagerInterruptCallback(const int32_t clientId, const sptr<IRemoteObject> &object)
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
    data.WriteInt32(clientId);
    (void)data.WriteRemoteObject(object);
    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_INTERRUPT_CALLBACK), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: set callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnsetAudioManagerInterruptCallback(const int32_t clientId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(clientId);

    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_INTERRUPT_CALLBACK), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: unset callback failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::RequestAudioFocus(const int32_t clientId, const AudioInterrupt &audioInterrupt)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(clientId);
    WriteAudioManagerInteruptParams(data, audioInterrupt);

    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::REQUEST_AUDIO_FOCUS), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy: activate interrupt failed, error: %{public}d", error);
        return error;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::AbandonAudioFocus(const int32_t clientId, const AudioInterrupt &audioInterrupt)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(clientId);
    WriteAudioManagerInteruptParams(data, audioInterrupt);

    int error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::ABANDON_AUDIO_FOCUS), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_STREAM_IN_FOCUS), data, reply, option);
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SESSION_INFO_IN_FOCUS), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("AudioPolicyProxy::GetSessionInfoInFocus failed, error: %d", error);
    }
    ReadAudioInterruptParams(reply, audioInterrupt);

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::SetVolumeKeyEventCallback(const int32_t clientPid,
    const sptr<IRemoteObject> &object, API_VERSION api_v)
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
    data.WriteInt32(static_cast<int32_t>(api_v));
    int result = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_VOLUME_KEY_EVENT_CALLBACK), data, reply, option);
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
    int result = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_VOLUME_KEY_EVENT_CALLBACK), data, reply, option);
    if (result != ERR_NONE) {
        AUDIO_ERR_LOG("UnsetVolumeKeyEventCallback failed, result: %{public}d", result);
        return result;
    }

    return reply.ReadInt32();
}

bool AudioPolicyProxy::VerifyClientMicrophonePermission(uint32_t appTokenId, int32_t appUid, bool privacyFlag,
    AudioPermissionState state)
{
    AUDIO_DEBUG_LOG("VerifyClientMicrophonePermission: [tid : %{public}d]", appTokenId);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("VerifyClientMicrophonePermission: WriteInterfaceToken failed");
        return false;
    }

    data.WriteUint32(appTokenId);
    data.WriteInt32(appUid);
    data.WriteBool(privacyFlag);
    data.WriteInt32(state);

    int result = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::QUERY_MICROPHONE_PERMISSION), data, reply, option);
    if (result != ERR_NONE) {
        AUDIO_ERR_LOG("VerifyClientMicrophonePermission failed, result: %{public}d", result);
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

    int result = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_USING_PEMISSION_FROM_PRIVACY), data, reply, option);
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

    int result = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::RECONFIGURE_CHANNEL), data, reply, option);
    if (result != ERR_NONE) {
        AUDIO_ERR_LOG("ReconfigureAudioChannel failed, result: %{public}d", result);
        return ERR_TRANSACTION_FAILED;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::RegisterAudioRendererEventListener(const int32_t clientPid, const sptr<IRemoteObject> &object)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_INFO_LOG("RegisterAudioRendererEventListener");
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("RegisterAudioRendererEventListener: WriteInterfaceToken failed");
        return ERROR;
    }
    if (object == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioRendererEventListener Event object is null");
        return ERR_NULL_OBJECT;
    }

    data.WriteInt32(clientPid);
    data.WriteRemoteObject(object);
    int32_t error = Remote() ->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::REGISTER_PLAYBACK_EVENT), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("RegisterAudioRendererEventListener register playback event failed , error: %d", error);
        return ERROR;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnregisterAudioRendererEventListener(const int32_t clientPid)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_INFO_LOG("AudioPolicyProxy::UnregisterAudioRendererEventListener");
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("UnregisterAudioRendererEventListener WriteInterfaceToken failed");
        return ERROR;
    }

    data.WriteInt32(clientPid);
    int32_t error = Remote() ->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UNREGISTER_PLAYBACK_EVENT), data, reply, option);
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

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AUDIO_LATENCY), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetAudioLatencyFromXml, error: %d", error);
        return ERR_TRANSACTION_FAILED;
    }

    return reply.ReadInt32();
}

bool AudioPolicyProxy::IsVolumeUnadjustable()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("IsVolumeUnadjustable: WriteInterfaceToken failed");
        return false;
    }
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_VOLUME_UNADJUSTABLE), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("isvolumeadjustable failed, error: %d", error);
    }
    return reply.ReadBool();
}

int32_t AudioPolicyProxy::AdjustVolumeByStep(VolumeAdjustType adjustType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: AdjustVolumeByStep WriteInterfaceToken failed");
        return IPC_PROXY_ERR;
    }

    data.WriteInt32(adjustType);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::ADJUST_VOLUME_BY_STEP), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetAudioLatencyFromXml, error: %d", error);
        return ERR_TRANSACTION_FAILED;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::AdjustSystemVolumeByStep(AudioVolumeType volumeType, VolumeAdjustType adjustType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: AdjustSystemVolumeByStep WriteInterfaceToken failed");
        return IPC_PROXY_ERR;
    }
    data.WriteInt32(volumeType);
    data.WriteInt32(adjustType);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::ADJUST_SYSTEM_VOLUME_BY_STEP), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetAudioLatencyFromXml, error: %d", error);
        return ERR_TRANSACTION_FAILED;
    }

    return reply.ReadInt32();
}

float AudioPolicyProxy::GetSystemVolumeInDb(AudioVolumeType volumeType, int32_t volumeLevel, DeviceType deviceType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy: GetSystemVolumeInDb failed");
        return false;
    }

    data.WriteInt32(static_cast<int32_t>(volumeType));
    data.WriteInt32(volumeLevel);
    data.WriteInt32(static_cast<int32_t>(deviceType));

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SYSTEM_VOLUME_IN_DB), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetSystemVolumeInDb failed, error: %d", error);
    }

    return reply.ReadFloat();
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

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SINK_LATENCY), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetSinkLatencyFromXml, error: %d", error);
        return 0;
    }

    return reply.ReadUint32();
}

int32_t AudioPolicyProxy::RegisterAudioCapturerEventListener(const int32_t clientPid, const sptr<IRemoteObject> &object)
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

    data.WriteInt32(clientPid);
    data.WriteRemoteObject(object);
    int32_t error = Remote() ->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::REGISTER_RECORDING_EVENT), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("RegisterAudioCapturerEventListener recording event failed , error: %d", error);
        return ERROR;
    }

    return reply.ReadInt32();
}

int32_t AudioPolicyProxy::UnregisterAudioCapturerEventListener(const int32_t clientPid)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    AUDIO_INFO_LOG("AudioPolicyProxy::UnregisterAudioCapturerEventListener");

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyProxy:: WriteInterfaceToken failed");
        return ERROR;
    }

    data.WriteInt32(clientPid);
    int32_t error = Remote() ->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UNREGISTER_RECORDING_EVENT), data, reply, option);
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

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::REGISTER_TRACKER), data, reply, option);
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

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UPDATE_TRACKER), data, reply, option);
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
    rendererChangeInfo->tokenId = reply.ReadInt32();

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
    rendererChangeInfo->outputDeviceInfo.displayName = reply.ReadString();
    rendererChangeInfo->outputDeviceInfo.networkId = reply.ReadString();
    rendererChangeInfo->outputDeviceInfo.interruptGroupId = reply.ReadInt32();
    rendererChangeInfo->outputDeviceInfo.volumeGroupId = reply.ReadInt32();
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
    capturerChangeInfo->inputDeviceInfo.displayName = reply.ReadString();
    capturerChangeInfo->inputDeviceInfo.networkId = reply.ReadString();
    capturerChangeInfo->inputDeviceInfo.interruptGroupId = reply.ReadInt32();
    capturerChangeInfo->inputDeviceInfo.volumeGroupId = reply.ReadInt32();
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

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_RENDERER_CHANGE_INFOS), data, reply, option);
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

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_CAPTURER_CHANGE_INFOS), data, reply, option);
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

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::UPDATE_STREAM_STATE), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("UPDATE_STREAM_STATE stream changed info event failed , error: %d", error);
        return ERROR;
    }

    return SUCCESS;
}

int32_t AudioPolicyProxy::GetVolumeGroupInfos(std::string networkId, std::vector<sptr<VolumeGroupInfo>> &infos)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG(" GetVolumeGroupById WriteInterfaceToken failed");
        return ERROR;
    }
    data.WriteString(networkId);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_VOLUME_GROUP_INFO), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetVolumeGroupInfo, error: %d", error);
        return error;
    }

    int32_t ret = reply.ReadInt32();
    if (ret > 0) {
        for (int32_t i = 0; i < ret; i++) {
            infos.push_back(VolumeGroupInfo::Unmarshalling(reply));
        }
        return SUCCESS;
    } else {
        return ret;
    }
}

int32_t AudioPolicyProxy::GetNetworkIdByGroupId(int32_t groupId, std::string &networkId)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG(" GetNetworkIdByGroupId WriteInterfaceToken failed");
        return ERROR;
    }
    data.WriteInt32(groupId);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_NETWORKID_BY_GROUP_ID), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetNetworkIdByGroupId, error: %d", error);
        return error;
    }

    networkId = reply.ReadString();
    int32_t ret = reply.ReadInt32();
    return ret;
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
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_AUDIO_RENDER_LOW_LATENCY_SUPPORTED), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("IsAudioRendererLowLatencySupported, error: %d", error);
        return ERR_TRANSACTION_FAILED;
    }

    return reply.ReadBool();
}

int32_t AudioPolicyProxy::SetSystemSoundUri(const std::string &key, const std::string &uri)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("SetSystemSoundUri WriteInterfaceToken failed");
        return IPC_PROXY_ERR;
    }
    data.WriteString(key);
    data.WriteString(uri);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_SYSTEM_SOUND_URI), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("SetSystemSoundUri failed, error: %d", error);
        return error;
    }
    return reply.ReadInt32();
}

std::string AudioPolicyProxy::GetSystemSoundUri(const std::string &key)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("GetSystemSoundUri WriteInterfaceToken failed");
        return "";
    }
    data.WriteString(key);
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SYSTEM_SOUND_URI), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetSystemSoundUri failed, error: %d", error);
        return "";
    }
    return reply.ReadString();
}

float AudioPolicyProxy::GetMinStreamVolume()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("GetMinStreamVolume WriteInterfaceToken failed");
        return -1;
    }
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MIN_VOLUME_STREAM), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get min volume for stream failed, error: %d", error);
        return error;
    }
    return reply.ReadFloat();
}

float AudioPolicyProxy::GetMaxStreamVolume()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("GetMaxStreamVolume: WriteInterfaceToken failed");
        return -1;
    }
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MAX_VOLUME_STREAM), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get max volume for stream failed, error: %d", error);
        return error;
    }
    return reply.ReadFloat();
}

int32_t AudioPolicyProxy::GetMaxRendererInstances()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("GetMaxRendererInstances WriteInterfaceToken failed");
        return ERROR;
    }
    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MAX_RENDERER_INSTANCES), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("GetMaxRendererInstances failed, error: %d", error);
        return ERROR;
    }
    return reply.ReadInt32();
}

static void PreprocessMode(Stream &stream, MessageParcel &reply, int countMode)
{
    int j, k;
    for (j = 0; j < countMode; j++) {
        StreamEffectMode streamEffectMode;
        streamEffectMode.mode = reply.ReadString();
        int countDev = reply.ReadInt32();
        if (countDev > 0) {
            for (k = 0; k < countDev; k++) {
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
    int j, k;
    for (j = 0; j < countMode; j++) {
        StreamEffectMode streamEffectMode;
        streamEffectMode.mode = reply.ReadString();
        int countDev = reply.ReadInt32();
        if (countDev > 0) {
            for (k = 0; k < countDev; k++) {
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

static int32_t QueryEffectSceneModeChkReply(int countPre, int countPost)
{
    if ((countPre < 0) || (countPre > AUDIO_EFFECT_COUNT_UPPER_LIMIT)) {
        AUDIO_ERR_LOG("QUERY_EFFECT_SCENEMODE read replyParcel failed");
        return -1;
    }
    if ((countPost < 0) || (countPost > AUDIO_EFFECT_COUNT_UPPER_LIMIT)) {
        AUDIO_ERR_LOG("QUERY_EFFECT_SCENEMODE read replyParcel failed");
        return -1;
    }
    return 0;
}

int32_t AudioPolicyProxy::QueryEffectSceneMode(SupportedEffectConfig &supportedEffectConfig)
{
    int i;
    int32_t error;
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("QueryEffectSceneMode: WriteInterfaceToken failed");
        return -1;
    }
    error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::QUERY_EFFECT_SCENEMODE), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get scene & mode failed, error: %d", error);
        return error;
    }
    int countPre = reply.ReadInt32();
    int countPost = reply.ReadInt32();
    error = QueryEffectSceneModeChkReply(countPre, countPost);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("get scene & mode failed, error: %d", error);
        return error;
    }
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
    return 0;
}

int32_t AudioPolicyProxy::SetPlaybackCapturerFilterInfos(const CaptureFilterOptions &filterOptions,
    uint32_t appTokenId, int32_t appUid, bool privacyFlag, AudioPermissionState state)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG(" SetPlaybackCapturerFilterInfos WriteInterfaceToken failed");
        return ERROR;
    }
    size_t ss = filterOptions.usages.size();
    data.WriteInt32(ss);
    for (size_t i = 0; i < ss; i++) {
        data.WriteInt32(filterOptions.usages[i]);
    }
    data.WriteUint32(appTokenId);
    data.WriteInt32(appUid);
    data.WriteBool(privacyFlag);
    data.WriteInt32(state);

    int32_t error = Remote()->SendRequest(
        static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_PLAYBACK_CAPTURER_FILTER_INFO), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("SetPlaybackCapturerFilterInfos failed, error: %d", error);
        return ERROR;
    }
    return reply.ReadInt32();
}

} // namespace AudioStandard
} // namespace OHOS
