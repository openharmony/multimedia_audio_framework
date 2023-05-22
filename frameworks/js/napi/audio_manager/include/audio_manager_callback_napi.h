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

#ifndef AUDIO_MANAGER_CALLBACK_NAPI_H_
#define AUDIO_MANAGER_CALLBACK_NAPI_H_

#include "audio_common_napi.h"
#include "audio_manager_napi.h"
#include "audio_system_manager.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
namespace {
    const std::string DEVICE_CHANGE_CALLBACK_NAME = "deviceChange";
    const std::string MIC_STATE_CHANGE_CALLBACK_NAME = "micStateChange";
}

class AudioManagerCallbackNapi : public AudioManagerDeviceChangeCallback {
public:
    explicit AudioManagerCallbackNapi(napi_env env);
    virtual ~AudioManagerCallbackNapi();
    void SaveCallbackReference(const std::string &callbackName, napi_value callback);
    void OnDeviceChange(const DeviceChangeAction &deviceChangeAction) override;

    void SaveAudioManagerDeviceChangeCbRef(DeviceFlag deviceFlag, napi_value callback);
    void RemoveAudioManagerDeviceChangeCbRef(napi_env env, napi_value callback);
    void RemoveAllAudioManagerDeviceChangeCb();
    int32_t GetAudioManagerDeviceChangeCbListSize();

    void SaveRoutingManagerDeviceChangeCbRef(DeviceFlag deviceFlag, napi_value callback);
    void RemoveRoutingManagerDeviceChangeCbRef(napi_env env, napi_value callback);
    void RemoveAllRoutingManagerDeviceChangeCb();
    int32_t GetRoutingManagerDeviceChangeCbListSize();
private:
    struct AudioManagerJsCallback {
        std::shared_ptr<AutoRef> callback = nullptr;
        std::string callbackName = "unknown";
        DeviceChangeAction deviceChangeAction;
    };

    void OnJsCallbackDeviceChange(std::unique_ptr<AudioManagerJsCallback> &jsCb);

    std::mutex mutex_;
    napi_env env_ = nullptr;
    std::shared_ptr<AutoRef> deviceChangeCallback_ = nullptr;
    std::list<std::pair<std::shared_ptr<AutoRef>, DeviceFlag>> audioManagerDeviceChangeCbList_;
    std::list<std::pair<std::shared_ptr<AutoRef>, DeviceFlag>> routingManagerDeviceChangeCbList_;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_MANAGER_CALLBACK_NAPI_H_
