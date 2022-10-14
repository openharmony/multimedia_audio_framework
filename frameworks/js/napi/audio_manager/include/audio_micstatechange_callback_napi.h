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

#ifndef AUDIO_MIC_STATE_CHANGE_MANAGER_CALLBACK_NAPI_H
#define AUDIO_MIC_STATE_CHANGE_MANAGER_CALLBACK_NAPI_H

#include "audio_common_napi.h"
#include "audio_routing_manager_napi.h"
#include "audio_routing_manager.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
class AudioManagerMicStateChangeCallbackNapi : public AudioManagerMicStateChangeCallback {
public:
    explicit AudioManagerMicStateChangeCallbackNapi(napi_env env);
    virtual ~AudioManagerMicStateChangeCallbackNapi();
    void SaveCallbackReference(const std::string &callbackName, napi_value callback);
    void OnMicStateUpdated(const MicStateChangeEvent &micStateChangeEvent) override;

private:
    struct AudioManagerMicStateChangeJsCallback {
        std::shared_ptr<AutoRef> callback = nullptr;
        std::string callbackName = "unknown";
        MicStateChangeEvent micStateChangeEvent;
    };

    void OnJsCallbackMicStateChange(std::unique_ptr<AudioManagerMicStateChangeJsCallback> &jsCb);

    std::mutex mutex_;
    napi_env env_ = nullptr;
    std::shared_ptr<AutoRef> micStateChangeCallback_ = nullptr;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_MIC_STATE_CHANGE_MANAGER_CALLBACK_NAPI_H
