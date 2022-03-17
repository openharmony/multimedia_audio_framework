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

#ifndef ST_AUDIO_POLICY_MANAGER_H
#define ST_AUDIO_POLICY_MANAGER_H

#include <cstdint>
#include "audio_info.h"
#include "audio_interrupt_callback.h"
#include "audio_policy_manager_listener_stub.h"
#include "audio_ringermode_update_listener_stub.h"
#include "audio_system_manager.h"
#include "audio_volume_key_event_callback_stub.h"
#include "i_audio_volume_key_event_callback.h"

namespace OHOS {
namespace AudioStandard {
using InternalDeviceType = DeviceType;

class AudioPolicyManager {
public:
    static AudioPolicyManager& GetInstance()
    {
        static AudioPolicyManager policyManager;
        if (!serverConnected) {
            policyManager.Init();
        }

        return policyManager;
    }

    int32_t SetStreamVolume(AudioStreamType streamType, float volume);

    float GetStreamVolume(AudioStreamType streamType);

    int32_t SetStreamMute(AudioStreamType streamType, bool mute);

    bool GetStreamMute(AudioStreamType streamType);

    bool IsStreamActive(AudioStreamType streamType);

    std::vector<sptr<AudioDeviceDescriptor>> GetDevices(DeviceFlag deviceFlag);

    int32_t SetDeviceActive(InternalDeviceType deviceType, bool active);

    bool IsDeviceActive(InternalDeviceType deviceType);

    int32_t SetRingerMode(AudioRingerMode ringMode);

    AudioRingerMode GetRingerMode();

    int32_t SetAudioScene(AudioScene scene);

    AudioScene GetAudioScene();

    int32_t SetDeviceChangeCallback(const std::shared_ptr<AudioManagerDeviceChangeCallback> &callback);

    int32_t SetRingerModeCallback(const int32_t clientId,
                                  const std::shared_ptr<AudioRingerModeCallback> &callback);

    int32_t UnsetRingerModeCallback(const int32_t clientId);

    int32_t SetAudioInterruptCallback(const uint32_t sessionID,
                                    const std::shared_ptr<AudioInterruptCallback> &callback);

    int32_t UnsetAudioInterruptCallback(const uint32_t sessionID);

    int32_t ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt);

    int32_t DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt);

    AudioStreamType GetStreamInFocus();

    int32_t GetSessionInfoInFocus(AudioInterrupt &audioInterrupt);

    int32_t SetVolumeKeyEventCallback(const int32_t clientPid,
                                      const std::shared_ptr<VolumeKeyEventCallback> &callback);
    int32_t UnsetVolumeKeyEventCallback(const int32_t clientPid);
private:
    AudioPolicyManager()
    {
        Init();
    }
    ~AudioPolicyManager() {}

    void Init();
    sptr<AudioPolicyManagerListenerStub> listenerStub_ = nullptr;
    std::mutex listenerStubMutex_;

    std::shared_ptr<VolumeKeyEventCallback> volumeKeyEventCallback_ = nullptr;
    sptr<AudioVolumeKeyEventCallbackStub> volumeKeyEventListenerStub_ = nullptr;
    sptr<AudioRingerModeUpdateListenerStub> ringerModelistenerStub_ = nullptr;
    static bool serverConnected;
    void RegisterAudioPolicyServerDeathRecipient();
    void AudioPolicyServerDied(pid_t pid);
};
} // namespce AudioStandard
} // namespace OHOS

#endif // ST_AUDIO_POLICY_MANAGER_H
