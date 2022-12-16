/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef AUDIO_VOLUME_GROUP_MNGR_NAPI_H_
#define AUDIO_VOLUME_GROUP_MNGR_NAPI_H_

#include <iostream>
#include <map>
#include <vector>
#include "audio_system_manager.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "audio_common_napi.h"
#include "audio_ringermode_callback_napi.h"
#include "audio_micstatechange_callback_napi.h"

namespace OHOS {
namespace AudioStandard {
static const std::string AUDIO_VOLUME_GROUP_MNGR_NAPI_CLASS_NAME = "AudioVolumeGroupManager";

class AudioVolumeGroupManagerNapi {
public:

    enum AudioRingMode {
        RINGER_MODE_SILENT = 0,
        RINGER_MODE_VIBRATE,
        RINGER_MODE_NORMAL
    };

    static napi_value Init(napi_env env, napi_value exports);
    static napi_value CreateAudioVolumeGroupManagerWrapper(napi_env env, int32_t groupId);
    static int32_t isConstructSuccess_;
private:
    static void Destructor(napi_env env, void *nativeObject, void *finalize_hint);
    static napi_value Construct(napi_env env, napi_callback_info info);
    static napi_value GetVolume(napi_env env, napi_callback_info info);
    static napi_value SetVolume(napi_env env, napi_callback_info info);
    static napi_value GetMaxVolume(napi_env env, napi_callback_info info);
    static napi_value GetMinVolume(napi_env env, napi_callback_info info);
    static napi_value SetMute(napi_env env, napi_callback_info info);
    static napi_value IsStreamMute(napi_env env, napi_callback_info info);
    static napi_value SetRingerMode(napi_env env, napi_callback_info info);
    static napi_value GetRingerMode(napi_env env, napi_callback_info info);
    static napi_value SetMicrophoneMute(napi_env env, napi_callback_info info);
    static napi_value IsMicrophoneMute(napi_env env, napi_callback_info info);
    static napi_value On(napi_env env, napi_callback_info info);
    std::shared_ptr<AudioGroupManager> audioGroupMngr_ = nullptr;
    int32_t cachedClientId_ = -1;

    std::shared_ptr<AudioRingerModeCallback> ringerModecallbackNapi_ = nullptr;
    std::shared_ptr<AudioManagerMicStateChangeCallback> micStateChangeCallbackNapi_ = nullptr;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_VOLUME_GROUP_MNGR_NAPI_H_
