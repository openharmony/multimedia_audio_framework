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

#ifndef AUDIO_STREAM_MGR_NAPI_H_
#define AUDIO_STREAM_MGR_NAPI_H_

#include "audio_renderer.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "audio_stream_manager.h"
#include "audio_system_manager.h"

namespace OHOS {
namespace AudioStandard {
static const std::string AUDIO_STREAM_MGR_NAPI_CLASS_NAME = "AudioStreamManager";

class AudioStreamMgrNapi {
public:
    AudioStreamMgrNapi();
    ~AudioStreamMgrNapi();

    static napi_value Init(napi_env env, napi_value exports);
    static napi_value CreateStreamManagerWrapper(napi_env env);
    static napi_value GetStreamManager(napi_env env, napi_callback_info info);

private:
    static napi_value GetCurrentAudioRendererInfos(napi_env env, napi_callback_info info);
    static napi_value GetCurrentAudioCapturerInfos(napi_env env, napi_callback_info info);
    static napi_value On(napi_env env, napi_callback_info info);
    static napi_value Off(napi_env env, napi_callback_info info);
    static napi_value IsAudioRendererLowLatencySupported(napi_env env, napi_callback_info info);
    static napi_value IsStreamActive(napi_env env, napi_callback_info info);
    static void RegisterCallback(napi_env env, napi_value jsThis,
                                napi_value* args, const std::string& cbName);
    static void RegisterCapturerStateChangeCallback(napi_env env, napi_value* args,
                                    const std::string& cbName, AudioStreamMgrNapi *streamMgrNapi);
    static void RegisterRendererStateChangeCallback(napi_env env, napi_value* args,
                                    const std::string& cbName, AudioStreamMgrNapi *streamMgrNapi);
    static void  UnregisterCallback(napi_env env, napi_value jsThis, const std::string& cbName);
    static napi_value Construct(napi_env env, napi_callback_info info);
    static void Destructor(napi_env env, void *nativeObject, void *finalize_hint);
    static bool ParseAudioStreamInfo(napi_env env, napi_value root, AudioStreamInfo &audioStreamInfo);
    static void IsLowLatencySupportedCallback(napi_env env, napi_status status, void *data);
    napi_env env_;
    AudioStreamManager *audioStreamMngr_;
    AudioSystemManager *audioMngr_;
    int32_t cachedClientId_ = -1;
    std::shared_ptr<AudioRendererStateChangeCallback> rendererStateChangeCallbackNapi_ = nullptr;
    std::shared_ptr<AudioCapturerStateChangeCallback> capturerStateChangeCallbackNapi_ = nullptr;
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* AUDIO_STREAM_MGR_NAPI_H_ */
