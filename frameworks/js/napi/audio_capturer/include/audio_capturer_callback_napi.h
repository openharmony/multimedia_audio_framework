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

#ifndef AUDIO_CAPTURER_CALLBACK_NAPI_H_
#define AUDIO_CAPTURER_CALLBACK_NAPI_H_

#include "audio_common_napi.h"
#include "audio_capturer.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
class AudioCapturerCallbackNapi : public AudioCapturerCallback {
public:
    explicit AudioCapturerCallbackNapi(napi_env env);
    virtual ~AudioCapturerCallbackNapi();
    void SaveCallbackReference(const std::string &callbackName, napi_value callback);
    void OnStateChange(const CapturerState state) override;
private:
    struct AudioCapturerJsCallback {
        std::shared_ptr<AutoRef> callback = nullptr;
        std::string callbackName = "unknown";
        CapturerState state;
    };

    void OnJsCallbackStateChange(std::unique_ptr<AudioCapturerJsCallback> &jsCb);

    std::mutex mutex_;
    napi_env env_ = nullptr;
    std::shared_ptr<AutoRef> stateChangeCallback_ = nullptr;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_CAPTURER_CALLBACK_NAPI_H_
