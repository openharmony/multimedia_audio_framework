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

#include "audio_capturer_napi.h"
#include "ability.h"
#include "audio_capturer_callback_napi.h"
#include "audio_errors.h"
#include "audio_log.h"
#include "audio_manager_napi.h"
#include "audio_parameters_napi.h"
#include "capturer_period_position_callback_napi.h"
#include "capturer_position_callback_napi.h"

#include "hilog/log.h"
#include "napi_base_context.h"
#include "securec.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
static __thread napi_ref g_capturerConstructor = nullptr;
std::unique_ptr<AudioParameters> AudioCapturerNapi::sAudioParameters_ = nullptr;
std::unique_ptr<AudioCapturerOptions> AudioCapturerNapi::sCapturerOptions_ = nullptr;
mutex AudioCapturerNapi::createMutex_;
int32_t AudioCapturerNapi::isConstructSuccess_ = SUCCESS;

namespace {
    constexpr int ARGS_ONE = 1;
    constexpr int ARGS_TWO = 2;
    constexpr int ARGS_THREE = 3;

    constexpr int PARAM0 = 0;
    constexpr int PARAM1 = 1;
    constexpr int PARAM2 = 2;

    constexpr int TYPE_COMMUNICATION = 7;
    constexpr int TYPE_VOICE_RECOGNITION = 1;
    constexpr int TYPE_MIC = 0;
    constexpr int TYPE_INVALID = -1;

    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "AudioCapturerNapi"};

    const std::string MARK_REACH_CALLBACK_NAME = "markReach";
    const std::string PERIOD_REACH_CALLBACK_NAME = "periodReach";
#define GET_PARAMS(env, info, num) \
    size_t argc = num;             \
    napi_value argv[num] = {0};    \
    napi_value thisVar = nullptr;  \
    void *data;                    \
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data)
}

AudioCapturerNapi::AudioCapturerNapi()
    : audioCapturer_(nullptr), contentType_(CONTENT_TYPE_MUSIC), streamUsage_(STREAM_USAGE_MEDIA),
      deviceRole_(INPUT_DEVICE), deviceType_(DEVICE_TYPE_MIC), sourceType_(SOURCE_TYPE_MIC),
      capturerFlags_(0), env_(nullptr) {}

AudioCapturerNapi::~AudioCapturerNapi() = default;

void AudioCapturerNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<AudioCapturerNapi *>(nativeObject);
        delete obj;
    }
}

napi_value AudioCapturerNapi::Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    const int32_t refCount = 1;
    napi_get_undefined(env, &result);

    napi_property_descriptor audio_capturer_properties[] = {
        DECLARE_NAPI_FUNCTION("getCapturerInfo", GetCapturerInfo),
        DECLARE_NAPI_FUNCTION("getStreamInfo", GetStreamInfo),
        DECLARE_NAPI_FUNCTION("start", Start),
        DECLARE_NAPI_FUNCTION("read", Read),
        DECLARE_NAPI_FUNCTION("getAudioTime", GetAudioTime),
        DECLARE_NAPI_FUNCTION("stop", Stop),
        DECLARE_NAPI_FUNCTION("release", Release),
        DECLARE_NAPI_FUNCTION("getBufferSize", GetBufferSize),
        DECLARE_NAPI_FUNCTION("getAudioStreamId", GetAudioStreamId),
        DECLARE_NAPI_FUNCTION("on", On),
        DECLARE_NAPI_FUNCTION("off", Off),
        DECLARE_NAPI_GETTER("state", GetState)
    };

    napi_property_descriptor static_prop[] = {
        DECLARE_NAPI_STATIC_FUNCTION("createAudioCapturer", CreateAudioCapturer)
    };

    status = napi_define_class(env, AUDIO_CAPTURER_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct, nullptr,
        sizeof(audio_capturer_properties) / sizeof(audio_capturer_properties[PARAM0]),
        audio_capturer_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }

    status = napi_create_reference(env, constructor, refCount, &g_capturerConstructor);
    if (status == napi_ok) {
        status = napi_set_named_property(env, exports, AUDIO_CAPTURER_NAPI_CLASS_NAME.c_str(), constructor);
        if (status == napi_ok) {
            status = napi_define_properties(env, exports,
                                            sizeof(static_prop) / sizeof(static_prop[PARAM0]), static_prop);
            if (status == napi_ok) {
                return exports;
            }
        }
    }

    HiLog::Error(LABEL, "Failure in AudioCapturerNapi::Init()");

    return result;
}

napi_status AudioCapturerNapi::AddNamedProperty(napi_env env, napi_value object,
                                                const std::string name, int32_t enumValue)
{
    napi_status status;
    napi_value enumNapiValue;

    status = napi_create_int32(env, enumValue, &enumNapiValue);
    if (status == napi_ok) {
        status = napi_set_named_property(env, object, name.c_str(), enumNapiValue);
    }

    return status;
}

static void SetValueInt32(const napi_env& env, const std::string& fieldStr, const int intValue, napi_value& result)
{
    napi_value value = nullptr;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static shared_ptr<AbilityRuntime::Context> GetAbilityContext(napi_env env)
{
    HiLog::Info(LABEL, "Getting context with FA model");
    auto ability = OHOS::AbilityRuntime::GetCurrentAbility(env);
    if (ability == nullptr) {
        HiLog::Error(LABEL, "Failed to obtain ability in FA mode");
        return nullptr;
    }

    auto faContext = ability->GetAbilityContext();
    if (faContext == nullptr) {
        HiLog::Error(LABEL, "GetAbilityContext returned null in FA model");
        return nullptr;
    }

    return faContext;
}

napi_value AudioCapturerNapi::Construct(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<AudioCapturerNapi> capturerNapi = make_unique<AudioCapturerNapi>();
    CHECK_AND_RETURN_RET_LOG(capturerNapi != nullptr, result, "No memory");
    capturerNapi->env_ = env;
    capturerNapi->sourceType_ = sCapturerOptions_->capturerInfo.sourceType;
    capturerNapi->capturerFlags_ = sCapturerOptions_->capturerInfo.capturerFlags;

    AudioCapturerOptions capturerOptions = {};
    capturerOptions.streamInfo.samplingRate = sCapturerOptions_->streamInfo.samplingRate;
    capturerOptions.streamInfo.encoding = sCapturerOptions_->streamInfo.encoding;
    capturerOptions.streamInfo.format = sCapturerOptions_->streamInfo.format;
    capturerOptions.streamInfo.channels = sCapturerOptions_->streamInfo.channels;

    capturerOptions.capturerInfo.sourceType = sCapturerOptions_->capturerInfo.sourceType;
    capturerOptions.capturerInfo.capturerFlags = sCapturerOptions_->capturerInfo.capturerFlags;

    std::shared_ptr<AbilityRuntime::Context> abilityContext = GetAbilityContext(env);
    std::string cacheDir = "";
    if (abilityContext != nullptr) {
        cacheDir = abilityContext->GetCacheDir();
    } else {
        cacheDir = "/data/storage/el2/base/haps/entry/files";
    }
    capturerNapi->audioCapturer_ = AudioCapturer::Create(capturerOptions, cacheDir);

    if (capturerNapi->audioCapturer_ == nullptr) {
        HiLog::Error(LABEL, "Capturer Create failed");
        AudioCapturerNapi::isConstructSuccess_ = NAPI_ERR_SYSTEM;
    }

    if (capturerNapi->audioCapturer_ != nullptr && capturerNapi->callbackNapi_ == nullptr) {
        capturerNapi->callbackNapi_ = std::make_shared<AudioCapturerCallbackNapi>(env);
        CHECK_AND_RETURN_RET_LOG(capturerNapi->callbackNapi_ != nullptr, result, "No memory");
        int32_t ret = capturerNapi->audioCapturer_->SetCapturerCallback(capturerNapi->callbackNapi_);
        if (ret) {
            AUDIO_DEBUG_LOG("AudioCapturerNapi::Construct SetCapturerCallback failed");
        }
    }

    status = napi_wrap(env, thisVar, static_cast<void*>(capturerNapi.get()),
                       AudioCapturerNapi::Destructor, nullptr, nullptr);
    if (status == napi_ok) {
        capturerNapi.release();
        return thisVar;
    }

    HiLog::Error(LABEL, "Failed in AudioCapturerNapi::Construct()!");
    return result;
}

napi_value AudioCapturerNapi::CreateAudioCapturer(napi_env env, napi_callback_info info)
{
    HiLog::Info(LABEL, "%{public}s IN", __func__);
    napi_status status;
    napi_value result = nullptr;
    bool inputRight = true;

    GET_PARAMS(env, info, ARGS_TWO);
    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    if (asyncContext == nullptr) {
        return result;
    }

    if (argc < ARGS_ONE) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }

    for (size_t i = PARAM0; i < argc; i++) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[i], &valueType);
        if (i == PARAM0 && valueType == napi_object) {
            if (ParseCapturerOptions(env, argv[i], &(asyncContext->capturerOptions)) == false) {
                HiLog::Error(LABEL, "Parsing of capturer options failed");
                inputRight = false;
            }
        } else if (i == PARAM1) {
            if (valueType == napi_function) {
                napi_create_reference(env, argv[i], REFERENCE_CREATION_COUNT, &asyncContext->callbackRef);
            }
            break;
        } else {
            asyncContext->status = NAPI_ERR_INVALID_PARAM;
        }
    }

    if (asyncContext->callbackRef == nullptr) {
        napi_create_promise(env, &asyncContext->deferred, &result);
    } else {
        napi_get_undefined(env, &result);
    }

    napi_value resource = nullptr;
    napi_create_string_utf8(env, "CreateAudioCapturer", NAPI_AUTO_LENGTH, &resource);

    if (inputRight == false){
        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->status = NAPI_ERR_INVALID_PARAM;
                HiLog::Error(LABEL, "ParseCapturerOptions fail, invalid param!");
            },
            CheckCapturerAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
    } else {
        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->status = SUCCESS;
            },
            GetCapturerAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
    }

    if (status != napi_ok) {
        result = nullptr;
    } else {
        status = napi_queue_async_work(env, asyncContext->work);
        if (status == napi_ok) {
            asyncContext.release();
        } else {
            result = nullptr;
        }
    }

    return result;
}

void AudioCapturerNapi::CommonCallbackRoutine(napi_env env, AudioCapturerAsyncContext* &asyncContext,
                                              const napi_value &valueParam)
{
    napi_value result[ARGS_TWO] = {0};
    napi_value retVal;

    if (!asyncContext->status) {
        napi_get_undefined(env, &result[PARAM0]);
        result[PARAM1] = valueParam;
    } else {
        napi_value message = nullptr;
        std::string messageValue = AudioCommonNapi::getMessageByCode(asyncContext->status);
        napi_create_string_utf8(env, messageValue.c_str(), NAPI_AUTO_LENGTH, &message);

        napi_value code = nullptr;
        napi_create_string_utf8(env, (std::to_string(asyncContext->status)).c_str(), NAPI_AUTO_LENGTH, &code);

        napi_create_error(env, code, message, &result[PARAM0]);
        napi_get_undefined(env, &result[PARAM1]);
    }

    if (asyncContext->deferred) {
        if (!asyncContext->status) {
            napi_resolve_deferred(env, asyncContext->deferred, result[PARAM1]);
        } else {
            napi_reject_deferred(env, asyncContext->deferred, result[PARAM0]);
        }
    } else {
        napi_value callback = nullptr;
        napi_get_reference_value(env, asyncContext->callbackRef, &callback);
        napi_call_function(env, nullptr, callback, ARGS_TWO, result, &retVal);
        napi_delete_reference(env, asyncContext->callbackRef);
    }
    napi_delete_async_work(env, asyncContext->work);

    delete asyncContext;
    asyncContext = nullptr;
}

void AudioCapturerNapi::GetCapturerAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    napi_value valueParam = nullptr;
    auto asyncContext = static_cast<AudioCapturerAsyncContext *>(data);

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            unique_ptr<AudioCapturerOptions> capturerOptions = make_unique<AudioCapturerOptions>();
            capturerOptions->streamInfo.samplingRate = asyncContext->capturerOptions.streamInfo.samplingRate;
            capturerOptions->streamInfo.encoding = asyncContext->capturerOptions.streamInfo.encoding;
            capturerOptions->streamInfo.format = asyncContext->capturerOptions.streamInfo.format;
            capturerOptions->streamInfo.channels = asyncContext->capturerOptions.streamInfo.channels;
            capturerOptions->capturerInfo.sourceType = asyncContext->capturerOptions.capturerInfo.sourceType;
            capturerOptions->capturerInfo.capturerFlags = asyncContext->capturerOptions.capturerInfo.capturerFlags;

            valueParam = CreateAudioCapturerWrapper(env, capturerOptions);
            asyncContext->status = AudioCapturerNapi::isConstructSuccess_;
            AudioCapturerNapi::isConstructSuccess_ = SUCCESS;
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: GetCapturerAsyncCallbackComplete is Null!");
    }
}

void AudioCapturerNapi::CheckCapturerAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    napi_value valueParam = nullptr;
    auto asyncContext = static_cast<AudioCapturerAsyncContext *>(data);
    if (asyncContext != nullptr) {
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: CheckCapturerAsyncCallbackComplete is Null!");
    }
}

void AudioCapturerNapi::SetFunctionAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::AudioCapturerInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            (void)napi_create_object(env, &valueParam);
            SetValueInt32(env, "source", static_cast<int32_t>(asyncContext->sourceType), valueParam);
            SetValueInt32(env, "capturerFlags", static_cast<int32_t>(asyncContext->capturerFlags), valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerInfoAsyncCallbackComplete* is Null!");
    }
}

void AudioCapturerNapi::AudioStreamInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            (void)napi_create_object(env, &valueParam);
            SetValueInt32(env, "samplingRate", static_cast<int32_t>(asyncContext->samplingRate), valueParam);
            SetValueInt32(env, "channels", static_cast<int32_t>(asyncContext->audioChannel), valueParam);
            SetValueInt32(env, "sampleFormat", static_cast<int32_t>(asyncContext->audioSampleFormat), valueParam);
            SetValueInt32(env, "encodingType", static_cast<int32_t>(asyncContext->audioEncoding), valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::ReadAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            uint8_t *native = nullptr;
            napi_create_arraybuffer(env, asyncContext->bytesRead, reinterpret_cast<void **>(&native), &valueParam);
            if (memcpy_s(native, asyncContext->bytesRead, asyncContext->buffer, asyncContext->bytesRead)) {
                valueParam = nullptr;
            }

            free(asyncContext->buffer);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::VoidAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::IsTrueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_boolean(env, asyncContext->isTrue, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::GetBufferSizeAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_create_uint32(env, asyncContext->bufferSize, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::GetAudioStreamIdCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_create_uint32(env, asyncContext->audioStreamId, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR::GetAudioStreamIdCallbackComplete* is Null!");
    }
}

void AudioCapturerNapi::GetIntValueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_create_int32(env, asyncContext->intValue, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

void AudioCapturerNapi::GetInt64ValueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioCapturerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_create_int64(env, asyncContext->time, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioCapturerAsyncContext* is Null!");
    }
}

napi_value AudioCapturerNapi::GetCapturerInfo(napi_env env, napi_callback_info info)
{
    HiLog::Info(LABEL, "%{public}s IN", __func__);
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();
    THROW_ERROR_ASSERT(env, asyncContext != nullptr, NAPI_ERR_NO_MEMORY);
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc > PARAM0) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetCapturerInfo", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                AudioCapturerInfo capturerInfo = {};
                context->status = context->objectInfo->audioCapturer_->GetCapturerInfo(capturerInfo);
                if (context->status == SUCCESS) {
                    context->sourceType = capturerInfo.sourceType;
                    context->capturerFlags = capturerInfo.capturerFlags;
                }
            },
            AudioCapturerInfoAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::GetStreamInfo(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc > PARAM0) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetStreamInfo", NAPI_AUTO_LENGTH, &resource);
        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);

                AudioStreamInfo streamInfo;
                context->status = context->objectInfo->audioCapturer_->GetStreamInfo(streamInfo);
                if (context->status == SUCCESS) {
                    context->audioSampleFormat = static_cast<AudioSampleFormat>(streamInfo.format);
                    context->samplingRate = streamInfo.samplingRate;
                    context->audioChannel = streamInfo.channels;
                    context->audioEncoding = streamInfo.encoding;
                }
            },
            AudioStreamInfoAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::GetAudioStreamId(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();
    THROW_ERROR_ASSERT(env, asyncContext != nullptr, NAPI_ERR_NO_MEMORY);

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc > PARAM0) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetAudioStreamId", NAPI_AUTO_LENGTH, &resource);
        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                int32_t streamIdStatus;
                streamIdStatus = context->objectInfo->audioCapturer_->
                    GetAudioStreamId(context->audioStreamId);
                if (streamIdStatus == ERR_ILLEGAL_STATE) {
                    context->status = NAPI_ERR_ILLEGAL_STATE;
                } else if (streamIdStatus == ERR_INVALID_INDEX) {
                    context->status = NAPI_ERR_SYSTEM;
                } else {
                    context->status = SUCCESS;
                }
            },
            GetAudioStreamIdCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }
    return result;
}

napi_value AudioCapturerNapi::Start(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();
    THROW_ERROR_ASSERT(env, asyncContext != nullptr, NAPI_ERR_NO_MEMORY);

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc > PARAM0) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "Start", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->isTrue = context->objectInfo->audioCapturer_->Start();
                if (context->isTrue) {
                    context->status = SUCCESS;
                } else {
                    context->status = NAPI_ERR_SYSTEM;
                }
            },
            VoidAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::Read(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_THREE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();
    if (argc < ARGS_TWO) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }

    status = napi_unwrap(env, thisVar, reinterpret_cast<void **>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if ((i == PARAM0) && (valueType == napi_number)) {
                napi_get_value_uint32(env, argv[i], &asyncContext->userSize);
            } else if ((i == PARAM1) && (valueType == napi_boolean)) {
                napi_get_value_bool(env, argv[i], &asyncContext->isBlocking);
            } else if (i == PARAM2) {
                if (valueType == napi_function) {
                    napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                }
                break;
            } else {
                asyncContext->status = NAPI_ERR_INVALID_PARAM;
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "Read", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                if (context->status == SUCCESS) {
                    context->status = NAPI_ERR_SYSTEM;
                    uint32_t userSize = context->userSize;
                    auto buffer = std::make_unique<uint8_t[]>(userSize);
                    if (!buffer) {
                        return;
                    }

                    size_t bytesRead = 0;
                    while (bytesRead < context->userSize) {
                        int32_t len = context->objectInfo->audioCapturer_->Read(*(buffer.get() + bytesRead),
                                                                                userSize - bytesRead,
                                                                                context->isBlocking);
                        if (len >= 0) {
                            bytesRead += len;
                        } else {
                            bytesRead = len;
                            break;
                        }
                    }

                    if (bytesRead > 0) {
                        context->bytesRead = bytesRead;
                        context->buffer = buffer.get();
                        buffer.release();
                        context->status = SUCCESS;
                    }
                }
            },
            ReadAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::GetAudioTime(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc > PARAM0) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetAudioTime", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->status = NAPI_ERR_SYSTEM;
                Timestamp timestamp;
                if (context->objectInfo->audioCapturer_->GetAudioTime(timestamp, Timestamp::Timestampbase::MONOTONIC)) {
                    const uint64_t secToNanosecond = 1000000000;
                    context->time = timestamp.time.tv_nsec + timestamp.time.tv_sec * secToNanosecond;
                    context->status = SUCCESS;
                }
            },
            GetInt64ValueAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::Stop(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc > PARAM0) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "Stop", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->isTrue = context->objectInfo->audioCapturer_->Stop();
                if (context->isTrue) {
                    context->status = SUCCESS;
                } else {
                    context->status = NAPI_ERR_SYSTEM;
                }
            },
            VoidAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::Release(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc > PARAM0) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "Release", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                context->isTrue = context->objectInfo->audioCapturer_->Release();
                if (context->isTrue) {
                    context->status = SUCCESS;
                } else {
                    context->status = NAPI_ERR_SYSTEM;
                }
            },
            VoidAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::RegisterPeriodPositionCallback(napi_env env, napi_value* argv, const std::string& cbName,
                                                             AudioCapturerNapi *capturerNapi)
{
    int64_t frameCount = 0;
    napi_get_value_int64(env, argv[PARAM1], &frameCount);

    if (frameCount > 0) {
        if (capturerNapi->periodPositionCBNapi_ == nullptr) {
            capturerNapi->periodPositionCBNapi_ = std::make_shared<CapturerPeriodPositionCallbackNapi>(env);
            THROW_ERROR_ASSERT(env, capturerNapi->periodPositionCBNapi_ != nullptr, NAPI_ERR_NO_MEMORY);

            int32_t ret = capturerNapi->audioCapturer_->SetCapturerPeriodPositionCallback(frameCount,
                capturerNapi->periodPositionCBNapi_);
            THROW_ERROR_ASSERT(env, ret == SUCCESS, NAPI_ERR_SYSTEM);

            std::shared_ptr<CapturerPeriodPositionCallbackNapi> cb =
                std::static_pointer_cast<CapturerPeriodPositionCallbackNapi>(capturerNapi->periodPositionCBNapi_);
            cb->SaveCallbackReference(cbName, argv[PARAM2]);
        } else {
            AUDIO_DEBUG_LOG("AudioCapturerNapi: periodReach already subscribed.");
            THROW_ERROR_ASSERT(env, false, NAPI_ERR_ILLEGAL_STATE);
        }
    } else {
        AUDIO_ERR_LOG("AudioCapturerNapi: frameCount value not supported!");
        THROW_ERROR_ASSERT(env, false, NAPI_ERR_INPUT_INVALID);
    }

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value AudioCapturerNapi::RegisterPositionCallback(napi_env env, napi_value* argv,
                                                       const std::string& cbName, AudioCapturerNapi *capturerNapi)
{
    int64_t markPosition = 0;
    napi_get_value_int64(env, argv[PARAM1], &markPosition);

    if (markPosition > 0) {
        capturerNapi->positionCBNapi_ = std::make_shared<CapturerPositionCallbackNapi>(env);
        THROW_ERROR_ASSERT(env, capturerNapi->positionCBNapi_ != nullptr, NAPI_ERR_NO_MEMORY);
        int32_t ret = capturerNapi->audioCapturer_->SetCapturerPositionCallback(markPosition,
            capturerNapi->positionCBNapi_);
        THROW_ERROR_ASSERT(env, ret == SUCCESS, NAPI_ERR_SYSTEM);

        std::shared_ptr<CapturerPositionCallbackNapi> cb =
            std::static_pointer_cast<CapturerPositionCallbackNapi>(capturerNapi->positionCBNapi_);
        cb->SaveCallbackReference(cbName, argv[PARAM2]);
    } else {
        AUDIO_ERR_LOG("AudioCapturerNapi: Mark Position value not supported!!");
        THROW_ERROR_ASSERT(env, false, NAPI_ERR_INPUT_INVALID);
    }

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value AudioCapturerNapi::RegisterCapturerCallback(napi_env env, napi_value* argv,
                                                       const std::string& cbName, AudioCapturerNapi *capturerNapi)
{
    THROW_ERROR_ASSERT(env, capturerNapi->callbackNapi_ != nullptr, NAPI_ERR_NO_MEMORY);

    std::shared_ptr<AudioCapturerCallbackNapi> cb =
        std::static_pointer_cast<AudioCapturerCallbackNapi>(capturerNapi->callbackNapi_);
    cb->SaveCallbackReference(cbName, argv[PARAM1]);

    if (!cbName.compare(STATE_CHANGE_CALLBACK_NAME)) {
        CapturerState state = capturerNapi->audioCapturer_->GetStatus();
        if (state == CAPTURER_PREPARED) {
            capturerNapi->callbackNapi_->OnStateChange(state);
        }
    }

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value AudioCapturerNapi::RegisterCallback(napi_env env, napi_value jsThis,
                                               napi_value* argv, const std::string& cbName)
{
    AudioCapturerNapi *capturerNapi = nullptr;
    napi_status status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&capturerNapi));
    THROW_ERROR_ASSERT(env, status == napi_ok, NAPI_ERR_SYSTEM);
    THROW_ERROR_ASSERT(env, capturerNapi != nullptr, NAPI_ERR_NO_MEMORY);
    THROW_ERROR_ASSERT(env, capturerNapi->audioCapturer_ != nullptr, NAPI_ERR_NO_MEMORY);

    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    if (!cbName.compare(STATE_CHANGE_CALLBACK_NAME)) {
        result = RegisterCapturerCallback(env, argv, cbName, capturerNapi);
    } else if (!cbName.compare(MARK_REACH_CALLBACK_NAME)) {
        result = RegisterPositionCallback(env, argv, cbName, capturerNapi);
    } else if (!cbName.compare(PERIOD_REACH_CALLBACK_NAME)) {
        result = RegisterPeriodPositionCallback(env, argv, cbName, capturerNapi);
    } else {
        bool unknownCallback = true;
        THROW_ERROR_ASSERT(env, !unknownCallback, NAPI_ERROR_INVALID_PARAM);
    }

    return result;
}

napi_value AudioCapturerNapi::On(napi_env env, napi_callback_info info)
{
    const size_t requireArgc = 2;
    size_t argc = 3;

    napi_value argv[requireArgc + 1] = {nullptr, nullptr, nullptr};
    napi_value jsThis = nullptr;
    napi_status status = napi_get_cb_info(env, info, &argc, argv, &jsThis, nullptr);
    THROW_ERROR_ASSERT(env, status == napi_ok, NAPI_ERR_SYSTEM);
    THROW_ERROR_ASSERT(env, argc >= requireArgc, NAPI_ERR_INPUT_INVALID);

    napi_valuetype eventType = napi_undefined;
    napi_typeof(env, argv[0], &eventType);
    THROW_ERROR_ASSERT(env, eventType == napi_string, NAPI_ERR_INPUT_INVALID);

    std::string callbackName = AudioCommonNapi::GetStringArgument(env, argv[0]);
    AUDIO_DEBUG_LOG("AudioCapturerNapi: On callbackName: %{public}s", callbackName.c_str());

    napi_valuetype handler = napi_undefined;
    if (argc == requireArgc) {
        napi_typeof(env, argv[1], &handler);
        THROW_ERROR_ASSERT(env, handler == napi_function, NAPI_ERR_INPUT_INVALID);
    } else {
        napi_valuetype paramArg1 = napi_undefined;
        napi_typeof(env, argv[1], &paramArg1);
        napi_valuetype expectedValType = napi_number;  // Default. Reset it with 'callbackName' if check, if required.
        THROW_ERROR_ASSERT(env, paramArg1 == expectedValType, NAPI_ERR_INPUT_INVALID);

        const int32_t arg2 = 2;
        napi_typeof(env, argv[arg2], &handler);
        THROW_ERROR_ASSERT(env, handler == napi_function, NAPI_ERR_INPUT_INVALID);
    }

    return RegisterCallback(env, jsThis, argv, callbackName);
}

napi_value AudioCapturerNapi::UnregisterCallback(napi_env env, napi_value jsThis, const std::string& cbName)
{
    AudioCapturerNapi *capturerNapi = nullptr;
    napi_status status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&capturerNapi));
    THROW_ERROR_ASSERT(env, status == napi_ok, NAPI_ERR_SYSTEM);
    THROW_ERROR_ASSERT(env, capturerNapi != nullptr, NAPI_ERR_NO_MEMORY);
    THROW_ERROR_ASSERT(env, capturerNapi->audioCapturer_ != nullptr, NAPI_ERR_NO_MEMORY);

    if (!cbName.compare(MARK_REACH_CALLBACK_NAME)) {
        capturerNapi->audioCapturer_->UnsetCapturerPositionCallback();
        capturerNapi->positionCBNapi_ = nullptr;
    } else if (!cbName.compare(PERIOD_REACH_CALLBACK_NAME)) {
        capturerNapi->audioCapturer_->UnsetCapturerPeriodPositionCallback();
        capturerNapi->periodPositionCBNapi_ = nullptr;
    } else {
        bool unknownCallback = true;
        THROW_ERROR_ASSERT(env, !unknownCallback, NAPI_ERR_UNSUPPORTED);
    }

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value AudioCapturerNapi::Off(napi_env env, napi_callback_info info)
{
    const size_t requireArgc = 1;
    size_t argc = 1;

    napi_value argv[requireArgc] = {nullptr};
    napi_value jsThis = nullptr;
    napi_status status = napi_get_cb_info(env, info, &argc, argv, &jsThis, nullptr);
    THROW_ERROR_ASSERT(env, status == napi_ok, NAPI_ERR_SYSTEM);
    THROW_ERROR_ASSERT(env, argc >= requireArgc, NAPI_ERR_INVALID_PARAM);

    napi_valuetype eventType = napi_undefined;
    napi_typeof(env, argv[0], &eventType);
    THROW_ERROR_ASSERT(env, eventType == napi_string, NAPI_ERR_INVALID_PARAM);

    std::string callbackName = AudioCommonNapi::GetStringArgument(env, argv[0]);
    AUDIO_DEBUG_LOG("AudioCapturerNapi: Off callbackName: %{public}s", callbackName.c_str());

    return UnregisterCallback(env, jsThis, callbackName);
}

napi_value AudioCapturerNapi::GetBufferSize(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioCapturerAsyncContext> asyncContext = make_unique<AudioCapturerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc > PARAM0) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetBufferSize", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioCapturerAsyncContext *>(data);
                size_t bufferSize;
                context->status = context->objectInfo->audioCapturer_->GetBufferSize(bufferSize);
                if (context->status == SUCCESS) {
                    context->bufferSize = bufferSize;
                }
            },
            GetBufferSizeAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioCapturerNapi::GetState(napi_env env, napi_callback_info info)
{
    napi_value jsThis = nullptr;
    napi_value undefinedResult = nullptr;
    napi_get_undefined(env, &undefinedResult);

    size_t argCount = 0;
    napi_status status = napi_get_cb_info(env, info, &argCount, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Failed to retrieve details about the callback");
        return undefinedResult;
    }

    AudioCapturerNapi *capturerNapi = nullptr;
    status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&capturerNapi));
    if (status != napi_ok || capturerNapi == nullptr) {
        HiLog::Error(LABEL, "Failed to get instance");
        return undefinedResult;
    }

    if (capturerNapi->audioCapturer_ == nullptr) {
        HiLog::Error(LABEL, "No memory");
        return undefinedResult;
    }

    int32_t capturerState = capturerNapi->audioCapturer_->GetStatus();

    napi_value jsResult = nullptr;
    status = napi_create_int32(env, capturerState, &jsResult);
    if (status != napi_ok) {
        HiLog::Error(LABEL, "napi_create_int32 error");
        return undefinedResult;
    }

    HiLog::Info(LABEL, "AudioCapturerNapi: GetState Complete, Current state: %{public}d", capturerState);
    return jsResult;
}

bool AudioCapturerNapi::ParseCapturerOptions(napi_env env, napi_value root, AudioCapturerOptions *opts)
{
    napi_value res = nullptr;
    bool result = false;

    if (napi_get_named_property(env, root, "streamInfo", &res) == napi_ok) {
        result = ParseStreamInfo(env, res, &(opts->streamInfo));
    }

    if (result == false) {
        return result;
    }

    if (napi_get_named_property(env, root, "capturerInfo", &res) == napi_ok) {
        result = ParseCapturerInfo(env, res, &(opts->capturerInfo));
    }

    return result;
}

bool AudioCapturerNapi::ParseCapturerInfo(napi_env env, napi_value root, AudioCapturerInfo *capturerInfo)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};

    if (napi_get_named_property(env, root, "source", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        switch (intValue) {
            case TYPE_INVALID:
            case TYPE_MIC:
            case TYPE_VOICE_RECOGNITION:
            case TYPE_COMMUNICATION:
                capturerInfo->sourceType = static_cast<SourceType>(intValue);
                break;
            default:
                HiLog::Error(LABEL, "Unknown SourceType: %{public}d", intValue);
                return false;
        }
    }

    if (napi_get_named_property(env, root, "capturerFlags", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &(capturerInfo->capturerFlags));
    }

    return true;
}

bool AudioCapturerNapi::ParseStreamInfo(napi_env env, napi_value root, AudioStreamInfo* streamInfo)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};

    if (napi_get_named_property(env, root, "samplingRate", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        if (intValue >= SAMPLE_RATE_8000 && intValue <= SAMPLE_RATE_96000) {
            streamInfo->samplingRate = static_cast<AudioSamplingRate>(intValue);
        } else {
            HiLog::Error(LABEL, "Unknown AudioSamplingRate: %{public}d", intValue);
            return false;
        }
    }

    if (napi_get_named_property(env, root, "channels", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        streamInfo->channels = static_cast<AudioChannel>(intValue);
    }

    if (napi_get_named_property(env, root, "sampleFormat", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        streamInfo->format = static_cast<OHOS::AudioStandard::AudioSampleFormat>(intValue);
    }

    if (napi_get_named_property(env, root, "encodingType", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        streamInfo->encoding = static_cast<AudioEncodingType>(intValue);
    }

    return true;
}

napi_value AudioCapturerNapi::CreateAudioCapturerWrapper(napi_env env, unique_ptr<AudioCapturerOptions> &captureOptions)
{
    lock_guard<mutex> lock(createMutex_);
    napi_status status;
    napi_value result = nullptr;
    napi_value constructor;

    if (captureOptions != nullptr) {
        status = napi_get_reference_value(env, g_capturerConstructor, &constructor);
        if (status == napi_ok) {
            sCapturerOptions_ = move(captureOptions);
            status = napi_new_instance(env, constructor, 0, nullptr, &result);
            sCapturerOptions_.release();
            if (status == napi_ok) {
                return result;
            }
        }
        HiLog::Error(LABEL, "Failed in CreateAudioCapturerWrapper, %{public}d", status);
    }

    napi_get_undefined(env, &result);

    return result;
}
}  // namespace AudioStandard
}  // namespace OHOS
