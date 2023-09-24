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

#ifndef AUDIO_RENDERER_STATE_CALLBACK_NAPI_H
#define AUDIO_RENDERER_STATE_CALLBACK_NAPI_H

#include "audio_common_napi.h"
#include "audio_manager_napi.h"
#include "audio_stream_manager.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
class AudioRendererStateCallbackNapi : public AudioRendererStateChangeCallback {
public:
    explicit AudioRendererStateCallbackNapi(napi_env env);
    virtual ~AudioRendererStateCallbackNapi();
    void SaveCallbackReference(napi_value callback);
    void RemoveCallbackReference();
    void OnRendererStateChange(
        const std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos) override;

private:
    struct AudioRendererStateJsCallback {
        std::shared_ptr<AutoRef> callback = nullptr;
        std::vector<std::unique_ptr<AudioRendererChangeInfo>> changeInfos;
    };

    void OnJsCallbackRendererState(std::unique_ptr<AudioRendererStateJsCallback> &jsCb);

    std::mutex mutex_;
    napi_env env_ = nullptr;
    std::shared_ptr<AutoRef> rendererStateCallback_ = nullptr;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_RENDERERSTATE_CALLBACK_NAPI_H
