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

#include "audio_stream_mgr_napi.h"

#include "audio_common_napi.h"
#include "audio_errors.h"
#include "audio_log.h"
#include "hilog/log.h"
#include "napi_base_context.h"
#include "audio_capturer_state_callback_napi.h"
#include "audio_renderer_state_callback_napi.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
static __thread napi_ref g_streamMgrConstructor = nullptr;

namespace {
    const std::string RENDERERCHANGE_CALLBACK_NAME = "audioRendererChange";
    const std::string CAPTURERCHANGE_CALLBACK_NAME = "audioCapturerChange";

    const int ARGS_ONE = 1;
    const int ARGS_TWO = 2;

    const int PARAM0 = 0;
    const int PARAM1 = 1;
    const int PARAM2 = 2;
    const int PARAM3 = 3;

    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "AudioStreamManagerNapi"};

#define GET_PARAMS(env, info, num) \
    size_t argc = num;             \
    napi_value argv[num] = {0};    \
    napi_value thisVar = nullptr;  \
    void *data;                    \
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data)
}

struct AudioStreamMgrAsyncContext {
    napi_env env;
    napi_async_work work;
    napi_deferred deferred;
    napi_ref callbackRef = nullptr;
    int32_t status = SUCCESS;
    int32_t volType;
    bool isTrue;
    bool isLowLatencySupported;
    bool isActive;
    AudioStreamInfo audioStreamInfo;
    AudioStreamMgrNapi *objectInfo;
    vector<unique_ptr<AudioRendererChangeInfo>> audioRendererChangeInfos;
    vector<unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
};

AudioStreamMgrNapi::AudioStreamMgrNapi()
    : env_(nullptr), audioStreamMngr_(nullptr) {}

AudioStreamMgrNapi::~AudioStreamMgrNapi() = default;

void AudioStreamMgrNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<AudioStreamMgrNapi *>(nativeObject);
        delete obj;
        obj = nullptr;
    }
}


static AudioVolumeType GetNativeAudioVolumeType(int32_t volumeType)
{
    AudioVolumeType result = STREAM_MUSIC;

    switch (volumeType) {
        case AudioManagerNapi::RINGTONE:
            result = STREAM_RING;
            break;
        case AudioManagerNapi::MEDIA:
            result = STREAM_MUSIC;
            break;
        case AudioManagerNapi::VOICE_CALL:
            result = STREAM_VOICE_CALL;
            break;
        case AudioManagerNapi::VOICE_ASSISTANT:
            result = STREAM_VOICE_ASSISTANT;
            break;
        case AudioManagerNapi::ALL:
            result = STREAM_ALL;
            break;
        default:
            result = STREAM_MUSIC;
            HiLog::Error(LABEL, "Unknown volume type, Set it to default MEDIA!");
            break;
    }

    return result;
}
static void SetValueInt32(const napi_env& env, const std::string& fieldStr, const int intValue, napi_value &result)
{
    napi_value value = nullptr;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void SetValueString(const napi_env &env, const std::string &fieldStr, const std::string stringValue,
    napi_value &result)
{
    napi_value value = nullptr;
    napi_create_string_utf8(env, stringValue.c_str(), NAPI_AUTO_LENGTH, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void SetDeviceDescriptors(const napi_env& env, napi_value &jsChangeInfoObj, const DeviceInfo &deviceInfo)
{
    napi_value jsDeviceDescriptorsObj = nullptr;
    napi_value valueParam = nullptr;
    napi_create_array_with_length(env, 1, &jsDeviceDescriptorsObj);

    (void)napi_create_object(env, &valueParam);
    SetValueInt32(env, "deviceRole", static_cast<int32_t>(deviceInfo.deviceRole), valueParam);
    SetValueInt32(env, "deviceType", static_cast<int32_t>(deviceInfo.deviceType), valueParam);
    SetValueInt32(env, "id", static_cast<int32_t>(deviceInfo.deviceId), valueParam);
    SetValueString(env, "name", deviceInfo.deviceName, valueParam);
    SetValueString(env, "address", deviceInfo.macAddress, valueParam);
    SetValueString(env, "networkId", deviceInfo.networkId, valueParam);

    napi_value value = nullptr;
    napi_value sampleRates;
    napi_create_array_with_length(env, 1, &sampleRates);
    napi_create_int32(env, deviceInfo.audioStreamInfo.samplingRate, &value);
    napi_set_element(env, sampleRates, 0, value);
    napi_set_named_property(env, valueParam, "sampleRates", sampleRates);

    napi_value channelCounts;
    napi_create_array_with_length(env, 1, &channelCounts);
    napi_create_int32(env, deviceInfo.audioStreamInfo.channels, &value);
    napi_set_element(env, channelCounts, 0, value);
    napi_set_named_property(env, valueParam, "channelCounts", channelCounts);

    napi_value channelMasks;
    napi_create_array_with_length(env, 1, &channelMasks);
    napi_create_int32(env, deviceInfo.channelMasks, &value);
    napi_set_element(env, channelMasks, 0, value);
    napi_set_named_property(env, valueParam, "channelMasks", channelMasks);

    napi_set_element(env, jsDeviceDescriptorsObj, 0, valueParam);
    napi_set_named_property(env, jsChangeInfoObj, "deviceDescriptors", jsDeviceDescriptorsObj);
}

static void GetCurrentRendererChangeInfosCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioStreamMgrAsyncContext*>(data);
    napi_value result[ARGS_TWO] = {0};
    napi_value jsChangeInfoObj = nullptr;
    napi_value jsRenInfoObj = nullptr;
    napi_value retVal;

    size_t size = asyncContext->audioRendererChangeInfos.size();
    int32_t position = 0;

    napi_create_array_with_length(env, size, &result[PARAM1]);
    for (const unique_ptr<AudioRendererChangeInfo> &changeInfo: asyncContext->audioRendererChangeInfos) {
        if (!changeInfo) {
            AUDIO_ERR_LOG("AudioStreamMgrNapi:AudioRendererChangeInfo Null, something wrong!!");
            continue;
        }

        napi_create_object(env, &jsChangeInfoObj);
        SetValueInt32(env, "streamId", changeInfo->sessionId, jsChangeInfoObj);
        SetValueInt32(env, "rendererState", static_cast<int32_t>(changeInfo->rendererState), jsChangeInfoObj);
        SetValueInt32(env, "clientUid", changeInfo->clientUID, jsChangeInfoObj);

        napi_create_object(env, &jsRenInfoObj);
        SetValueInt32(env, "content", static_cast<int32_t>(changeInfo->rendererInfo.contentType), jsRenInfoObj);
        SetValueInt32(env, "usage", static_cast<int32_t>(changeInfo->rendererInfo.streamUsage), jsRenInfoObj);
        SetValueInt32(env, "rendererFlags", changeInfo->rendererInfo.rendererFlags, jsRenInfoObj);
        napi_set_named_property(env, jsChangeInfoObj, "rendererInfo", jsRenInfoObj);
        SetDeviceDescriptors(env, jsChangeInfoObj, changeInfo->outputDeviceInfo);

        napi_set_element(env, result[PARAM1], position, jsChangeInfoObj);
        position++;
    }

    napi_get_undefined(env, &result[PARAM0]);

    if (asyncContext->deferred) {
        napi_resolve_deferred(env, asyncContext->deferred, result[PARAM1]);
    } else {
        napi_value callback = nullptr;
        napi_get_reference_value(env, asyncContext->callbackRef, &callback);
        napi_call_function(env, nullptr, callback, ARGS_TWO, result, &retVal);
        napi_delete_reference(env, asyncContext->callbackRef);
    }
    napi_delete_async_work(env, asyncContext->work);

    delete asyncContext;
}

static void GetCurrentCapturerChangeInfosCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioStreamMgrAsyncContext*>(data);
    napi_value result[ARGS_TWO] = {0};
    napi_value jsChangeInfoObj = nullptr;
    napi_value jsCapInfoObj = nullptr;
    napi_value retVal;

    size_t size = asyncContext->audioCapturerChangeInfos.size();
    int32_t position = 0;

    napi_create_array_with_length(env, size, &result[PARAM1]);
    for (const unique_ptr<AudioCapturerChangeInfo> &changeInfo: asyncContext->audioCapturerChangeInfos) {
        if (!changeInfo) {
            AUDIO_ERR_LOG("AudioStreamMgrNapi:AudioCapturerChangeInfo Null, something wrong!!");
            continue;
        }

        napi_create_object(env, &jsChangeInfoObj);
        SetValueInt32(env, "streamId", changeInfo->sessionId, jsChangeInfoObj);
        SetValueInt32(env, "capturerState", static_cast<int32_t>(changeInfo->capturerState), jsChangeInfoObj);
        SetValueInt32(env, "clientUid", changeInfo->clientUID, jsChangeInfoObj);

        napi_create_object(env, &jsCapInfoObj);
        SetValueInt32(env, "source", static_cast<int32_t>(changeInfo->capturerInfo.sourceType), jsCapInfoObj);
        SetValueInt32(env, "capturerFlags", changeInfo->capturerInfo.capturerFlags, jsCapInfoObj);
        napi_set_named_property(env, jsChangeInfoObj, "capturerInfo", jsCapInfoObj);
        SetDeviceDescriptors(env, jsChangeInfoObj, changeInfo->inputDeviceInfo);

        napi_set_element(env, result[PARAM1], position, jsChangeInfoObj);
        position++;
    }

    napi_get_undefined(env, &result[PARAM0]);

    if (asyncContext->deferred) {
        napi_resolve_deferred(env, asyncContext->deferred, result[PARAM1]);
    } else {
        napi_value callback = nullptr;
        napi_get_reference_value(env, asyncContext->callbackRef, &callback);
        napi_call_function(env, nullptr, callback, ARGS_TWO, result, &retVal);
        napi_delete_reference(env, asyncContext->callbackRef);
    }
    napi_delete_async_work(env, asyncContext->work);

    delete asyncContext;
}

napi_value AudioStreamMgrNapi::Init(napi_env env, napi_value exports)
{
    AUDIO_INFO_LOG("AudioStreamMgrNapi::Init");
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    const int32_t refCount = 1;
    napi_get_undefined(env, &result);

    napi_property_descriptor audio_stream_mgr_properties[] = {
        DECLARE_NAPI_FUNCTION("on", On),
        DECLARE_NAPI_FUNCTION("off", Off),
        DECLARE_NAPI_FUNCTION("getCurrentAudioRendererInfoArray", GetCurrentAudioRendererInfos),
        DECLARE_NAPI_FUNCTION("getCurrentAudioCapturerInfoArray", GetCurrentAudioCapturerInfos),
        DECLARE_NAPI_FUNCTION("isAudioRendererLowLatencySupported", IsAudioRendererLowLatencySupported),
        DECLARE_NAPI_FUNCTION("isActive", IsStreamActive),

    };

    status = napi_define_class(env, AUDIO_STREAM_MGR_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct, nullptr,
        sizeof(audio_stream_mgr_properties) / sizeof(audio_stream_mgr_properties[PARAM0]),
        audio_stream_mgr_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }

    status = napi_create_reference(env, constructor, refCount, &g_streamMgrConstructor);
    if (status == napi_ok) {
        status = napi_set_named_property(env, exports, AUDIO_STREAM_MGR_NAPI_CLASS_NAME.c_str(), constructor);
        if (status == napi_ok) {
            return exports;
        }
    }

    HiLog::Error(LABEL, "Failure in AudioStreamMgrNapi::Init()");
    return result;
}

napi_value AudioStreamMgrNapi::CreateStreamManagerWrapper(napi_env env)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value constructor;

    status = napi_get_reference_value(env, g_streamMgrConstructor, &constructor);
    if (status == napi_ok) {
        status = napi_new_instance(env, constructor, 0, nullptr, &result);
        if (status == napi_ok) {
            return result;
        }
    }
    HiLog::Error(LABEL, "Failed in AudioStreamManagerNapi::CreateStreamMngrWrapper!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioStreamMgrNapi::Construct(napi_env env, napi_callback_info info)
{
    AUDIO_INFO_LOG("AudioStreamMgrNapi::Construct");
    napi_status status;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<AudioStreamMgrNapi> streamMgrNapi = make_unique<AudioStreamMgrNapi>();
    CHECK_AND_RETURN_RET_LOG(streamMgrNapi != nullptr, result, "No memory");

    streamMgrNapi->env_ = env;
    streamMgrNapi->audioStreamMngr_ = AudioStreamManager::GetInstance();
    streamMgrNapi->cachedClientId_ = getpid();

    status = napi_wrap(env, thisVar, static_cast<void*>(streamMgrNapi.get()),
                       AudioStreamMgrNapi::Destructor, nullptr, nullptr);
    if (status == napi_ok) {
        streamMgrNapi.release();
        return thisVar;
    }

    HiLog::Error(LABEL, "Failed in AudioStreamManager::Construct()!");
    return result;
}

void AudioStreamMgrNapi::RegisterRendererStateChangeCallback(napi_env env, napi_value* args,
    const std::string& cbName, AudioStreamMgrNapi *streamMgrNapi)
{
    if (!streamMgrNapi->rendererStateChangeCallbackNapi_) {
        streamMgrNapi->rendererStateChangeCallbackNapi_ = std::make_shared<AudioRendererStateCallbackNapi>(env);
        if (!streamMgrNapi->rendererStateChangeCallbackNapi_) {
            AUDIO_ERR_LOG("AudioStreamMgrNapi: Memory Allocation Failed !!");
            return;
        }

        int32_t ret =
            streamMgrNapi->audioStreamMngr_->RegisterAudioRendererEventListener(streamMgrNapi->cachedClientId_,
            streamMgrNapi->rendererStateChangeCallbackNapi_);
        if (ret) {
            AUDIO_ERR_LOG("AudioStreamMgrNapi: Registering of Renderer State Change Callback Failed");
            return;
        }
    }

    std::shared_ptr<AudioRendererStateCallbackNapi> cb =
    std::static_pointer_cast<AudioRendererStateCallbackNapi>(streamMgrNapi->rendererStateChangeCallbackNapi_);
    cb->SaveCallbackReference(args[PARAM1]);

    AUDIO_INFO_LOG("AudioStreamMgrNapi::OnRendererStateChangeCallback is successful");
}

void AudioStreamMgrNapi::RegisterCapturerStateChangeCallback(napi_env env, napi_value* args,
    const std::string& cbName, AudioStreamMgrNapi *streamMgrNapi)
{
    if (!streamMgrNapi->capturerStateChangeCallbackNapi_) {
        streamMgrNapi->capturerStateChangeCallbackNapi_ = std::make_shared<AudioCapturerStateCallbackNapi>(env);
        if (!streamMgrNapi->capturerStateChangeCallbackNapi_) {
            AUDIO_ERR_LOG("AudioStreamMgrNapi: Memory Allocation Failed !!");
            return;
        }

        int32_t ret =
            streamMgrNapi->audioStreamMngr_->RegisterAudioCapturerEventListener(streamMgrNapi->cachedClientId_,
            streamMgrNapi->capturerStateChangeCallbackNapi_);
        if (ret) {
            AUDIO_ERR_LOG("AudioStreamMgrNapi: Registering of Capturer State Change Callback Failed");
            return;
        }
    }

    std::shared_ptr<AudioCapturerStateCallbackNapi> cb =
    std::static_pointer_cast<AudioCapturerStateCallbackNapi>(streamMgrNapi->capturerStateChangeCallbackNapi_);
    cb->SaveCallbackReference(args[PARAM1]);

    AUDIO_INFO_LOG("AudioStreamMgrNapi::OnCapturerStateChangeCallback is successful");
}

void AudioStreamMgrNapi::RegisterCallback(napi_env env, napi_value jsThis,
    napi_value* args, const std::string& cbName)
{
    AudioStreamMgrNapi *streamMgrNapi = nullptr;
    napi_status status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&streamMgrNapi));
    if ((status != napi_ok) || (streamMgrNapi == nullptr) || (streamMgrNapi->audioStreamMngr_ == nullptr)) {
        AUDIO_ERR_LOG("AudioStreamMgrNapi::Failed to retrieve stream mgr napi instance.");
        return;
    }

    if (!cbName.compare(RENDERERCHANGE_CALLBACK_NAME)) {
        RegisterRendererStateChangeCallback(env, args, cbName, streamMgrNapi);
    } else if (!cbName.compare(CAPTURERCHANGE_CALLBACK_NAME)) {
        RegisterCapturerStateChangeCallback(env, args, cbName, streamMgrNapi);
    } else {
        AUDIO_ERR_LOG("AudioStreamMgrNapi::No such callback supported");
        AudioCommonNapi::throwError(env, NAPI_ERR_INVALID_PARAM);
    }
}

napi_value AudioStreamMgrNapi::On(napi_env env, napi_callback_info info)
{
    const size_t requireArgc = PARAM2;
    size_t argc = PARAM3;

    napi_value undefinedResult = nullptr;
    napi_get_undefined(env, &undefinedResult);

    napi_value args[requireArgc + 1] = {nullptr, nullptr, nullptr};
    napi_value jsThis = nullptr;
    napi_status status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    THROW_ERROR_ASSERT(env, status == napi_ok && argc == requireArgc, NAPI_ERR_INPUT_INVALID);

    napi_valuetype eventType = napi_undefined;
    napi_typeof(env, args[0], &eventType);
    THROW_ERROR_ASSERT(env, eventType == napi_string, NAPI_ERR_INPUT_INVALID);

    std::string callbackName = AudioCommonNapi::GetStringArgument(env, args[0]);
    AUDIO_DEBUG_LOG("AudioStreamMgrNapi: On callbackName: %{public}s", callbackName.c_str());

    napi_valuetype handler = napi_undefined;
 
    napi_typeof(env, args[1], &handler);
    THROW_ERROR_ASSERT(env, handler == napi_function, NAPI_ERR_INPUT_INVALID);
  
    RegisterCallback(env, jsThis, args, callbackName);

    return undefinedResult;
}

void AudioStreamMgrNapi::UnregisterCallback(napi_env env, napi_value jsThis, const std::string& cbName)
{
    AUDIO_INFO_LOG("AudioStreamMgrNapi::UnregisterCallback");
    AudioStreamMgrNapi *streamMgrNapi = nullptr;
    napi_status status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&streamMgrNapi));
    if ((status != napi_ok) || (streamMgrNapi == nullptr) || (streamMgrNapi->audioStreamMngr_ == nullptr)) {
        AUDIO_ERR_LOG("AudioStreamMgrNapi::Failed to retrieve stream mgr napi instance.");
        return;
    }

    if (!cbName.compare(RENDERERCHANGE_CALLBACK_NAME)) {
        int32_t ret = streamMgrNapi->audioStreamMngr_->
            UnregisterAudioRendererEventListener(streamMgrNapi->cachedClientId_);
        if (ret) {
            AUDIO_ERR_LOG("AudioStreamMgrNapi:UnRegistering of Renderer State Change Callback Failed");
            return;
        }
        if (streamMgrNapi->rendererStateChangeCallbackNapi_ != nullptr) {
            streamMgrNapi->rendererStateChangeCallbackNapi_.reset();
            streamMgrNapi->rendererStateChangeCallbackNapi_ = nullptr;
        }
        AUDIO_INFO_LOG("AudioStreamMgrNapi:UnRegistering of renderer State Change Callback successful");
    } else if (!cbName.compare(CAPTURERCHANGE_CALLBACK_NAME)) {
        int32_t ret = streamMgrNapi->audioStreamMngr_->
            UnregisterAudioCapturerEventListener(streamMgrNapi->cachedClientId_);
        if (ret) {
            AUDIO_ERR_LOG("AudioStreamMgrNapi:UnRegistering of capturer State Change Callback Failed");
            return;
        }
        if (streamMgrNapi->capturerStateChangeCallbackNapi_ != nullptr) {
            streamMgrNapi->capturerStateChangeCallbackNapi_.reset();
            streamMgrNapi->capturerStateChangeCallbackNapi_ = nullptr;
        }
        AUDIO_INFO_LOG("AudioStreamMgrNapi:UnRegistering of capturer State Change Callback successful");
    } else {
        AUDIO_ERR_LOG("AudioStreamMgrNapi::No such callback supported");
        AudioCommonNapi::throwError(env, NAPI_ERR_INVALID_PARAM);
    }
}

napi_value AudioStreamMgrNapi::Off(napi_env env, napi_callback_info info)
{
    const size_t requireArgc = 1;
    size_t argc = 1;

    napi_value undefinedResult = nullptr;
    napi_get_undefined(env, &undefinedResult);

    napi_value args[requireArgc] = {nullptr};
    napi_value jsThis = nullptr;
    napi_status status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    THROW_ERROR_ASSERT(env, status == napi_ok && argc >= requireArgc, NAPI_ERR_INPUT_INVALID);

    napi_valuetype eventType = napi_undefined;
    napi_typeof(env, args[0], &eventType);
    THROW_ERROR_ASSERT(env, eventType == napi_string, NAPI_ERR_INPUT_INVALID);

    std::string callbackName = AudioCommonNapi::GetStringArgument(env, args[0]);
    AUDIO_DEBUG_LOG("AudioStreamMgrNapi: Off callbackName: %{public}s", callbackName.c_str());

    UnregisterCallback(env, jsThis, callbackName);
    return undefinedResult;
}

napi_value AudioStreamMgrNapi::GetCurrentAudioRendererInfos(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioStreamMgrAsyncContext> asyncContext = make_unique<AudioStreamMgrAsyncContext>();
    if (!asyncContext) {
        AUDIO_ERR_LOG("AudioStreamMgrNapi:Audio manager async context failed");
        return result;
    }

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if ((status == napi_ok && asyncContext->objectInfo != nullptr)
        && (asyncContext->objectInfo->audioStreamMngr_ != nullptr)) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0) {
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
        napi_create_string_utf8(env, "getCurrentAudioRendererInfoArray", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioStreamMgrAsyncContext *>(data);
                if (context->status == SUCCESS) {
                    context->status = context->objectInfo->audioStreamMngr_->
                        GetCurrentRendererChangeInfos(context->audioRendererChangeInfos);
                    context->status = context->status == SUCCESS ? SUCCESS : NAPI_ERR_SYSTEM;
                }
            },
            GetCurrentRendererChangeInfosCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioStreamMgrNapi::GetCurrentAudioCapturerInfos(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    unique_ptr<AudioStreamMgrAsyncContext> asyncContext = make_unique<AudioStreamMgrAsyncContext>();
    if (!asyncContext) {
        AUDIO_ERR_LOG("AudioStreamMgrNapi:async context memory alloc failed");
        return result;
    }

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if ((status == napi_ok && asyncContext->objectInfo != nullptr)
        && (asyncContext->objectInfo->audioStreamMngr_ != nullptr)) {
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0) {
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
        napi_create_string_utf8(env, "getCurrentAudioCapturerInfoArray", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioStreamMgrAsyncContext*>(data);
                if (context->status == SUCCESS) {
                    context->objectInfo->audioStreamMngr_->
                        GetCurrentCapturerChangeInfos(context->audioCapturerChangeInfos);
                    context->status = SUCCESS;
                }
            },
            GetCurrentCapturerChangeInfosCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioStreamMgrNapi::IsAudioRendererLowLatencySupported(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    const int32_t refCount = 1;
    napi_get_undefined(env, &result);
    GET_PARAMS(env, info, ARGS_TWO);
    unique_ptr<AudioStreamMgrAsyncContext> asyncContext = make_unique<AudioStreamMgrAsyncContext>();
    CHECK_AND_RETURN_RET_LOG(asyncContext != nullptr, result, "No memory");
    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status != napi_ok) {
        return result;
    }

    for (size_t i = PARAM0; i < argc; i++) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[i], &valueType);
        if (i == PARAM0 && valueType == napi_object) {
            if (!ParseAudioStreamInfo(env, argv[i], asyncContext->audioStreamInfo)) {
                HiLog::Error(LABEL, "Parsing of audiostream failed");
                asyncContext->status = asyncContext->status ==
                    NAPI_ERR_INVALID_PARAM ? NAPI_ERR_INVALID_PARAM : NAPI_ERR_UNSUPPORTED;
            }
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
    napi_create_string_utf8(env, "IsAudioRendererLowLatencySupported", NAPI_AUTO_LENGTH, &resource);
    status = napi_create_async_work(
        env, nullptr, resource,
        [](napi_env env, void *data) {
            auto context = static_cast<AudioStreamMgrAsyncContext*>(data);
            if (context->status == SUCCESS) {
                context->isLowLatencySupported =
                    context->objectInfo->audioStreamMngr_->IsAudioRendererLowLatencySupported(context->audioStreamInfo);
                context->isTrue = context->isLowLatencySupported;
                context->status = SUCCESS;
            }
        },
        IsLowLatencySupportedCallback, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

bool AudioStreamMgrNapi::ParseAudioStreamInfo(napi_env env, napi_value root, AudioStreamInfo &audioStreamInfo)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};

    if (napi_get_named_property(env, root, "samplingRate", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        audioStreamInfo.samplingRate = static_cast<AudioSamplingRate>(intValue);
    }

    if (napi_get_named_property(env, root, "channels", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        audioStreamInfo.channels = static_cast<AudioChannel>(intValue);
    }

    if (napi_get_named_property(env, root, "sampleFormat", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        audioStreamInfo.format = static_cast<OHOS::AudioStandard::AudioSampleFormat>(intValue);
    }

    if (napi_get_named_property(env, root, "encodingType", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        audioStreamInfo.encoding = static_cast<AudioEncodingType>(intValue);
    }

    return true;
}

static void CommonCallbackRoutine(napi_env env, AudioStreamMgrAsyncContext* &asyncContext,
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

void AudioStreamMgrNapi::IsLowLatencySupportedCallback(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioStreamMgrAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_boolean(env, asyncContext->isTrue, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioStreamMgrAsyncContext* is Null!");
    }
}


static void IsTrueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioStreamMgrAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_boolean(env, asyncContext->isTrue, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioStreamMgrAsyncContext* is Null!");
    }
}

napi_value AudioStreamMgrNapi::IsStreamActive(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<AudioStreamMgrAsyncContext> asyncContext = make_unique<AudioStreamMgrAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc < ARGS_ONE) {
            asyncContext->status = NAPI_ERR_INVALID_PARAM;
        }
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_number) {
                napi_get_value_int32(env, argv[i], &asyncContext->volType);
                if (!AudioCommonNapi::IsLegalInputArgumentVolType(asyncContext->volType)) {
                    asyncContext->status = (asyncContext->status ==
                        NAPI_ERR_INVALID_PARAM) ? NAPI_ERR_INVALID_PARAM : NAPI_ERR_UNSUPPORTED;
                }
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
        napi_create_string_utf8(env, "IsStreamActive", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioStreamMgrAsyncContext*>(data);
                if (context->status == SUCCESS) {
                    context->isActive =
                        context->objectInfo->audioMngr_->IsStreamActive(GetNativeAudioVolumeType(context->volType));
                    context->isTrue = context->isActive;
                    context->status = SUCCESS;
                }
            },
            IsTrueAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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
