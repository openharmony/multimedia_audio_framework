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
#ifndef NAPI_AUDIO_RENDERER_POLICY_SERVICE_DIED_CALLBACK_H
#define NAPI_AUDIO_RENDERER_POLICY_SERVICE_DIED_CALLBACK_H

#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "napi_audio_renderer.h"

namespace OHOS {
namespace AudioStandard {
class NapiAudioRendererPolicyServiceDiedCallback : public AudioRendererPolicyServiceDiedCallback {
public:
    explicit NapiAudioRendererPolicyServiceDiedCallback(NapiAudioRenderer *rendererNapi);
    virtual ~NapiAudioRendererPolicyServiceDiedCallback();
    void OnAudioPolicyServiceDied() override;

private:
    std::mutex mutex_;
    NapiAudioRenderer *rendererNapi_;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // NAPI_AUDIO_RENDERER_POLICY_SERVICE_DIED_CALLBACK_H
