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

#ifndef AUDIO_VOLUME_MANAGER_NAPI_H_
#define AUDIO_VOLUME_MANAGER_NAPI_H_

#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "audio_system_manager.h"
#include "audio_manager_callback_napi.h"
#include "audio_volume_group_manager_napi.h"

namespace OHOS {
namespace AudioStandard {
static const std::string AUDIO_VOLUME_MANAGER_NAPI_CLASS_NAME = "AudioVolumeManager";

struct AudioVolumeManagerAsyncContext;
class AudioVolumeManagerNapi {
public:
    AudioVolumeManagerNapi();
    ~AudioVolumeManagerNapi();

    static napi_value Init(napi_env env, napi_value exports);
    static napi_value CreateVolumeManagerWrapper(napi_env env);
private:
    static napi_value GetVolumeGroupInfos(napi_env env, napi_callback_info info);
    static napi_value GetVolumeGroupManager(napi_env env, napi_callback_info info);
    static napi_value SetRingerMode(napi_env env, napi_callback_info info);
    static napi_value GetRingerMode(napi_env env, napi_callback_info info);
    static napi_value SetMicrophoneMute(napi_env env, napi_callback_info info);
    static napi_value IsMicrophoneMute(napi_env env, napi_callback_info info);
    static napi_value On(napi_env env, napi_callback_info info);

    static napi_value Construct(napi_env env, napi_callback_info info);
    static void Destructor(napi_env env, void *nativeObject, void *finalize_hint);
    AudioSystemManager *audioSystemMngr_;

    int32_t cachedClientId_ = -1;
    std::shared_ptr<VolumeKeyEventCallback> volumeKeyEventCallbackNapi_ = nullptr;

    napi_env env_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* AUDIO_ROUTING_MANAGER_NAPI_H_ */
