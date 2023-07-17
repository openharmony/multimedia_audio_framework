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

#include "audio_policy_manager_stub.h"

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_policy_ipc_interface_code.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;

void AudioPolicyManagerStub::ReadAudioInterruptParams(MessageParcel &data, AudioInterrupt &audioInterrupt)
{
    audioInterrupt.streamUsage = static_cast<StreamUsage>(data.ReadInt32());
    audioInterrupt.contentType = static_cast<ContentType>(data.ReadInt32());
    audioInterrupt.audioFocusType.streamType = static_cast<AudioStreamType>(data.ReadInt32());
    audioInterrupt.audioFocusType.sourceType = static_cast<SourceType>(data.ReadInt32());
    audioInterrupt.audioFocusType.isPlay = data.ReadBool();
    audioInterrupt.sessionID = data.ReadUint32();
    audioInterrupt.pid = data.ReadInt32();
    audioInterrupt.mode = static_cast<InterruptMode>(data.ReadInt32());
}

void AudioPolicyManagerStub::ReadAudioManagerInterruptParams(MessageParcel &data, AudioInterrupt &audioInterrupt)
{
    audioInterrupt.streamUsage = static_cast<StreamUsage>(data.ReadInt32());
    audioInterrupt.contentType = static_cast<ContentType>(data.ReadInt32());
    audioInterrupt.audioFocusType.streamType = static_cast<AudioStreamType>(data.ReadInt32());
    audioInterrupt.audioFocusType.sourceType = static_cast<SourceType>(data.ReadInt32());
    audioInterrupt.audioFocusType.isPlay = data.ReadBool();
    audioInterrupt.pauseWhenDucked = data.ReadBool();
    audioInterrupt.pid = data.ReadInt32();
    audioInterrupt.mode = static_cast<InterruptMode>(data.ReadInt32());
}

void AudioPolicyManagerStub::WriteAudioInteruptParams(MessageParcel &reply, const AudioInterrupt &audioInterrupt)
{
    reply.WriteInt32(static_cast<int32_t>(audioInterrupt.streamUsage));
    reply.WriteInt32(static_cast<int32_t>(audioInterrupt.contentType));
    reply.WriteInt32(static_cast<int32_t>(audioInterrupt.audioFocusType.streamType));
    reply.WriteInt32(static_cast<int32_t>(audioInterrupt.audioFocusType.sourceType));
    reply.WriteBool(audioInterrupt.audioFocusType.isPlay);
    reply.WriteUint32(audioInterrupt.sessionID);
    reply.WriteInt32(audioInterrupt.pid);
    reply.WriteInt32(static_cast<int32_t>(audioInterrupt.mode));
}

void AudioPolicyManagerStub::ReadStreamChangeInfo(MessageParcel &data, const AudioMode &mode,
    AudioStreamChangeInfo &streamChangeInfo)
{
    if (mode == AUDIO_MODE_PLAYBACK) {
        streamChangeInfo.audioRendererChangeInfo.sessionId = data.ReadInt32();
        streamChangeInfo.audioRendererChangeInfo.rendererState = static_cast<RendererState>(data.ReadInt32());
        streamChangeInfo.audioRendererChangeInfo.clientUID = data.ReadInt32();
        streamChangeInfo.audioRendererChangeInfo.rendererInfo.contentType = static_cast<ContentType>(data.ReadInt32());
        streamChangeInfo.audioRendererChangeInfo.rendererInfo.streamUsage = static_cast<StreamUsage>(data.ReadInt32());
        streamChangeInfo.audioRendererChangeInfo.rendererInfo.rendererFlags = data.ReadInt32();
        return;
    } else {
        // mode == AUDIO_MODE_RECORDING
        streamChangeInfo.audioCapturerChangeInfo.sessionId = data.ReadInt32();
        streamChangeInfo.audioCapturerChangeInfo.capturerState = static_cast<CapturerState>(data.ReadInt32());
        streamChangeInfo.audioCapturerChangeInfo.clientUID = data.ReadInt32();
        streamChangeInfo.audioCapturerChangeInfo.capturerInfo.sourceType = static_cast<SourceType>(data.ReadInt32());
        streamChangeInfo.audioCapturerChangeInfo.capturerInfo.capturerFlags = data.ReadInt32();
    }
}

void AudioPolicyManagerStub::GetMaxVolumeLevelInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioVolumeType volumeType = static_cast<AudioVolumeType>(data.ReadInt32());
    int32_t maxLevel = GetMaxVolumeLevel(volumeType);
    reply.WriteInt32(maxLevel);
}

void AudioPolicyManagerStub::GetMinVolumeLevelInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioVolumeType volumeType = static_cast<AudioVolumeType>(data.ReadInt32());
    int32_t minLevel = GetMinVolumeLevel(volumeType);
    reply.WriteInt32(minLevel);
}

void AudioPolicyManagerStub::SetSystemVolumeLevelInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
    int32_t volumeLevel = data.ReadInt32();
    API_VERSION api_v = static_cast<API_VERSION>(data.ReadInt32());
    int result = SetSystemVolumeLevel(streamType, volumeLevel, api_v);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::SetRingerModeInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioRingerMode rMode = static_cast<AudioRingerMode>(data.ReadInt32());
    API_VERSION api_v = static_cast<API_VERSION>(data.ReadInt32());
    int32_t result = SetRingerMode(rMode, api_v);
    reply.WriteInt32(result);
}

#ifdef FEATURE_DTMF_TONE
void AudioPolicyManagerStub::GetToneInfoInternal(MessageParcel &data, MessageParcel &reply)
{
    std::shared_ptr<ToneInfo> ltoneInfo = GetToneConfig(data.ReadInt32());
    if (ltoneInfo == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: GetToneInfoInternal obj is null");
        return;
    }
    reply.WriteUint32(ltoneInfo->segmentCnt);
    reply.WriteUint32(ltoneInfo->repeatCnt);
    reply.WriteUint32(ltoneInfo->repeatSegment);
    for (uint32_t i = 0; i < ltoneInfo->segmentCnt; i++) {
        reply.WriteUint32(ltoneInfo->segments[i].duration);
        reply.WriteUint16(ltoneInfo->segments[i].loopCnt);
        reply.WriteUint16(ltoneInfo->segments[i].loopIndx);
        for (uint32_t j = 0; j < TONEINFO_MAX_WAVES + 1; j++) {
            reply.WriteUint16(ltoneInfo->segments[i].waveFreq[j]);
        }
    }
}

void AudioPolicyManagerStub::GetSupportedTonesInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t lToneListSize = 0;
    std::vector<int32_t> lToneList = GetSupportedTones();
    lToneListSize = static_cast<int32_t>(lToneList.size());
    reply.WriteInt32(lToneListSize);
    for (int i = 0; i < lToneListSize; i++) {
        reply.WriteInt32(lToneList[i]);
    }
}
#endif

void AudioPolicyManagerStub::GetRingerModeInternal(MessageParcel &reply)
{
    AudioRingerMode rMode = GetRingerMode();
    reply.WriteInt32(static_cast<int>(rMode));
}

void AudioPolicyManagerStub::SetAudioSceneInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioScene audioScene = static_cast<AudioScene>(data.ReadInt32());
    int32_t result = SetAudioScene(audioScene);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::SetMicrophoneMuteInternal(MessageParcel &data, MessageParcel &reply)
{
    bool isMute = data.ReadBool();
    int32_t result = SetMicrophoneMute(isMute);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::SetMicrophoneMuteAudioConfigInternal(MessageParcel &data, MessageParcel &reply)
{
    bool isMute = data.ReadBool();
    int32_t result = SetMicrophoneMuteAudioConfig(isMute);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::IsMicrophoneMuteInternal(MessageParcel &data, MessageParcel &reply)
{
    API_VERSION api_v = static_cast<API_VERSION>(data.ReadInt32());
    int32_t result = IsMicrophoneMute(api_v);
    reply.WriteBool(result);
}

void AudioPolicyManagerStub::GetAudioSceneInternal(MessageParcel &reply)
{
    AudioScene audioScene = GetAudioScene();
    reply.WriteInt32(static_cast<int>(audioScene));
}

void AudioPolicyManagerStub::GetSystemVolumeLevelInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
    int32_t volumeLevel = GetSystemVolumeLevel(streamType);
    reply.WriteInt32(volumeLevel);
}

void AudioPolicyManagerStub::SetLowPowerVolumeInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t streamId = data.ReadInt32();
    float volume = data.ReadFloat();
    int result = SetLowPowerVolume(streamId, volume);
    if (result == SUCCESS)
        reply.WriteInt32(AUDIO_OK);
    else
        reply.WriteInt32(AUDIO_ERR);
}

void AudioPolicyManagerStub::GetLowPowerVolumeInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t streamId = data.ReadInt32();
    float volume = GetLowPowerVolume(streamId);
    reply.WriteFloat(volume);
}

void AudioPolicyManagerStub::GetSingleStreamVolumeInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t streamId = data.ReadInt32();
    float volume = GetSingleStreamVolume(streamId);
    reply.WriteFloat(volume);
}

void AudioPolicyManagerStub::SetStreamMuteInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
    bool mute = data.ReadBool();
    API_VERSION api_v = static_cast<API_VERSION>(data.ReadInt32());
    int result = SetStreamMute(streamType, mute, api_v);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::GetStreamMuteInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
    bool mute = GetStreamMute(streamType);
    reply.WriteBool(mute);
}

void AudioPolicyManagerStub::IsStreamActiveInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
    bool isActive = IsStreamActive(streamType);
    reply.WriteBool(isActive);
}

void AudioPolicyManagerStub::AdjustVolumeByStepInternal(MessageParcel &data, MessageParcel &reply)
{
    VolumeAdjustType adjustType = static_cast<VolumeAdjustType>(data.ReadInt32());
    int32_t result = AdjustVolumeByStep(adjustType);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::GetSystemVolumeInDbInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioVolumeType volumeType = static_cast<AudioVolumeType>(data.ReadInt32());
    int32_t volumeLevel = data.ReadInt32();
    DeviceType deviceType = static_cast<DeviceType>(data.ReadInt32());
    float result = GetSystemVolumeInDb(volumeType, volumeLevel, deviceType);
    reply.WriteFloat(result);
}

void AudioPolicyManagerStub::IsVolumeUnadjustableInternal(MessageParcel &data, MessageParcel &reply)
{
    bool isVolumeUnadjustable = IsVolumeUnadjustable();
    reply.WriteBool(isVolumeUnadjustable);
}

void AudioPolicyManagerStub::AdjustSystemVolumeByStepInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioVolumeType volumeType = static_cast<AudioVolumeType>(data.ReadInt32());
    VolumeAdjustType adjustType = static_cast<VolumeAdjustType>(data.ReadInt32());
    int32_t result = AdjustSystemVolumeByStep(volumeType, adjustType);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::GetDevicesInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("GET_DEVICES AudioManagerStub");
    int deviceFlag = data.ReadInt32();
    DeviceFlag deviceFlagConfig = static_cast<DeviceFlag>(deviceFlag);
    std::vector<sptr<AudioDeviceDescriptor>> devices = GetDevices(deviceFlagConfig);
    int32_t size = static_cast<int32_t>(devices.size());
    AUDIO_DEBUG_LOG("GET_DEVICES size= %{public}d", size);
    reply.WriteInt32(size);
    for (int i = 0; i < size; i++) {
        devices[i]->Marshalling(reply);
    }
}

void AudioPolicyManagerStub::SetWakeUpAudioCapturerInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("SetWakeUpAudioCapturerInternal AudioManagerStub");
    InternalAudioCapturerOptions capturerOptions;
    capturerOptions.streamInfo.samplingRate = static_cast<AudioSamplingRate>(data.ReadInt32());
    capturerOptions.streamInfo.encoding = static_cast<AudioEncodingType>(data.ReadInt32());
    capturerOptions.streamInfo.format = static_cast<AudioSampleFormat>(data.ReadInt32());
    capturerOptions.streamInfo.channels = static_cast<AudioChannel>(data.ReadInt32());
    capturerOptions.capturerInfo.sourceType = static_cast<SourceType>(data.ReadInt32());
    capturerOptions.capturerInfo.capturerFlags = data.ReadInt32();
    bool result = SetWakeUpAudioCapturer(capturerOptions);
    if (result) {
        reply.WriteInt32(AUDIO_OK);
    } else {
        reply.WriteInt32(AUDIO_ERR);
    }
}

void AudioPolicyManagerStub::CloseWakeUpAudioCapturerInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("CloseWakeUpAudioCapturerInternal AudioManagerStub");
    bool result = CloseWakeUpAudioCapturer();
    if (result) {
        reply.WriteInt32(AUDIO_OK);
    } else {
        reply.WriteInt32(AUDIO_ERR);
    }
}

void AudioPolicyManagerStub::GetPreferOutputDeviceDescriptorsInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("GET_ACTIVE_OUTPUT_DEVICE_DESCRIPTORS AudioManagerStub");
    AudioRendererInfo rendererInfo;
    std::vector<sptr<AudioDeviceDescriptor>> devices = GetPreferOutputDeviceDescriptors(rendererInfo);
    int32_t size = static_cast<int32_t>(devices.size());
    AUDIO_DEBUG_LOG("GET_ACTIVE_OUTPUT_DEVICE_DESCRIPTORS size= %{public}d", size);
    reply.WriteInt32(size);
    for (int i = 0; i < size; i++) {
        devices[i]->Marshalling(reply);
    }
}

void AudioPolicyManagerStub::SetDeviceActiveInternal(MessageParcel &data, MessageParcel &reply)
{
    InternalDeviceType deviceType = static_cast<InternalDeviceType>(data.ReadInt32());
    bool active = data.ReadBool();
    int32_t result = SetDeviceActive(deviceType, active);
    if (result == SUCCESS)
        reply.WriteInt32(AUDIO_OK);
    else
        reply.WriteInt32(AUDIO_ERR);
}

void AudioPolicyManagerStub::IsDeviceActiveInternal(MessageParcel &data, MessageParcel &reply)
{
    InternalDeviceType deviceType = static_cast<InternalDeviceType>(data.ReadInt32());
    bool result = IsDeviceActive(deviceType);
    reply.WriteBool(result);
}

void AudioPolicyManagerStub::GetActiveOutputDeviceInternal(MessageParcel &data, MessageParcel &reply)
{
    InternalDeviceType deviceType = GetActiveOutputDevice();
    reply.WriteInt32(static_cast<int>(deviceType));
}

void AudioPolicyManagerStub::GetActiveInputDeviceInternal(MessageParcel &data, MessageParcel &reply)
{
    InternalDeviceType deviceType = GetActiveInputDevice();
    reply.WriteInt32(static_cast<int>(deviceType));
}

void AudioPolicyManagerStub::SetPreferOutputDeviceChangeCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: SetRingerModeCallback obj is null");
        return;
    }
    int32_t result = SetPreferOutputDeviceChangeCallback(clientId, object);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::UnsetPreferOutputDeviceChangeCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    int32_t result = UnsetPreferOutputDeviceChangeCallback(clientId);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::WriteAudioFocusInfo(MessageParcel &reply,
    const std::pair<AudioInterrupt, AudioFocuState> &focusInfo)
{
    reply.WriteInt32(focusInfo.first.streamUsage);
    reply.WriteInt32(focusInfo.first.contentType);
    reply.WriteInt32(focusInfo.first.audioFocusType.streamType);
    reply.WriteInt32(focusInfo.first.audioFocusType.sourceType);
    reply.WriteBool(focusInfo.first.audioFocusType.isPlay);
    reply.WriteInt32(focusInfo.first.sessionID);
    reply.WriteBool(focusInfo.first.pauseWhenDucked);
    reply.WriteInt32(focusInfo.first.mode);

    reply.WriteInt32(focusInfo.second);
}

void AudioPolicyManagerStub::GetAudioFocusInfoListInternal(MessageParcel &data, MessageParcel &reply)
{
    std::list<std::pair<AudioInterrupt, AudioFocuState>> focusInfoList;
    int32_t result = GetAudioFocusInfoList(focusInfoList);
    int32_t size = static_cast<int32_t>(focusInfoList.size());
    reply.WriteInt32(result);
    reply.WriteInt32(size);
    if (result == SUCCESS) {
        AUDIO_DEBUG_LOG("GetAudioFocusInfoList size= %{public}d", size);
        for (std::pair<AudioInterrupt, AudioFocuState> focusInfo : focusInfoList) {
            WriteAudioFocusInfo(reply, focusInfo);
        }
    }
}

void AudioPolicyManagerStub::RegisterFocusInfoChangeCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioFocusInfoCallback obj is null");
        return;
    }
    int32_t result = RegisterFocusInfoChangeCallback(clientId, object);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::UnregisterFocusInfoChangeCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    int32_t result = UnregisterFocusInfoChangeCallback(clientId);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::SetRingerModeCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    API_VERSION api_v = static_cast<API_VERSION>(data.ReadInt32());
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: SetRingerModeCallback obj is null");
        return;
    }
    int32_t result = SetRingerModeCallback(clientId, object, api_v);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::SetMicStateChangeCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: AudioInterruptCallback obj is null");
        return;
    }
    int32_t result = SetMicStateChangeCallback(clientId, object);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::UnsetRingerModeCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    int32_t result = UnsetRingerModeCallback(clientId);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::SelectOutputDeviceInternal(MessageParcel &data, MessageParcel &reply)
{
    sptr<AudioRendererFilter> audioRendererFilter = AudioRendererFilter::Unmarshalling(data);
    if (audioRendererFilter == nullptr) {
        AUDIO_ERR_LOG("AudioRendererFilter unmarshall fail.");
        return;
    }

    int validSize = 20; // Use 20 as limit.
    int size = data.ReadInt32();
    if (size <= 0 || size > validSize) {
        AUDIO_ERR_LOG("SelectOutputDevice get invalid device size.");
        return;
    }
    std::vector<sptr<AudioDeviceDescriptor>> targetOutputDevice;
    for (int i = 0; i < size; i++) {
        sptr<AudioDeviceDescriptor> audioDeviceDescriptor = AudioDeviceDescriptor::Unmarshalling(data);
        if (audioDeviceDescriptor == nullptr) {
            AUDIO_ERR_LOG("Unmarshalling fail.");
            return;
        }
        targetOutputDevice.push_back(audioDeviceDescriptor);
    }

    int32_t ret = SelectOutputDevice(audioRendererFilter, targetOutputDevice);
    reply.WriteInt32(ret);
}

void AudioPolicyManagerStub::GetSelectedDeviceInfoInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t uid = data.ReadInt32();
    int32_t pid = data.ReadInt32();
    AudioStreamType streamType =  static_cast<AudioStreamType>(data.ReadInt32());

    std::string deviceName = GetSelectedDeviceInfo(uid, pid, streamType);
    reply.WriteString(deviceName);
}

void AudioPolicyManagerStub::SelectInputDeviceInternal(MessageParcel &data, MessageParcel &reply)
{
    sptr<AudioCapturerFilter> audioCapturerFilter = AudioCapturerFilter::Unmarshalling(data);
    if (audioCapturerFilter == nullptr) {
        AUDIO_ERR_LOG("AudioCapturerFilter unmarshall fail.");
        return;
    }

    int validSize = 10; // Use 10 as limit.
    int size = data.ReadInt32();
    if (size <= 0 || size > validSize) {
        AUDIO_ERR_LOG("SelectInputDevice get invalid device size.");
        return;
    }
    std::vector<sptr<AudioDeviceDescriptor>> targetInputDevice;
    for (int i = 0; i < size; i++) {
        sptr<AudioDeviceDescriptor> audioDeviceDescriptor = AudioDeviceDescriptor::Unmarshalling(data);
        if (audioDeviceDescriptor == nullptr) {
            AUDIO_ERR_LOG("Unmarshalling fail.");
            return;
        }
        targetInputDevice.push_back(audioDeviceDescriptor);
    }

    int32_t ret = SelectInputDevice(audioCapturerFilter, targetInputDevice);
    reply.WriteInt32(ret);
}

void AudioPolicyManagerStub::SetDeviceChangeCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    DeviceFlag flag = static_cast<DeviceFlag>(data.ReadInt32());
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: AudioInterruptCallback obj is null");
        return;
    }
    int32_t result = SetDeviceChangeCallback(clientId, flag, object);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::UnsetDeviceChangeCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    DeviceFlag flag = static_cast<DeviceFlag>(data.ReadInt32());
    int32_t result = UnsetDeviceChangeCallback(clientId, flag);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::SetInterruptCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    uint32_t sessionID = data.ReadUint32();
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: AudioInterruptCallback obj is null");
        return;
    }
    int32_t result = SetAudioInterruptCallback(sessionID, object);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::UnsetInterruptCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    uint32_t sessionID = data.ReadUint32();
    int32_t result = UnsetAudioInterruptCallback(sessionID);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::ActivateInterruptInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioInterrupt audioInterrupt = {};
    ReadAudioInterruptParams(data, audioInterrupt);
    int32_t result = ActivateAudioInterrupt(audioInterrupt);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::DeactivateInterruptInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioInterrupt audioInterrupt = {};
    ReadAudioInterruptParams(data, audioInterrupt);
    int32_t result = DeactivateAudioInterrupt(audioInterrupt);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::SetAudioManagerInterruptCbInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: AudioInterruptCallback obj is null");
        return;
    }
    int32_t result = SetAudioManagerInterruptCallback(clientId, object);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::UnsetAudioManagerInterruptCbInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientId = data.ReadInt32();
    int32_t result = UnsetAudioManagerInterruptCallback(clientId);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::RequestAudioFocusInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioInterrupt audioInterrupt = {};
    int32_t clientId = data.ReadInt32();
    ReadAudioManagerInterruptParams(data, audioInterrupt);
    int32_t result = RequestAudioFocus(clientId, audioInterrupt);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::AbandonAudioFocusInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioInterrupt audioInterrupt = {};
    int32_t clientId = data.ReadInt32();
    ReadAudioManagerInterruptParams(data, audioInterrupt);
    int32_t result = AbandonAudioFocus(clientId, audioInterrupt);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::GetStreamInFocusInternal(MessageParcel &reply)
{
    AudioStreamType streamInFocus = GetStreamInFocus();
    reply.WriteInt32(static_cast<int32_t>(streamInFocus));
}

void AudioPolicyManagerStub::GetSessionInfoInFocusInternal(MessageParcel &reply)
{
    uint32_t invalidSessionID = static_cast<uint32_t>(-1);
    AudioInterrupt audioInterrupt {STREAM_USAGE_UNKNOWN, CONTENT_TYPE_UNKNOWN,
        {AudioStreamType::STREAM_DEFAULT, SourceType::SOURCE_TYPE_INVALID, true}, invalidSessionID};
    int32_t ret = GetSessionInfoInFocus(audioInterrupt);
    WriteAudioInteruptParams(reply, audioInterrupt);
    reply.WriteInt32(ret);
}

void AudioPolicyManagerStub::SetVolumeKeyEventCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientPid =  data.ReadInt32();
    sptr<IRemoteObject> remoteObject = data.ReadRemoteObject();
    API_VERSION api_v = static_cast<API_VERSION>(data.ReadInt32());
    if (remoteObject == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: AudioManagerCallback obj is null");
        return;
    }
    int ret = SetVolumeKeyEventCallback(clientPid, remoteObject, api_v);
    reply.WriteInt32(ret);
}

void AudioPolicyManagerStub::UnsetVolumeKeyEventCallbackInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t clientPid = data.ReadInt32();
    int ret = UnsetVolumeKeyEventCallback(clientPid);
    reply.WriteInt32(ret);
}

void AudioPolicyManagerStub::VerifyClientMicrophonePermissionInternal(MessageParcel &data, MessageParcel &reply)
{
    uint32_t appTokenId = data.ReadUint32();
    uint32_t appUid = data.ReadInt32();
    bool privacyFlag = data.ReadBool();
    AudioPermissionState state = static_cast<AudioPermissionState>(data.ReadInt32());
    bool ret = VerifyClientMicrophonePermission(appTokenId, appUid, privacyFlag, state);
    reply.WriteBool(ret);
}

void AudioPolicyManagerStub::getUsingPemissionFromPrivacyInternal(MessageParcel &data, MessageParcel &reply)
{
    std::string permissionName = data.ReadString();
    uint32_t appTokenId = data.ReadUint32();
    AudioPermissionState state = static_cast<AudioPermissionState>(data.ReadInt32());
    bool ret = getUsingPemissionFromPrivacy(permissionName, appTokenId, state);
    reply.WriteBool(ret);
}

void AudioPolicyManagerStub::GetAudioLatencyFromXmlInternal(MessageParcel &data, MessageParcel &reply)
{
    int ret = GetAudioLatencyFromXml();
    reply.WriteInt32(ret);
}

void AudioPolicyManagerStub::GetSinkLatencyFromXmlInternal(MessageParcel &data, MessageParcel &reply)
{
    uint32_t ret = GetSinkLatencyFromXml();
    reply.WriteUint32(ret);
}

void AudioPolicyManagerStub::ReconfigureAudioChannelInternal(MessageParcel &data, MessageParcel &reply)
{
    uint32_t count = data.ReadUint32();
    DeviceType deviceType = static_cast<DeviceType>(data.ReadInt32());
    int32_t ret = ReconfigureAudioChannel(count, deviceType);
    reply.WriteInt32(ret);
}

void AudioPolicyManagerStub::RegisterAudioRendererEventListenerInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:register event listener entered");
    int32_t clientUid =  data.ReadInt32();
    sptr<IRemoteObject> remoteObject = data.ReadRemoteObject();
    if (remoteObject == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: AudioRendererStateCallback obj is null");
        return;
    }
    int ret = RegisterAudioRendererEventListener(clientUid, remoteObject);
    reply.WriteInt32(ret);
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:register event listener exit");
}

void AudioPolicyManagerStub::UnregisterAudioRendererEventListenerInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:unregister event listener entered");
    int32_t clientUid = data.ReadInt32();
    int ret = UnregisterAudioRendererEventListener(clientUid);
    reply.WriteInt32(ret);
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:unregister event listener exit");
}

void AudioPolicyManagerStub::RegisterAudioCapturerEventListenerInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:cap register event listener entered");
    int32_t clientUid =  data.ReadInt32();
    sptr<IRemoteObject> remoteObject = data.ReadRemoteObject();
    if (remoteObject == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: AudioCapturerStateCallback obj is null");
        return;
    }
    int ret = RegisterAudioCapturerEventListener(clientUid, remoteObject);
    reply.WriteInt32(ret);
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:cap register event listener exit");
}

void AudioPolicyManagerStub::UnregisterAudioCapturerEventListenerInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:cap unnregister event listener entered");
    int32_t clientUid = data.ReadInt32();
    int ret = UnregisterAudioCapturerEventListener(clientUid);
    reply.WriteInt32(ret);
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:cap unregister event listener exit");
}

void AudioPolicyManagerStub::RegisterTrackerInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:register tracker internal entered");

    AudioStreamChangeInfo streamChangeInfo = {};
    AudioMode mode = static_cast<AudioMode> (data.ReadInt32());
    ReadStreamChangeInfo(data, mode, streamChangeInfo);
    sptr<IRemoteObject> remoteObject = data.ReadRemoteObject();
    if (remoteObject == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub: Client Tracker obj is null");
        return;
    }

    int ret = RegisterTracker(mode, streamChangeInfo, remoteObject);
    reply.WriteInt32(ret);
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:register tracker internal ret = %{public}d", ret);
}

void AudioPolicyManagerStub::UpdateTrackerInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:update tracker internal entered");

    AudioStreamChangeInfo streamChangeInfo = {};
    AudioMode mode = static_cast<AudioMode> (data.ReadInt32());
    ReadStreamChangeInfo(data, mode, streamChangeInfo);
    int ret = UpdateTracker(mode, streamChangeInfo);
    reply.WriteInt32(ret);
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:update tracker internal ret = %{public}d", ret);
}

void AudioPolicyManagerStub::GetRendererChangeInfosInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:Renderer change info internal entered");

    size_t size = 0;
    vector<unique_ptr<AudioRendererChangeInfo>> audioRendererChangeInfos;
    int ret = GetCurrentRendererChangeInfos(audioRendererChangeInfos);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub:GetRendererChangeInfos Error!!");
        reply.WriteInt32(size);
        return;
    }

    size = audioRendererChangeInfos.size();
    reply.WriteInt32(size);
    for (const unique_ptr<AudioRendererChangeInfo> &rendererChangeInfo: audioRendererChangeInfos) {
        if (!rendererChangeInfo) {
            AUDIO_ERR_LOG("AudioPolicyManagerStub:Renderer change info null, something wrong!!");
            continue;
        }
        reply.WriteInt32(rendererChangeInfo->sessionId);
        reply.WriteInt32(rendererChangeInfo->rendererState);
        reply.WriteInt32(rendererChangeInfo->clientUID);
        reply.WriteInt32(rendererChangeInfo->tokenId);

        reply.WriteInt32(rendererChangeInfo->rendererInfo.contentType);
        reply.WriteInt32(rendererChangeInfo->rendererInfo.streamUsage);
        reply.WriteInt32(rendererChangeInfo->rendererInfo.rendererFlags);

        reply.WriteInt32(rendererChangeInfo->outputDeviceInfo.deviceType);
        reply.WriteInt32(rendererChangeInfo->outputDeviceInfo.deviceRole);
        reply.WriteInt32(rendererChangeInfo->outputDeviceInfo.deviceId);
        reply.WriteInt32(rendererChangeInfo->outputDeviceInfo.channelMasks);
        reply.WriteInt32(rendererChangeInfo->outputDeviceInfo.audioStreamInfo.samplingRate);
        reply.WriteInt32(rendererChangeInfo->outputDeviceInfo.audioStreamInfo.encoding);
        reply.WriteInt32(rendererChangeInfo->outputDeviceInfo.audioStreamInfo.format);
        reply.WriteInt32(rendererChangeInfo->outputDeviceInfo.audioStreamInfo.channels);
        reply.WriteString(rendererChangeInfo->outputDeviceInfo.deviceName);
        reply.WriteString(rendererChangeInfo->outputDeviceInfo.macAddress);
        reply.WriteString(rendererChangeInfo->outputDeviceInfo.displayName);
        reply.WriteString(rendererChangeInfo->outputDeviceInfo.networkId);
        reply.WriteInt32(rendererChangeInfo->outputDeviceInfo.interruptGroupId);
        reply.WriteInt32(rendererChangeInfo->outputDeviceInfo.volumeGroupId);
    }

    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:Renderer change info internal exit");
}

void AudioPolicyManagerStub::GetCapturerChangeInfosInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:Capturer change info internal entered");
    size_t size = 0;
    vector<unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
    int32_t ret = GetCurrentCapturerChangeInfos(audioCapturerChangeInfos);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("AudioPolicyManagerStub:GetCapturerChangeInfos Error!!");
        reply.WriteInt32(size);
        return;
    }

    size = audioCapturerChangeInfos.size();
    reply.WriteInt32(size);
    for (const unique_ptr<AudioCapturerChangeInfo> &capturerChangeInfo: audioCapturerChangeInfos) {
        if (!capturerChangeInfo) {
            AUDIO_ERR_LOG("AudioPolicyManagerStub:Capturer change info null, something wrong!!");
            continue;
        }
        reply.WriteInt32(capturerChangeInfo->sessionId);
        reply.WriteInt32(capturerChangeInfo->capturerState);
        reply.WriteInt32(capturerChangeInfo->clientUID);
        reply.WriteInt32(capturerChangeInfo->capturerInfo.sourceType);
        reply.WriteInt32(capturerChangeInfo->capturerInfo.capturerFlags);

        reply.WriteInt32(capturerChangeInfo->inputDeviceInfo.deviceType);
        reply.WriteInt32(capturerChangeInfo->inputDeviceInfo.deviceRole);
        reply.WriteInt32(capturerChangeInfo->inputDeviceInfo.deviceId);
        reply.WriteInt32(capturerChangeInfo->inputDeviceInfo.channelMasks);
        reply.WriteInt32(capturerChangeInfo->inputDeviceInfo.audioStreamInfo.samplingRate);
        reply.WriteInt32(capturerChangeInfo->inputDeviceInfo.audioStreamInfo.encoding);
        reply.WriteInt32(capturerChangeInfo->inputDeviceInfo.audioStreamInfo.format);
        reply.WriteInt32(capturerChangeInfo->inputDeviceInfo.audioStreamInfo.channels);
        reply.WriteString(capturerChangeInfo->inputDeviceInfo.deviceName);
        reply.WriteString(capturerChangeInfo->inputDeviceInfo.macAddress);
        reply.WriteString(capturerChangeInfo->inputDeviceInfo.displayName);
        reply.WriteString(capturerChangeInfo->inputDeviceInfo.networkId);
        reply.WriteInt32(capturerChangeInfo->inputDeviceInfo.interruptGroupId);
        reply.WriteInt32(capturerChangeInfo->inputDeviceInfo.volumeGroupId);
    }
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:Capturer change info internal exit");
}

void AudioPolicyManagerStub::UpdateStreamStateInternal(MessageParcel &data, MessageParcel &reply)
{
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:UpdateStreamStateInternal change info internal entered");
    int32_t clientUid = data.ReadInt32();
    StreamSetState streamSetState = static_cast<StreamSetState>(data.ReadInt32());
    AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());

    int32_t result = UpdateStreamState(clientUid, streamSetState, streamType);
    reply.WriteInt32(result);
    AUDIO_DEBUG_LOG("AudioPolicyManagerStub:UpdateStreamStateInternal change info internal exit");
}

void AudioPolicyManagerStub::GetVolumeGroupInfoInternal(MessageParcel& data, MessageParcel& reply)
{
    AUDIO_DEBUG_LOG("GetVolumeGroupInfoInternal entered");
    std::string networkId = data.ReadString();
    std::vector<sptr<VolumeGroupInfo>> groupInfos;
    int32_t ret = GetVolumeGroupInfos(networkId, groupInfos);
    int32_t size = static_cast<int32_t>(groupInfos.size());
    AUDIO_DEBUG_LOG("GET_DEVICES size= %{public}d", size);
    
    if (ret == SUCCESS && size > 0) {
        reply.WriteInt32(size);
        for (int i = 0; i < size; i++) {
            groupInfos[i]->Marshalling(reply);
        }
    } else {
        reply.WriteInt32(ret);
    }
    
    AUDIO_DEBUG_LOG("GetVolumeGroups internal exit");
}

void AudioPolicyManagerStub::GetNetworkIdByGroupIdInternal(MessageParcel& data, MessageParcel& reply)
{
    AUDIO_DEBUG_LOG("GetNetworkIdByGroupId entered");
    int32_t groupId = data.ReadInt32();
    std::string networkId;
    int32_t ret = GetNetworkIdByGroupId(groupId, networkId);

    reply.WriteString(networkId);
    reply.WriteInt32(ret);
}

void AudioPolicyManagerStub::IsAudioRendererLowLatencySupportedInternal(MessageParcel &data, MessageParcel &reply)
{
    AudioStreamInfo audioStreamInfo = {};
    audioStreamInfo.samplingRate = static_cast<AudioSamplingRate>(data.ReadInt32());
    audioStreamInfo.channels = static_cast<AudioChannel>(data.ReadInt32());
    audioStreamInfo.format = static_cast<OHOS::AudioStandard::AudioSampleFormat>(data.ReadInt32());
    audioStreamInfo.encoding = static_cast<AudioEncodingType>(data.ReadInt32());
    bool isSupported = IsAudioRendererLowLatencySupported(audioStreamInfo);
    reply.WriteBool(isSupported);
}

void AudioPolicyManagerStub::SetSystemSoundUriInternal(MessageParcel &data, MessageParcel &reply)
{
    std::string key = data.ReadString();
    std::string value = data.ReadString();
    int32_t result =  SetSystemSoundUri(key, value);
    reply.WriteInt32(result);
}

void AudioPolicyManagerStub::GetSystemSoundUriInternal(MessageParcel &data, MessageParcel &reply)
{
    std::string key = data.ReadString();
    std::string result = GetSystemSoundUri(key);
    reply.WriteString(result);
}

void AudioPolicyManagerStub::GetMinStreamVolumeInternal(MessageParcel &data, MessageParcel &reply)
{
    float volume = GetMinStreamVolume();
    reply.WriteFloat(volume);
}

void AudioPolicyManagerStub::GetMaxStreamVolumeInternal(MessageParcel &data, MessageParcel &reply)
{
    float volume = GetMaxStreamVolume();
    reply.WriteFloat(volume);
}

void AudioPolicyManagerStub::GetMaxRendererInstancesInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t result =  GetMaxRendererInstances();
    reply.WriteInt32(result);
}

static void PreprocessMode(SupportedEffectConfig &supportedEffectConfig, MessageParcel &reply, int32_t i, int32_t j)
{
    reply.WriteString(supportedEffectConfig.preProcessNew.stream[i].streamEffectMode[j].mode);
    int32_t countDev = supportedEffectConfig.preProcessNew.stream[i].streamEffectMode[j].devicePort.size();
    reply.WriteInt32(countDev);
    if (countDev > 0) {
        for (int32_t k = 0; k < countDev; k++) {
            reply.WriteString(supportedEffectConfig.preProcessNew.stream[i].streamEffectMode[j].devicePort[k].type);
            reply.WriteString(supportedEffectConfig.preProcessNew.stream[i].streamEffectMode[j].devicePort[k].chain);
        }
    }
}
static void PreprocessProcess(SupportedEffectConfig &supportedEffectConfig, MessageParcel &reply, int32_t i)
{
    reply.WriteString(supportedEffectConfig.preProcessNew.stream[i].scene);
    int32_t countMode = supportedEffectConfig.preProcessNew.stream[i].streamEffectMode.size();
    reply.WriteInt32(countMode);
    if (countMode > 0) {
        for (int32_t j = 0; j < countMode; j++) {
            PreprocessMode(supportedEffectConfig, reply, i, j);
        }
    }
}
static void PostprocessMode(SupportedEffectConfig &supportedEffectConfig, MessageParcel &reply, int32_t i, int32_t j)
{
    reply.WriteString(supportedEffectConfig.postProcessNew.stream[i].streamEffectMode[j].mode);
    int32_t countDev = supportedEffectConfig.postProcessNew.stream[i].streamEffectMode[j].devicePort.size();
    reply.WriteInt32(countDev);
    if (countDev > 0) {
        for (int32_t k = 0; k < countDev; k++) {
            reply.WriteString(supportedEffectConfig.postProcessNew.stream[i].streamEffectMode[j].devicePort[k].type);
            reply.WriteString(supportedEffectConfig.postProcessNew.stream[i].streamEffectMode[j].devicePort[k].chain);
        }
    }
}
static void PostprocessProcess(SupportedEffectConfig &supportedEffectConfig, MessageParcel &reply, int32_t i)
{
    // i th stream
    reply.WriteString(supportedEffectConfig.postProcessNew.stream[i].scene);
    int countMode = supportedEffectConfig.postProcessNew.stream[i].streamEffectMode.size();
    reply.WriteInt32(countMode);
    if (countMode > 0) {
        for (int32_t j = 0; j < countMode; j++) {
            PostprocessMode(supportedEffectConfig, reply, i, j);
        }
    }
}

void AudioPolicyManagerStub::QueryEffectSceneModeInternal(MessageParcel &data, MessageParcel &reply)
{
    int32_t i;
    SupportedEffectConfig supportedEffectConfig;
    int32_t ret = QueryEffectSceneMode(supportedEffectConfig); // audio_policy_server.cpp
    if (ret == -1) {
        AUDIO_ERR_LOG("default mode is unavailable !");
        return;
    }

    int32_t countPre = supportedEffectConfig.preProcessNew.stream.size();
    int32_t countPost = supportedEffectConfig.postProcessNew.stream.size();
    reply.WriteInt32(countPre);
    reply.WriteInt32(countPost);

    if (countPre > 0) {
        for (i = 0; i < countPre; i++) {
            PreprocessProcess(supportedEffectConfig, reply, i);
        }
    }
    if (countPost > 0) {
        for (i = 0; i < countPost; i++) {
            PostprocessProcess(supportedEffectConfig, reply, i);
        }
    }
}

void AudioPolicyManagerStub::SetPlaybackCapturerFilterInfosInternal(MessageParcel &data, MessageParcel &reply)
{
    CaptureFilterOptions filterInfo;
    int32_t ss = data.ReadInt32();
    int32_t tmp_usage;
    if (ss < 0 || ss >= INT32_MAX) {
        reply.WriteInt32(ERROR);
        return;
    }
    for (int32_t i = 0; i < ss; i++) {
        tmp_usage = data.ReadInt32();
        if (std::find(AUDIO_SUPPORTED_STREAM_USAGES.begin(), AUDIO_SUPPORTED_STREAM_USAGES.end(), tmp_usage) ==
            AUDIO_SUPPORTED_STREAM_USAGES.end()) {
            continue;
        }
        filterInfo.usages.push_back(static_cast<StreamUsage>(tmp_usage));
    }
    uint32_t appTokenId = data.ReadUint32();
    uint32_t appUid = data.ReadInt32();
    bool privacyFlag = data.ReadBool();
    AudioPermissionState state = static_cast<AudioPermissionState>(data.ReadInt32());

    int32_t ret = SetPlaybackCapturerFilterInfos(filterInfo, appTokenId, appUid, privacyFlag, state);
    reply.WriteInt32(ret);
}

int AudioPolicyManagerStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    if (data.ReadInterfaceToken() != GetDescriptor()) {
        AUDIO_ERR_LOG("OnRemoteRequest: ReadInterfaceToken failed");
        return -1;
    }
    switch (code) {
        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MAX_VOLUMELEVEL):
            GetMaxVolumeLevelInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MIN_VOLUMELEVEL):
            GetMinVolumeLevelInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_SYSTEM_VOLUMELEVEL):
            SetSystemVolumeLevelInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_RINGER_MODE):
            SetRingerModeInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_RINGER_MODE):
            GetRingerModeInternal(reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_AUDIO_SCENE):
            SetAudioSceneInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AUDIO_SCENE):
            GetAudioSceneInternal(reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_MICROPHONE_MUTE):
            SetMicrophoneMuteInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_MICROPHONE_MUTE_AUDIO_CONFIG):
            SetMicrophoneMuteAudioConfigInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_MICROPHONE_MUTE):
            IsMicrophoneMuteInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SYSTEM_VOLUMELEVEL):
            GetSystemVolumeLevelInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_STREAM_MUTE):
            SetStreamMuteInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_STREAM_MUTE):
            GetStreamMuteInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_STREAM_ACTIVE):
            IsStreamActiveInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_DEVICE_ACTIVE):
            SetDeviceActiveInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_DEVICE_ACTIVE):
            IsDeviceActiveInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_ACTIVE_OUTPUT_DEVICE):
            GetActiveOutputDeviceInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_ACTIVE_INPUT_DEVICE):
            GetActiveInputDeviceInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_RINGERMODE_CALLBACK):
            SetRingerModeCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_RINGERMODE_CALLBACK):
            UnsetRingerModeCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_MIC_STATE_CHANGE_CALLBACK):
            SetMicStateChangeCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_DEVICE_CHANGE_CALLBACK):
            SetDeviceChangeCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_DEVICE_CHANGE_CALLBACK):
            UnsetDeviceChangeCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_CALLBACK):
            SetInterruptCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_CALLBACK):
            UnsetInterruptCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::ACTIVATE_INTERRUPT):
            ActivateInterruptInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::DEACTIVATE_INTERRUPT):
            DeactivateInterruptInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_INTERRUPT_CALLBACK):
            SetAudioManagerInterruptCbInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_INTERRUPT_CALLBACK):
            UnsetAudioManagerInterruptCbInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::REQUEST_AUDIO_FOCUS):
            RequestAudioFocusInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::ABANDON_AUDIO_FOCUS):
            AbandonAudioFocusInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_VOLUME_KEY_EVENT_CALLBACK):
            SetVolumeKeyEventCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_VOLUME_KEY_EVENT_CALLBACK):
            UnsetVolumeKeyEventCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_STREAM_IN_FOCUS):
            GetStreamInFocusInternal(reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SESSION_INFO_IN_FOCUS):
            GetSessionInfoInFocusInternal(reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_DEVICES):
            GetDevicesInternal(data, reply);
            break;
        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_WAKEUP_AUDIOCAPTURER):
            SetWakeUpAudioCapturerInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::CLOSE_WAKEUP_AUDIOCAPTURER):
            CloseWakeUpAudioCapturerInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::QUERY_MICROPHONE_PERMISSION):
            VerifyClientMicrophonePermissionInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SELECT_OUTPUT_DEVICE):
            SelectOutputDeviceInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SELECTED_DEVICE_INFO):
            GetSelectedDeviceInfoInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SELECT_INPUT_DEVICE):
            SelectInputDeviceInternal(data, reply);
            break;
#ifdef FEATURE_DTMF_TONE
        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_TONEINFO):
            GetToneInfoInternal(data, reply);
            break;
        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SUPPORTED_TONES):
            GetSupportedTonesInternal(data, reply);
            break;
#endif
        case static_cast<uint32_t>(AudioPolicyInterfaceCode::RECONFIGURE_CHANNEL):
            ReconfigureAudioChannelInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AUDIO_LATENCY):
            GetAudioLatencyFromXmlInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SINK_LATENCY):
            GetSinkLatencyFromXmlInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::REGISTER_PLAYBACK_EVENT):
            RegisterAudioRendererEventListenerInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UNREGISTER_PLAYBACK_EVENT):
            UnregisterAudioRendererEventListenerInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::REGISTER_RECORDING_EVENT):
            RegisterAudioCapturerEventListenerInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UNREGISTER_RECORDING_EVENT):
            UnregisterAudioCapturerEventListenerInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::REGISTER_TRACKER):
            RegisterTrackerInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UPDATE_TRACKER):
            UpdateTrackerInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_RENDERER_CHANGE_INFOS):
            GetRendererChangeInfosInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_CAPTURER_CHANGE_INFOS):
            GetCapturerChangeInfosInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UPDATE_STREAM_STATE):
            UpdateStreamStateInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_LOW_POWER_STREM_VOLUME):
            SetLowPowerVolumeInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_LOW_POWRR_STREM_VOLUME):
            GetLowPowerVolumeInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SINGLE_STREAM_VOLUME):
            GetSingleStreamVolumeInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_VOLUME_GROUP_INFO):
            GetVolumeGroupInfoInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_NETWORKID_BY_GROUP_ID):
            GetNetworkIdByGroupIdInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_AUDIO_RENDER_LOW_LATENCY_SUPPORTED):
             IsAudioRendererLowLatencySupportedInternal(data, reply);
             break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_USING_PEMISSION_FROM_PRIVACY):
             getUsingPemissionFromPrivacyInternal(data, reply);
             break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_ACTIVE_OUTPUT_DEVICE_DESCRIPTORS):
            GetPreferOutputDeviceDescriptorsInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_ACTIVE_OUTPUT_DEVICE_CHANGE_CALLBACK):
            SetPreferOutputDeviceChangeCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UNSET_ACTIVE_OUTPUT_DEVICE_CHANGE_CALLBACK):
            UnsetPreferOutputDeviceChangeCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_AUDIO_FOCUS_INFO_LIST):
            GetAudioFocusInfoListInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::REGISTER_FOCUS_INFO_CHANGE_CALLBACK):
            RegisterFocusInfoChangeCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::UNREGISTER_FOCUS_INFO_CHANGE_CALLBACK):
            UnregisterFocusInfoChangeCallbackInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_SYSTEM_SOUND_URI):
            SetSystemSoundUriInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SYSTEM_SOUND_URI):
            GetSystemSoundUriInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MIN_VOLUME_STREAM):
            GetMinStreamVolumeInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MAX_VOLUME_STREAM):
            GetMaxStreamVolumeInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_MAX_RENDERER_INSTANCES):
            GetMaxRendererInstancesInternal(data, reply);
            break;
        
        case static_cast<uint32_t>(AudioPolicyInterfaceCode::QUERY_EFFECT_SCENEMODE):
            QueryEffectSceneModeInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::IS_VOLUME_UNADJUSTABLE):
            IsVolumeUnadjustableInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::ADJUST_VOLUME_BY_STEP):
            AdjustVolumeByStepInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::ADJUST_SYSTEM_VOLUME_BY_STEP):
            AdjustSystemVolumeByStepInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::GET_SYSTEM_VOLUME_IN_DB):
            GetSystemVolumeInDbInternal(data, reply);
            break;

        case static_cast<uint32_t>(AudioPolicyInterfaceCode::SET_PLAYBACK_CAPTURER_FILTER_INFO):
            SetPlaybackCapturerFilterInfosInternal(data, reply);
            break;

        default:
            AUDIO_ERR_LOG("default case, need check AudioPolicyManagerStub");
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
    }
    return AUDIO_OK;
}
} // namespace audio_policy
} // namespace OHOS
