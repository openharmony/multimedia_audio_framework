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

#ifndef AUDIO_MNGR_NAPI_H_
#define AUDIO_MNGR_NAPI_H_

#include <iostream>
#include <map>
#include <vector>
#include "audio_system_manager.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "audio_stream_mgr_napi.h"
#include "audio_routing_manager_napi.h"

namespace OHOS {
namespace AudioStandard {
static const std::string AUDIO_MNGR_NAPI_CLASS_NAME = "AudioManager";

class AudioManagerNapi {
public:
    AudioManagerNapi();
    ~AudioManagerNapi();

    enum AudioVolumeType {
        VOLUMETYPE_DEFAULT = -1,
        VOICE_CALL = 0,
        RINGTONE = 2,
        MEDIA = 3,
        VOICE_ASSISTANT = 9,
        VOLUMETYPE_MAX,
        ALL = 100
    };

    enum AudioRingMode {
        RINGER_MODE_SILENT = 0,
        RINGER_MODE_VIBRATE,
        RINGER_MODE_NORMAL
    };

    enum InterruptMode {
        SHARE_MODE = 0,
        INDEPENDENT_MODE = 1
    };

    enum FocusType {
        FOCUS_TYPE_RECORDING
    };

    static napi_value Init(napi_env env, napi_value exports);

private:
    
    static void Destructor(napi_env env, void *nativeObject, void *finalize_hint);
    static napi_value Construct(napi_env env, napi_callback_info info);
    static napi_value CreateAudioManagerWrapper(napi_env env);
    static napi_value GetAudioManager(napi_env env, napi_callback_info info);
    static napi_value SetVolume(napi_env env, napi_callback_info info);
    static napi_value GetVolume(napi_env env, napi_callback_info info);
    static napi_value GetMaxVolume(napi_env env, napi_callback_info info);
    static napi_value GetMinVolume(napi_env env, napi_callback_info info);
    static napi_value GetDevices(napi_env env, napi_callback_info info);
    static napi_value SetStreamMute(napi_env env, napi_callback_info info);
    static napi_value IsStreamMute(napi_env env, napi_callback_info info);
    static napi_value IsStreamActive(napi_env env, napi_callback_info info);
    static napi_value SetRingerMode(napi_env env, napi_callback_info info);
    static napi_value GetRingerMode(napi_env env, napi_callback_info info);
    static napi_value SetAudioScene(napi_env env, napi_callback_info info);
    static napi_value GetAudioScene(napi_env env, napi_callback_info info);
    static napi_value SetDeviceActive(napi_env env, napi_callback_info info);
    static napi_value IsDeviceActive(napi_env env, napi_callback_info info);
    static napi_value SetAudioParameter(napi_env env, napi_callback_info info);
    static napi_value GetAudioParameter(napi_env env, napi_callback_info info);
    static napi_value SetMicrophoneMute(napi_env env, napi_callback_info info);
    static napi_value IsMicrophoneMute(napi_env env, napi_callback_info info);
    static napi_status AddNamedProperty(napi_env env, napi_value object, const std::string name, int32_t enumValue);
    static napi_value CreateAudioVolumeTypeObject(napi_env env);
    static napi_value CreateDeviceFlagObject(napi_env env);
    static napi_value CreateActiveDeviceTypeObject(napi_env env);
    static napi_value CreateConnectTypeObject(napi_env env);
    static napi_value CreateInterruptActionTypeObject(napi_env env);
    static napi_value CreateAudioRingModeObject(napi_env env);
    static napi_value On(napi_env env, napi_callback_info info);
    static napi_value Off(napi_env env, napi_callback_info info);
    static napi_value CreateDeviceChangeTypeObject(napi_env env);
    static napi_value CreateAudioSceneObject(napi_env env);
    static napi_value RequestIndependentInterrupt(napi_env env, napi_callback_info info);
    static napi_value AbandonIndependentInterrupt(napi_env env, napi_callback_info info);
    static napi_value GetStreamManager(napi_env env, napi_callback_info info);
    static napi_value GetRoutingManager(napi_env env, napi_callback_info info);
    static napi_value GetVolumeManager(napi_env env, napi_callback_info info);
    static napi_value GetInterruptManager(napi_env env, napi_callback_info info);
    static void AddPropName(std::string& propName, napi_status& status, napi_env env, napi_value& result);

    template<typename T> static napi_value CreatePropertyBase(napi_env env, T& t_map, napi_ref ref);

    static napi_ref audioVolumeTypeRef_;
    static napi_ref deviceFlagRef_;
    static napi_ref deviceRoleRef_;
    static napi_ref deviceTypeRef_;
    static napi_ref activeDeviceTypeRef_;
    static napi_ref audioRingModeRef_;
    static napi_ref deviceChangeType_;
    static napi_ref interruptActionType_;
    static napi_ref interruptHint_;
    static napi_ref interruptType_;
    static napi_ref audioScene_;
    static napi_ref interruptMode_;
    static napi_ref focusType_;
    static napi_ref connectTypeRef_;
    static napi_ref audioErrors_;
    static napi_ref communicationDeviceType_;
    static napi_ref interruptRequestType_;
    static napi_ref interruptRequestResultType_;

    AudioSystemManager *audioMngr_;
    int32_t cachedClientId_ = -1;
    std::shared_ptr<AudioManagerDeviceChangeCallback> deviceChangeCallbackNapi_ = nullptr;
    std::shared_ptr<AudioManagerCallback> interruptCallbackNapi_ = nullptr;
    std::shared_ptr<AudioRingerModeCallback> ringerModecallbackNapi_ = nullptr;
    std::shared_ptr<VolumeKeyEventCallback> volumeKeyEventCallbackNapi_ = nullptr;
    napi_env env_;
};

static const std::map<std::string, DeviceChangeType> DEVICE_CHANGE_TYPE_MAP = {
    {"CONNECT", CONNECT},
    {"DISCONNECT", DISCONNECT}
};

static const std::map<std::string, AudioScene> AUDIO_SCENE_MAP = {
    {"AUDIO_SCENE_DEFAULT", AUDIO_SCENE_DEFAULT},
    {"AUDIO_SCENE_RINGING", AUDIO_SCENE_RINGING},
    {"AUDIO_SCENE_PHONE_CALL", AUDIO_SCENE_PHONE_CALL},
    {"AUDIO_SCENE_VOICE_CHAT", AUDIO_SCENE_PHONE_CHAT}
};

static const std::map<std::string, InterruptActionType> INTERRUPT_ACTION_TYPE_MAP = {
    {"TYPE_ACTIVATED", TYPE_ACTIVATED},
    {"TYPE_INTERRUPT", TYPE_INTERRUPT}
};

static const std::map<std::string, AudioManagerNapi::AudioVolumeType> VOLUME_TYPE_MAP = {
    {"VOICE_CALL", AudioManagerNapi::VOICE_CALL},
    {"RINGTONE", AudioManagerNapi::RINGTONE},
    {"MEDIA", AudioManagerNapi::MEDIA},
    {"VOICE_ASSISTANT", AudioManagerNapi::VOICE_ASSISTANT}
};

static const std::map<std::string, AudioStandard::DeviceType> DEVICE_TYPE_MAP = {
    {"INVALID", DEVICE_TYPE_INVALID},
    {"EARPIECE", DEVICE_TYPE_EARPIECE},
    {"SPEAKER", DEVICE_TYPE_SPEAKER},
    {"WIRED_HEADSET", DEVICE_TYPE_WIRED_HEADSET},
    {"WIRED_HEADPHONES", DEVICE_TYPE_WIRED_HEADPHONES},
    {"BLUETOOTH_SCO", DEVICE_TYPE_BLUETOOTH_SCO},
    {"BLUETOOTH_A2DP", DEVICE_TYPE_BLUETOOTH_A2DP},
    {"MIC", DEVICE_TYPE_MIC},
    {"USB_HEADSET", DEVICE_TYPE_USB_HEADSET}
};

static const std::map<std::string, AudioStandard::ActiveDeviceType> ACTIVE_DEVICE_TYPE = {
    {"SPEAKER", SPEAKER},
    {"BLUETOOTH_SCO", BLUETOOTH_SCO}
};

static const std::map<std::string, AudioStandard::InterruptMode> INTERRUPT_MODE_MAP = {
    {"SHARE_MODE", SHARE_MODE},
    {"INDEPENDENT_MODE", INDEPENDENT_MODE}
};

static const std::map<std::string, AudioStandard::FocusType> FOCUS_TYPE_MAP = {
    {"FOCUS_TYPE_RECORDING", FOCUS_TYPE_RECORDING}
};

static const std::map<std::string, AudioStandard::AudioErrors> AUDIO_ERRORS_MAP = {
    {"ERROR_INVALID_PARAM", ERROR_INVALID_PARAM},
    {"ERROR_NO_MEMORY", ERROR_NO_MEMORY},
    {"ERROR_ILLEGAL_STATE", ERROR_ILLEGAL_STATE},
    {"ERROR_UNSUPPORTED", ERROR_UNSUPPORTED},
    {"ERROR_TIMEOUT", ERROR_TIMEOUT},
    {"ERROR_STREAM_LIMIT", ERROR_STREAM_LIMIT},
    {"ERROR_SYSTEM", ERROR_SYSTEM}
};

static const std::map<std::string, AudioStandard::CommunicationDeviceType> COMMUNICATION_DEVICE_TYPE_MAP = {
    {"SPEAKER", COMMUNICATION_SPEAKER}
};

static const std::map<std::string, AudioStandard::InterruptRequestType> INTERRUPT_REQUEST_TYPE_MAP = {
    {"INTERRUPT_REQUEST_TYPE_DEFAULT", INTERRUPT_REQUEST_TYPE_DEFAULT},
};
static const std::map<std::string, AudioStandard::InterruptRequestResultType> INTERRUPT_REQUEST_RESULT_TYPE_MAP = {
    {"INTERRUPT_REQUEST_GRANT", INTERRUPT_REQUEST_GRANT},
    {"INTERRUPT_REQUEST_REJECT", INTERRUPT_REQUEST_REJECT},
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* AUDIO_MNGR_NAPI_H_ */
