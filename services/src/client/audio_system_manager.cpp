/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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

#include "audio_errors.h"
#include "audio_info.h"
#include "audio_manager_proxy.h"
#include "audio_system_manager.h"
#include "iservice_registry.h"
#include "media_log.h"
#include "system_ability_definition.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;
static sptr<IStandardAudioService> g_sProxy = nullptr;

AudioSystemManager::AudioSystemManager()
{
    MEDIA_DEBUG_LOG("AudioSystemManager start");
    init();
}

AudioSystemManager::~AudioSystemManager()
{
    MEDIA_DEBUG_LOG("AudioSystemManager::~AudioSystemManager");
}

AudioSystemManager* AudioSystemManager::GetInstance()
{
    static AudioSystemManager audioManager;
    return &audioManager;
}

void AudioSystemManager::init()
{
    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (samgr == nullptr) {
        MEDIA_ERR_LOG("AudioSystemManager::init failed");
        return;
    }

    sptr<IRemoteObject> object = samgr->GetSystemAbility(AUDIO_DISTRIBUTED_SERVICE_ID);
    if (object == nullptr) {
        MEDIA_DEBUG_LOG("AudioSystemManager::object is NULL.");
    }
    g_sProxy = iface_cast<IStandardAudioService>(object);
    if (g_sProxy == nullptr) {
        MEDIA_DEBUG_LOG("AudioSystemManager::init g_sProxy is NULL.");
    } else {
        MEDIA_DEBUG_LOG("AudioSystemManager::init g_sProxy is assigned.");
    }
}

bool AudioSystemManager::SetRingerMode(AudioRingerMode ringMode)
{
    /* Call Audio Policy SetRingerMode */
    return AudioPolicyManager::GetInstance().SetRingerMode(ringMode);
}

AudioRingerMode AudioSystemManager::GetRingerMode()
{
    /* Call Audio Policy GetRingerMode */
    return (AudioPolicyManager::GetInstance().GetRingerMode());
}

int32_t AudioSystemManager::SetDeviceActive(AudioDeviceDescriptor::DeviceType deviceType, bool flag)
{
    switch (deviceType) {
        case SPEAKER:
        case BLUETOOTH_A2DP:
        case MIC:
            break;
        default:
            MEDIA_ERR_LOG("SetDeviceActive device=%{public}d not supported", deviceType);
            return ERR_NOT_SUPPORTED;
    }

    /* Call Audio Policy SetDeviceActive */
    DeviceType devType = (DeviceType)deviceType;
    return (AudioPolicyManager::GetInstance().SetDeviceActive(devType, flag));
}

bool AudioSystemManager::IsDeviceActive(AudioDeviceDescriptor::DeviceType deviceType)
{
    switch (deviceType) {
        case SPEAKER:
        case BLUETOOTH_A2DP:
        case MIC:
            break;
        default:
            MEDIA_ERR_LOG("IsDeviceActive device=%{public}d not supported", deviceType);
            return false;
    }

    /* Call Audio Policy IsDeviceActive */
    DeviceType devType = (DeviceType)deviceType;
    return (AudioPolicyManager::GetInstance().IsDeviceActive(devType));
}

bool AudioSystemManager::IsStreamActive(AudioSystemManager::AudioVolumeType volumeType)
{
    switch (volumeType) {
        case STREAM_MUSIC:
        case STREAM_RING:
            break;
        default:
            MEDIA_ERR_LOG("IsStreamActive volumeType=%{public}d not supported", volumeType);
            return false;
    }

    AudioStreamType StreamVolType = (AudioStreamType)volumeType;
    return AudioPolicyManager::GetInstance().IsStreamActive(StreamVolType);
}

const std::string AudioSystemManager::GetAudioParameter(const std::string key)
{
    return g_sProxy->GetAudioParameter(key);
}

void AudioSystemManager::SetAudioParameter(const std::string key, const std::string value)
{
    g_sProxy->SetAudioParameter(key, value);
}

int32_t AudioSystemManager::SetVolume(AudioSystemManager::AudioVolumeType volumeType, float volume)
{
    /* Validate and return INVALID_PARAMS error */
    if ((volume < 0) || (volume > 1)) {
        MEDIA_ERR_LOG("Invalid Volume Input!");
        return ERR_INVALID_PARAM;
    }

    switch (volumeType) {
        case STREAM_MUSIC:
        case STREAM_RING:
            break;
        default:
            MEDIA_ERR_LOG("SetVolume volumeType=%{public}d not supported", volumeType);
            return ERR_NOT_SUPPORTED;
    }

    /* Call Audio Policy SetStreamVolume */
    AudioStreamType StreamVolType = (AudioStreamType)volumeType;
    return AudioPolicyManager::GetInstance().SetStreamVolume(StreamVolType, volume);
}

float AudioSystemManager::GetVolume(AudioSystemManager::AudioVolumeType volumeType)
{
    switch (volumeType) {
        case STREAM_MUSIC:
        case STREAM_RING:
            break;
        default:
            MEDIA_ERR_LOG("GetVolume volumeType=%{public}d not supported", volumeType);
            return ERR_NOT_SUPPORTED;
    }

    /* Call Audio Policy SetStreamMute */
    AudioStreamType StreamVolType = (AudioStreamType)volumeType;
    return AudioPolicyManager::GetInstance().GetStreamVolume(StreamVolType);
}

float AudioSystemManager::GetMaxVolume(AudioSystemManager::AudioVolumeType volumeType)
{
    return g_sProxy->GetMaxVolume(volumeType);
}

float AudioSystemManager::GetMinVolume(AudioSystemManager::AudioVolumeType volumeType)
{
    return g_sProxy->GetMinVolume(volumeType);
}

int32_t AudioSystemManager::SetMute(AudioSystemManager::AudioVolumeType volumeType, bool mute)
{
    switch (volumeType) {
        case STREAM_MUSIC:
        case STREAM_RING:
            break;
        default:
            MEDIA_ERR_LOG("SetMute volumeType=%{public}d not supported", volumeType);
            return ERR_NOT_SUPPORTED;
    }

    /* Call Audio Policy SetStreamMute */
    AudioStreamType StreamVolType = (AudioStreamType)volumeType;
    return AudioPolicyManager::GetInstance().SetStreamMute(StreamVolType, mute);
}

bool AudioSystemManager::IsStreamMute(AudioSystemManager::AudioVolumeType volumeType)
{
    MEDIA_DEBUG_LOG("AudioSystemManager::GetMute Client");

    switch (volumeType) {
        case STREAM_MUSIC:
        case STREAM_RING:
            break;
        default:
            MEDIA_ERR_LOG("IsStreamMute volumeType=%{public}d not supported", volumeType);
            return false;
    }

    /* Call Audio Policy SetStreamVolume */
    AudioStreamType StreamVolType = (AudioStreamType)volumeType;
    return AudioPolicyManager::GetInstance().GetStreamMute(StreamVolType);
}

int32_t AudioSystemManager::SetMicrophoneMute(bool isMute)
{
    return g_sProxy->SetMicrophoneMute(isMute);
}

bool AudioSystemManager::IsMicrophoneMute()
{
    return g_sProxy->IsMicrophoneMute();
}

std::vector<sptr<AudioDeviceDescriptor>> AudioSystemManager::GetDevices(AudioDeviceDescriptor::DeviceFlag deviceFlag)
{
    return g_sProxy->GetDevices(deviceFlag);
}
} // namespace AudioStandard
} // namespace OHOS
