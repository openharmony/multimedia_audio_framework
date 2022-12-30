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

#include "ability.h"
#include "audio_common_napi.h"
#include "audio_errors.h"
#include "audio_info.h"
#include "audio_log.h"
#include "audio_manager_napi.h"
#include "audio_parameters_napi.h"
#include "hilog/log.h"
#include "napi_base_context.h"
#include "securec.h"
#include "tone_player.h"
#include "toneplayer_napi.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
static __thread napi_ref g_tonePlayerConstructor = nullptr;
std::unique_ptr<AudioRendererInfo> TonePlayerNapi::sRendererInfo_ = nullptr;
napi_ref TonePlayerNapi::toneType_ = nullptr;
mutex TonePlayerNapi::createMutex_;
namespace {
    const int ARGS_ONE = 1;
    const int ARGS_TWO = 2;

    const int PARAM0 = 0;
    const int PARAM1 = 1;

    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "TonePlayerNapi"};

#define GET_PARAMS(env, info, num) \
    size_t argc = num;             \
    napi_value argv[num] = {0};    \
    napi_value thisVar = nullptr;  \
    void *data;                    \
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data)
}
TonePlayerNapi::TonePlayerNapi()
    : env_(nullptr), tonePlayer_(nullptr) {}

TonePlayerNapi::~TonePlayerNapi() = default;

void TonePlayerNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<TonePlayerNapi *>(nativeObject);
        delete obj;
        obj = nullptr;
    }
}

napi_status TonePlayerNapi::AddNamedProperty(napi_env env, napi_value object,
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

napi_value TonePlayerNapi::CreateToneTypeObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    AUDIO_DEBUG_LOG("CreateToneTypeObject start");
    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        AUDIO_DEBUG_LOG("CreateToneTypeObject: napi_create_object");
        std::string propName;
        for (auto &iter: toneTypeMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop in CreateToneTypeObject!");
                break;
            }
            propName.clear();
        }
        int32_t refCount = 1;
        if (status == napi_ok) {
            AUDIO_DEBUG_LOG("CreateToneTypeObject: AddNamedProperty");
            status = napi_create_reference(env, result, refCount, &toneType_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateToneTypeObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

bool TonePlayerNapi::ParseRendererInfo(napi_env env, napi_value root, AudioRendererInfo *rendererInfo)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};
    AUDIO_DEBUG_LOG("ParseRendererInfo::Init");
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

napi_value TonePlayerNapi::CreateTonePlayerWrapper(napi_env env, unique_ptr<AudioRendererInfo> &rendererInfo)
{
    lock_guard<mutex> lock(createMutex_);
    napi_status status;
    napi_value result = nullptr;
    napi_value constructor;

    if (rendererInfo != nullptr) {
        status = napi_get_reference_value(env, g_tonePlayerConstructor, &constructor);
        if (status == napi_ok) {
            sRendererInfo_ = move(rendererInfo);
            status = napi_new_instance(env, constructor, 0, nullptr, &result);
            sRendererInfo_.release();
            if (status == napi_ok) {
                return result;
            }
        }
        HiLog::Error(LABEL, "Failed in CreateAudioRendererWrapper, %{public}d", status);
    }

    napi_get_undefined(env, &result);
    return result;
}

void TonePlayerNapi::CommonCallbackRoutine(napi_env env, TonePlayerAsyncContext* &asyncContext,
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

void TonePlayerNapi::GetTonePlayerAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    napi_value valueParam = nullptr;
    auto asyncContext = static_cast<TonePlayerAsyncContext *>(data);

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            unique_ptr<AudioRendererInfo> rendererInfo = make_unique<AudioRendererInfo>();
            rendererInfo->contentType = asyncContext->rendererInfo.contentType;
            rendererInfo->streamUsage = asyncContext->rendererInfo.streamUsage;
            rendererInfo->rendererFlags = asyncContext->rendererInfo.rendererFlags;

            valueParam = CreateTonePlayerWrapper(env, rendererInfo);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: GetTonePlayerAsyncCallbackComplete asyncContext is Null!");
    }
}

napi_value TonePlayerNapi::CreateTonePlayer(napi_env env, napi_callback_info info)
{
    HiLog::Info(LABEL, "%{public}s IN", __func__);
    napi_status status;
    napi_value result = nullptr;
    bool inputRight = true;

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<TonePlayerAsyncContext> asyncContext = make_unique<TonePlayerAsyncContext>();
    CHECK_AND_RETURN_RET_LOG(asyncContext != nullptr, nullptr, "TonePlayerAsyncContext object creation failed");
    if (argc < ARGS_ONE) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }

    for (size_t i = PARAM0; i < argc; i++) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[i], &valueType);
        if (i == PARAM0 && valueType == napi_object) {
            if (!ParseRendererInfo(env, argv[i], &(asyncContext->rendererInfo))) {
                HiLog::Error(LABEL, "Parsing of renderer options failed");
                inputRight = false;
            }
        } else if (i == PARAM1) {
            if (valueType == napi_function) {
                napi_create_reference(env, argv[i], REFERENCE_CREATION_COUNT, &asyncContext->callbackRef);
            }
            break;
        } else {
            HiLog::Error(LABEL, "type mismatch");
            inputRight = false;
        }
    }

    if (asyncContext->callbackRef == nullptr) {
        napi_create_promise(env, &asyncContext->deferred, &result);
    } else {
        napi_get_undefined(env, &result);
    }
    AUDIO_DEBUG_LOG("CreateTonePlayer::CreateAsyncWork");
    napi_value resource = nullptr;
    napi_create_string_utf8(env, "CreateTonePlayer", NAPI_AUTO_LENGTH, &resource);

    if (!inputRight) {
        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<TonePlayerAsyncContext *>(data);
                context->status = NAPI_ERR_INVALID_PARAM;
                HiLog::Error(LABEL, "CreateTonePlayer fail, invalid param!");
            },
            GetTonePlayerAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
    } else {
        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<TonePlayerAsyncContext *>(data);
                context->status = SUCCESS;
            },
            GetTonePlayerAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
    }
    if (status != napi_ok) {
        napi_delete_reference(env, asyncContext->callbackRef);
        result = nullptr;
    } else {
        status = napi_queue_async_work(env, asyncContext->work);
        if (status == napi_ok) {
            asyncContext.release();
        } else {
            result = nullptr;
        }
    }
    AUDIO_DEBUG_LOG("CreateTonePlayer::finished");
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

napi_value TonePlayerNapi::Construct(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<TonePlayerNapi> tonePlayerNapi = make_unique<TonePlayerNapi>();
    CHECK_AND_RETURN_RET_LOG(tonePlayerNapi != nullptr, result, "No memory");

    tonePlayerNapi->env_ = env;
    AudioRendererInfo rendererInfo = {};
    CHECK_AND_RETURN_RET_LOG(sRendererInfo_ != nullptr, result, "Construct sRendererInfo_ is null");
    rendererInfo.contentType = sRendererInfo_->contentType;
    rendererInfo.streamUsage = sRendererInfo_->streamUsage;
    rendererInfo.rendererFlags = sRendererInfo_->rendererFlags;
    std::shared_ptr<AbilityRuntime::Context> abilityContext = GetAbilityContext(env);
    std::string cacheDir = "";
    if (abilityContext != nullptr) {
        cacheDir = abilityContext->GetCacheDir();
    } else {
        cacheDir = "/data/storage/el2/base/haps/entry/files";
    }
    tonePlayerNapi->tonePlayer_ = TonePlayer::Create(cacheDir, rendererInfo);

    CHECK_AND_RETURN_RET_LOG(tonePlayerNapi->tonePlayer_ != nullptr, result, "Toneplayer Create failed");

    status = napi_wrap(env, thisVar, static_cast<void*>(tonePlayerNapi.get()),
                       TonePlayerNapi::Destructor, nullptr, nullptr);
    if (status == napi_ok) {
        tonePlayerNapi.release();
        return thisVar;
    }

    HiLog::Error(LABEL, "Failed in TonePlayerNapi::Construct()!");
    return result;
}

bool TonePlayerNapi::toneTypeCheck(napi_env env, int32_t type)
{
    int32_t len = sizeof(toneTypeArr) / sizeof(toneTypeArr[0]);
    for (int32_t i = 0; i < len; i++) {
        if (toneTypeArr[i] == type) {
            return true;
        }
    }
    return false;
}

napi_value TonePlayerNapi::Load(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;
    bool inputRight = true;

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<TonePlayerAsyncContext> asyncContext = make_unique<TonePlayerAsyncContext>();
    CHECK_AND_RETURN_RET_LOG(asyncContext != nullptr, result, "Load TonePlayerAsyncContext object creation failed");
    if (argc < ARGS_ONE) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }

    status = napi_unwrap(env, thisVar, reinterpret_cast<void **>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_number) {
                napi_get_value_int32(env, argv[PARAM0], &asyncContext->toneType);
                if (!toneTypeCheck(env, asyncContext->toneType)) {
                    HiLog::Error(LABEL, "The Load parameter is invalid");
                    inputRight = false;
                }
            } else if (i == PARAM1) {
                if (valueType == napi_function) {
                    napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                }
                break;
            } else {
                HiLog::Error(LABEL, "type mismatch");
                inputRight = false;
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "Load", NAPI_AUTO_LENGTH, &resource);

        if (!inputRight) {
            status = napi_create_async_work(
                env, nullptr, resource,
                [](napi_env env, void *data) {
                    auto context = static_cast<TonePlayerAsyncContext *>(data);
                    context->status = NAPI_ERR_INVALID_PARAM;
                    HiLog::Error(LABEL, "The Load parameter is invalid");
                },
                VoidAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
        } else {
            status = napi_create_async_work(
                env, nullptr, resource,
                [](napi_env env, void *data) {
                    auto context = static_cast<TonePlayerAsyncContext *>(data);
                    if (context->status == SUCCESS) {
                        ToneType toneType = static_cast<ToneType>(context->toneType);
                        context->intValue = context->objectInfo->tonePlayer_->LoadTone(toneType);
                        if (context->intValue) {
                            context->status = SUCCESS;
                        } else {
                            context->status = NAPI_ERR_SYSTEM;
                        }
                    }
                },
                VoidAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
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
    }
    return result;
}

napi_value TonePlayerNapi::Start(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<TonePlayerAsyncContext> asyncContext = make_unique<TonePlayerAsyncContext>();
    CHECK_AND_RETURN_RET_LOG(asyncContext != nullptr, nullptr, "Start TonePlayerAsyncContext object creation failed");

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
                auto context = static_cast<TonePlayerAsyncContext *>(data);
                context->isTrue = context->objectInfo->tonePlayer_->StartTone();
                if (context->isTrue) {
                    context->status = SUCCESS;
                } else {
                    HiLog::Error(LABEL, "Start call failed, wrong timing!");
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

napi_value TonePlayerNapi::Stop(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<TonePlayerAsyncContext> asyncContext = make_unique<TonePlayerAsyncContext>();
    CHECK_AND_RETURN_RET_LOG(asyncContext != nullptr, nullptr, "Stop TonePlayerAsyncContext object creation failed");

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
                auto context = static_cast<TonePlayerAsyncContext *>(data);
                context->isTrue = context->objectInfo->tonePlayer_->StopTone();
                if (context->isTrue) {
                    context->status = SUCCESS;
                } else {
                    HiLog::Error(LABEL, "Stop call failed, wrong timing");
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

napi_value TonePlayerNapi::Release(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<TonePlayerAsyncContext> asyncContext = make_unique<TonePlayerAsyncContext>();
    CHECK_AND_RETURN_RET_LOG(asyncContext != nullptr, nullptr, "Release TonePlayerAsyncContext object creation failed");

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
                auto context = static_cast<TonePlayerAsyncContext *>(data);
                context->isTrue = context->objectInfo->tonePlayer_->Release();
                if (context->isTrue) {
                    context->status = SUCCESS;
                } else {
                    HiLog::Error(LABEL, "Release call failed, wrong timing");
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

void TonePlayerNapi::VoidAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<TonePlayerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: VoidAsyncCallbackComplete* is Null!");
    }
}

napi_value TonePlayerNapi::Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    const int32_t refCount = 1;
    napi_get_undefined(env, &result);
    AUDIO_DEBUG_LOG("TonePlayerNapi::Init");
    napi_property_descriptor audio_toneplayer_properties[] = {
        DECLARE_NAPI_FUNCTION("load", Load),
        DECLARE_NAPI_FUNCTION("start", Start),
        DECLARE_NAPI_FUNCTION("stop", Stop),
        DECLARE_NAPI_FUNCTION("release", Release),
    };

    napi_property_descriptor static_prop[] = {
        DECLARE_NAPI_STATIC_FUNCTION("createTonePlayer", CreateTonePlayer),
        DECLARE_NAPI_PROPERTY("ToneType", CreateToneTypeObject(env)),
    };

    status = napi_define_class(env, TONE_PLAYER_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct, nullptr,
        sizeof(audio_toneplayer_properties) / sizeof(audio_toneplayer_properties[PARAM0]),
        audio_toneplayer_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }

    status = napi_create_reference(env, constructor, refCount, &g_tonePlayerConstructor);
    if (status == napi_ok) {
        AUDIO_DEBUG_LOG("TonePlayerNapi::Init napi_create_reference");
        status = napi_set_named_property(env, exports, TONE_PLAYER_NAPI_CLASS_NAME.c_str(), constructor);
        if (status == napi_ok) {
            AUDIO_DEBUG_LOG("TonePlayerNapi::Init napi_set_named_property");
            status = napi_define_properties(env, exports,
                                            sizeof(static_prop) / sizeof(static_prop[PARAM0]), static_prop);
            if (status == napi_ok) {
                AUDIO_DEBUG_LOG("TonePlayerNapi::Init napi_define_properties");
                return exports;
            }
        }
    }

    HiLog::Error(LABEL, "Failure in TonePlayerNapi::Init()");
    return result;
}
}
} //