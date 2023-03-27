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

#include "audio_renderer_napi.h"
#include "ability.h"
#include "audio_renderer_callback_napi.h"
#include "renderer_data_request_callback_napi.h"
#include "renderer_period_position_callback_napi.h"
#include "renderer_position_callback_napi.h"

#include "audio_common_napi.h"
#include "audio_errors.h"
#include "audio_log.h"
#include "audio_manager_napi.h"
#include "audio_parameters_napi.h"
#include "hilog/log.h"
#include "napi_base_context.h"
#include "securec.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
static __thread napi_ref g_rendererConstructor = nullptr;
std::unique_ptr<AudioParameters> AudioRendererNapi::sAudioParameters_ = nullptr;
std::unique_ptr<AudioRendererOptions> AudioRendererNapi::sRendererOptions_ = nullptr;
napi_ref AudioRendererNapi::audioRendererRate_ = nullptr;
napi_ref AudioRendererNapi::interruptEventType_ = nullptr;
napi_ref AudioRendererNapi::interruptHintType_ = nullptr;
napi_ref AudioRendererNapi::interruptForceType_ = nullptr;
napi_ref AudioRendererNapi::audioState_ = nullptr;
napi_ref AudioRendererNapi::sampleFormat_ = nullptr;
mutex AudioRendererNapi::createMutex_;
int32_t AudioRendererNapi::isConstructSuccess_ = SUCCESS;

namespace {
    const int ARGS_ONE = 1;
    const int ARGS_TWO = 2;
    const int ARGS_THREE = 3;

    const int PARAM0 = 0;
    const int PARAM1 = 1;
    const int PARAM2 = 2;

    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "AudioRendererNapi"};

    const std::string MARK_REACH_CALLBACK_NAME = "markReach";
    const std::string PERIOD_REACH_CALLBACK_NAME = "periodReach";
    const std::string DATA_REQUEST_CALLBACK_NAME = "dataRequest";

#define GET_PARAMS(env, info, num) \
    size_t argc = num;             \
    napi_value argv[num] = {0};    \
    napi_value thisVar = nullptr;  \
    void *data;                    \
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data)
}

AudioRendererNapi::AudioRendererNapi()
    : audioRenderer_(nullptr), contentType_(CONTENT_TYPE_MUSIC), streamUsage_(STREAM_USAGE_MEDIA),
      deviceRole_(OUTPUT_DEVICE), deviceType_(DEVICE_TYPE_SPEAKER), env_(nullptr),
      scheduleFromApiCall_(true), doNotScheduleWrite_(false), isDrainWriteQInProgress_(false) {}

AudioRendererNapi::~AudioRendererNapi() = default;

void AudioRendererNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<AudioRendererNapi *>(nativeObject);
        delete obj;
        obj = nullptr;
    }
}

napi_status AudioRendererNapi::AddNamedProperty(napi_env env, napi_value object,
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

static void SetValueInt32(const napi_env& env, const std::string& fieldStr, const int intValue, napi_value &result)
{
    napi_value value = nullptr;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static AudioStandard::InterruptMode  GetNativeInterruptMode(int32_t interruptMode)
{
    AudioStandard::InterruptMode result;
    switch (interruptMode) {
        case AudioManagerNapi::InterruptMode::SHARE_MODE:
            result = AudioStandard::InterruptMode::SHARE_MODE;
            break;
        case AudioManagerNapi::InterruptMode::INDEPENDENT_MODE:
            result = AudioStandard::InterruptMode::INDEPENDENT_MODE;
            break;
        default:
            result = AudioStandard::InterruptMode::SHARE_MODE;
            HiLog::Error(LABEL, "Unknown interruptMode type, Set it to default SHARE_MODE!");
            break;
    }
    return result;
}

napi_value AudioRendererNapi::CreateAudioSampleFormatObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (int i = AudioRendererNapi::SAMPLE_FORMAT_INVALID; i <= AudioRendererNapi::SAMPLE_FORMAT_F32LE; i++) {
            switch (i) {
                case AudioRendererNapi::SAMPLE_FORMAT_INVALID:
                    propName = "SAMPLE_FORMAT_INVALID";
                    break;
                case AudioRendererNapi::SAMPLE_FORMAT_U8:
                    propName = "SAMPLE_FORMAT_U8";
                    break;
                case AudioRendererNapi::SAMPLE_FORMAT_S16LE:
                    propName = "SAMPLE_FORMAT_S16LE";
                    break;
                case AudioRendererNapi::SAMPLE_FORMAT_S24LE:
                    propName = "SAMPLE_FORMAT_S24LE";
                    break;
                case AudioRendererNapi::SAMPLE_FORMAT_S32LE:
                    propName = "SAMPLE_FORMAT_S32LE";
                    break;
                case AudioRendererNapi::SAMPLE_FORMAT_F32LE:
                    propName = "SAMPLE_FORMAT_F32LE";
                    break;
                default:
                    HiLog::Error(LABEL, "CreateAudioSampleFormatObject: No prop with this value try next value!");
                    continue;
            }
            status = AddNamedProperty(env, result, propName, i);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &sampleFormat_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateAudioSampleFormatObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioRendererNapi::CreateAudioRendererRateObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: rendererRateMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &audioRendererRate_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateAudioRendererRateObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioRendererNapi::CreateAudioStateObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;
    int32_t refCount = 1;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: audioStateMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop in CreateAudioStateObject!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, refCount, &audioState_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateAudioStateObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioRendererNapi::CreateInterruptEventTypeObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;
    int32_t refCount = 1;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: interruptEventTypeMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop in CreateInterruptEventTypeObject!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, refCount, &interruptEventType_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateInterruptEventTypeObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioRendererNapi::CreateInterruptForceTypeObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;
    int32_t refCount = 1;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: interruptForceTypeMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop in CreateInterruptTypeObject!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, refCount, &interruptForceType_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateInterruptForceTypeObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioRendererNapi::CreateInterruptHintTypeObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;
    int32_t refCount = 1;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: interruptHintTypeMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop in CreateInterruptHintTypeObject!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, refCount, &interruptHintType_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateInterruptHintTypeObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioRendererNapi::Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    const int32_t refCount = 1;
    napi_get_undefined(env, &result);

    napi_property_descriptor audio_renderer_properties[] = {
        DECLARE_NAPI_FUNCTION("setRenderRate", SetRenderRate),
        DECLARE_NAPI_FUNCTION("getRenderRate", GetRenderRate),
        DECLARE_NAPI_FUNCTION("start", Start),
        DECLARE_NAPI_FUNCTION("write", Write),
        DECLARE_NAPI_FUNCTION("getAudioTime", GetAudioTime),
        DECLARE_NAPI_FUNCTION("drain", Drain),
        DECLARE_NAPI_FUNCTION("pause", Pause),
        DECLARE_NAPI_FUNCTION("stop", Stop),
        DECLARE_NAPI_FUNCTION("release", Release),
        DECLARE_NAPI_FUNCTION("getBufferSize", GetBufferSize),
        DECLARE_NAPI_FUNCTION("getAudioStreamId", GetAudioStreamId),
        DECLARE_NAPI_FUNCTION("setVolume", SetVolume),
        DECLARE_NAPI_FUNCTION("on", On),
        DECLARE_NAPI_FUNCTION("off", Off),
        DECLARE_NAPI_FUNCTION("getRendererInfo", GetRendererInfo),
        DECLARE_NAPI_FUNCTION("getStreamInfo", GetStreamInfo),
        DECLARE_NAPI_FUNCTION("setInterruptMode", SetInterruptMode),
        DECLARE_NAPI_GETTER("state", GetState)
    };

    napi_property_descriptor static_prop[] = {
        DECLARE_NAPI_STATIC_FUNCTION("createAudioRenderer", CreateAudioRenderer),
        DECLARE_NAPI_PROPERTY("AudioRendererRate", CreateAudioRendererRateObject(env)),
        DECLARE_NAPI_PROPERTY("InterruptType", CreateInterruptEventTypeObject(env)),
        DECLARE_NAPI_PROPERTY("InterruptForceType", CreateInterruptForceTypeObject(env)),
        DECLARE_NAPI_PROPERTY("InterruptHint", CreateInterruptHintTypeObject(env)),
        DECLARE_NAPI_PROPERTY("AudioState", CreateAudioStateObject(env)),
        DECLARE_NAPI_PROPERTY("AudioSampleFormat", CreateAudioSampleFormatObject(env)),
    };

    status = napi_define_class(env, AUDIO_RENDERER_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct, nullptr,
        sizeof(audio_renderer_properties) / sizeof(audio_renderer_properties[PARAM0]),
        audio_renderer_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }

    status = napi_create_reference(env, constructor, refCount, &g_rendererConstructor);
    if (status == napi_ok) {
        status = napi_set_named_property(env, exports, AUDIO_RENDERER_NAPI_CLASS_NAME.c_str(), constructor);
        if (status == napi_ok) {
            status = napi_define_properties(env, exports,
                                            sizeof(static_prop) / sizeof(static_prop[PARAM0]), static_prop);
            if (status == napi_ok) {
                return exports;
            }
        }
    }
    
    HiLog::Error(LABEL, "Failure in AudioRendererNapi::Init()");
    return result;
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

napi_value AudioRendererNapi::Construct(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<AudioRendererNapi> rendererNapi = make_unique<AudioRendererNapi>();
    CHECK_AND_RETURN_RET_LOG(rendererNapi != nullptr, result, "No memory");

    rendererNapi->env_ = env;
    rendererNapi->contentType_ = sRendererOptions_->rendererInfo.contentType;
    rendererNapi->streamUsage_ = sRendererOptions_->rendererInfo.streamUsage;
    rendererNapi->rendererFlags_ = sRendererOptions_->rendererInfo.rendererFlags;

    AudioRendererOptions rendererOptions = {};
    rendererOptions.streamInfo.samplingRate = sRendererOptions_->streamInfo.samplingRate;
    rendererOptions.streamInfo.encoding = sRendererOptions_->streamInfo.encoding;
    rendererOptions.streamInfo.format = sRendererOptions_->streamInfo.format;
    rendererOptions.streamInfo.channels = sRendererOptions_->streamInfo.channels;
    rendererOptions.rendererInfo.contentType = sRendererOptions_->rendererInfo.contentType;
    rendererOptions.rendererInfo.streamUsage = sRendererOptions_->rendererInfo.streamUsage;
    rendererOptions.rendererInfo.rendererFlags = sRendererOptions_->rendererInfo.rendererFlags;

    std::shared_ptr<AbilityRuntime::Context> abilityContext = GetAbilityContext(env);
    std::string cacheDir = "";
    if (abilityContext != nullptr) {
        cacheDir = abilityContext->GetCacheDir();
    } else {
        cacheDir = "/data/storage/el2/base/haps/entry/files";
    }
    rendererNapi->audioRenderer_ = AudioRenderer::Create(cacheDir, rendererOptions);

    if (rendererNapi->audioRenderer_ == nullptr) {
        HiLog::Error(LABEL, "Renderer Create failed");
        unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();
        AudioRendererNapi::isConstructSuccess_ = NAPI_ERR_SYSTEM;
    }

    if (rendererNapi->audioRenderer_ != nullptr && rendererNapi->callbackNapi_ == nullptr) {
        rendererNapi->callbackNapi_ = std::make_shared<AudioRendererCallbackNapi>(env);
        CHECK_AND_RETURN_RET_LOG(rendererNapi->callbackNapi_ != nullptr, result, "No memory");
        int32_t ret = rendererNapi->audioRenderer_->SetRendererCallback(rendererNapi->callbackNapi_);
        if (ret) {
            AUDIO_DEBUG_LOG("AudioRendererNapi::Construct SetRendererCallback failed");
        }
    }

    status = napi_wrap(env, thisVar, static_cast<void*>(rendererNapi.get()),
                       AudioRendererNapi::Destructor, nullptr, nullptr);
    if (status == napi_ok) {
        rendererNapi.release();
        return thisVar;
    }

    HiLog::Error(LABEL, "Failed in AudioRendererNapi::Construct()!");
    return result;
}

napi_value AudioRendererNapi::CreateAudioRenderer(napi_env env, napi_callback_info info)
{
    HiLog::Info(LABEL, "%{public}s IN", __func__);
    napi_status status;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();
    CHECK_AND_RETURN_RET_LOG(asyncContext != nullptr, nullptr, "AudioRendererAsyncContext object creation failed");
    if (argc < ARGS_ONE) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }

    for (size_t i = PARAM0; i < argc; i++) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[i], &valueType);
        if (i == PARAM0 && valueType == napi_object) {
            if (!ParseRendererOptions(env, argv[i], &(asyncContext->rendererOptions))) {
                HiLog::Error(LABEL, "Parsing of renderer options failed");
                return result;
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
    napi_create_string_utf8(env, "CreateAudioRenderer", NAPI_AUTO_LENGTH, &resource);

    status = napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            auto context = static_cast<AudioRendererAsyncContext *>(data);
            context->status = SUCCESS;
        },
        GetRendererAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
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

void AudioRendererNapi::CommonCallbackRoutine(napi_env env, AudioRendererAsyncContext* &asyncContext,
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

void AudioRendererNapi::WriteAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    napi_value result[ARGS_TWO] = {0};
    napi_value retVal;

    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &result[PARAM0]);
            napi_create_uint32(env, asyncContext->totalBytesWritten, &result[PARAM1]);
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
        // queue the next write request from internal queue to napi queue
        if (!asyncContext->objectInfo->doNotScheduleWrite_ && !asyncContext->objectInfo->isDrainWriteQInProgress_) {
            if (!asyncContext->objectInfo->writeRequestQ_.empty()) {
                napi_queue_async_work(env, asyncContext->objectInfo->writeRequestQ_.front());
                asyncContext->objectInfo->writeRequestQ_.pop();
            } else {
                asyncContext->objectInfo->scheduleFromApiCall_ = true;
            }
        }

        delete asyncContext;
        asyncContext = nullptr;
    } else {
            HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioRendererNapi::PauseAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    napi_value result[ARGS_TWO] = {0};
    napi_value retVal;

    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            // set pause result to doNotScheduleWrite_
            asyncContext->objectInfo->doNotScheduleWrite_ = asyncContext->isTrue;
            napi_get_undefined(env, &valueParam);
        }
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
        // Pause failed . Continue Write
        if (!asyncContext->isTrue) {
            HiLog::Info(LABEL, "PauseAsyncCallbackComplete: Pasue failed, Continue Write");
            if (!asyncContext->objectInfo->writeRequestQ_.empty()) {
                napi_queue_async_work(env, asyncContext->objectInfo->writeRequestQ_.front());
                asyncContext->objectInfo->writeRequestQ_.pop();
            } else {
                asyncContext->objectInfo->scheduleFromApiCall_ = true;
            }
        }

        delete asyncContext;
        asyncContext = nullptr;
    } else {
            HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioRendererNapi::StartAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    napi_value result[ARGS_TWO] = {0};
    napi_value retVal;

    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
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
        // If start success , set doNotScheduleWrite_ = false and queue write request
        if (asyncContext->isTrue) {
            asyncContext->objectInfo->doNotScheduleWrite_ = false;
            if (!asyncContext->objectInfo->writeRequestQ_.empty()) {
                napi_queue_async_work(env, asyncContext->objectInfo->writeRequestQ_.front());
                asyncContext->objectInfo->writeRequestQ_.pop();
            } else {
                asyncContext->objectInfo->scheduleFromApiCall_ = true;
            }
        }

        delete asyncContext;
        asyncContext = nullptr;
    } else {
            HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioRendererNapi::StopAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    napi_value result[ARGS_TWO] = {0};
    napi_value retVal;

    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            // set pause result to doNotScheduleWrite_
            asyncContext->objectInfo->doNotScheduleWrite_ = asyncContext->isTrue;
            napi_get_undefined(env, &valueParam);
        }
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
    } else {
            HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioRendererNapi::SetFunctionAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioRendererNapi::IsTrueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRendererAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_boolean(env, asyncContext->isTrue, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioRendererNapi::VoidAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRendererAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioRendererNapi::GetBufferSizeAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_create_uint32(env, asyncContext->bufferSize, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioRendererNapi::GetAudioStreamIdCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);
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

void AudioRendererNapi::GetIntValueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_create_int32(env, asyncContext->intValue, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioRendererNapi::GetRendererAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    napi_value valueParam = nullptr;
    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            unique_ptr<AudioRendererOptions> rendererOptions = make_unique<AudioRendererOptions>();
            rendererOptions->streamInfo.samplingRate = asyncContext->rendererOptions.streamInfo.samplingRate;
            rendererOptions->streamInfo.encoding = asyncContext->rendererOptions.streamInfo.encoding;
            rendererOptions->streamInfo.format = asyncContext->rendererOptions.streamInfo.format;
            rendererOptions->streamInfo.channels = asyncContext->rendererOptions.streamInfo.channels;
            rendererOptions->rendererInfo.contentType = asyncContext->rendererOptions.rendererInfo.contentType;
            rendererOptions->rendererInfo.streamUsage = asyncContext->rendererOptions.rendererInfo.streamUsage;
            rendererOptions->rendererInfo.rendererFlags = asyncContext->rendererOptions.rendererInfo.rendererFlags;

            valueParam = CreateAudioRendererWrapper(env, rendererOptions);
            asyncContext->status = AudioRendererNapi::isConstructSuccess_;
            AudioRendererNapi::isConstructSuccess_ = SUCCESS;
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: GetRendererAsyncCallbackComplete asyncContext is Null!");
    }
}

void AudioRendererNapi::GetInt64ValueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRendererAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_create_int64(env, asyncContext->time, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioRendererAsyncContext* is Null!");
    }
}

void AudioRendererNapi::AudioRendererInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (asyncContext->status == SUCCESS) {
            (void)napi_create_object(env, &valueParam);
            SetValueInt32(env, "content", static_cast<int32_t>(asyncContext->contentType), valueParam);
            SetValueInt32(env, "usage", static_cast<int32_t>(asyncContext->usage), valueParam);
            SetValueInt32(env, "rendererFlags", asyncContext->rendererFlags, valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: asyncContext is Null!");
    }
}

void AudioRendererNapi::AudioStreamInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRendererAsyncContext *>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (asyncContext->status == SUCCESS) {
            (void)napi_create_object(env, &valueParam);
            SetValueInt32(env, "samplingRate", static_cast<int32_t>(asyncContext->samplingRate), valueParam);
            SetValueInt32(env, "channels", static_cast<int32_t>(asyncContext->channelCount), valueParam);
            SetValueInt32(env, "sampleFormat", static_cast<int32_t>(asyncContext->sampleFormat), valueParam);
            SetValueInt32(env, "encodingType", static_cast<int32_t>(asyncContext->encodingType), valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioStreamInfoAsyncCallbackComplete* is Null!");
    }
}

napi_value AudioRendererNapi::SetRenderRate(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_TWO);
    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();
    if (argc < ARGS_ONE) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }

    status = napi_unwrap(env, thisVar, reinterpret_cast<void **>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_number) {
                napi_get_value_int32(env, argv[PARAM0], &asyncContext->audioRendererRate);
            } else if (i == PARAM1) {
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
        napi_create_string_utf8(env, "SetRenderRate", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                if (context->status == SUCCESS) {
                    AudioRendererRate audioRenderRate = static_cast<AudioRendererRate>(context->audioRendererRate);
                    int32_t audioClientInvalidParamsErr = -2;
                    context->intValue = context->objectInfo->audioRenderer_->SetRenderRate(audioRenderRate);
                    if (context->intValue == SUCCESS) {
                        context->status = SUCCESS;
                    } else if (context->intValue == audioClientInvalidParamsErr) {
                        context->status = NAPI_ERR_UNSUPPORTED;
                    } else {
                        context->status = NAPI_ERR_SYSTEM;
                    }
                }
            },
            VoidAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioRendererNapi::GetRenderRate(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();
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
        napi_create_string_utf8(env, "GetRenderRate", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                context->intValue = context->objectInfo->audioRenderer_->GetRenderRate();
                context->status = SUCCESS;
            },
            GetIntValueAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioRendererNapi::Start(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();

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
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                context->isTrue = context->objectInfo->audioRenderer_->Start();
                context->status = context->isTrue ? SUCCESS : NAPI_ERR_SYSTEM;
            },
            StartAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioRendererNapi::Write(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_TWO);
    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();
    if (argc < ARGS_ONE) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }
    
    status = napi_unwrap(env, thisVar, reinterpret_cast<void **>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if ((i == PARAM0) && (valueType == napi_object)) {
                napi_get_arraybuffer_info(env, argv[i], &asyncContext->data, &asyncContext->bufferLen);
            } else if (i == PARAM1) {
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
        napi_create_string_utf8(env, "Write", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                if (context->status == SUCCESS) {
                    context->status = NAPI_ERR_SYSTEM;
                    size_t bufferLen = context->bufferLen;
                    auto buffer = std::make_unique<uint8_t[]>(bufferLen);
                    if (buffer == nullptr) {
                        HiLog::Error(LABEL, "Renderer write buffer allocation failed");
                        return;
                    }

                    if (memcpy_s(buffer.get(), bufferLen, context->data, bufferLen)) {
                        HiLog::Info(LABEL, "Renderer mem copy failed");
                        return;
                    }

                    int32_t bytesWritten = 0;
                    size_t totalBytesWritten = 0;
                    size_t minBytes = 4;
                    while ((totalBytesWritten < bufferLen) && ((bufferLen - totalBytesWritten) > minBytes)) {
                        bytesWritten = context->objectInfo->audioRenderer_->Write(buffer.get() + totalBytesWritten,
                            bufferLen - totalBytesWritten);
                        if (bytesWritten < 0) {
                            break;
                        }

                        totalBytesWritten += bytesWritten;
                    }

                    context->status = SUCCESS;
                    context->totalBytesWritten = totalBytesWritten;
                }
            },
            WriteAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else if (asyncContext->objectInfo->scheduleFromApiCall_) {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext->objectInfo->scheduleFromApiCall_ = false;
                asyncContext.release();
            } else {
                result = nullptr;
            }
        } else {
            asyncContext->objectInfo->writeRequestQ_.push(asyncContext->work);
            asyncContext.release();
        }
    }

    return result;
}

napi_value AudioRendererNapi::GetAudioTime(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void **>(&asyncContext->objectInfo));
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
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                Timestamp timestamp;
                if (context->objectInfo->audioRenderer_->GetAudioTime(timestamp, Timestamp::Timestampbase::MONOTONIC)) {
                    const uint64_t secToNanosecond = 1000000000;
                    context->time = timestamp.time.tv_nsec + timestamp.time.tv_sec * secToNanosecond;
                    context->status = SUCCESS;
                } else {
                    context->status = NAPI_ERR_SYSTEM;
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

napi_value AudioRendererNapi::Drain(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();

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
        JudgeFuncDrain(env, result, asyncContext);
    }

    return result;
}

void AudioRendererNapi::JudgeFuncDrain(napi_env &env, napi_value &result,
    unique_ptr<AudioRendererAsyncContext> &asyncContext)
{
    napi_status status;
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "Drain", NAPI_AUTO_LENGTH, &resource);
    status = napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            auto context = static_cast<AudioRendererAsyncContext *>(data);
            if (!CheckContextStatus(context)) {
                return;
            }
            context->isTrue = context->objectInfo->audioRenderer_->Drain();
            context->status = context->isTrue ? SUCCESS : NAPI_ERR_SYSTEM;
        },
        VoidAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
    if (status != napi_ok) {
        result = nullptr;
    } else {
        if (!asyncContext->objectInfo->doNotScheduleWrite_) {
            asyncContext->objectInfo->isDrainWriteQInProgress_ = true;
            while (!asyncContext->objectInfo->writeRequestQ_.empty()) {
                napi_queue_async_work(env, asyncContext->objectInfo->writeRequestQ_.front());
                asyncContext->objectInfo->writeRequestQ_.pop();
            }
            asyncContext->objectInfo->isDrainWriteQInProgress_ = false;
        }
        status = napi_queue_async_work(env, asyncContext->work);
        if (status == napi_ok) {
            asyncContext.release();
        } else {
            result = nullptr;
        }
    }
}

napi_value AudioRendererNapi::Pause(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();

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
        napi_create_string_utf8(env, "Pause", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                context->isTrue = context->objectInfo->audioRenderer_->Pause();
                context->status = context->isTrue ? SUCCESS : NAPI_ERR_SYSTEM;
            },
            PauseAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            result = nullptr;
        } else {
            status = napi_queue_async_work(env, asyncContext->work);
            if (status == napi_ok) {
                asyncContext->objectInfo->doNotScheduleWrite_ = true;
                asyncContext.release();
            } else {
                result = nullptr;
            }
        }
    }

    return result;
}

napi_value AudioRendererNapi::Stop(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();

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
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                context->isTrue = context->objectInfo->audioRenderer_->Stop();
                context->status = context->isTrue ? SUCCESS : NAPI_ERR_SYSTEM;
            },
            StopAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioRendererNapi::Release(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();

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
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                context->isTrue = context->objectInfo->audioRenderer_->Release();
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

napi_value AudioRendererNapi::GetBufferSize(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();

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
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                size_t bufferSize;
                context->status = context->objectInfo->audioRenderer_->GetBufferSize(bufferSize);
                if (context->status == SUCCESS) {
                    context->bufferSize = bufferSize;
                }
            },
            GetBufferSizeAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioRendererNapi::GetAudioStreamId(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();

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
        JudgeFuncGetAudioStreamId(env, result, asyncContext);
    }

    return result;
}

void AudioRendererNapi::JudgeFuncGetAudioStreamId(napi_env &env, napi_value &result,
    unique_ptr<AudioRendererAsyncContext> &asyncContext)
{
    napi_status status;
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "GetAudioStreamId", NAPI_AUTO_LENGTH, &resource);

    status = napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            auto context = static_cast<AudioRendererAsyncContext *>(data);
            if (!CheckContextStatus(context)) {
                return;
            }
            int32_t streamIdStatus;
            streamIdStatus = context->objectInfo->audioRenderer_->
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

napi_value AudioRendererNapi::SetVolume(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_THREE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();
    if (argc < ARGS_ONE) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);
            if (i == PARAM0 && valueType == napi_number) {
                napi_get_value_double(env, argv[i], &asyncContext->volLevel);
            } else if (i == PARAM1) {
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
        napi_create_string_utf8(env, "SetVolume", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRendererAsyncContext*>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                if (context->status == SUCCESS) {
                    if (context->volLevel < MIN_VOLUME_IN_DOUBLE || context->volLevel > MAX_VOLUME_IN_DOUBLE) {
                        context->status = NAPI_ERR_UNSUPPORTED;
                    } else {
                        context->status = context->objectInfo->audioRenderer_->
                            SetVolume(static_cast<float>(context->volLevel));
                    }
                }
            },
            SetFunctionAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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


napi_value AudioRendererNapi::GetRendererInfo(napi_env env, napi_callback_info info)
{
    HiLog::Info(LABEL, "Entered GetRendererInfo");
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();

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
        napi_create_string_utf8(env, "GetRendererInfo", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                AudioRendererInfo rendererInfo = {};
                context->status = context->objectInfo->audioRenderer_->GetRendererInfo(rendererInfo);
                if (context->status == SUCCESS) {
                    context->contentType = rendererInfo.contentType;
                    context->usage = rendererInfo.streamUsage;
                    context->rendererFlags = rendererInfo.rendererFlags;
                }
            },
            AudioRendererInfoAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioRendererNapi::GetStreamInfo(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();

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
                auto context = static_cast<AudioRendererAsyncContext *>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                AudioStreamInfo streamInfo;
                context->status = context->objectInfo->audioRenderer_->GetStreamInfo(streamInfo);
                if (context->status == SUCCESS) {
                    context->sampleFormat = static_cast<AudioSampleFormat>(streamInfo.format);
                    context->samplingRate = streamInfo.samplingRate;
                    context->channelCount = streamInfo.channels;
                    context->encodingType = streamInfo.encoding;
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

napi_value AudioRendererNapi::GetState(napi_env env, napi_callback_info info)
{
    napi_value jsThis = nullptr;
    napi_value undefinedResult = nullptr;
    napi_get_undefined(env, &undefinedResult);

    size_t argCount = 0;
    napi_status status = napi_get_cb_info(env, info, &argCount, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        AUDIO_INFO_LOG("Failed to retrieve details about the callback");
        return undefinedResult;
    }

    AudioRendererNapi *rendererNapi = nullptr;
    status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&rendererNapi));
    CHECK_AND_RETURN_RET_LOG(status == napi_ok && rendererNapi != nullptr, undefinedResult, "Failed to get instance");

    CHECK_AND_RETURN_RET_LOG(rendererNapi->audioRenderer_ != nullptr, undefinedResult, "No memory");
    int32_t rendererState = rendererNapi->audioRenderer_->GetStatus();

    napi_value jsResult = nullptr;
    status = napi_create_int32(env, rendererState, &jsResult);
    CHECK_AND_RETURN_RET_LOG(status == napi_ok, undefinedResult, "napi_create_int32 error");

    AUDIO_INFO_LOG("AudioRendererNapi: GetState Complete, Current state: %{public}d", rendererState);
    return jsResult;
}

napi_value AudioRendererNapi::RegisterPeriodPositionCallback(napi_env env, napi_value* argv, const std::string& cbName,
                                                             AudioRendererNapi *rendererNapi)
{
    int64_t frameCount = 0;
    napi_get_value_int64(env, argv[PARAM1], &frameCount);

    if (frameCount > 0) {
        if (rendererNapi->periodPositionCBNapi_ == nullptr) {
            rendererNapi->periodPositionCBNapi_ = std::make_shared<RendererPeriodPositionCallbackNapi>(env);
            THROW_ERROR_ASSERT(env, rendererNapi->periodPositionCBNapi_ != nullptr, NAPI_ERR_NO_MEMORY);

            int32_t ret = rendererNapi->audioRenderer_->SetRendererPeriodPositionCallback(frameCount,
                rendererNapi->periodPositionCBNapi_);
            THROW_ERROR_ASSERT(env, ret == SUCCESS, NAPI_ERR_SYSTEM);

            std::shared_ptr<RendererPeriodPositionCallbackNapi> cb =
                std::static_pointer_cast<RendererPeriodPositionCallbackNapi>(rendererNapi->periodPositionCBNapi_);
            cb->SaveCallbackReference(cbName, argv[PARAM2]);
        } else {
            AUDIO_DEBUG_LOG("AudioRendererNapi: periodReach already subscribed.");
        }
    } else {
        AUDIO_ERR_LOG("AudioRendererNapi: frameCount value not supported!!");
    }

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value AudioRendererNapi::RegisterPositionCallback(napi_env env, napi_value* argv,
                                                       const std::string& cbName, AudioRendererNapi *rendererNapi)
{
    int64_t markPosition = 0;
    napi_get_value_int64(env, argv[PARAM1], &markPosition);

    if (markPosition > 0) {
        rendererNapi->positionCBNapi_ = std::make_shared<RendererPositionCallbackNapi>(env);
        THROW_ERROR_ASSERT(env, rendererNapi->positionCBNapi_ != nullptr, NAPI_ERR_NO_MEMORY);
        int32_t ret = rendererNapi->audioRenderer_->SetRendererPositionCallback(markPosition,
            rendererNapi->positionCBNapi_);
        THROW_ERROR_ASSERT(env, ret == SUCCESS, NAPI_ERR_SYSTEM);

        std::shared_ptr<RendererPositionCallbackNapi> cb =
            std::static_pointer_cast<RendererPositionCallbackNapi>(rendererNapi->positionCBNapi_);
        cb->SaveCallbackReference(cbName, argv[PARAM2]);
    } else {
        AUDIO_ERR_LOG("AudioRendererNapi: Mark Position value not supported!!");
        THROW_ERROR_ASSERT(env, false, NAPI_ERR_INPUT_INVALID);
    }

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value AudioRendererNapi::RegisterRendererCallback(napi_env env, napi_value* argv,
                                                       const std::string& cbName, AudioRendererNapi *rendererNapi)
{
    THROW_ERROR_ASSERT(env, rendererNapi->callbackNapi_ != nullptr, NAPI_ERR_NO_MEMORY);

    std::shared_ptr<AudioRendererCallbackNapi> cb =
        std::static_pointer_cast<AudioRendererCallbackNapi>(rendererNapi->callbackNapi_);
    cb->SaveCallbackReference(cbName, argv[PARAM1]);

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value AudioRendererNapi::RegisterDataRequestCallback(napi_env env, napi_value* argv,
    const std::string& cbName, AudioRendererNapi *rendererNapi)
{
    if (rendererNapi->dataRequestCBNapi_ == nullptr) {
        rendererNapi->dataRequestCBNapi_ = std::make_shared<RendererDataRequestCallbackNapi>(env, rendererNapi);
        rendererNapi->audioRenderer_->SetRenderMode(RENDER_MODE_CALLBACK);
        THROW_ERROR_ASSERT(env, rendererNapi->dataRequestCBNapi_ != nullptr, NAPI_ERR_NO_MEMORY);
        int32_t ret = rendererNapi->audioRenderer_->SetRendererWriteCallback(rendererNapi->dataRequestCBNapi_);
        THROW_ERROR_ASSERT(env, ret == SUCCESS, NAPI_ERR_SYSTEM);
        std::shared_ptr<RendererDataRequestCallbackNapi> cb =
            std::static_pointer_cast<RendererDataRequestCallbackNapi>(rendererNapi->dataRequestCBNapi_);
        cb->SaveCallbackReference(cbName, argv[PARAM1]);
    } else {
        AUDIO_DEBUG_LOG("AudioRendererNapi:dataRequest already subscribed.");
        THROW_ERROR_ASSERT(env, false, NAPI_ERR_ILLEGAL_STATE);
    }
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value AudioRendererNapi::RegisterCallback(napi_env env, napi_value jsThis,
                                               napi_value* argv, const std::string& cbName)
{
    AudioRendererNapi *rendererNapi = nullptr;
    napi_status status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&rendererNapi));
    THROW_ERROR_ASSERT(env, status == napi_ok, NAPI_ERR_SYSTEM);
    THROW_ERROR_ASSERT(env, rendererNapi != nullptr, NAPI_ERR_NO_MEMORY);

    THROW_ERROR_ASSERT(env, rendererNapi->audioRenderer_ != nullptr, NAPI_ERR_NO_MEMORY);

    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    if (!cbName.compare(INTERRUPT_CALLBACK_NAME) ||
        !cbName.compare(AUDIO_INTERRUPT_CALLBACK_NAME) ||
        !cbName.compare(STATE_CHANGE_CALLBACK_NAME)) {
        result = RegisterRendererCallback(env, argv, cbName, rendererNapi);
    } else if (!cbName.compare(MARK_REACH_CALLBACK_NAME)) {
        result = RegisterPositionCallback(env, argv, cbName, rendererNapi);
    } else if (!cbName.compare(PERIOD_REACH_CALLBACK_NAME)) {
        result = RegisterPeriodPositionCallback(env, argv, cbName, rendererNapi);
    } else if (!cbName.compare(DATA_REQUEST_CALLBACK_NAME)) {
        result = RegisterDataRequestCallback(env, argv, cbName, rendererNapi);
    } else {
        bool unknownCallback = true;
        THROW_ERROR_ASSERT(env, !unknownCallback, NAPI_ERROR_INVALID_PARAM);
    }

    return result;
}

napi_value AudioRendererNapi::On(napi_env env, napi_callback_info info)
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
    AUDIO_DEBUG_LOG("AudioRendererNapi: On callbackName: %{public}s", callbackName.c_str());

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

napi_value AudioRendererNapi::UnregisterCallback(napi_env env, napi_value jsThis, const std::string& cbName)
{
    AudioRendererNapi *rendererNapi = nullptr;
    napi_status status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&rendererNapi));
    THROW_ERROR_ASSERT(env, status == napi_ok, NAPI_ERR_SYSTEM);
    THROW_ERROR_ASSERT(env, rendererNapi != nullptr, NAPI_ERR_NO_MEMORY);
    THROW_ERROR_ASSERT(env, rendererNapi->audioRenderer_ != nullptr, NAPI_ERR_NO_MEMORY);

    if (!cbName.compare(MARK_REACH_CALLBACK_NAME)) {
        rendererNapi->audioRenderer_->UnsetRendererPositionCallback();
        rendererNapi->positionCBNapi_ = nullptr;
    } else if (!cbName.compare(PERIOD_REACH_CALLBACK_NAME)) {
        rendererNapi->audioRenderer_->UnsetRendererPeriodPositionCallback();
        rendererNapi->periodPositionCBNapi_ = nullptr;
    } else {
        bool unknownCallback = true;
        THROW_ERROR_ASSERT(env, !unknownCallback, NAPI_ERR_UNSUPPORTED);
    }

    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

napi_value AudioRendererNapi::Off(napi_env env, napi_callback_info info)
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
    AUDIO_DEBUG_LOG("AudioRendererNapi: Off callbackName: %{public}s", callbackName.c_str());

    return UnregisterCallback(env, jsThis, callbackName);
}

bool AudioRendererNapi::ParseRendererOptions(napi_env env, napi_value root, AudioRendererOptions *opts)
{
    napi_value res = nullptr;

    if (napi_get_named_property(env, root, "rendererInfo", &res) == napi_ok) {
        ParseRendererInfo(env, res, &(opts->rendererInfo));
    }

    if (napi_get_named_property(env, root, "streamInfo", &res) == napi_ok) {
        ParseStreamInfo(env, res, &(opts->streamInfo));
    }

    return true;
}

bool AudioRendererNapi::ParseRendererInfo(napi_env env, napi_value root, AudioRendererInfo *rendererInfo)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};

    if (napi_get_named_property(env, root, "content", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        rendererInfo->contentType = static_cast<ContentType>(intValue);
    }

    if (napi_get_named_property(env, root, "usage", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        rendererInfo->streamUsage = static_cast<StreamUsage>(intValue);
    }

    if (napi_get_named_property(env, root, "rendererFlags", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &(rendererInfo->rendererFlags));
    }

    return true;
}

bool AudioRendererNapi::ParseStreamInfo(napi_env env, napi_value root, AudioStreamInfo* streamInfo)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};

    if (napi_get_named_property(env, root, "samplingRate", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        streamInfo->samplingRate = static_cast<AudioSamplingRate>(intValue);
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

bool AudioRendererNapi::CheckContextStatus(AudioRendererAsyncContext *context)
{
    if (context == nullptr) {
        AUDIO_ERR_LOG("context object is nullptr.");
        return false;
    }
    if (context->objectInfo == nullptr || context->objectInfo->audioRenderer_ == nullptr) {
        context->status = NAPI_ERR_SYSTEM;
        AUDIO_ERR_LOG("context object state is error.");
        return false;
    }
    return true;
}

napi_value AudioRendererNapi::CreateAudioRendererWrapper(napi_env env, unique_ptr<AudioRendererOptions> &renderOptions)
{
    lock_guard<mutex> lock(createMutex_);
    napi_status status;
    napi_value result = nullptr;
    napi_value constructor;

    if (renderOptions != nullptr) {
        status = napi_get_reference_value(env, g_rendererConstructor, &constructor);
        if (status == napi_ok) {
            sRendererOptions_ = move(renderOptions);
            status = napi_new_instance(env, constructor, 0, nullptr, &result);
            sRendererOptions_.release();
            if (status == napi_ok) {
                return result;
            }
        }
        HiLog::Error(LABEL, "Failed in CreateAudioRendererWrapper, %{public}d", status);
    }

    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioRendererNapi::SetInterruptMode(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;
    
    GET_PARAMS(env, info, ARGS_TWO);
    
    unique_ptr<AudioRendererAsyncContext> asyncContext = make_unique<AudioRendererAsyncContext>();
    CHECK_AND_RETURN_RET_LOG(asyncContext != nullptr, nullptr, "AudioRendererAsyncContext object creation failed");
    if (argc < ARGS_ONE) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);
            if (i == PARAM0 && valueType == napi_number) {
                napi_get_value_int32(env, argv[i], &asyncContext->interruptMode);
            } else if (i == PARAM1) {
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
        napi_create_string_utf8(env, "SetInterruptMode", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRendererAsyncContext*>(data);
                if (!CheckContextStatus(context)) {
                    return;
                }
                if (context->status == SUCCESS) {
                    AudioStandard::InterruptMode interruptMode_ = GetNativeInterruptMode(context->interruptMode);
                    context->objectInfo->audioRenderer_->SetInterruptMode(interruptMode_);
                    context->status = SUCCESS;
                    context->intValue = SUCCESS;
                }
            },
            GetIntValueAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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
} // namespace AudioStandard
} // namespace OHOS
