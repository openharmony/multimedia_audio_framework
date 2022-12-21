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

#ifndef AUDIO_CAPTURER_NAPI_H_
#define AUDIO_CAPTURER_NAPI_H_

#include <iostream>
#include <map>

#include "audio_capturer.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
static const std::string AUDIO_CAPTURER_NAPI_CLASS_NAME = "AudioCapturer";

class AudioCapturerNapi {
public:
    AudioCapturerNapi();
    ~AudioCapturerNapi();

    enum AudioSampleFormat {
        SAMPLE_FORMAT_INVALID = -1,
        SAMPLE_FORMAT_U8 = 0,
        SAMPLE_FORMAT_S16LE = 1,
        SAMPLE_FORMAT_S24LE = 2,
        SAMPLE_FORMAT_S32LE = 3,
        SAMPLE_FORMAT_F32LE = 4
    };

    static napi_value Init(napi_env env, napi_value exports);
private:
    struct AudioCapturerAsyncContext {
        napi_env env;
        napi_async_work work;
        napi_deferred deferred;
        napi_ref callbackRef = nullptr;
        uint64_t time;
        int32_t status;
        int32_t intValue;
        uint32_t userSize;
        uint8_t *buffer = nullptr;
        size_t bytesRead;
        size_t bufferSize;
        uint32_t audioStreamId;
        bool isBlocking;
        bool isTrue;
        AudioSampleFormat audioSampleFormat;
        AudioSamplingRate samplingRate;
        AudioChannel audioChannel;
        AudioEncodingType audioEncoding;
        ContentType contentType;
        StreamUsage usage;
        DeviceRole deviceRole;
        DeviceType deviceType;
        SourceType sourceType;
        int32_t capturerFlags;
        AudioCapturerNapi *objectInfo;
        AudioCapturerOptions capturerOptions;
    };

    static void Destructor(napi_env env, void *nativeObject, void *finalize_hint);
    static napi_value Construct(napi_env env, napi_callback_info info);
    static napi_value CreateAudioCapturer(napi_env env, napi_callback_info info);
    static napi_value GetCapturerInfo(napi_env env, napi_callback_info info);
    static napi_value GetStreamInfo(napi_env env, napi_callback_info info);
    static napi_value Start(napi_env env, napi_callback_info info);
    static napi_value Read(napi_env env, napi_callback_info info);
    static napi_value GetAudioTime(napi_env env, napi_callback_info info);
    static napi_value Stop(napi_env env, napi_callback_info info);
    static napi_value Release(napi_env env, napi_callback_info info);
    static napi_value GetBufferSize(napi_env env, napi_callback_info info);
    static napi_value GetAudioStreamId(napi_env env, napi_callback_info info);
    static napi_value On(napi_env env, napi_callback_info info);
    static napi_value Off(napi_env env, napi_callback_info info);
    static napi_value GetState(napi_env env, napi_callback_info info);
    static napi_value CreateAudioCapturerWrapper(napi_env env, std::unique_ptr<AudioCapturerOptions> &captureOptions);

    static void CommonCallbackRoutine(napi_env env, AudioCapturerAsyncContext* &asyncContext,
                                      const napi_value &valueParam);
    static void SetFunctionAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void AudioParamsAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void AudioCapturerInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void AudioStreamInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void ReadAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void IsTrueAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void GetBufferSizeAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void GetAudioStreamIdCallbackComplete(napi_env env, napi_status status, void *data);
    static void GetIntValueAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void GetInt64ValueAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void VoidAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void GetCapturerAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void CheckCapturerAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static napi_status CreateReadAsyncWork(const AudioCapturerAsyncContext &asyncContext);
    static napi_status AddNamedProperty(napi_env env, napi_value object, const std::string name, int32_t enumValue);
    static bool ParseCapturerOptions(napi_env env, napi_value root, AudioCapturerOptions *opts);
    static bool ParseCapturerInfo(napi_env env, napi_value root, AudioCapturerInfo *capturerInfo);
    static bool ParseStreamInfo(napi_env env, napi_value root, AudioStreamInfo* streamInfo);

    static napi_value RegisterCallback(napi_env env, napi_value jsThis,
                                       napi_value* argv, const std::string& cbName);
    static napi_value RegisterCapturerCallback(napi_env env, napi_value* argv,
                                               const std::string& cbName, AudioCapturerNapi *capturerNapi);
    static napi_value RegisterPositionCallback(napi_env env, napi_value* argv,
                                               const std::string& cbName, AudioCapturerNapi *capturerNapi);
    static napi_value RegisterPeriodPositionCallback(napi_env env, napi_value* argv,
                                                     const std::string& cbName, AudioCapturerNapi *capturerNapi);
    static napi_value UnregisterCallback(napi_env env, napi_value jsThis, const std::string& cbName);

    static std::unique_ptr<AudioParameters> sAudioParameters_;
    static std::unique_ptr<AudioCapturerOptions> sCapturerOptions_;
    static std::mutex createMutex_;
    static int32_t isConstructSuccess_;

    std::unique_ptr<AudioCapturer> audioCapturer_;
    ContentType contentType_;
    StreamUsage streamUsage_;
    DeviceRole deviceRole_;
    DeviceType deviceType_;
    SourceType sourceType_;
    int32_t capturerFlags_;
    napi_env env_;
    std::shared_ptr<CapturerPositionCallback> positionCBNapi_ = nullptr;
    std::shared_ptr<CapturerPeriodPositionCallback> periodPositionCBNapi_ = nullptr;
    std::shared_ptr<AudioCapturerCallback> callbackNapi_ = nullptr;
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* AUDIO_CAPTURER_NAPI_H_ */
