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

#include "audio_volume_manager_napi.h"
#include "audio_volume_group_manager_napi.h"

#include "audio_common_napi.h"
#include "audio_volume_key_event_napi.h"
#include "audio_errors.h"
#include "audio_log.h"
#include "hilog/log.h"
#include "napi_base_context.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
static __thread napi_ref g_volumeManagerConstructor = nullptr;

namespace {
    const int ARGS_ONE = 1;
    const int ARGS_TWO = 2;
    const int PARAM0 = 0;
    const int PARAM1 = 1;
    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "AudioVolumeManagerNapi"};
    const std::string VOLUME_CHANGE_CALLBACK_NAME = "volumeChange";
}

struct AudioVolumeManagerAsyncContext {
    napi_env env;
    napi_async_work work;
    napi_deferred deferred;
    napi_ref callbackRef = nullptr;
    int32_t deviceFlag;
    bool bArgTransFlag = true;
    int32_t status = SUCCESS;
    int32_t groupId;
    int32_t intValue;
    int32_t ringMode;
    bool isMute;
    bool isTrue;
    std::string networkId;
    AudioVolumeManagerNapi *objectInfo;
    vector<sptr<VolumeGroupInfo>> volumeGroupInfos;
};

AudioVolumeManagerNapi::AudioVolumeManagerNapi()
    : audioSystemMngr_(nullptr), env_(nullptr) {}

AudioVolumeManagerNapi::~AudioVolumeManagerNapi() = default;

void AudioVolumeManagerNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<AudioVolumeManagerNapi *>(nativeObject);
        delete obj;
        obj = nullptr;
    }
}

napi_value AudioVolumeManagerNapi::Construct(napi_env env, napi_callback_info info)
{
    AUDIO_INFO_LOG("AudioVolumeManagerNapi::Construct");
    napi_status status;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    size_t argc = ARGS_TWO;
    napi_value argv[ARGS_TWO] = {0};
    napi_value thisVar = nullptr;
    void *data = nullptr;
    
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);
    unique_ptr<AudioVolumeManagerNapi> audioVolumeManagerNapi = make_unique<AudioVolumeManagerNapi>();
    CHECK_AND_RETURN_RET_LOG(audioVolumeManagerNapi != nullptr, result, "No memory");

    audioVolumeManagerNapi->audioSystemMngr_ = AudioSystemManager::GetInstance();
    audioVolumeManagerNapi->env_ = env;
    audioVolumeManagerNapi->cachedClientId_ = getpid();

    status = napi_wrap(env, thisVar, static_cast<void*>(audioVolumeManagerNapi.get()),
        AudioVolumeManagerNapi::Destructor, nullptr, nullptr);
    if (status == napi_ok) {
        audioVolumeManagerNapi.release();
        return thisVar;
    }

    HiLog::Error(LABEL, "Failed in AudioVolumeManager::Construct()!");
    return result;
}

napi_value AudioVolumeManagerNapi::CreateVolumeManagerWrapper(napi_env env)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value constructor;

    status = napi_get_reference_value(env, g_volumeManagerConstructor, &constructor);
    if (status == napi_ok) {
        status = napi_new_instance(env, constructor, 0, nullptr, &result);
        if (status == napi_ok) {
            return result;
        }
    }
    HiLog::Error(LABEL, "Failed in AudioVolumeManagerNapi::CreateVolumeManagerWrapper!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioVolumeManagerNapi::Init(napi_env env, napi_value exports)
{
    AUDIO_INFO_LOG("AudioVolumeManagerNapi::Init");
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    const int32_t refCount = 1;
    napi_get_undefined(env, &result);

    napi_property_descriptor audio_volume_manager_properties[] = {
        DECLARE_NAPI_FUNCTION("getVolumeGroupInfos", GetVolumeGroupInfos),
        DECLARE_NAPI_FUNCTION("getVolumeGroupManager", GetVolumeGroupManager),
        DECLARE_NAPI_FUNCTION("on", On),

    };

    status = napi_define_class(env, AUDIO_VOLUME_MANAGER_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct, nullptr,
        sizeof(audio_volume_manager_properties) / sizeof(audio_volume_manager_properties[PARAM0]),
        audio_volume_manager_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }
    status = napi_create_reference(env, constructor, refCount, &g_volumeManagerConstructor);
    if (status == napi_ok) {
        status = napi_set_named_property(env, exports, AUDIO_VOLUME_MANAGER_NAPI_CLASS_NAME.c_str(), constructor);
        if (status == napi_ok) {
            return exports;
        }
    }

    HiLog::Error(LABEL, "Failure in AudioVolumeManagerNapi::Init()");
    return result;
}

static void SetValueInt32(const napi_env& env, const std::string& fieldStr, const int intValue, napi_value& result)
{
    napi_value value = nullptr;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void SetValueString(const napi_env& env, const std::string& fieldStr, const std::string stringValue,
    napi_value& result)
{
    napi_value value = nullptr;
    napi_create_string_utf8(env, stringValue.c_str(), NAPI_AUTO_LENGTH, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void CommonCallbackRoutine(napi_env env, AudioVolumeManagerAsyncContext *&asyncContext,
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

static void GetVolumeGroupInfosAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioVolumeManagerAsyncContext*>(data);

    napi_value result[ARGS_TWO] = { 0 };
    napi_value valueParam = nullptr;
    if (asyncContext != nullptr) {
        if (asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        } else {
            size_t size = asyncContext->volumeGroupInfos.size();
            HiLog::Info(LABEL, "number of devices = %{public}zu", size);
            napi_create_array_with_length(env, size, &result[PARAM1]);
            for (size_t i = 0; i < size; i++) {
                if (asyncContext->volumeGroupInfos[i] != nullptr) {
                    (void)napi_create_object(env, &valueParam);
                    SetValueString(env, "networkId", static_cast<std::string>(
                        asyncContext->volumeGroupInfos[i]->networkId_), valueParam);
                    SetValueInt32(env, "groupId", static_cast<int32_t>(
                        asyncContext->volumeGroupInfos[i]->volumeGroupId_), valueParam);
                    SetValueInt32(env, "mappingId", static_cast<int32_t>(
                        asyncContext->volumeGroupInfos[i]->mappingId_), valueParam);
                    SetValueString(env, "groupName", static_cast<std::string>(
                        asyncContext->volumeGroupInfos[i]->groupName_), valueParam);
                    SetValueInt32(env, "ConnectType", static_cast<int32_t>(
                        asyncContext->volumeGroupInfos[i]->connectType_), valueParam);
                    napi_set_element(env, result[PARAM1], i, valueParam);
                }
            }
        }
        CommonCallbackRoutine(env, asyncContext, result[PARAM1]);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioRoutingManagerAsyncContext* is Null!");
    }
}

napi_value AudioVolumeManagerNapi::GetVolumeGroupInfos(napi_env env, napi_callback_info info)
{
    HiLog::Info(LABEL, " %{public}s IN", __func__);

    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<AudioVolumeManagerAsyncContext> asyncContext = make_unique<AudioVolumeManagerAsyncContext>();
    if (argc < ARGS_ONE) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);
            if (i == PARAM0 && valueType == napi_string) {
                asyncContext->networkId = AudioCommonNapi::GetStringArgument(env, argv[i]);
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
        napi_create_string_utf8(env, "GetVolumeGroupInfos", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void* data) {
                auto context = static_cast<AudioVolumeManagerAsyncContext*>(data);
                if (!context->status) {
                    context->volumeGroupInfos = context->objectInfo->audioSystemMngr_->
                        GetVolumeGroups(context->networkId);
                    context->status = 0;
                }
            },
            GetVolumeGroupInfosAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
        if (status != napi_ok) {
            AudioCommonNapi::throwError(env, NAPI_ERR_SYSTEM);
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

static void GetGroupMgrAsyncCallbackComplete(napi_env env, napi_status status, void* data)
{
    napi_value valueParam = nullptr;
    auto asyncContext = static_cast<AudioVolumeManagerAsyncContext *>(data);

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            valueParam = AudioVolumeGroupManagerNapi::CreateAudioVolumeGroupManagerWrapper(env, asyncContext->groupId);
            asyncContext->status = AudioVolumeGroupManagerNapi::isConstructSuccess_;
            AudioVolumeGroupManagerNapi::isConstructSuccess_ = SUCCESS;
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: GetStreamMgrAsyncCallbackComplete asyncContext is Null!");
    }
}

napi_value AudioVolumeManagerNapi::GetVolumeGroupManager(napi_env env, napi_callback_info info)
{
    HiLog::Info(LABEL, " %{public}s IN", __func__);
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<AudioVolumeManagerAsyncContext> asyncContext = make_unique<AudioVolumeManagerAsyncContext>();
    CHECK_AND_RETURN_RET_LOG(asyncContext != nullptr, nullptr, "AudioVolumeManagerAsyncContext object creation failed");
    if (argc < ARGS_ONE) {
        asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }
    for (size_t i = PARAM0; i < argc; i++) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[i], &valueType);
        if (i == PARAM0 && valueType == napi_number) {
            napi_get_value_int32(env, argv[i], &asyncContext->groupId);
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
    napi_create_string_utf8(env, "GetVolumeGroupManager", NAPI_AUTO_LENGTH, &resource);

    status = napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            auto context = static_cast<AudioVolumeManagerAsyncContext *>(data);
            if (!context->status) {
                context->status = SUCCESS;
            }
        },
        GetGroupMgrAsyncCallbackComplete, static_cast<void *>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioVolumeManagerNapi::On(napi_env env, napi_callback_info info)
{
    napi_value undefinedResult = nullptr;
    napi_get_undefined(env, &undefinedResult);

    const size_t minArgCount = 2;
    size_t argCount = 3;
    napi_value args[minArgCount + 1] = {nullptr, nullptr, nullptr};
    napi_value jsThis = nullptr;
    napi_status status = napi_get_cb_info(env, info, &argCount, args, &jsThis, nullptr);
    if (status != napi_ok || argCount < minArgCount) {
        AUDIO_ERR_LOG("On fail to napi_get_cb_info/Requires min 2 parameters");
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
    }

    napi_valuetype eventType = napi_undefined;
    if (napi_typeof(env, args[PARAM0], &eventType) != napi_ok || eventType != napi_string) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return undefinedResult;
    }
    std::string callbackName = AudioCommonNapi::GetStringArgument(env, args[0]);
    AUDIO_INFO_LOG("AudioVolumeManagerNapi::On callbackName: %{public}s", callbackName.c_str());

    AudioVolumeManagerNapi *volumeManagerNapi = nullptr;
    status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&volumeManagerNapi));
    NAPI_ASSERT(env, status == napi_ok && volumeManagerNapi != nullptr,
        "Failed to retrieve audio manager napi instance.");
    NAPI_ASSERT(env, volumeManagerNapi->audioSystemMngr_ != nullptr,
        "audio system manager instance is null.");

    napi_valuetype handler = napi_undefined;
    if (napi_typeof(env, args[PARAM1], &handler) != napi_ok || handler != napi_function) {
        AUDIO_ERR_LOG("AudioVolumeManagerNapi::On type mismatch for parameter 2");
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return undefinedResult;
    }

    if (!callbackName.compare(VOLUME_CHANGE_CALLBACK_NAME)) {
        if (volumeManagerNapi->volumeKeyEventCallbackNapi_ == nullptr) {
            volumeManagerNapi->volumeKeyEventCallbackNapi_ = std::make_shared<AudioVolumeKeyEventNapi>(env);
            int32_t ret = volumeManagerNapi->audioSystemMngr_->RegisterVolumeKeyEventCallback(
                volumeManagerNapi->cachedClientId_, volumeManagerNapi->volumeKeyEventCallbackNapi_);
            if (ret) {
                AUDIO_ERR_LOG("AudioVolumeManagerNapi:: RegisterVolumeKeyEventCallback Failed");
            } else {
                AUDIO_DEBUG_LOG("AudioVolumeManagerNapi:: RegisterVolumeKeyEventCallback Success");
            }
        }
        std::shared_ptr<AudioVolumeKeyEventNapi> cb =
            std::static_pointer_cast<AudioVolumeKeyEventNapi>(volumeManagerNapi->volumeKeyEventCallbackNapi_);
        cb->SaveCallbackReference(callbackName, args[PARAM1]);
    } else {
        AUDIO_ERR_LOG("AudioVolumeManagerNapi::No such callback supported");
        AudioCommonNapi::throwError(env, NAPI_ERR_INVALID_PARAM);
    }
    return undefinedResult;
}
} // namespace AudioStandard
} // namespace OHOS
