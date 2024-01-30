/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#include "audio_effect.h"
#include "pulseaudio_ipc_interface_code.h"

namespace OHOS {
namespace AudioStandard {
class AudioDeviceDescriptor;
class IStandardAudioService : public IRemoteBroker {
public:
    /**
     * Sets Microphone Mute status.
     *
     * @param isMute Mute status true or false to be set.
     * @return Returns 0 if success. Otherwise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t SetMicrophoneMute(bool isMute) = 0;

    /**
     * @brief Set the Voice Volume.
     *
     * @param volume Voice colume to be set.
     * @return int32_t Returns 0 if success. Otherwise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t SetVoiceVolume(float volume) = 0;

    virtual int32_t GetCapturePresentationPosition(const std::string& deviceClass, uint64_t& frames, int64_t& timeSec,
        int64_t& timeNanoSec) = 0;

    virtual int32_t GetRenderPresentationPosition(const std::string& deviceClass, uint64_t& frames, int64_t& timeSec,
        int64_t& timeNanoSec) = 0;

    virtual int32_t OffloadSetVolume(float volume) = 0;
    virtual int32_t OffloadDrain() = 0;
    virtual int32_t OffloadGetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec) = 0;
    virtual int32_t OffloadSetBufferSize(uint32_t sizeMs) = 0;

    /**
     * Sets Audio modes.
     *
     * @param audioScene Audio scene type.
     * @param activeDevice Currently active priority device
     * @return Returns 0 if success. Otherwise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t SetAudioScene(AudioScene audioScene, DeviceType activeDevice) = 0;

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
     * Set Extra Audio Parameters.
     *
     * @param key main key for the extra audio parameter to be set
     * @param kvpairs associated with the sub keys and values for the extra audio parameter to be set
     * @return none.
     */
    virtual void SetExtraParameters(const std::string &key,
        const std::vector<std::pair<std::string, std::string>> &kvpairs) = 0;

    /**
     * Get Extra Audio Parameters.
     *
     * @param mainKey main key for the extra audio parameter to be get
     * @param subKeys associated with the key for the extra audio parameter to be get
     * @return Returns value associated to the key requested.
     */
    virtual const std::vector<std::pair<std::string, std::string>> GetExtraParameters(const std::string &mainKey,
        const std::vector<std::string> &subKyes) = 0;

    /**
     * Update the audio route after device is detected and route is decided
     *
     * @return Returns 0 if success. Otherwise returns Errocode defined in audio_errors.h.
     */
    virtual int32_t UpdateActiveDeviceRoute(DeviceType type, DeviceFlag flag) = 0;

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
     * Set wakeupSource callback
     *
     * @return Returns the setting result
     */
    virtual int32_t SetWakeupSourceCallback(const sptr<IRemoteObject>& object) = 0;

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

    /**
     * Create AudioProcess for play.
     *
     * @param config the config of the AudioProcess
     *
     * @return Returns AudioProcess client.
    */
    virtual sptr<IRemoteObject> CreateAudioProcess(const AudioProcessConfig &config) = 0;

    /**
     * Use effect manager information to load effect libraries.
     *
     * @return true/false.
     */
    virtual bool LoadAudioEffectLibraries(std::vector<Library> libraries, std::vector<Effect> effects,
        std::vector<Effect> &successEffects) = 0;

    /**
     * Create effect chain manager for audio effect processing.
     *
     * @return true/false.
     */
    virtual bool CreateEffectChainManager(std::vector<EffectChain> &effectChains,
        std::unordered_map<std::string, std::string> &map) = 0;

    /**
     * Set output device sink for effect chain manager.
     *
     * @return true/false.
     */
    virtual bool SetOutputDeviceSink(int32_t device, std::string &sinkName) = 0;

    /**
     * Regiest policy provider.
     *
     * @return result code.
     */
    virtual int32_t RegiestPolicyProvider(const sptr<IRemoteObject> &object) = 0;

    /**
     * Request thread priority for client thread.
     */
    virtual void RequestThreadPriority(uint32_t tid, std::string bundleName) = 0;

    /**
     * Create playback capturer manager.
     *
     * @return true/false.
     */
    virtual bool CreatePlaybackCapturerManager() = 0;

    /**
     * Set StreamUsage set which support playback capturer.
     *
     * @param usage value of StreamUsage which support inner capturer.
     *
     * @return result of setting. 0 if success, error number else.
    */
    virtual int32_t SetSupportStreamUsage(std::vector<int32_t> usage) = 0;

    /**
     * Mark if playback capture silently.
     *
     * @param state identify the capture state
     *
     * @return result of setting. 0 if success, error number else.
    */
    virtual int32_t SetCaptureSilentState(bool state) = 0;

    /**
     * Update spatialization enabled state and head tracking enabled state.
     *
     * @param state identify the enabled state
     *
     * @return result of setting. 0 if success, error number else.
    */
    virtual int32_t UpdateSpatializationState(AudioSpatializationState spatializationState) = 0;

    /**
     * Notify Stream volume changed.
     *
     * @param streamType specified streamType whose volume to be notified
     * @param volume stream volume in float
     *
     * @return result of notify. 0 if success, error number else.
    */
    virtual int32_t NotifyStreamVolumeChanged(AudioStreamType streamType, float volume) = 0;

public:
    DECLARE_INTERFACE_DESCRIPTOR(u"IStandardAudioService");
};

class AudioManagerStub : public IRemoteStub<IStandardAudioService> {
public:
    virtual int OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply,
        MessageOption &option) override;

private:
    int HandleGetAudioParameter(MessageParcel &data, MessageParcel &reply);
    int HandleSetAudioParameter(MessageParcel &data, MessageParcel &reply);
    int HandleGetExtraAudioParameters(MessageParcel &data, MessageParcel &reply);
    int HandleSetExtraAudioParameters(MessageParcel &data, MessageParcel &reply);
    int HandleSetMicrophoneMute(MessageParcel &data, MessageParcel &reply);
    int HandleSetAudioScene(MessageParcel &data, MessageParcel &reply);
    int HandleUpdateActiveDeviceRoute(MessageParcel &data, MessageParcel &reply);
    int HandleGetTransactionId(MessageParcel &data, MessageParcel &reply);
    int HandleSetParameterCallback(MessageParcel &data, MessageParcel &reply);
    int HandleGetRemoteAudioParameter(MessageParcel &data, MessageParcel &reply);
    int HandleSetRemoteAudioParameter(MessageParcel &data, MessageParcel &reply);
    int HandleNotifyDeviceInfo(MessageParcel &data, MessageParcel &reply);
    int HandleCheckRemoteDeviceState(MessageParcel &data, MessageParcel &reply);
    int HandleSetVoiceVolume(MessageParcel &data, MessageParcel &reply);
    int HandleSetAudioMonoState(MessageParcel &data, MessageParcel &reply);
    int HandleSetAudioBalanceValue(MessageParcel &data, MessageParcel &reply);
    int HandleCreateAudioProcess(MessageParcel &data, MessageParcel &reply);
    int HandleLoadAudioEffectLibraries(MessageParcel &data, MessageParcel &reply);
    int HandleRequestThreadPriority(MessageParcel &data, MessageParcel &reply);
    int HandleCreateAudioEffectChainManager(MessageParcel &data, MessageParcel &reply);
    int HandleSetOutputDeviceSink(MessageParcel &data, MessageParcel &reply);
    int HandleCreatePlaybackCapturerManager(MessageParcel &data, MessageParcel &reply);
    int HandleSetSupportStreamUsage(MessageParcel &data, MessageParcel &reply);
    int HandleRegiestPolicyProvider(MessageParcel &data, MessageParcel &reply);
    int HandleSetWakeupSourceCallback(MessageParcel &data, MessageParcel &reply);
    int HandleSetCaptureSilentState(MessageParcel &data, MessageParcel &reply);
    int HandleUpdateSpatializationState(MessageParcel &data, MessageParcel &reply);
    int HandleGetCapturePresentationPosition(MessageParcel &data, MessageParcel &reply);
    int HandleGetRenderPresentationPosition(MessageParcel &data, MessageParcel &reply);
    int HandleOffloadSetVolume(MessageParcel &data, MessageParcel &reply);
    int HandleOffloadDrain(MessageParcel &data, MessageParcel &reply);
    int HandleOffloadGetPresentationPosition(MessageParcel &data, MessageParcel &reply);
    int HandleOffloadSetBufferSize(MessageParcel &data, MessageParcel &reply);
    int HandleNotifyStreamVolumeChanged(MessageParcel &data, MessageParcel &reply);

    using HandlerFunc = int (AudioManagerStub::*)(MessageParcel &data, MessageParcel &reply);
    static inline HandlerFunc handlers[] = {
        &AudioManagerStub::HandleGetAudioParameter,
        &AudioManagerStub::HandleSetAudioParameter,
        &AudioManagerStub::HandleGetExtraAudioParameters,
        &AudioManagerStub::HandleSetExtraAudioParameters,
        &AudioManagerStub::HandleSetMicrophoneMute,
        &AudioManagerStub::HandleSetAudioScene,
        &AudioManagerStub::HandleUpdateActiveDeviceRoute,
        &AudioManagerStub::HandleGetTransactionId,
        &AudioManagerStub::HandleSetParameterCallback,
        &AudioManagerStub::HandleGetRemoteAudioParameter,
        &AudioManagerStub::HandleSetRemoteAudioParameter,
        &AudioManagerStub::HandleNotifyDeviceInfo,
        &AudioManagerStub::HandleCheckRemoteDeviceState,
        &AudioManagerStub::HandleSetVoiceVolume,
        &AudioManagerStub::HandleSetAudioMonoState,
        &AudioManagerStub::HandleSetAudioBalanceValue,
        &AudioManagerStub::HandleCreateAudioProcess,
        &AudioManagerStub::HandleLoadAudioEffectLibraries,
        &AudioManagerStub::HandleRequestThreadPriority,
        &AudioManagerStub::HandleCreateAudioEffectChainManager,
        &AudioManagerStub::HandleSetOutputDeviceSink,
        &AudioManagerStub::HandleCreatePlaybackCapturerManager,
        &AudioManagerStub::HandleSetSupportStreamUsage,
        &AudioManagerStub::HandleRegiestPolicyProvider,
        &AudioManagerStub::HandleSetWakeupSourceCallback,
        &AudioManagerStub::HandleSetCaptureSilentState,
        &AudioManagerStub::HandleUpdateSpatializationState,
        &AudioManagerStub::HandleOffloadSetVolume,
        &AudioManagerStub::HandleOffloadDrain,
        &AudioManagerStub::HandleOffloadGetPresentationPosition,
        &AudioManagerStub::HandleOffloadSetBufferSize,
        &AudioManagerStub::HandleNotifyStreamVolumeChanged,
        &AudioManagerStub::HandleGetCapturePresentationPosition,
        &AudioManagerStub::HandleGetRenderPresentationPosition,
    };
    static constexpr size_t handlersNums = sizeof(handlers) / sizeof(HandlerFunc);
    static_assert(handlersNums == (static_cast<size_t> (AudioServerInterfaceCode::AUDIO_SERVER_CODE_MAX) + 1),
        "please check pulseaudio_ipc_interface_code");
};
} // namespace AudioStandard
} // namespace OHOS
#endif // I_ST_AUDIO_MANAGER_BASE_H
