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

#ifndef RENDERER_POSITION_CALLBACK_NAPI_H_
#define RENDERER_POSITION_CALLBACK_NAPI_H_

#include "audio_common_napi.h"
#include "audio_renderer.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
class RendererPositionCallbackNapi : public RendererPositionCallback {
public:
    explicit RendererPositionCallbackNapi(napi_env env);
    virtual ~RendererPositionCallbackNapi();
    void SaveCallbackReference(const std::string &callbackName, napi_value callback);
    void OnMarkReached(const int64_t &framePosition) override;

private:
    struct RendererPositionJsCallback {
        std::shared_ptr<AutoRef> callback = nullptr;
        std::string callbackName = "unknown";
        int64_t position = 0;
    };

    void OnJsRendererPositionCallback(std::unique_ptr<RendererPositionJsCallback> &jsCb);

    std::mutex mutex_;
    napi_env env_ = nullptr;
    std::shared_ptr<AutoRef> renderPositionCallback_ = nullptr;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // RENDERER_POSITION_CALLBACK_NAPI_H_
