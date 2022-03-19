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

#ifndef I_ST_AUDIO_MANAGER_BASE_H
#define I_ST_AUDIO_MANAGER_BASE_H

#include "ipc_types.h"
#include "iremote_broker.h"
#include "iremote_proxy.h"
#include "iremote_stub.h"
#include "audio_system_manager.h"

namespace OHOS {
namespace AudioStandard {
class IStandardAudioService : public IRemoteBroker {
public:
    /**
     * Obtains max volume.
     *
     * @return Returns the max volume.
     */
    virtual int32_t GetMaxVolume(AudioSystemManager::AudioVolumeType volumeType) = 0;

    /**
     * Obtains min volume.
     *
     * @return Returns the min volume.
     */
    virtual int32_t GetMinVolume(AudioSystemManager::AudioVolumeType volumeType) = 0;

    /**
     * Sets Microphone Mute status.
     *
     * @param isMute Mute status true or false to be set.
     * @return Returns 0 if success. Otherise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t SetMicrophoneMute(bool isMute) = 0;

     /**
     * Gets Microphone Mute status.
     *
     * @return Returns true or false
     */
    virtual bool IsMicrophoneMute() = 0;

    /**
     * Sets Audio modes.
     *
     * @param audioScene Audio scene type.
     * @param activeDeviceList List of active device types.
     * @return Returns 0 if success. Otherise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t SetAudioScene(AudioScene audioScene) = 0;

    /**
     * Obtains device array.
     *
     * @return Returns the array of audio device descriptor.
     */
    virtual std::vector<sptr<AudioDeviceDescriptor>> GetDevices(DeviceFlag deviceFlag) = 0;

    /**
     * Set Audio Parameter.
     *
     * @param  key for the audio parameter to be set
     * @param  value associated with the key for the audio parameter to be set
     * @return none.
     */
    virtual void SetAudioParameter(const std::string &key, const std::string &value) = 0;

    /**
     * Get Audio Parameter.
     *
     * @param  key for the audio parameter to be set
     * @return Returns value associated to the key requested.
     */
    virtual const std::string GetAudioParameter(const std::string &key) = 0;

    /**
     * Update the audio route after device is detected and route is decided
     *
     * @return Returns 0 if success. Otherise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t UpdateActiveDeviceRoute(DeviceType type, DeviceFlag flag) = 0;

    /**
     * Retrieve cookie information from the service
     *
     * @return Returns cookie information, null if failed.
     */
    virtual const char *RetrieveCookie(int32_t &size) = 0;

    enum {
        GET_MAX_VOLUME = 0,
        GET_MIN_VOLUME = 1,
        GET_DEVICES = 2,
        GET_AUDIO_PARAMETER = 3,
        SET_AUDIO_PARAMETER = 4,
        SET_MICROPHONE_MUTE = 5,
        IS_MICROPHONE_MUTE = 6,
        SET_AUDIO_SCENE = 7,
        UPDATE_ROUTE_REQ = 8,
        RETRIEVE_COOKIE = 9,
    };

public:
    DECLARE_INTERFACE_DESCRIPTOR(u"IStandardAudioService");
};

class AudioManagerStub : public IRemoteStub<IStandardAudioService> {
public:
    virtual int OnRemoteRequest(uint32_t code, MessageParcel &data,
                                MessageParcel &reply, MessageOption &option) override;
    bool IsPermissionValid();
};
} // namespace AudioStandard
} // namespace OHOS
#endif // I_ST_AUDIO_MANAGER_BASE_H
