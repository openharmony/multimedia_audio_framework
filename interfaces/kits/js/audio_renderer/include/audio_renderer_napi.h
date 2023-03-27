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

#ifndef AUDIO_RENDERER_NAPI_H_
#define AUDIO_RENDERER_NAPI_H_

#include <iostream>
#include <map>
#include <queue>

#include "audio_renderer.h"
#include "audio_errors.h"
#include "audio_system_manager.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
static const std::string AUDIO_RENDERER_NAPI_CLASS_NAME = "AudioRenderer";

static const std::map<std::string, AudioRendererRate> rendererRateMap = {
    {"RENDER_RATE_NORMAL", RENDER_RATE_NORMAL},
    {"RENDER_RATE_DOUBLE", RENDER_RATE_DOUBLE},
    {"RENDER_RATE_HALF", RENDER_RATE_HALF}
};

class AudioRendererNapi {
public:
    AudioRendererNapi();
    ~AudioRendererNapi();

    enum AudioSampleFormat {
        SAMPLE_FORMAT_INVALID = -1,
        SAMPLE_FORMAT_U8 = 0,
        SAMPLE_FORMAT_S16LE = 1,
        SAMPLE_FORMAT_S24LE = 2,
        SAMPLE_FORMAT_S32LE = 3,
        SAMPLE_FORMAT_F32LE = 4
    };
    std::unique_ptr<AudioRenderer> audioRenderer_;
    static napi_value Init(napi_env env, napi_value exports);
private:
    struct AudioRendererAsyncContext {
        napi_env env;
        napi_async_work work;
        napi_deferred deferred;
        napi_ref callbackRef = nullptr;
        int32_t status = SUCCESS;
        int32_t intValue;
        int32_t audioRendererRate;
        int32_t rendererFlags;
        int32_t interruptMode;
        bool isTrue;
        uint64_t time;
        size_t bufferLen;
        size_t bufferSize;
        int32_t volType;
        double volLevel;
        uint32_t audioStreamId;
        size_t totalBytesWritten;
        void *data;
        AudioSampleFormat sampleFormat;
        AudioSamplingRate samplingRate;
        AudioChannel channelCount;
        AudioEncodingType encodingType;
        ContentType contentType;
        StreamUsage usage;
        DeviceRole deviceRole;
        DeviceType deviceType;
        AudioRendererNapi *objectInfo;
        AudioRendererOptions rendererOptions;
    };

    static void Destructor(napi_env env, void *nativeObject, void *finalize_hint);
    static napi_value Construct(napi_env env, napi_callback_info info);
    static napi_value CreateAudioRenderer(napi_env env, napi_callback_info info);
    static napi_value SetRenderRate(napi_env env, napi_callback_info info);
    static napi_value GetRenderRate(napi_env env, napi_callback_info info);
    static napi_value Start(napi_env env, napi_callback_info info);
    static napi_value Write(napi_env env, napi_callback_info info);
    static napi_value GetAudioTime(napi_env env, napi_callback_info info);
    static napi_value Drain(napi_env env, napi_callback_info info);
    static napi_value Pause(napi_env env, napi_callback_info info);
    static napi_value Stop(napi_env env, napi_callback_info info);
    static napi_value Release(napi_env env, napi_callback_info info);
    static napi_value GetBufferSize(napi_env env, napi_callback_info info);
    static napi_value GetAudioStreamId(napi_env env, napi_callback_info info);
    static napi_value SetVolume(napi_env env, napi_callback_info info);
    static napi_value GetState(napi_env env, napi_callback_info info);
    static napi_value GetRendererInfo(napi_env env, napi_callback_info info);
    static napi_value GetStreamInfo(napi_env env, napi_callback_info info);
    static napi_value On(napi_env env, napi_callback_info info);
    static napi_value Off(napi_env env, napi_callback_info info);
    static napi_value CreateAudioRendererWrapper(napi_env env, std::unique_ptr<AudioRendererOptions> &renderOptions);
    static napi_value SetInterruptMode(napi_env env, napi_callback_info info);

    static void JudgeFuncDrain(napi_env &env, napi_value &result,
        std::unique_ptr<AudioRendererAsyncContext> &asyncContext);
    static void JudgeFuncGetAudioStreamId(napi_env &env, napi_value &result,
        std::unique_ptr<AudioRendererAsyncContext> &asyncContext);

    static bool ParseRendererOptions(napi_env env, napi_value root, AudioRendererOptions *opts);
    static bool ParseRendererInfo(napi_env env, napi_value root, AudioRendererInfo *rendererInfo);
    static bool ParseStreamInfo(napi_env env, napi_value root, AudioStreamInfo* streamInfo);
    static bool CheckContextStatus(AudioRendererAsyncContext *context);

    static void CommonCallbackRoutine(napi_env env, AudioRendererAsyncContext* &asyncContext,
                                      const napi_value &valueParam);
    static void SetFunctionAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void AudioParamsAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void IsTrueAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void GetBufferSizeAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void GetAudioStreamIdCallbackComplete(napi_env env, napi_status status, void *data);
    static void GetIntValueAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void GetInt64ValueAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void WriteAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void PauseAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void StartAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void StopAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void AudioRendererInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void AudioStreamInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void GetRendererAsyncCallbackComplete(napi_env env, napi_status status, void *data);
    static void VoidAsyncCallbackComplete(napi_env env, napi_status status, void *data);

    static napi_value RegisterCallback(napi_env env, napi_value jsThis,
                                       napi_value* argv, const std::string& cbName);
    static napi_value RegisterRendererCallback(napi_env env, napi_value* argv,
                                               const std::string& cbName, AudioRendererNapi *rendererNapi);
    static napi_value RegisterPositionCallback(napi_env env, napi_value* argv,
                                               const std::string& cbName, AudioRendererNapi *rendererNapi);
    static napi_value RegisterPeriodPositionCallback(napi_env env, napi_value* argv,
                                                     const std::string& cbName, AudioRendererNapi *rendererNapi);
    static napi_value RegisterDataRequestCallback(napi_env env, napi_value* argv,
                                                     const std::string& cbName, AudioRendererNapi *rendererNapi);
    static napi_value UnregisterCallback(napi_env env, napi_value jsThis, const std::string& cbName);

    static napi_status AddNamedProperty(napi_env env, napi_value object, const std::string name, int32_t enumValue);
    static napi_value CreateAudioRendererRateObject(napi_env env);
    static napi_value CreateInterruptEventTypeObject(napi_env env);
    static napi_value CreateInterruptForceTypeObject(napi_env env);
    static napi_value CreateInterruptHintTypeObject(napi_env env);
    static napi_value CreateAudioStateObject(napi_env env);
    static napi_value CreateAudioSampleFormatObject(napi_env env);

    static napi_ref audioRendererRate_;
    static napi_ref interruptEventType_;
    static napi_ref interruptForceType_;
    static napi_ref interruptHintType_;
    static napi_ref audioState_;
    static napi_ref sampleFormat_;
    static std::unique_ptr<AudioParameters> sAudioParameters_;
    static std::unique_ptr<AudioRendererOptions> sRendererOptions_;
    static std::mutex createMutex_;
    static int32_t isConstructSuccess_;

    ContentType contentType_;
    StreamUsage streamUsage_;
    DeviceRole deviceRole_;
    DeviceType deviceType_;
    int32_t rendererFlags_;
    napi_env env_;
    std::queue<napi_async_work> writeRequestQ_;
    std::atomic<bool> scheduleFromApiCall_;
    std::atomic<bool> doNotScheduleWrite_;
    std::atomic<bool> isDrainWriteQInProgress_;
    std::shared_ptr<AudioRendererCallback> callbackNapi_ = nullptr;
    std::shared_ptr<RendererPositionCallback> positionCBNapi_ = nullptr;
    std::shared_ptr<RendererPeriodPositionCallback> periodPositionCBNapi_ = nullptr;
    std::shared_ptr<AudioRendererWriteCallback> dataRequestCBNapi_ = nullptr;
    static constexpr double MIN_VOLUME_IN_DOUBLE = 0.0;
    static constexpr double MAX_VOLUME_IN_DOUBLE = 1.0;
};

static const std::map<std::string, InterruptType> interruptEventTypeMap = {
    {"INTERRUPT_TYPE_BEGIN", INTERRUPT_TYPE_BEGIN},
    {"INTERRUPT_TYPE_END", INTERRUPT_TYPE_END}
};

static const std::map<std::string, InterruptForceType> interruptForceTypeMap = {
    {"INTERRUPT_FORCE", INTERRUPT_FORCE},
    {"INTERRUPT_SHARE", INTERRUPT_SHARE},
};

static const std::map<std::string, InterruptHint> interruptHintTypeMap = {
    {"INTERRUPT_HINT_NONE", INTERRUPT_HINT_NONE},
    {"INTERRUPT_HINT_PAUSE", INTERRUPT_HINT_PAUSE},
    {"INTERRUPT_HINT_RESUME", INTERRUPT_HINT_RESUME},
    {"INTERRUPT_HINT_STOP", INTERRUPT_HINT_STOP},
    {"INTERRUPT_HINT_DUCK", INTERRUPT_HINT_DUCK},
    {"INTERRUPT_HINT_UNDUCK", INTERRUPT_HINT_UNDUCK}
};

static const std::map<std::string, RendererState> audioStateMap = {
    {"STATE_INVALID", RENDERER_INVALID},
    {"STATE_NEW", RENDERER_NEW},
    {"STATE_PREPARED", RENDERER_PREPARED},
    {"STATE_RUNNING", RENDERER_RUNNING},
    {"STATE_STOPPED", RENDERER_STOPPED},
    {"STATE_RELEASED", RENDERER_RELEASED},
    {"STATE_PAUSED", RENDERER_PAUSED}
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* AUDIO_RENDERER_NAPI_H_ */
