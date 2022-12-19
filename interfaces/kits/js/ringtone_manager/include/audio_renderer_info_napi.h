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

#ifndef AUDIO_RENDERER_INFO_NAPI_H_
#define AUDIO_RENDERER_INFO_NAPI_H_

#include "audio_info.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
static const std::string AUDIO_RENDERER_INFO_NAPI_CLASS_NAME = "AudioRendererInfo";

class AudioRendererInfoNapi {
public:
    AudioRendererInfoNapi();
    ~AudioRendererInfoNapi();

    static napi_value Init(napi_env env, napi_value exports);
    static napi_value CreateAudioRendererInfoWrapper(napi_env env,
        std::unique_ptr<AudioRendererInfo> &audioRendererInfo);

private:
    static void Destructor(napi_env env, void *nativeObject, void *finalize_hint);
    static napi_value Construct(napi_env env, napi_callback_info info);
    static napi_value GetContentType(napi_env env, napi_callback_info info);
    static napi_value SetContentType(napi_env env, napi_callback_info info);
    static napi_value GetStreamUsage(napi_env env, napi_callback_info info);
    static napi_value SetStreamUsage(napi_env env, napi_callback_info info);
    static napi_value GetRendererFlags(napi_env env, napi_callback_info info);
    static napi_value SetRendererFlags(napi_env env, napi_callback_info info);

    static napi_ref sConstructor_;

    static std::unique_ptr<AudioRendererInfo> sAudioRendererInfo_;

    std::unique_ptr<AudioRendererInfo> audioRendererInfo_;
    napi_env env_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* AUDIO_RENDERER_INFO_NAPI_H_ */