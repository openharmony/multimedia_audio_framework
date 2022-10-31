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

#ifndef AUDIO_RENDERER_CALLBACK_NAPI_H_
#define AUDIO_RENDERER_CALLBACK_NAPI_H_

#include "audio_common_napi.h"
#include "audio_renderer.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
class AudioRendererCallbackNapi : public AudioRendererCallback {
public:
    explicit AudioRendererCallbackNapi(napi_env env);
    virtual ~AudioRendererCallbackNapi();
    void SaveCallbackReference(const std::string &callbackName, napi_value callback);
    void OnInterrupt(const InterruptEvent &interruptEvent) override;
    void OnStateChange(const RendererState state, const StateChangeCmdType __attribute__((unused)) cmdType) override;
private:
    struct AudioRendererJsCallback {
        std::shared_ptr<AutoRef> callback = nullptr;
        std::string callbackName = "unknown";
        InterruptEvent interruptEvent;
        RendererState state;
    };

    void OnJsCallbackInterrupt(std::unique_ptr<AudioRendererJsCallback> &jsCb);
    void OnJsCallbackStateChange(std::unique_ptr<AudioRendererJsCallback> &jsCb);

    std::mutex mutex_;
    napi_env env_ = nullptr;
    std::shared_ptr<AutoRef> interruptCallback_ = nullptr;
    std::shared_ptr<AutoRef> stateChangeCallback_ = nullptr;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_RENDERER_CALLBACK_NAPI_H_
