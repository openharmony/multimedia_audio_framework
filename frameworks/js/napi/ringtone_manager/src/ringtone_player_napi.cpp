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

#include "ringtone_player_napi.h"

#include "audio_renderer_info_napi.h"
#include "hilog/log.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace {
    /* Constants for array index */
    const int32_t PARAM0 = 0;
    const int32_t PARAM1 = 1;

    /* Constants for array size */
    const int32_t ARGS_ONE = 1;
    const int32_t ARGS_TWO = 2;

    const int SUCCESS = 0;

    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "RingtonePlayerNapi"};
}

namespace OHOS {
namespace AudioStandard {
napi_ref RingtonePlayerNapi::sConstructor_ = nullptr;
shared_ptr<IRingtonePlayer> RingtonePlayerNapi::sIRingtonePlayer_ = nullptr;

RingtonePlayerNapi::RingtonePlayerNapi() : env_(nullptr) {}

RingtonePlayerNapi::~RingtonePlayerNapi() = default;

napi_status RingtonePlayerNapi::AddNamedProperty(napi_env env, napi_value object,
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

napi_value RingtonePlayerNapi::Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value ctorObj;
    int32_t refCount = 1;

    napi_property_descriptor ringtone_player_prop[] = {
        DECLARE_NAPI_FUNCTION("getTitle", GetTitle),
        DECLARE_NAPI_FUNCTION("getAudioRendererInfo", GetAudioRendererInfo),
        DECLARE_NAPI_FUNCTION("configure", Configure),
        DECLARE_NAPI_FUNCTION("start", Start),
        DECLARE_NAPI_FUNCTION("stop", Stop),
        DECLARE_NAPI_FUNCTION("release", Release),
        DECLARE_NAPI_GETTER("state", GetAudioState)
    };

    status = napi_define_class(env, RINGTONE_PLAYER_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH,
                               RingtonePlayerNapiConstructor, nullptr, sizeof(ringtone_player_prop)
                               / sizeof(ringtone_player_prop[0]), ringtone_player_prop, &ctorObj);
    if (status == napi_ok) {
        if (napi_create_reference(env, ctorObj, refCount, &sConstructor_) == napi_ok) {
            status = napi_set_named_property(env, exports, RINGTONE_PLAYER_NAPI_CLASS_NAME.c_str(), ctorObj);
            if (status == napi_ok) {
                return exports;
            }
        }
    }

    return nullptr;
}

napi_value RingtonePlayerNapi::RingtonePlayerNapiConstructor(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value thisVar = nullptr;

    napi_get_undefined(env, &result);
    status = napi_get_cb_info(env, info, nullptr, nullptr, &thisVar, nullptr);
    if (status == napi_ok && thisVar != nullptr) {
        std::unique_ptr<RingtonePlayerNapi> obj = std::make_unique<RingtonePlayerNapi>();
        if (obj != nullptr) {
            obj->env_ = env;
            if (obj->sIRingtonePlayer_ != nullptr) {
                obj->iRingtonePlayer = move(obj->sIRingtonePlayer_);
            } else {
                HiLog::Error(LABEL, "Failed to create sIRingtonePlayer_ instance.");
                return result;
            }

            status = napi_wrap(env, thisVar, reinterpret_cast<void*>(obj.get()),
                RingtonePlayerNapi::RingtonePlayerNapiDestructor, nullptr, nullptr);
            if (status == napi_ok) {
                obj.release();
                return thisVar;
            } else {
                HiLog::Error(LABEL, "Failed to wrap the native rngplyrmngr object with JS.");
            }
        }
    }

    return result;
}

void RingtonePlayerNapi::RingtonePlayerNapiDestructor(napi_env env, void* nativeObject, void* finalize_hint)
{
    RingtonePlayerNapi *ringtonePlayerHelper = reinterpret_cast<RingtonePlayerNapi*>(nativeObject);
    if (ringtonePlayerHelper != nullptr) {
        ringtonePlayerHelper->~RingtonePlayerNapi();
    }
}

napi_value RingtonePlayerNapi::GetRingtonePlayerInstance(napi_env env,
    shared_ptr<IRingtonePlayer> &iRingtonePlayer)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value ctor;

    status = napi_get_reference_value(env, sConstructor_, &ctor);
    if (status == napi_ok) {
        sIRingtonePlayer_ = iRingtonePlayer;
        status = napi_new_instance(env, ctor, 0, nullptr, &result);
        if (status == napi_ok) {
            return result;
        } else {
            HiLog::Error(LABEL, "New instance could not be obtained.");
        }
    }

    napi_get_undefined(env, &result);
    return result;
}

static void CommonAsyncCallbackComp(napi_env env, napi_status status, void* data)
{
    auto context = static_cast<RingtonePlayerAsyncContext *>(data);
    napi_value callback = nullptr;
    napi_value retVal = nullptr;
    napi_value result[2] = {};

    napi_get_undefined(env, &result[PARAM1]);
    if (!context->status) {
        napi_get_undefined(env, &result[PARAM0]);
    } else {
        napi_value message = nullptr;
        napi_create_string_utf8(env, "Error, Operation not supported or Failed", NAPI_AUTO_LENGTH, &message);
        napi_create_error(env, nullptr, message, &result[PARAM0]);
    }

    if (context->deferred) {
        if (!context->status) {
            napi_resolve_deferred(env, context->deferred, result[PARAM1]);
        } else {
            napi_reject_deferred(env, context->deferred, result[PARAM0]);
        }
    } else {
        napi_get_reference_value(env, context->callbackRef, &callback);
        napi_call_function(env, nullptr, callback, ARGS_TWO, result, &retVal);
        napi_delete_reference(env, context->callbackRef);
    }
    napi_delete_async_work(env, context->work);

    delete context;
    context = nullptr;
}

static void GetTitleAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto context = static_cast<RingtonePlayerAsyncContext *>(data);
    napi_value callback = nullptr;
    napi_value retVal = nullptr;
    napi_value result[2] = {};

    if (!context->status) {
        napi_get_undefined(env, &result[PARAM0]);
        napi_create_string_utf8(env, context->title.c_str(), NAPI_AUTO_LENGTH, &result[PARAM1]);
    } else {
        napi_value message = nullptr;
        napi_create_string_utf8(env, "Error, Operation not supported or Failed", NAPI_AUTO_LENGTH, &message);
        napi_create_error(env, nullptr, message, &result[PARAM0]);
        napi_get_undefined(env, &result[PARAM1]);
    }

    if (context->deferred) {
        if (!context->status) {
            napi_resolve_deferred(env, context->deferred, result[PARAM1]);
        } else {
            napi_reject_deferred(env, context->deferred, result[PARAM0]);
        }
    } else {
        napi_get_reference_value(env, context->callbackRef, &callback);
        napi_call_function(env, nullptr, callback, ARGS_TWO, result, &retVal);
        napi_delete_reference(env, context->callbackRef);
    }
    napi_delete_async_work(env, context->work);

    delete context;
    context = nullptr;
}

napi_value RingtonePlayerNapi::GetTitle(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value resource = nullptr;
    size_t argc = ARGS_ONE;
    napi_value argv[ARGS_ONE] = {0};
    napi_value thisVar = nullptr;
    const int32_t refCount = 1;

    status = napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    napi_get_undefined(env, &result);
    if (status != napi_ok || thisVar == nullptr) {
        HiLog::Error(LABEL, "Failed to retrieve details about the callback");
        return result;
    }

    NAPI_ASSERT(env, argc <= ARGS_ONE, "requires 1 parameter maximum");
    std::unique_ptr<RingtonePlayerAsyncContext> asyncContext = std::make_unique<RingtonePlayerAsyncContext>();
    status = napi_unwrap(env, thisVar, reinterpret_cast<void **>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc == ARGS_ONE) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        } else {
            napi_create_promise(env, &asyncContext->deferred, &result);
        }

        napi_create_string_utf8(env, "GetTitle", NAPI_AUTO_LENGTH, &resource);
        status = napi_create_async_work(env, nullptr, resource,
            [](napi_env env, void *data) {
                RingtonePlayerAsyncContext *context = static_cast<RingtonePlayerAsyncContext *>(data);
                context->title = context->objectInfo->iRingtonePlayer->GetTitle();
                context->status = SUCCESS;
            },
            GetTitleAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            HiLog::Error(LABEL, "Failed to get create async work");
            napi_get_undefined(env, &result);
        } else {
            napi_queue_async_work(env, asyncContext->work);
            asyncContext.release();
        }
    }

    return result;
}

void AudioRendererInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto context = static_cast<RingtonePlayerAsyncContext *>(data);
    napi_value callback = nullptr;
    napi_value retVal = nullptr;
    napi_value valueParam = nullptr;
    napi_value result[2] = {};

    if (!context->status) {
        unique_ptr<AudioRendererInfo> audioRendererInfo = make_unique<AudioRendererInfo>();
        audioRendererInfo->contentType = context->contentType;
        audioRendererInfo->streamUsage = context->streamUsage;
        audioRendererInfo->rendererFlags = context->rendererFlags;

        valueParam = AudioRendererInfoNapi::CreateAudioRendererInfoWrapper(env, audioRendererInfo);
        napi_get_undefined(env, &result[PARAM0]);
        result[PARAM1] = valueParam;
    } else {
        napi_value message = nullptr;
        napi_create_string_utf8(env, "Error, Operation not supported or Failed", NAPI_AUTO_LENGTH, &message);
        napi_create_error(env, nullptr, message, &result[PARAM0]);
        napi_get_undefined(env, &result[PARAM1]);
    }

    if (context->deferred) {
        if (!context->status) {
            napi_resolve_deferred(env, context->deferred, result[PARAM1]);
        } else {
            napi_reject_deferred(env, context->deferred, result[PARAM0]);
        }
    } else {
        napi_get_reference_value(env, context->callbackRef, &callback);
        napi_call_function(env, nullptr, callback, ARGS_TWO, result, &retVal);
        napi_delete_reference(env, context->callbackRef);
    }
    napi_delete_async_work(env, context->work);

    delete context;
    context = nullptr;
}

napi_value RingtonePlayerNapi::GetAudioRendererInfo(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value resource = nullptr;
    size_t argc = ARGS_ONE;
    napi_value argv[ARGS_ONE] = {0};
    napi_value thisVar = nullptr;
    const int32_t refCount = 1;

    status = napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    napi_get_undefined(env, &result);
    if (status != napi_ok || thisVar == nullptr) {
        HiLog::Error(LABEL, "Failed to retrieve details about the callback");
        return result;
    }

    NAPI_ASSERT(env, argc <= ARGS_ONE, "requires 1 parameter maximum");
    std::unique_ptr<RingtonePlayerAsyncContext> asyncContext = std::make_unique<RingtonePlayerAsyncContext>();
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc == ARGS_ONE) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        } else {
            napi_create_promise(env, &asyncContext->deferred, &result);
        }

        napi_create_string_utf8(env, "GetAudioRendererInfo", NAPI_AUTO_LENGTH, &resource);
        status = napi_create_async_work(env, nullptr, resource,
            [](napi_env env, void *data) {
                RingtonePlayerAsyncContext *context = static_cast<RingtonePlayerAsyncContext *>(data);
                AudioRendererInfo rendererInfo;
                context->status
                    = context->objectInfo->iRingtonePlayer->GetAudioRendererInfo(rendererInfo);
                if (context->status == SUCCESS) {
                    context->contentType = rendererInfo.contentType;
                    context->streamUsage = rendererInfo.streamUsage;
                    context->rendererFlags  = rendererInfo.rendererFlags;
                }
            },
            AudioRendererInfoAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            HiLog::Error(LABEL, "Failed to get create async work");
            napi_get_undefined(env, &result);
        } else {
            napi_queue_async_work(env, asyncContext->work);
            asyncContext.release();
        }
    }

    return result;
}

napi_value RingtonePlayerNapi::Configure(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value resource = nullptr;
    napi_value property = nullptr;
    size_t argc = ARGS_TWO;
    napi_value argv[ARGS_TWO] = {0};
    napi_value thisVar = nullptr;
    const int32_t refCount = 1;
    double volume;

    status = napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    napi_get_undefined(env, &result);
    if (status != napi_ok || thisVar == nullptr) {
        HiLog::Error(LABEL, "Failed to retrieve details about the callback");
        return result;
    }

    NAPI_ASSERT(env, (argc == ARGS_ONE || argc == ARGS_TWO), "requires 2 parameters maximum");
    std::unique_ptr<RingtonePlayerAsyncContext> asyncContext = std::make_unique<RingtonePlayerAsyncContext>();
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[PARAM0], &valueType);
        if (valueType == napi_object) {
            if ((napi_get_named_property(env, argv[PARAM0], "volume", &property) != napi_ok)
                || napi_get_value_double(env, property, &volume) != napi_ok) {
                HiLog::Error(LABEL, "Could not get the volume argument!");
                NAPI_ASSERT(env, false, "missing properties");
            }
            asyncContext->volume = (float)volume;

            if ((napi_get_named_property(env, argv[PARAM0], "loop", &property) != napi_ok)
                || napi_get_value_bool(env, property, &asyncContext->loop) != napi_ok) {
                HiLog::Error(LABEL, "Could not get the loop argument!");
                NAPI_ASSERT(env, false, "missing properties");
            }
        }

        if (argc == ARGS_TWO) {
            napi_typeof(env, argv[PARAM1], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM1], refCount, &asyncContext->callbackRef);
            }
        } else {
            napi_create_promise(env, &asyncContext->deferred, &result);
        }

        napi_create_string_utf8(env, "Configure", NAPI_AUTO_LENGTH, &resource);
        status = napi_create_async_work(env, nullptr, resource,
            [](napi_env env, void* data) {
                RingtonePlayerAsyncContext* context = static_cast<RingtonePlayerAsyncContext*>(data);
                context->status = context->objectInfo->iRingtonePlayer->Configure(
                    context->volume, context->loop);
            },
            CommonAsyncCallbackComp, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            HiLog::Error(LABEL, "Failed to get create async work");
            napi_get_undefined(env, &result);
        } else {
            napi_queue_async_work(env, asyncContext->work);
            asyncContext.release();
        }
    }

    return result;
}

napi_value RingtonePlayerNapi::Start(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value resource = nullptr;
    size_t argc = ARGS_ONE;
    napi_value argv[ARGS_ONE] = {0};
    napi_value thisVar = nullptr;
    const int32_t refCount = 1;

    status = napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    napi_get_undefined(env, &result);
    if (status != napi_ok || thisVar == nullptr) {
        HiLog::Error(LABEL, "Failed to retrieve details about the callback");
        return result;
    }

    NAPI_ASSERT(env, argc <= ARGS_ONE, "requires 1 parameter maximum");
    std::unique_ptr<RingtonePlayerAsyncContext> asyncContext = std::make_unique<RingtonePlayerAsyncContext>();
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc == ARGS_ONE) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        } else {
            napi_create_promise(env, &asyncContext->deferred, &result);
        }

        napi_create_string_utf8(env, "Start", NAPI_AUTO_LENGTH, &resource);
        status = napi_create_async_work(env, nullptr, resource,
            [](napi_env env, void* data) {
                RingtonePlayerAsyncContext* context = static_cast<RingtonePlayerAsyncContext*>(data);
                context->status = context->objectInfo->iRingtonePlayer->Start();
            },
            CommonAsyncCallbackComp, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            HiLog::Error(LABEL, "Failed to get create async work");
            napi_get_undefined(env, &result);
        } else {
            napi_queue_async_work(env, asyncContext->work);
            asyncContext.release();
        }
    }

    return result;
}

napi_value RingtonePlayerNapi::Stop(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value resource = nullptr;
    size_t argc = ARGS_ONE;
    napi_value argv[ARGS_ONE] = {0};
    napi_value thisVar = nullptr;
    const int32_t refCount = 1;

    status = napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    napi_get_undefined(env, &result);
    if (status != napi_ok || thisVar == nullptr) {
        HiLog::Error(LABEL, "Failed to retrieve details about the callback");
        return result;
    }

    NAPI_ASSERT(env, argc <= ARGS_ONE, "requires 1 parameter maximum");
    std::unique_ptr<RingtonePlayerAsyncContext> asyncContext = std::make_unique<RingtonePlayerAsyncContext>();
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc == ARGS_ONE) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        } else {
            napi_create_promise(env, &asyncContext->deferred, &result);
        }

        napi_create_string_utf8(env, "Stop", NAPI_AUTO_LENGTH, &resource);
        status = napi_create_async_work(env, nullptr, resource,
            [](napi_env env, void* data) {
                RingtonePlayerAsyncContext* context = static_cast<RingtonePlayerAsyncContext*>(data);
                context->status = context->objectInfo->iRingtonePlayer->Stop();
            },
            CommonAsyncCallbackComp, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            HiLog::Error(LABEL, "Failed to get create async work");
            napi_get_undefined(env, &result);
        } else {
            napi_queue_async_work(env, asyncContext->work);
            asyncContext.release();
        }
    }

    return result;
}

napi_value RingtonePlayerNapi::Release(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value resource = nullptr;
    size_t argc = ARGS_ONE;
    napi_value argv[ARGS_ONE] = {0};
    napi_value thisVar = nullptr;
    const int32_t refCount = 1;

    status = napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    napi_get_undefined(env, &result);
    if (status != napi_ok || thisVar == nullptr) {
        HiLog::Error(LABEL, "Failed to retrieve details about the callback");
        return result;
    }

    NAPI_ASSERT(env, argc <= ARGS_ONE, "requires 1 parameter maximum");
    std::unique_ptr<RingtonePlayerAsyncContext> asyncContext = std::make_unique<RingtonePlayerAsyncContext>();
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc == ARGS_ONE) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[PARAM0], &valueType);
            if (valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
            }
        } else {
            napi_create_promise(env, &asyncContext->deferred, &result);
        }

        napi_create_string_utf8(env, "Release", NAPI_AUTO_LENGTH, &resource);
        status = napi_create_async_work(env, nullptr, resource,
            [](napi_env env, void* data) {
                RingtonePlayerAsyncContext* context = static_cast<RingtonePlayerAsyncContext*>(data);
                context->status = context->objectInfo->iRingtonePlayer->Release();
            },
            CommonAsyncCallbackComp, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            HiLog::Error(LABEL, "Failed to get create async work");
            napi_get_undefined(env, &result);
        } else {
            napi_queue_async_work(env, asyncContext->work);
            asyncContext.release();
        }
    }

    return result;
}

napi_value RingtonePlayerNapi::GetAudioState(napi_env env, napi_callback_info info)
{
    napi_status status;
    size_t argc = 0;
    napi_value thisVar = nullptr;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    status = napi_get_cb_info(env, info, &argc, nullptr, &thisVar, nullptr);
    if (status != napi_ok || thisVar == nullptr) {
        HiLog::Error(LABEL, "Get volume fail to napi_get_cb_info");
        return result;
    }

    std::unique_ptr<RingtonePlayerAsyncContext> asyncContext
        = std::make_unique<RingtonePlayerAsyncContext>();
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok) {
        int32_t state = asyncContext->objectInfo->iRingtonePlayer->GetRingtoneState();
        napi_create_int32(env, state, &result);
        if (status == napi_ok) {
            return result;
        }
    }

    return result;
}
} // namespace AudioStandard
} // namespace OHOS