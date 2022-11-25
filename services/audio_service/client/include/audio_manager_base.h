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
#include "audio_info.h"

namespace OHOS {
namespace AudioStandard {
class AudioDeviceDescriptor;
class IStandardAudioService : public IRemoteBroker {
public:
    /**
     * Obtains max volume.
     *
     * @return Returns the max volume.
     */
    virtual int32_t GetMaxVolume(AudioVolumeType volumeType) = 0;

    /**
     * Obtains min volume.
     *
     * @return Returns the min volume.
     */
    virtual int32_t GetMinVolume(AudioVolumeType volumeType) = 0;

    /**
     * Sets Microphone Mute status.
     *
     * @param isMute Mute status true or false to be set.
     * @return Returns 0 if success. Otherwise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t SetMicrophoneMute(bool isMute) = 0;

     /**
     * Gets Microphone Mute status.
     *
     * @return Returns true or false
     */
    virtual bool IsMicrophoneMute() = 0;

    /**
     * @brief Set the Voice Volume.
     *
     * @param volume Voice colume to be set.
     * @return int32_t Returns 0 if success. Otherwise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t SetVoiceVolume(float volume) = 0;

    /**
     * Sets Audio modes.
     *
     * @param audioScene Audio scene type.
     * @param activeDevice Currently active priority device
     * @return Returns 0 if success. Otherwise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t SetAudioScene(AudioScene audioScene, DeviceType activeDevice) = 0;

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
     * Set Audio Parameter.
     *
     * @param  networkId for the distributed device
     * @param  key for the audio parameter to be set
     * @param  condition for the audio parameter to be set
     * @param  value associated with the key for the audio parameter to be set
     * @return none.
     */
    virtual void SetAudioParameter(const std::string& networkId, const AudioParamKey key, const std::string& condition,
        const std::string& value) = 0;

    /**
     * Get Audio Parameter.
     *
     * @param  key for the audio parameter to be set
     * @return Returns value associated to the key requested.
     */
    virtual const std::string GetAudioParameter(const std::string &key) = 0;

    /**
     * Set Audio Parameter.
     *
     * @param  networkId for the distributed device
     * @param  key for the audio parameter to be set
     * @param  condition for the audio parameter to be set
     * @return none.
     */
    virtual const std::string GetAudioParameter(const std::string& networkId, const AudioParamKey key,
        const std::string& condition) = 0;

    /**
     * Update the audio route after device is detected and route is decided
     *
     * @return Returns 0 if success. Otherwise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t UpdateActiveDeviceRoute(DeviceType type, DeviceFlag flag) = 0;

    /**
     * Retrieve cookie information from the service
     *
     * @return Returns cookie information, null if failed.
     */
    virtual const char *RetrieveCookie(int32_t &size) = 0;

    /**
     * Get the transaction Id
     *
     * @return Returns transaction id.
     */
    virtual uint64_t GetTransactionId(DeviceType deviceType, DeviceRole deviceRole)
    {
        (void)deviceType;
        (void)deviceRole;
        return 0;
    }

    /**
     * Notify device connect info
     *
     * @return Returns transaction id.
     */
    virtual void NotifyDeviceInfo(std::string networkId, bool connected) = 0;

    /**
     * Check remote device state.
     *
     * @return Returns transaction id.
     */
    virtual int32_t CheckRemoteDeviceState(std::string networkId, DeviceRole deviceRole, bool isStartDevice) = 0;

    /**
     * Set parameter callback
     *
     * @return Returns the setting result
     */
    virtual int32_t SetParameterCallback(const sptr<IRemoteObject>& object) = 0;

    /**
     * Set audio mono state for accessibility
     *
     * @param  audioMono the state of mono audio for accessibility
     * @return none.
     */
    virtual void SetAudioMonoState(bool audioMono) = 0;

    /**
     * Set audio balance value for accessibility
     *
     * @param  audioBalance the value of audio balance for accessibility
     * @return none.
     */
    virtual void SetAudioBalanceValue(float audioBalance) = 0;

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
        GET_TRANSACTION_ID = 10,
        SET_PARAMETER_CALLBACK = 11,
        GET_REMOTE_AUDIO_PARAMETER = 12,
        SET_REMOTE_AUDIO_PARAMETER = 13,
        NOTIFY_DEVICE_INFO = 14,
        CHECK_REMOTE_DEVICE_STATE = 15,
        SET_VOICE_VOLUME = 16,
        SET_AUDIO_MONO_STATE = 17,
        SET_AUDIO_BALANCE_VALUE = 18,
    };

public:
    DECLARE_INTERFACE_DESCRIPTOR(u"IStandardAudioService");
};

class AudioManagerStub : public IRemoteStub<IStandardAudioService> {
public:
    virtual int OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply,
        MessageOption &option) override;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // I_ST_AUDIO_MANAGER_BASE_H
