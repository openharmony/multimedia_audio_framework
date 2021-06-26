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

#ifndef ST_AUDIO_SYSTEM_MANAGER_H
#define ST_AUDIO_SYSTEM_MANAGER_H

#include <cstdlib>

#include "audio_device_descriptor.h"
#include "audio_stream.h"
#include "audio_policy_manager.h"

namespace OHOS {
namespace AudioStandard {
/**
 * @brief The AudioSystemManager class is an abstract definition of audio manager.
 *        Provides a series of client/interfaces for audio management
 */

class AudioSystemManager {
public:
enum AudioVolumeType {
        /**
         * Indicates audio streams of voices in calls.
         */
        STREAM_VOICE_CALL = 1,
        /**
         * Indicates audio streams for system sounds.
         */
        STREAM_SYSTEM = 2,
        /**
         * Indicates audio streams for ringtones.
         */
        STREAM_RING = 3,
        /**
         * Indicates audio streams for music playback.
         */
        STREAM_MUSIC = 4,
        /**
         * Indicates audio streams for alarms.
         */
        STREAM_ALARM = 5,
        /**
         * Indicates audio streams for notifications.
         */
        STREAM_NOTIFICATION = 6,
        /**
         * Indicates audio streams for voice calls routed through a connected Bluetooth device.
         */
        STREAM_BLUETOOTH_SCO = 7,
        /**
         * Indicates audio streams for dual-tone multi-frequency (DTMF) tones.
         */
        STREAM_DTMF = 8,
        /**
         * Indicates audio streams exclusively transmitted through the speaker (text-to-speech) of a device.
         */
        STREAM_TTS =  9,
        /**
         * Indicates audio streams used for prompts in terms of accessibility.
         */
        STREAM_ACCESSIBILITY = 10
    };
    static AudioSystemManager* GetInstance();
    int32_t SetVolume(AudioSystemManager::AudioVolumeType volumeType, float volume);
    float GetVolume(AudioSystemManager::AudioVolumeType volumeType);
    float GetMaxVolume(AudioSystemManager::AudioVolumeType volumeType);
    float GetMinVolume(AudioSystemManager::AudioVolumeType volumeType);
    int32_t SetMute(AudioSystemManager::AudioVolumeType volumeType, bool mute);
    bool IsStreamMute(AudioSystemManager::AudioVolumeType volumeType);
    int32_t SetMicrophoneMute(bool IsMute);
    bool IsMicrophoneMute(void);
    std::vector<sptr<AudioDeviceDescriptor>> GetDevices(AudioDeviceDescriptor::DeviceFlag deviceFlag);
    const std::string GetAudioParameter(const std::string key);
    void SetAudioParameter(const std::string key, const std::string value);
    int32_t SetDeviceActive(AudioDeviceDescriptor::DeviceType deviceType, bool flag);
    bool IsDeviceActive(AudioDeviceDescriptor::DeviceType deviceType);
    bool IsStreamActive(AudioSystemManager::AudioVolumeType volumeType);
    bool SetRingerMode(AudioRingerMode ringMode);
    AudioRingerMode GetRingerMode();
private:
    AudioSystemManager();
    virtual ~AudioSystemManager();
    void init();
};
} // namespace AudioStandard
} // namespace OHOS
#endif // ST_AUDIO_SYSTEM_MANAGER_H
