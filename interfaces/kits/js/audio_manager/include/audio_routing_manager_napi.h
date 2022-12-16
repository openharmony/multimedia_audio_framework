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

#ifndef AUDIO_ROUTING_MANAGER_NAPI_H_
#define AUDIO_ROUTING_MANAGER_NAPI_H_

#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "audio_renderer_info_napi.h"
#include "audio_system_manager.h"
#include "audio_manager_callback_napi.h"
#include "audio_routing_manager.h"

namespace OHOS {
namespace AudioStandard {
static const std::string AUDIO_ROUTING_MANAGER_NAPI_CLASS_NAME = "AudioRoutingManager";

struct AudioRoutingManagerAsyncContext;
class AudioRoutingManagerNapi {
public:
    AudioRoutingManagerNapi();
    ~AudioRoutingManagerNapi();

    static napi_value Init(napi_env env, napi_value exports);
    static napi_value CreateRoutingManagerWrapper(napi_env env);
private:
    static napi_value GetDevices(napi_env env, napi_callback_info info);

    static void CheckParams(size_t argc, napi_env env, napi_value* argv,
        std::unique_ptr<AudioRoutingManagerAsyncContext>& asyncContext, const int32_t refCount, napi_value& result);

    static napi_value SelectOutputDevice(napi_env env, napi_callback_info info);
    static napi_value SelectInputDevice(napi_env env, napi_callback_info info);
    static napi_value SelectOutputDeviceByFilter(napi_env env, napi_callback_info info);
    static napi_value SelectInputDeviceByFilter(napi_env env, napi_callback_info info);
    static napi_value SetCommunicationDevice(napi_env env, napi_callback_info info);
    static napi_value IsCommunicationDeviceActive(napi_env env, napi_callback_info info);
    static napi_value GetActiveOutputDeviceDescriptors(napi_env env, napi_callback_info info);

    static void RegisterDeviceChangeCallback(napi_env env, napi_value* args, const std::string& cbName, int32_t flag,
        AudioRoutingManagerNapi* routingMgrNapi);
    static void RegisterCallback(napi_env env, napi_value jsThis, napi_value* args, const std::string& cbName,
        int32_t flag);
    static napi_value On(napi_env env, napi_callback_info info);
    static napi_value Off(napi_env env, napi_callback_info info);

    static napi_value Construct(napi_env env, napi_callback_info info);
    static void Destructor(napi_env env, void *nativeObject, void *finalize_hint);
    AudioSystemManager *audioMngr_;
    AudioRoutingManager *audioRoutingMngr_;
    std::shared_ptr<AudioManagerDeviceChangeCallback> deviceChangeCallbackNapi_ = nullptr;
    std::shared_ptr<AudioManagerMicStateChangeCallback> micStateChangeCallbackNapi_ = nullptr;

    napi_env env_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* AUDIO_ROUTING_MANAGER_NAPI_H_ */
