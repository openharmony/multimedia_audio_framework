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

#include "audio_system_manager.h"

#include "iservice_registry.h"
#include "system_ability_definition.h"

#include "audio_log.h"
#include "audio_errors.h"
#include "audio_manager_proxy.h"
#include "audio_server_death_recipient.h"
#include "audio_stream.h"
#include "audio_policy_manager.h"
#include "audio_volume_key_event_callback_stub.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;

const map<pair<ContentType, StreamUsage>, AudioStreamType> AudioSystemManager::streamTypeMap_
    = AudioSystemManager::CreateStreamMap();
mutex g_asProxyMutex;
sptr<IStandardAudioService> g_asProxy = nullptr;

AudioSystemManager::AudioSystemManager()
{
    AUDIO_DEBUG_LOG("AudioSystemManager start");
}

AudioSystemManager::~AudioSystemManager()
{
    AUDIO_DEBUG_LOG("AudioSystemManager::~AudioSystemManager");
    if (cbClientId_ != -1) {
        UnsetRingerModeCallback(cbClientId_);
    }

    if (volumeChangeClientPid_ != -1) {
        AUDIO_DEBUG_LOG("AudioSystemManager::~AudioSystemManager UnregisterVolumeKeyEventCallback");
        (void)UnregisterVolumeKeyEventCallback(volumeChangeClientPid_);
    }
}

AudioSystemManager *AudioSystemManager::GetInstance()
{
    static AudioSystemManager audioManager;
    return &audioManager;
}

uint32_t AudioSystemManager::GetCallingPid()
{
    return getpid();
}

map<pair<ContentType, StreamUsage>, AudioStreamType> AudioSystemManager::CreateStreamMap()
{
    map<pair<ContentType, StreamUsage>, AudioStreamType> streamMap;

    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_UNKNOWN)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_MEDIA)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_VOICE_COMMUNICATION)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_VOICE_ASSISTANT)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_NOTIFICATION_RINGTONE)] = AudioStreamType::STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_UNKNOWN)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_MEDIA)] = AudioStreamType::STREAM_VOICE_ASSISTANT;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_VOICE_COMMUNICATION)] = AudioStreamType::STREAM_VOICE_CALL;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_VOICE_ASSISTANT)] = AudioStreamType::STREAM_VOICE_ASSISTANT;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_NOTIFICATION_RINGTONE)] = AudioStreamType::STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_UNKNOWN)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_MEDIA)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_VOICE_COMMUNICATION)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_VOICE_ASSISTANT)] = AudioStreamType::STREAM_VOICE_ASSISTANT;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_NOTIFICATION_RINGTONE)] = AudioStreamType::STREAM_RING;

    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_UNKNOWN)] = AudioStreamType::STREAM_MEDIA;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_MEDIA)] = AudioStreamType::STREAM_MEDIA;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_VOICE_COMMUNICATION)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_VOICE_ASSISTANT)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_NOTIFICATION_RINGTONE)] = AudioStreamType::STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_UNKNOWN)] = AudioStreamType::STREAM_NOTIFICATION;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_MEDIA)] = AudioStreamType::STREAM_NOTIFICATION;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_VOICE_COMMUNICATION)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_VOICE_ASSISTANT)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_NOTIFICATION_RINGTONE)] = AudioStreamType::STREAM_RING;

    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_UNKNOWN)] = AudioStreamType::STREAM_RING;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_MEDIA)] = AudioStreamType::STREAM_RING;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_VOICE_COMMUNICATION)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_VOICE_ASSISTANT)] = AudioStreamType::STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_NOTIFICATION_RINGTONE)] = AudioStreamType::STREAM_RING;

    return streamMap;
}

AudioStreamType AudioSystemManager::GetStreamType(ContentType contentType, StreamUsage streamUsage)
{
    AudioStreamType streamType = AudioStreamType::STREAM_MUSIC;
    auto pos = streamTypeMap_.find(make_pair(contentType, streamUsage));
    if (pos != streamTypeMap_.end()) {
        streamType = pos->second;
    }

    if (streamType == AudioStreamType::STREAM_MEDIA) {
        streamType = AudioStreamType::STREAM_MUSIC;
    }

    return streamType;
}

const sptr<IStandardAudioService> AudioSystemManager::GetAudioSystemManagerProxy()
{
    lock_guard<mutex> lock(g_asProxyMutex);
    if (g_asProxy == nullptr) {
        auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        if (samgr == nullptr) {
            AUDIO_ERR_LOG("GetAudioSystemManagerProxy: get sa manager failed");
            return nullptr;
        }
        sptr<IRemoteObject> object = samgr->GetSystemAbility(AUDIO_DISTRIBUTED_SERVICE_ID);
        if (object == nullptr) {
            AUDIO_ERR_LOG("GetAudioSystemManagerProxy: get audio service remote object failed");
            return nullptr;
        }
        g_asProxy = iface_cast<IStandardAudioService>(object);
        if (g_asProxy == nullptr) {
            AUDIO_ERR_LOG("GetAudioSystemManagerProxy: get audio service proxy failed");
            return nullptr;
        }

        // register death recipent to restore proxy
        sptr<AudioServerDeathRecipient> asDeathRecipient = new(std::nothrow) AudioServerDeathRecipient(getpid());
        if (asDeathRecipient != nullptr) {
            asDeathRecipient->SetNotifyCb(std::bind(&AudioSystemManager::AudioServerDied, this,
                std::placeholders::_1));
            bool result = object->AddDeathRecipient(asDeathRecipient);
            if (!result) {
                AUDIO_ERR_LOG("GetAudioSystemManagerProxy: failed to add deathRecipient");
            }
        }
    }
    sptr<IStandardAudioService> gasp = g_asProxy;
    return gasp;
}

void AudioSystemManager::AudioServerDied(pid_t pid)
{
    AUDIO_INFO_LOG("audio server died, will restore proxy in next call");
    lock_guard<mutex> lock(g_asProxyMutex);
    g_asProxy = nullptr;
}

int32_t AudioSystemManager::SetRingerMode(AudioRingerMode ringMode)
{
    std::lock_guard<std::mutex> lockSet(ringerModeCallbackMutex_);
    ringModeBackup_ = ringMode;
    if (ringerModeCallback_ != nullptr) {
        ringerModeCallback_->OnRingerModeUpdated(ringModeBackup_);
    }
    return SUCCESS;
}

AudioRingerMode AudioSystemManager::GetRingerMode()
{
    return ringModeBackup_;
}

int32_t AudioSystemManager::SetAudioScene(const AudioScene &scene)
{
    AUDIO_DEBUG_LOG("SetAudioScene audioScene_=%{public}d done", scene);
    return AudioPolicyManager::GetInstance().SetAudioScene(scene);
}

AudioScene AudioSystemManager::GetAudioScene() const
{
    return AudioPolicyManager::GetInstance().GetAudioScene();
}

int32_t AudioSystemManager::SetDeviceActive(ActiveDeviceType deviceType, bool flag) const
{
    switch (deviceType) {
        case SPEAKER:
        case BLUETOOTH_SCO:
        case FILE_SINK_DEVICE:
            break;
        default:
            AUDIO_ERR_LOG("SetDeviceActive device=%{public}d not supported", deviceType);
            return ERR_NOT_SUPPORTED;
    }

    /* Call Audio Policy SetDeviceActive */
    return (AudioPolicyManager::GetInstance().SetDeviceActive(static_cast<InternalDeviceType>(deviceType), flag));
}

bool AudioSystemManager::IsDeviceActive(ActiveDeviceType deviceType) const
{
    switch (deviceType) {
        case SPEAKER:
        case BLUETOOTH_SCO:
        case FILE_SINK_DEVICE:
            break;
        default:
            AUDIO_ERR_LOG("IsDeviceActive device=%{public}d not supported", deviceType);
            return false;
    }

    /* Call Audio Policy IsDeviceActive */
    return (AudioPolicyManager::GetInstance().IsDeviceActive(static_cast<InternalDeviceType>(deviceType)));
}

DeviceType AudioSystemManager::GetActiveOutputDevice()
{
    return AudioPolicyManager::GetInstance().GetActiveOutputDevice();
}

DeviceType AudioSystemManager::GetActiveInputDevice()
{
    return AudioPolicyManager::GetInstance().GetActiveInputDevice();
}

bool AudioSystemManager::IsStreamActive(AudioVolumeType volumeType) const
{
    switch (volumeType) {
        case STREAM_MUSIC:
        case STREAM_RING:
        case STREAM_VOICE_CALL:
        case STREAM_VOICE_ASSISTANT:
            break;
        default:
            AUDIO_ERR_LOG("IsStreamActive volumeType=%{public}d not supported", volumeType);
            return false;
    }

    AudioStreamType StreamVolType = (AudioStreamType)volumeType;
    return AudioPolicyManager::GetInstance().IsStreamActive(StreamVolType);
}

const std::string AudioSystemManager::GetAudioParameter(const std::string key)
{
    const sptr<IStandardAudioService> gasp = GetAudioSystemManagerProxy();
    if (gasp == nullptr) {
        AUDIO_ERR_LOG("GetAudioParameter::Audio service unavailable.");
        return nullptr;
    }
    return gasp->GetAudioParameter(key);
}

void AudioSystemManager::SetAudioParameter(const std::string &key, const std::string &value)
{
    const sptr<IStandardAudioService> gasp = GetAudioSystemManagerProxy();
    if (gasp == nullptr) {
        AUDIO_ERR_LOG("SetAudioParameter::Audio service unavailable.");
        return;
    }
    gasp->SetAudioParameter(key, value);
}

const char *AudioSystemManager::RetrieveCookie(int32_t &size)
{
    const sptr<IStandardAudioService> gasp = GetAudioSystemManagerProxy();
    if (gasp == nullptr) {
        AUDIO_ERR_LOG("RetrieveCookie::Audio service unavailable.");
        return nullptr;
    }
    return gasp->RetrieveCookie(size);
}

uint64_t AudioSystemManager::GetTransactionId(DeviceType deviceType, DeviceRole deviceRole)
{
    const sptr<IStandardAudioService> gasp = GetAudioSystemManagerProxy();
    if (gasp == nullptr) {
        AUDIO_ERR_LOG("GetTransactionId::Audio service unavailable.");
        return 0;
    }
    return gasp->GetTransactionId(deviceType, deviceRole);
}

int32_t AudioSystemManager::SetVolume(AudioVolumeType volumeType, int32_t volume) const
{
    AUDIO_DEBUG_LOG("AudioSystemManager SetVolume volumeType=%{public}d ", volumeType);

    /* Validate and return INVALID_PARAMS error */
    if ((volume < MIN_VOLUME_LEVEL) || (volume > MAX_VOLUME_LEVEL)) {
        AUDIO_ERR_LOG("Invalid Volume Input!");
        return ERR_INVALID_PARAM;
    }

    switch (volumeType) {
        case STREAM_MUSIC:
        case STREAM_RING:
        case STREAM_NOTIFICATION:
        case STREAM_VOICE_CALL:
        case STREAM_VOICE_ASSISTANT:
        case STREAM_ALL:
            break;
        default:
            AUDIO_ERR_LOG("SetVolume volumeType=%{public}d not supported", volumeType);
            return ERR_NOT_SUPPORTED;
    }

    /* Call Audio Policy SetStreamVolume */
    AudioStreamType StreamVolType = (AudioStreamType)volumeType;
    float volumeToHdi = MapVolumeToHDI(volume);

    if (volumeType == STREAM_ALL) {
        for (auto audioVolumeType : GET_STREAM_ALL_VOLUME_TYPES) {
            StreamVolType = (AudioStreamType)audioVolumeType;
            int32_t setResult = AudioPolicyManager::GetInstance().SetStreamVolume(StreamVolType, volumeToHdi);
            AUDIO_DEBUG_LOG("SetVolume of STREAM_ALL, volumeType=%{public}d ", StreamVolType);
            if (setResult != SUCCESS) {
                return setResult;
            }
        }
        return SUCCESS;
    }

    return AudioPolicyManager::GetInstance().SetStreamVolume(StreamVolType, volumeToHdi);
}

int32_t AudioSystemManager::GetVolume(AudioVolumeType volumeType) const
{
    switch (volumeType) {
        case STREAM_MUSIC:
        case STREAM_RING:
        case STREAM_NOTIFICATION:
        case STREAM_VOICE_CALL:
        case STREAM_VOICE_ASSISTANT:
        case STREAM_ALL:
            break;
        default:
            AUDIO_ERR_LOG("GetVolume volumeType=%{public}d not supported", volumeType);
            return ERR_NOT_SUPPORTED;
    }

    if (volumeType == STREAM_ALL) {
        volumeType = STREAM_MUSIC;
        AUDIO_DEBUG_LOG("GetVolume of STREAM_ALL for volumeType=%{public}d ", volumeType);
    }

    /* Call Audio Policy SetStreamMute */
    AudioStreamType StreamVolType = (AudioStreamType)volumeType;
    float volumeFromHdi = AudioPolicyManager::GetInstance().GetStreamVolume(StreamVolType);

    return MapVolumeFromHDI(volumeFromHdi);
}

int32_t AudioSystemManager::SetLowPowerVolume(int32_t streamId, float volume) const
{
    AUDIO_INFO_LOG("AudioSystemManager SetLowPowerVolume, streamId:%{public}d, vol:%{public}f.", streamId, volume);
    if ((volume < 0) || (volume > 1.0)) {
        AUDIO_ERR_LOG("Invalid Volume Input!");
        return ERR_INVALID_PARAM;
    }

    return AudioPolicyManager::GetInstance().SetLowPowerVolume(streamId, volume);
}

float AudioSystemManager::GetLowPowerVolume(int32_t streamId) const
{
    return AudioPolicyManager::GetInstance().GetLowPowerVolume(streamId);
}

float AudioSystemManager::GetSingleStreamVolume(int32_t streamId) const
{
    return AudioPolicyManager::GetInstance().GetSingleStreamVolume(streamId);
}

float AudioSystemManager::MapVolumeToHDI(int32_t volume)
{
    float value = (float)volume / MAX_VOLUME_LEVEL;
    float roundValue = (int)(value * CONST_FACTOR);

    return (float)roundValue / CONST_FACTOR;
}

int32_t AudioSystemManager::MapVolumeFromHDI(float volume)
{
    float value = (float)volume * MAX_VOLUME_LEVEL;
    return nearbyint(value);
}

int32_t AudioSystemManager::GetMaxVolume(AudioVolumeType volumeType)
{
    if (volumeType == STREAM_ALL) {
        volumeType = STREAM_MUSIC;
    }
    const sptr<IStandardAudioService> gasp = GetAudioSystemManagerProxy();
    if (gasp == nullptr) {
        AUDIO_ERR_LOG("GetMaxVolume::Audio service unavailable.");
        return ERR_OPERATION_FAILED;
    }
    return gasp->GetMaxVolume(volumeType);
}

int32_t AudioSystemManager::GetMinVolume(AudioVolumeType volumeType)
{
    if (volumeType == STREAM_ALL) {
        volumeType = STREAM_MUSIC;
    }
    const sptr<IStandardAudioService> gasp = GetAudioSystemManagerProxy();
    if (gasp == nullptr) {
        AUDIO_ERR_LOG("GetMinVolume::Audio service unavailable.");
        return ERR_OPERATION_FAILED;
    }
    return gasp->GetMinVolume(volumeType);
}

int32_t AudioSystemManager::SetMute(AudioVolumeType volumeType, bool mute) const
{
    AUDIO_DEBUG_LOG("AudioSystemManager SetMute for volumeType=%{public}d", volumeType);
    switch (volumeType) {
        case STREAM_MUSIC:
        case STREAM_RING:
        case STREAM_NOTIFICATION:
        case STREAM_VOICE_CALL:
        case STREAM_VOICE_ASSISTANT:
        case STREAM_ALL:
            break;
        default:
            AUDIO_ERR_LOG("SetMute volumeType=%{public}d not supported", volumeType);
            return ERR_NOT_SUPPORTED;
    }

    /* Call Audio Policy SetStreamMute */
    AudioStreamType StreamVolType = (AudioStreamType)volumeType;

    if (volumeType == STREAM_ALL) {
        for (auto audioVolumeType : GET_STREAM_ALL_VOLUME_TYPES) {
            StreamVolType = (AudioStreamType)audioVolumeType;
            int32_t setResult = AudioPolicyManager::GetInstance().SetStreamMute(StreamVolType, mute);
            AUDIO_DEBUG_LOG("SetMute of STREAM_ALL for volumeType=%{public}d ", StreamVolType);
            if (setResult != SUCCESS) {
                return setResult;
            }
        }
        return SUCCESS;
    }

    return AudioPolicyManager::GetInstance().SetStreamMute(StreamVolType, mute);
}

bool AudioSystemManager::IsStreamMute(AudioVolumeType volumeType) const
{
    AUDIO_DEBUG_LOG("AudioSystemManager::GetMute Client");

    switch (volumeType) {
        case STREAM_MUSIC:
        case STREAM_RING:
        case STREAM_NOTIFICATION:
        case STREAM_VOICE_CALL:
        case STREAM_VOICE_ASSISTANT:
        case STREAM_ALL:
            break;
        default:
            AUDIO_ERR_LOG("IsStreamMute volumeType=%{public}d not supported", volumeType);
            return false;
    }

    if (volumeType == STREAM_ALL) {
        volumeType = STREAM_MUSIC;
    }

    /* Call Audio Policy SetStreamVolume */
    AudioStreamType StreamVolType = (AudioStreamType)volumeType;
    return AudioPolicyManager::GetInstance().GetStreamMute(StreamVolType);
}

int32_t AudioSystemManager::SetDeviceChangeCallback(const DeviceFlag flag,
    const std::shared_ptr<AudioManagerDeviceChangeCallback>& callback)
{
    AUDIO_INFO_LOG("Entered AudioSystemManager::%{public}s", __func__);
    if (callback == nullptr) {
        AUDIO_ERR_LOG("SetDeviceChangeCallback: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    int32_t clientId = static_cast<int32_t>(GetCallingPid());
    return AudioPolicyManager::GetInstance().SetDeviceChangeCallback(clientId, flag, callback);
}

int32_t AudioSystemManager::UnsetDeviceChangeCallback()
{
    AUDIO_INFO_LOG("Entered AudioSystemManager::%{public}s", __func__);
    int32_t clientId = static_cast<int32_t>(GetCallingPid());
    return AudioPolicyManager::GetInstance().UnsetDeviceChangeCallback(clientId);
}

int32_t AudioSystemManager::SetRingerModeCallback(const int32_t clientId,
                                                  const std::shared_ptr<AudioRingerModeCallback> &callback)
{
    std::lock_guard<std::mutex> lockSet(ringerModeCallbackMutex_);
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioSystemManager: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    cbClientId_ = clientId;
    ringerModeCallback_ = callback;

    return SUCCESS;
}

int32_t AudioSystemManager::UnsetRingerModeCallback(const int32_t clientId) const
{
    if (clientId != cbClientId_) {
        return ERR_INVALID_OPERATION;
    }

    return SUCCESS;
}

int32_t AudioSystemManager::SetMicrophoneMute(bool isMute)
{
    return AudioPolicyManager::GetInstance().SetMicrophoneMute(isMute);
}

bool AudioSystemManager::IsMicrophoneMute(API_VERSION api_v)
{
    std::shared_ptr<AudioGroupManager> groupManager = GetGroupManager(DEFAULT_VOLUME_GROUP_ID);
    if (groupManager == nullptr) {
        AUDIO_ERR_LOG("IsMicrophoneMute failed, groupManager is null");
        return false;
    }
    return groupManager->IsMicrophoneMute(api_v);
}

int32_t AudioSystemManager::SelectOutputDevice(std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) const
{
    if (audioDeviceDescriptors.size() != 1 || audioDeviceDescriptors[0] == nullptr) {
        AUDIO_ERR_LOG("SelectOutputDevice: invalid parameter");
        return ERR_INVALID_PARAM;
    }
    if (audioDeviceDescriptors[0]->deviceRole_ != DeviceRole::OUTPUT_DEVICE) {
        AUDIO_ERR_LOG("SelectOutputDevice: not an output device.");
        return ERR_INVALID_OPERATION;
    }
    size_t validSize = 64;
    if (audioDeviceDescriptors[0]->networkId_ != LOCAL_NETWORK_ID &&
        audioDeviceDescriptors[0]->networkId_.size() != validSize) {
        AUDIO_ERR_LOG("SelectOutputDevice: invalid networkId.");
        return ERR_INVALID_PARAM;
    }
    sptr<AudioRendererFilter> audioRendererFilter = new(std::nothrow) AudioRendererFilter();
    audioRendererFilter->uid = -1;
    int32_t ret = AudioPolicyManager::GetInstance().SelectOutputDevice(audioRendererFilter, audioDeviceDescriptors);
    return ret;
}

int32_t AudioSystemManager::SelectInputDevice(std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) const
{
    if (audioDeviceDescriptors.size() != 1 || audioDeviceDescriptors[0] == nullptr) {
        AUDIO_ERR_LOG("SelectInputDevice: invalid parameter");
        return ERR_INVALID_PARAM;
    }
    if (audioDeviceDescriptors[0]->deviceRole_ != DeviceRole::INPUT_DEVICE) {
        AUDIO_ERR_LOG("SelectInputDevice: not an output device.");
        return ERR_INVALID_OPERATION;
    }
    sptr<AudioCapturerFilter> audioCapturerFilter = new(std::nothrow) AudioCapturerFilter();
    audioCapturerFilter->uid = -1;
    int32_t ret = AudioPolicyManager::GetInstance().SelectInputDevice(audioCapturerFilter, audioDeviceDescriptors);
    return ret;
}

std::string AudioSystemManager::GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType) const
{
    return AudioPolicyManager::GetInstance().GetSelectedDeviceInfo(uid, pid, streamType);
}

int32_t AudioSystemManager::SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) const
{
    // basic check
    if (audioRendererFilter == nullptr || audioDeviceDescriptors.size() == 0) {
        AUDIO_ERR_LOG("SelectOutputDevice: invalid parameter");
        return ERR_INVALID_PARAM;
    }

    size_t validDeviceSize = 1;
    if (audioDeviceDescriptors.size() > validDeviceSize || audioDeviceDescriptors[0] == nullptr) {
        AUDIO_ERR_LOG("SelectOutputDevice: device error");
        return ERR_INVALID_OPERATION;
    }
    audioRendererFilter->streamType = AudioSystemManager::GetStreamType(audioRendererFilter->rendererInfo.contentType,
        audioRendererFilter->rendererInfo.streamUsage);
    // operation chack
    if (audioDeviceDescriptors[0]->deviceRole_ != DeviceRole::OUTPUT_DEVICE) {
        AUDIO_ERR_LOG("SelectOutputDevice: not an output device.");
        return ERR_INVALID_OPERATION;
    }
    size_t validSize = 64;
    if (audioDeviceDescriptors[0]->networkId_ != LOCAL_NETWORK_ID &&
        audioDeviceDescriptors[0]->networkId_.size() != validSize) {
        AUDIO_ERR_LOG("SelectOutputDevice: invalid networkId.");
        return ERR_INVALID_PARAM;
    }
    if (audioRendererFilter->uid < 0) {
        AUDIO_ERR_LOG("SelectOutputDevice: invalid uid.");
        return ERR_INVALID_PARAM;
    }
    AUDIO_DEBUG_LOG("[%{public}d] SelectOutputDevice: uid<%{public}d> streamType<%{public}d> device<name:%{public}s>",
        getpid(), audioRendererFilter->uid, static_cast<int32_t>(audioRendererFilter->streamType),
        (audioDeviceDescriptors[0]->networkId_.c_str()));

    return AudioPolicyManager::GetInstance().SelectOutputDevice(audioRendererFilter, audioDeviceDescriptors);
}

int32_t AudioSystemManager::SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors) const
{
    // basic check
    if (audioCapturerFilter == nullptr || audioDeviceDescriptors.size() == 0) {
        AUDIO_ERR_LOG("SelectInputDevice: invalid parameter");
        return ERR_INVALID_PARAM;
    }

    size_t validDeviceSize = 1;
    if (audioDeviceDescriptors.size() > validDeviceSize || audioDeviceDescriptors[0] == nullptr) {
        AUDIO_ERR_LOG("SelectInputDevice: device error.");
        return ERR_INVALID_OPERATION;
    }
    // operation chack
    if (audioDeviceDescriptors[0]->deviceRole_ != DeviceRole::INPUT_DEVICE) {
        AUDIO_ERR_LOG("SelectInputDevice: not an input device");
        return ERR_INVALID_OPERATION;
    }
    if (audioCapturerFilter->uid < 0) {
        AUDIO_ERR_LOG("SelectInputDevice: invalid uid.");
        return ERR_INVALID_PARAM;
    }
    AUDIO_DEBUG_LOG("[%{public}d] SelectInputDevice: uid<%{public}d> device<type:%{public}d>",
        getpid(), audioCapturerFilter->uid, static_cast<int32_t>(audioDeviceDescriptors[0]->deviceType_));

    return AudioPolicyManager::GetInstance().SelectInputDevice(audioCapturerFilter, audioDeviceDescriptors);
}

std::vector<sptr<AudioDeviceDescriptor>> AudioSystemManager::GetDevices(DeviceFlag deviceFlag)
{
    return AudioPolicyManager::GetInstance().GetDevices(deviceFlag);
}

std::vector<sptr<AudioDeviceDescriptor>> AudioSystemManager::GetActiveOutputDeviceDescriptors()
{
    return AudioPolicyManager::GetInstance().GetActiveOutputDeviceDescriptors();
}

int32_t AudioSystemManager::RegisterVolumeKeyEventCallback(const int32_t clientPid,
                                                           const std::shared_ptr<VolumeKeyEventCallback> &callback)
{
    AUDIO_DEBUG_LOG("AudioSystemManager RegisterVolumeKeyEventCallback");

    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioSystemManager::RegisterVolumeKeyEventCallbackcallback is nullptr");
        return ERR_INVALID_PARAM;
    }
    volumeChangeClientPid_ = clientPid;

    return AudioPolicyManager::GetInstance().SetVolumeKeyEventCallback(clientPid, callback);
}

int32_t AudioSystemManager::UnregisterVolumeKeyEventCallback(const int32_t clientPid)
{
    AUDIO_DEBUG_LOG("AudioSystemManager::UnregisterVolumeKeyEventCallback");
    int32_t ret = AudioPolicyManager::GetInstance().UnsetVolumeKeyEventCallback(clientPid);
    if (!ret) {
        AUDIO_DEBUG_LOG("AudioSystemManager::UnregisterVolumeKeyEventCallback success");
        volumeChangeClientPid_ = -1;
    }

    return ret;
}

// Below stub implementation is added to handle compilation error in call manager
// Once call manager adapt to new interrupt implementation, this will be removed
int32_t AudioSystemManager::SetAudioManagerCallback(const AudioVolumeType streamType,
                                                    const std::shared_ptr<AudioManagerCallback> &callback)
{
    AUDIO_DEBUG_LOG("AudioSystemManager SetAudioManagerCallback stub implementation");
    return SUCCESS;
}

int32_t AudioSystemManager::UnsetAudioManagerCallback(const AudioVolumeType streamType) const
{
    AUDIO_DEBUG_LOG("AudioSystemManager UnsetAudioManagerCallback stub implementation");
    return SUCCESS;
}

int32_t AudioSystemManager::ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt)
{
    AUDIO_DEBUG_LOG("AudioSystemManager ActivateAudioInterrupt stub implementation");
    return SUCCESS;
}

int32_t AudioSystemManager::DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt) const
{
    AUDIO_DEBUG_LOG("AudioSystemManager DeactivateAudioInterrupt stub implementation");
    return SUCCESS;
}

int32_t AudioSystemManager::SetAudioManagerInterruptCallback(const std::shared_ptr<AudioManagerCallback> &callback)
{
    uint32_t clientID = GetCallingPid();
    AUDIO_INFO_LOG("AudioSystemManager:: SetAudioManagerInterruptCallback client id: %{public}d", clientID);
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioSystemManager::callback is null");
        return ERR_INVALID_PARAM;
    }

    if (audioInterruptCallback_ != nullptr) {
        AUDIO_DEBUG_LOG("AudioSystemManager reset existing callback object");
        AudioPolicyManager::GetInstance().UnsetAudioManagerInterruptCallback(clientID);
        audioInterruptCallback_.reset();
        audioInterruptCallback_ = nullptr;
    }

    audioInterruptCallback_ = std::make_shared<AudioManagerInterruptCallbackImpl>();
    if (audioInterruptCallback_ == nullptr) {
        AUDIO_ERR_LOG("AudioSystemManager::Failed to allocate memory for audioInterruptCallback");
        return ERROR;
    }

    int32_t ret = AudioPolicyManager::GetInstance().SetAudioManagerInterruptCallback(clientID, audioInterruptCallback_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("AudioSystemManager::Failed set callback");
        return ERROR;
    }

    std::shared_ptr<AudioManagerInterruptCallbackImpl> cbInterrupt =
        std::static_pointer_cast<AudioManagerInterruptCallbackImpl>(audioInterruptCallback_);
    if (cbInterrupt == nullptr) {
        AUDIO_ERR_LOG("AudioSystemManager::cbInterrupt is nullptr");
        return ERROR;
    }
    cbInterrupt->SaveCallback(callback);

    return SUCCESS;
}

int32_t AudioSystemManager::UnsetAudioManagerInterruptCallback()
{
    uint32_t clientID = GetCallingPid();
    AUDIO_INFO_LOG("AudioSystemManager:: UnsetAudioManagerInterruptCallback client id: %{public}d", clientID);

    int32_t ret = AudioPolicyManager::GetInstance().UnsetAudioManagerInterruptCallback(clientID);
    if (audioInterruptCallback_ != nullptr) {
        audioInterruptCallback_.reset();
        audioInterruptCallback_ = nullptr;
    }

    return ret;
}

int32_t AudioSystemManager::RequestAudioFocus(const AudioInterrupt &audioInterrupt)
{
    uint32_t clientID = GetCallingPid();
    AUDIO_INFO_LOG("AudioSystemManager:: RequestAudioFocus client id: %{public}d", clientID);
    CHECK_AND_RETURN_RET_LOG(audioInterrupt.contentType >= CONTENT_TYPE_UNKNOWN
                             && audioInterrupt.contentType <= CONTENT_TYPE_RINGTONE, ERR_INVALID_PARAM,
                             "Invalid content type");
    CHECK_AND_RETURN_RET_LOG(audioInterrupt.streamUsage >= STREAM_USAGE_UNKNOWN
                             && audioInterrupt.streamUsage <= STREAM_USAGE_NOTIFICATION_RINGTONE,
                             ERR_INVALID_PARAM, "Invalid stream usage");
    CHECK_AND_RETURN_RET_LOG(audioInterrupt.streamType >= AudioStreamType::STREAM_VOICE_CALL
                             && audioInterrupt.streamType <= AudioStreamType::STREAM_RECORDING,
                             ERR_INVALID_PARAM, "Invalid stream type");
    return AudioPolicyManager::GetInstance().RequestAudioFocus(clientID, audioInterrupt);
}

int32_t AudioSystemManager::AbandonAudioFocus(const AudioInterrupt &audioInterrupt)
{
    uint32_t clientID = GetCallingPid();
    AUDIO_INFO_LOG("AudioSystemManager:: AbandonAudioFocus client id: %{public}d", clientID);
    CHECK_AND_RETURN_RET_LOG(audioInterrupt.contentType >= CONTENT_TYPE_UNKNOWN
                             && audioInterrupt.contentType <= CONTENT_TYPE_RINGTONE, ERR_INVALID_PARAM,
                             "Invalid content type");
    CHECK_AND_RETURN_RET_LOG(audioInterrupt.streamUsage >= STREAM_USAGE_UNKNOWN
                             && audioInterrupt.streamUsage <= STREAM_USAGE_NOTIFICATION_RINGTONE,
                             ERR_INVALID_PARAM, "Invalid stream usage");
    CHECK_AND_RETURN_RET_LOG(audioInterrupt.streamType >= AudioStreamType::STREAM_VOICE_CALL
                             && audioInterrupt.streamType <= AudioStreamType::STREAM_RECORDING,
                             ERR_INVALID_PARAM, "Invalid stream type");
    return AudioPolicyManager::GetInstance().AbandonAudioFocus(clientID, audioInterrupt);
}

int32_t AudioSystemManager::ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType)
{
    return AudioPolicyManager::GetInstance().ReconfigureAudioChannel(count, deviceType);
}

std::vector<sptr<VolumeGroupInfo>> AudioSystemManager::GetVolumeGroups(std::string networkId)
{
    std::vector<sptr<VolumeGroupInfo>> infos = {};
    infos = AudioPolicyManager::GetInstance().GetVolumeGroupInfos();

    auto filter = [&networkId](const sptr<VolumeGroupInfo>& info) {
        return networkId != info->networkId_;
    };
    infos.erase(std::remove_if(infos.begin(), infos.end(), filter), infos.end());
    return infos;
}

std::shared_ptr<AudioGroupManager> AudioSystemManager::GetGroupManager(int32_t groupId)
{
    std::vector<std::shared_ptr<AudioGroupManager>>::iterator iter = groupManagerMap_.begin();
    while (iter != groupManagerMap_.end()) {
        if ((*iter)->GetGroupId() == groupId) {
            return *iter;
        } else {
            iter++;
        }
    }

    std::shared_ptr<AudioGroupManager> groupManager = std::make_shared<AudioGroupManager>(groupId);
    if (groupManager->Init() == SUCCESS) {
        groupManagerMap_.push_back(groupManager);
    } else {
        groupManager = nullptr;
    }
    return groupManager;
}

AudioManagerInterruptCallbackImpl::AudioManagerInterruptCallbackImpl()
{
    AUDIO_INFO_LOG("AudioManagerInterruptCallbackImpl constructor");
}

AudioManagerInterruptCallbackImpl::~AudioManagerInterruptCallbackImpl()
{
    AUDIO_DEBUG_LOG("AudioManagerInterruptCallbackImpl: instance destroy");
}

void AudioManagerInterruptCallbackImpl::SaveCallback(const std::weak_ptr<AudioManagerCallback> &callback)
{
    auto wp = callback.lock();
    if (wp != nullptr) {
        callback_ = callback;
    } else {
        AUDIO_ERR_LOG("AudioManagerInterruptCallbackImpl::SaveCallback: callback is nullptr");
    }
}

void AudioManagerInterruptCallbackImpl::OnInterrupt(const InterruptEventInternal &interruptEvent)
{
    cb_ = callback_.lock();
    if (cb_ != nullptr) {
        InterruptAction interruptAction = {};
        interruptAction.actionType = (interruptEvent.eventType == INTERRUPT_TYPE_BEGIN)
            ? TYPE_INTERRUPT : TYPE_ACTIVATED;
        interruptAction.interruptType = interruptEvent.eventType;
        interruptAction.interruptHint = interruptEvent.hintType;
        interruptAction.activated = (interruptEvent.eventType == INTERRUPT_TYPE_BEGIN) ? false : true;
        cb_->OnInterrupt(interruptAction);
        AUDIO_DEBUG_LOG("AudioManagerInterruptCallbackImpl: OnInterrupt : Notify event to app complete");
    } else {
        AUDIO_ERR_LOG("AudioManagerInterruptCallbackImpl: callback is null");
    }

    return;
}

bool AudioSystemManager::RequestIndependentInterrupt(FocusType focusType)
{
    AUDIO_INFO_LOG("AudioSystemManager: requestIndependentInterrupt : foncusType");
    AudioInterrupt audioInterrupt;
    uint32_t clientID = GetCallingPid();
    audioInterrupt.contentType = ContentType::CONTENT_TYPE_SPEECH;
    audioInterrupt.streamUsage = StreamUsage::STREAM_USAGE_MEDIA;
    audioInterrupt.streamType = AudioStreamType::STREAM_RECORDING;
    audioInterrupt.sessionID = clientID;
    int32_t result = AudioPolicyManager::GetInstance().ActivateAudioInterrupt(audioInterrupt);

    AUDIO_INFO_LOG("AudioSystemManager: requestIndependentInterrupt : result -> %{public}d", result);
    return (result == SUCCESS) ? true:false;
}
bool AudioSystemManager::AbandonIndependentInterrupt(FocusType focusType)
{
    AUDIO_INFO_LOG("AudioSystemManager: abandonIndependentInterrupt : foncusType");
    AudioInterrupt audioInterrupt;
    uint32_t clientID = GetCallingPid();
    audioInterrupt.contentType = ContentType::CONTENT_TYPE_SPEECH;
    audioInterrupt.streamUsage = StreamUsage::STREAM_USAGE_MEDIA;
    audioInterrupt.streamType = AudioStreamType::STREAM_RECORDING;
    audioInterrupt.sessionID = clientID;
    int32_t result = AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt);
    AUDIO_INFO_LOG("AudioSystemManager: abandonIndependentInterrupt : result -> %{public}d", result);
    return (result == SUCCESS) ? true:false;
}

int32_t AudioSystemManager::GetAudioLatencyFromXml() const
{
    return AudioPolicyManager::GetInstance().GetAudioLatencyFromXml();
}

uint32_t AudioSystemManager::GetSinkLatencyFromXml() const
{
    return AudioPolicyManager::GetInstance().GetSinkLatencyFromXml();
}

int32_t AudioSystemManager::UpdateStreamState(const int32_t clientUid,
    StreamSetState streamSetState, AudioStreamType audioStreamType)
{
    AUDIO_INFO_LOG("AudioSystemManager::UpdateStreamState::clientUid:%{public}d streamSetState:%{public}d",
        clientUid, streamSetState);
    int32_t result = 0;
    
    result = AudioPolicyManager::GetInstance().UpdateStreamState(clientUid, streamSetState, audioStreamType);
    return result;
}

AudioPin AudioSystemManager::GetPinValueFromType(DeviceType deviceType, DeviceRole deviceRole) const
{
    AudioPin pin = AUDIO_PIN_NONE;
    switch (deviceType) {
        case OHOS::AudioStandard::DEVICE_TYPE_NONE:
        case OHOS::AudioStandard::DEVICE_TYPE_INVALID:
            pin = AUDIO_PIN_NONE;
            break;
        case OHOS::AudioStandard::DEVICE_TYPE_DEFAULT:
            if (deviceRole == DeviceRole::INPUT_DEVICE) {
                pin = AUDIO_PIN_IN_DAUDIO_DEFAULT;
            } else {
                pin = AUDIO_PIN_OUT_DAUDIO_DEFAULT;
            }
            break;
        case OHOS::AudioStandard::DEVICE_TYPE_SPEAKER:
            pin = AUDIO_PIN_OUT_SPEAKER;
            break;
        case OHOS::AudioStandard::DEVICE_TYPE_MIC:
            pin = AUDIO_PIN_IN_MIC;
            break;
        case OHOS::AudioStandard::DEVICE_TYPE_WIRED_HEADSET:
            if (deviceRole == DeviceRole::INPUT_DEVICE) {
                pin = AUDIO_PIN_IN_HS_MIC;
            } else {
                pin = AUDIO_PIN_OUT_HEADSET;
            }
            break;
        case OHOS::AudioStandard::DEVICE_TYPE_USB_HEADSET:
        case OHOS::AudioStandard::DEVICE_TYPE_FILE_SINK:
        case OHOS::AudioStandard::DEVICE_TYPE_FILE_SOURCE:
        case OHOS::AudioStandard::DEVICE_TYPE_BLUETOOTH_SCO:
        case OHOS::AudioStandard::DEVICE_TYPE_BLUETOOTH_A2DP:
        case OHOS::AudioStandard::DEVICE_TYPE_MAX:
            AUDIO_INFO_LOG("AudioSystemManager: GetPinValueFromType :don't supported the device type");
            break;
        default:
            AUDIO_INFO_LOG("AudioSystemManager: GetPinValueFromType : invalid input parameter");
            break;
    }
    return pin;
}

DeviceType AudioSystemManager::GetTypeValueFromPin(AudioPin pin) const
{
    DeviceType type = DEVICE_TYPE_NONE;
    switch (pin) {
        case OHOS::AudioStandard::AUDIO_PIN_NONE:
            type = DEVICE_TYPE_NONE;
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_SPEAKER:
            type = DEVICE_TYPE_SPEAKER;
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_HEADSET:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_LINEOUT:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_HDMI:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_USB:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_USB_EXT:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_DAUDIO_DEFAULT:
            type = DEVICE_TYPE_DEFAULT;
            break;
        case OHOS::AudioStandard::AUDIO_PIN_IN_MIC:
            type = DEVICE_TYPE_MIC;
            break;
        case OHOS::AudioStandard::AUDIO_PIN_IN_HS_MIC:
            type = DEVICE_TYPE_WIRED_HEADSET;
            break;
        case OHOS::AudioStandard::AUDIO_PIN_IN_LINEIN:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_IN_USB_EXT:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_IN_DAUDIO_DEFAULT:
            type = DEVICE_TYPE_DEFAULT;
            break;
        default:
            AUDIO_INFO_LOG("AudioSystemManager: GetTypeValueFromPin : invalid input parameter");
            break;
    }
    return type;
}
} // namespace AudioStandard
} // namespace OHOS
