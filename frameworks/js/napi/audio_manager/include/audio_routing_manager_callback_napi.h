/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef AUDIO_ROUTING_MANAGER_CALLBACK_NAPI_H
#define AUDIO_ROUTING_MANAGER_CALLBACK_NAPI_H

#include "audio_common_napi.h"
#include "audio_routing_manager_napi.h"
#include "audio_routing_manager.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include <algorithm>

namespace {
    const std::string PREFER_OUTPUT_DEVICE_CALLBACK_NAME = "preferOutputDeviceChangeForRendererInfo";
    const std::string PREFER_OUTPUT_DEVICE_FILTER_CALLBACK_NAME = "preferOutputDeviceChangeByFilter";
}

namespace OHOS {
namespace AudioStandard {
class AudioPreferOutputDeviceChangeCallbackNapi : public AudioPreferOutputDeviceChangeCallback {
public:
    explicit AudioPreferOutputDeviceChangeCallbackNapi(napi_env env);
    virtual ~AudioPreferOutputDeviceChangeCallbackNapi();
    void SaveCallbackReference(AudioStreamType streamType, napi_value callback);
    void OnPreferOutputDeviceUpdated(const std::vector<sptr<AudioDeviceDescriptor>> &desc) override;
    void RemoveCallbackReference(napi_env env, napi_value args);
    void RemoveAllCallbacks();

private:
    struct AudioActiveOutputDeviceChangeJsCallback {
        std::shared_ptr<AutoRef> callback = nullptr;
        std::string callbackName = "unknown";
        std::vector<sptr<AudioDeviceDescriptor>> desc;
    };

    void OnJsCallbackActiveOutputDeviceChange(std::unique_ptr<AudioActiveOutputDeviceChangeJsCallback> &jsCb);

    std::mutex mutex_;
    napi_env env_ = nullptr;
    std::shared_ptr<AutoRef> preferOutputDeviceCallback_ = nullptr;
    std::list<std::pair<std::shared_ptr<AutoRef>, AudioStreamType>> preferOutputDeviceCbList_;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_ROUTING_MANAGER_CALLBACK_NAPI_H
