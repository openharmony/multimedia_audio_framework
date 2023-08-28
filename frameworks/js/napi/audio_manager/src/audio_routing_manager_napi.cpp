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

#include "audio_routing_manager_napi.h"

#include "audio_common_napi.h"
#include "audio_routing_manager_callback_napi.h"
#include "audio_errors.h"
#include "audio_log.h"
#include "hilog/log.h"
#include "napi_base_context.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
static __thread napi_ref g_routingManagerConstructor = nullptr;

namespace {
    const int ARGS_ONE = 1;
    const int ARGS_TWO = 2;
    const int ARGS_THREE = 3;
    const int SIZE = 100;
    const int PARAM0 = 0;
    const int PARAM1 = 1;
    const int PARAM2 = 2;
    const int PARAM3 = 3;

    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "AudioRoutingManagerNapi"};
}

struct AudioRoutingManagerAsyncContext {
    napi_env env;
    napi_async_work work;
    napi_deferred deferred;
    napi_ref callbackRef = nullptr;
    int32_t deviceFlag;
    int32_t status = SUCCESS;
    int32_t deviceType;
    bool isActive;
    bool isTrue;
    bool bArgTransFlag = true;
    AudioRoutingManagerNapi *objectInfo;
    AudioRendererInfo rendererInfo;
    AudioCapturerInfo captureInfo;
    sptr<AudioRendererFilter> audioRendererFilter;
    sptr<AudioCapturerFilter> audioCapturerFilter;
    vector<sptr<AudioDeviceDescriptor>> deviceDescriptors;
    vector<sptr<AudioDeviceDescriptor>> outDeviceDescriptors;
    vector<sptr<AudioDeviceDescriptor>> inputDeviceDescriptors;
};

AudioRoutingManagerNapi::AudioRoutingManagerNapi()
    : audioMngr_(nullptr), env_(nullptr) {}

AudioRoutingManagerNapi::~AudioRoutingManagerNapi() = default;

void AudioRoutingManagerNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<AudioRoutingManagerNapi *>(nativeObject);
        delete obj;
        obj = nullptr;
    }
}

napi_value AudioRoutingManagerNapi::Init(napi_env env, napi_value exports)
{
    AUDIO_INFO_LOG("Init");
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    const int32_t refCount = 1;
    napi_get_undefined(env, &result);

    napi_property_descriptor audio_routing_manager_properties[] = {
        DECLARE_NAPI_FUNCTION("getDevices", GetDevices),
        DECLARE_NAPI_FUNCTION("getDevicesSync", GetDevicesSync),
        DECLARE_NAPI_FUNCTION("on", AudioRoutingManagerNapi::On),
        DECLARE_NAPI_FUNCTION("off", AudioRoutingManagerNapi::Off),
        DECLARE_NAPI_FUNCTION("selectOutputDevice", SelectOutputDevice),
        DECLARE_NAPI_FUNCTION("selectOutputDeviceByFilter", SelectOutputDeviceByFilter),
        DECLARE_NAPI_FUNCTION("selectInputDevice", SelectInputDevice),
        DECLARE_NAPI_FUNCTION("selectInputDeviceByFilter", SelectInputDeviceByFilter),
        DECLARE_NAPI_FUNCTION("setCommunicationDevice", SetCommunicationDevice),
        DECLARE_NAPI_FUNCTION("isCommunicationDeviceActive", IsCommunicationDeviceActive),
        DECLARE_NAPI_FUNCTION("isCommunicationDeviceActiveSync", IsCommunicationDeviceActiveSync),
        DECLARE_NAPI_FUNCTION("getActiveOutputDeviceDescriptors", GetActiveOutputDeviceDescriptors),
        DECLARE_NAPI_FUNCTION("getPreferredOutputDeviceForRendererInfo", GetPreferredOutputDeviceForRendererInfo),
        DECLARE_NAPI_FUNCTION("getPreferOutputDeviceForRendererInfo", GetPreferOutputDeviceForRendererInfo),
        DECLARE_NAPI_FUNCTION("getPreferredOutputDeviceForRendererInfoSync",
            GetPreferredOutputDeviceForRendererInfoSync),
        DECLARE_NAPI_FUNCTION("getPreferredInputDeviceForCapturerInfo", GetPreferredInputDeviceForCapturerInfo),
        DECLARE_NAPI_FUNCTION("getPreferredInputDeviceForCapturerInfoSync", GetPreferredInputDeviceForCapturerInfoSync),
    };

    status = napi_define_class(env, AUDIO_ROUTING_MANAGER_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct, nullptr,
        sizeof(audio_routing_manager_properties) / sizeof(audio_routing_manager_properties[PARAM0]),
        audio_routing_manager_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }
    status = napi_create_reference(env, constructor, refCount, &g_routingManagerConstructor);
    if (status == napi_ok) {
        status = napi_set_named_property(env, exports, AUDIO_ROUTING_MANAGER_NAPI_CLASS_NAME.c_str(), constructor);
        if (status == napi_ok) {
            return exports;
        }
    }

    HiLog::Error(LABEL, "Failure in AudioRoutingManagerNapi::Init()");
    return result;
}

napi_value AudioRoutingManagerNapi::CreateRoutingManagerWrapper(napi_env env)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value constructor;

    status = napi_get_reference_value(env, g_routingManagerConstructor, &constructor);
    if (status == napi_ok) {
        status = napi_new_instance(env, constructor, 0, nullptr, &result);
        if (status == napi_ok) {
            return result;
        }
    }
    HiLog::Error(LABEL, "Failed in AudioRoutingManagerNapi::CreateRoutingManagerWrapper!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioRoutingManagerNapi::Construct(napi_env env, napi_callback_info info)
{
    AUDIO_INFO_LOG("Construct");
    napi_status status;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    size_t argc = ARGS_TWO;
    napi_value argv[ARGS_TWO] = {0};
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);
    unique_ptr<AudioRoutingManagerNapi> audioRoutingManagerNapi = make_unique<AudioRoutingManagerNapi>();
    CHECK_AND_RETURN_RET_LOG(audioRoutingManagerNapi != nullptr, result, "No memory");

    audioRoutingManagerNapi->audioMngr_ = AudioSystemManager::GetInstance();
    
    audioRoutingManagerNapi->audioRoutingMngr_ = AudioRoutingManager::GetInstance();
    
    audioRoutingManagerNapi->env_ = env;

    status = napi_wrap(env, thisVar, static_cast<void*>(audioRoutingManagerNapi.get()),
        AudioRoutingManagerNapi::Destructor, nullptr, nullptr);
    if (status == napi_ok) {
        audioRoutingManagerNapi.release();
        return thisVar;
    }

    HiLog::Error(LABEL, "Failed in AudioRoutingManager::Construct()!");
    return result;
}

static void CommonCallbackRoutine(napi_env env, AudioRoutingManagerAsyncContext *&asyncContext,
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

static void SetFunctionAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRoutingManagerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioManagerAsyncContext* is Null!");
    }
}

static void IsTrueAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRoutingManagerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_boolean(env, asyncContext->isTrue, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioManagerAsyncContext* is Null!");
    }
}

static void ParseAudioRendererInfo(napi_env env, napi_value root, AudioRendererInfo *rendererInfo)
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
}


static void ParseAudioRendererFilter(napi_env env, napi_value root,
    sptr<AudioRendererFilter> &audioRendererFilter, bool &argTransFlag)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};
    argTransFlag = true;
    audioRendererFilter = new(std::nothrow) AudioRendererFilter();

    if (napi_get_named_property(env, root, "uid", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        audioRendererFilter->uid = intValue;
    }

    if (napi_get_named_property(env, root, "rendererInfo", &tempValue) == napi_ok) {
        ParseAudioRendererInfo(env, tempValue, &(audioRendererFilter->rendererInfo));
    }

    if (napi_get_named_property(env, root, "rendererId", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        audioRendererFilter->streamId = intValue;
    }
}

static void ParseAudioDeviceDescriptor(napi_env env, napi_value root,
    sptr<AudioDeviceDescriptor> &selectedAudioDevice, bool &argTransFlag)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};
    char buffer[SIZE];
    size_t res = 0;
    argTransFlag = true;
    bool hasDeviceRole = true;
    bool hasNetworkId  = true;
    napi_has_named_property(env, root, "deviceRole", &hasDeviceRole);
    napi_has_named_property(env, root, "networkId", &hasNetworkId);

    if ((!hasDeviceRole) || (!hasNetworkId)) {
        argTransFlag = false;
        return;
    }

    if (napi_get_named_property(env, root, "deviceRole", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        selectedAudioDevice->deviceRole_ = static_cast<DeviceRole>(intValue);
    }

    if (napi_get_named_property(env, root, "deviceType", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        selectedAudioDevice->deviceType_ = static_cast<DeviceType>(intValue);
    }

    if (napi_get_named_property(env, root, "networkId", &tempValue) == napi_ok) {
        napi_get_value_string_utf8(env, tempValue, buffer, SIZE, &res);
        selectedAudioDevice->networkId_ = std::string(buffer);
    }

    if (napi_get_named_property(env, root, "displayName", &tempValue) == napi_ok) {
        napi_get_value_string_utf8(env, tempValue, buffer, SIZE, &res);
        selectedAudioDevice->displayName_ = std::string(buffer);
    }

    if (napi_get_named_property(env, root, "interruptGroupId", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        selectedAudioDevice->interruptGroupId_ = intValue;
    }

    if (napi_get_named_property(env, root, "volumeGroupId", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        selectedAudioDevice->volumeGroupId_ = intValue;
    }
}

static void ParseAudioDeviceDescriptorVector(napi_env env, napi_value root,
    vector<sptr<AudioDeviceDescriptor>> &deviceDescriptorsVector, bool &argTransFlag)
{
    uint32_t arrayLen = 0;
    napi_get_array_length(env, root, &arrayLen);
    if (arrayLen == 0) {
        deviceDescriptorsVector = {};
        AUDIO_INFO_LOG("Error: AudioDeviceDescriptor vector is NULL!");
    }

    for (size_t i = 0; i < arrayLen; i++) {
        napi_value element;
        napi_get_element(env, root, i, &element);
        sptr<AudioDeviceDescriptor> selectedAudioDevice = new(std::nothrow) AudioDeviceDescriptor();
        ParseAudioDeviceDescriptor(env, element, selectedAudioDevice, argTransFlag);
        if (!argTransFlag) {
            return;
        }
        deviceDescriptorsVector.push_back(selectedAudioDevice);
    }
}


static void SelectOutputDeviceAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRoutingManagerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioRoutingManagerAsyncContext* is Null!");
    }
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

static void SetDeviceDescriptor(const napi_env& env, napi_value &valueParam, const AudioDeviceDescriptor &deviceInfo)
{
    SetValueInt32(env, "deviceRole", static_cast<int32_t>(deviceInfo.deviceRole_), valueParam);
    SetValueInt32(env, "deviceType", static_cast<int32_t>(deviceInfo.deviceType_), valueParam);
    SetValueInt32(env, "id", static_cast<int32_t>(deviceInfo.deviceId_), valueParam);
    SetValueString(env, "name", deviceInfo.deviceName_, valueParam);
    SetValueString(env, "address", deviceInfo.macAddress_, valueParam);
    SetValueString(env, "networkId", deviceInfo.networkId_, valueParam);
    SetValueString(env, "displayName", deviceInfo.displayName_, valueParam);
    SetValueInt32(env, "interruptGroupId", static_cast<int32_t>(deviceInfo.interruptGroupId_), valueParam);
    SetValueInt32(env, "volumeGroupId", static_cast<int32_t>(deviceInfo.volumeGroupId_), valueParam);

    napi_value value = nullptr;
    napi_value sampleRates;
    napi_create_array_with_length(env, 1, &sampleRates);
    napi_create_int32(env, deviceInfo.audioStreamInfo_.samplingRate, &value);
    napi_set_element(env, sampleRates, 0, value);
    napi_set_named_property(env, valueParam, "sampleRates", sampleRates);

    napi_value channelCounts;
    napi_create_array_with_length(env, 1, &channelCounts);
    napi_create_int32(env, deviceInfo.audioStreamInfo_.channels, &value);
    napi_set_element(env, channelCounts, 0, value);
    napi_set_named_property(env, valueParam, "channelCounts", channelCounts);

    napi_value channelMasks;
    napi_create_array_with_length(env, 1, &channelMasks);
    napi_create_int32(env, deviceInfo.channelMasks_, &value);
    napi_set_element(env, channelMasks, 0, value);
    napi_set_named_property(env, valueParam, "channelMasks", channelMasks);

    napi_value encodingTypes;
    napi_create_array_with_length(env, 1, &encodingTypes);
    napi_create_int32(env, deviceInfo.audioStreamInfo_.encoding, &value);
    napi_set_element(env, encodingTypes, 0, value);
    napi_set_named_property(env, valueParam, "encodingTypes", encodingTypes);
}

static void SetDeviceDescriptors(const napi_env &env, napi_value &result, napi_value &valueParam,
    const vector<sptr<AudioDeviceDescriptor>> deviceDescriptors)
{
    napi_create_array_with_length(env, deviceDescriptors.size(), &result);
    for (size_t i = 0; i < deviceDescriptors.size(); i++) {
        if (deviceDescriptors[i] != nullptr) {
            (void)napi_create_object(env, &valueParam);
            SetDeviceDescriptor(env, valueParam, deviceDescriptors[i]);
            napi_set_element(env, result, i, valueParam);
        }
    }
}

static void SetDevicesInfo(vector<sptr<AudioDeviceDescriptor>> deviceDescriptors, napi_env env, napi_value* result,
    int32_t arrayLength, napi_value valueParam)
{
    size_t size = deviceDescriptors.size();
    HiLog::Info(LABEL, "number of devices = %{public}zu", size);

    if (arrayLength > PARAM1) {
        SetDeviceDescriptors(env, result[PARAM1], valueParam, deviceDescriptors);
    } else {
        HiLog::Error(LABEL, "ERROR: Array access out of bounds, result size is %{public}d", arrayLength);
        return;
    }
}

static void GetDevicesAsyncCallbackComplete(napi_env env, napi_status status, void* data)
{
    auto asyncContext = static_cast<AudioRoutingManagerAsyncContext*>(data);
    if (asyncContext == nullptr) {
        HiLog::Error(LABEL, "ERROR: AudioRoutingManagerAsyncContext* is Null!");
        return;
    }
    napi_value result[ARGS_TWO] = {0};
    napi_value valueParam = nullptr;
    SetDevicesInfo(asyncContext->deviceDescriptors, env, result, ARGS_TWO, valueParam);

    napi_get_undefined(env, &result[PARAM0]);
    if (!asyncContext->status) {
        napi_get_undefined(env, &valueParam);
    }
    CommonCallbackRoutine(env, asyncContext, result[PARAM1]);
}

napi_value AudioRoutingManagerNapi::GetDevices(napi_env env, napi_callback_info info)
{
    const int32_t refCount = 1;
    napi_value result = nullptr;

    size_t argc = ARGS_TWO;
    napi_value argv[ARGS_TWO] = { 0 };
    napi_value thisVar = nullptr;
    void* data;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);

    unique_ptr<AudioRoutingManagerAsyncContext> asyncContext = make_unique<AudioRoutingManagerAsyncContext>();

    napi_status status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        CheckParams(argc, env, argv, asyncContext, refCount, result);

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetDevices", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource, [](napi_env env, void *data) {
                auto context = static_cast<AudioRoutingManagerAsyncContext*>(data);
                if (context->status == SUCCESS) {
                    context->deviceDescriptors = context->objectInfo->audioMngr_->GetDevices(
                        static_cast<DeviceFlag>(context->deviceFlag));
                    context->status = 0;
                }
            }, GetDevicesAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioRoutingManagerNapi::GetDevicesSync(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    void *native = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    if (argc < ARGS_ONE) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return result;
    }

    status = napi_unwrap(env, thisVar, &native);
    auto *audioRoutingManagerNapi = reinterpret_cast<AudioRoutingManagerNapi *>(native);
    if (status != napi_ok || audioRoutingManagerNapi == nullptr) {
        AUDIO_ERR_LOG("GetDevicesSync unwrap failure!");
        return result;
    }

    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv[PARAM0], &valueType);
    if (valueType != napi_number) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return result;
    }

    int32_t deviceFlag;
    napi_get_value_int32(env, argv[PARAM0], &deviceFlag);
    if (!AudioCommonNapi::IsLegalInputArgumentDeviceFlag(deviceFlag)) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INVALID_PARAM);
        return result;
    }

    vector<sptr<AudioDeviceDescriptor>> deviceDescriptors = audioRoutingManagerNapi->audioMngr_->GetDevices(
        static_cast<DeviceFlag>(deviceFlag));

    napi_value valueParam = nullptr;
    SetDeviceDescriptors(env, result, valueParam, deviceDescriptors);

    return result;
}

void AudioRoutingManagerNapi::CheckParams(size_t argc, napi_env env, napi_value* argv,
    std::unique_ptr<AudioRoutingManagerAsyncContext>& asyncContext, const int32_t refCount, napi_value& result)
{
    if (argc < ARGS_ONE) {
            asyncContext->status = NAPI_ERR_INVALID_PARAM;
    }
    for (size_t i = PARAM0; i < argc; i++) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[i], &valueType);

        if (i == PARAM0 && valueType == napi_number) {
            napi_get_value_int32(env, argv[i], &asyncContext->deviceFlag);
            HiLog::Info(LABEL, " GetDevices deviceFlag = %{public}d", asyncContext->deviceFlag);
            if (!AudioCommonNapi::IsLegalInputArgumentDeviceFlag(asyncContext->deviceFlag)) {
                asyncContext->status = asyncContext->status ==
                    NAPI_ERR_INVALID_PARAM? NAPI_ERR_INVALID_PARAM : NAPI_ERR_UNSUPPORTED;
            }
        } else if (i == PARAM1) {
            if (valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
            }
            break;
        } else {
            asyncContext->status = NAPI_ERR_INVALID_PARAM;
            HiLog::Error(LABEL, "ERROR: type mismatch");
        }
    }

    if (asyncContext->callbackRef == nullptr) {
        napi_create_promise(env, &asyncContext->deferred, &result);
    } else {
        napi_get_undefined(env, &result);
    }
}

napi_value AudioRoutingManagerNapi::SelectOutputDevice(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;
    size_t argc = ARGS_TWO;
    napi_value argv[ARGS_TWO] = {0};
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);

    unique_ptr<AudioRoutingManagerAsyncContext> asyncContext = make_unique<AudioRoutingManagerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc < ARGS_ONE) {
            asyncContext->status = NAPI_ERR_INVALID_PARAM;
        }
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_object) {
                ParseAudioDeviceDescriptorVector(env, argv[i], asyncContext->deviceDescriptors,
                    asyncContext->bArgTransFlag);
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
        napi_create_string_utf8(env, "SelectOutputDevice", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRoutingManagerAsyncContext*>(data);
                if (context->bArgTransFlag) {
                    context->status = context->objectInfo->audioMngr_->SelectOutputDevice(context->deviceDescriptors);
                } else {
                    context->status = context->status ==
                        NAPI_ERR_INVALID_PARAM? NAPI_ERR_INVALID_PARAM : NAPI_ERR_UNSUPPORTED;
                }
            },
            SelectOutputDeviceAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

static void GetActiveOutputDeviceAsyncCallbackComplete(napi_env env, napi_status status, void* data)
{
    auto asyncContext = static_cast<AudioRoutingManagerAsyncContext*>(data);
    napi_value result[ARGS_TWO] = {0};
    napi_value valueParam = nullptr;
    if (asyncContext == nullptr) {
        HiLog::Error(LABEL, "ERROR: AudioRoutingManagerAsyncContext* is Null!");
        return;
    }
    SetDevicesInfo(asyncContext->outDeviceDescriptors, env, result, ARGS_TWO, valueParam);

    napi_get_undefined(env, &result[PARAM0]);
    if (!asyncContext->status) {
        napi_get_undefined(env, &valueParam);
    }
    CommonCallbackRoutine(env, asyncContext, result[PARAM1]);
}

napi_value AudioRoutingManagerNapi::GetActiveOutputDeviceDescriptors(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;
    size_t argc = ARGS_ONE;
    napi_value argv[ARGS_ONE] = {0};
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);

    unique_ptr<AudioRoutingManagerAsyncContext> asyncContext = make_unique<AudioRoutingManagerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[PARAM0], &valueType);

        if (argc == PARAM1 && valueType == napi_function) {
                napi_create_reference(env, argv[PARAM0], refCount, &asyncContext->callbackRef);
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetActiveOutputDeviceDescriptors", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRoutingManagerAsyncContext*>(data);
                if (context->status == SUCCESS) {
                    context->outDeviceDescriptors = context->objectInfo->audioMngr_->GetActiveOutputDeviceDescriptors();
                    context->status = SUCCESS;
                }
            },
            GetActiveOutputDeviceAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

static void GetPreferredOutputDeviceForRendererInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRoutingManagerAsyncContext*>(data);
    napi_value result[ARGS_TWO] = {0};
    napi_value valueParam = nullptr;
    if (asyncContext == nullptr) {
        HiLog::Error(LABEL, "ERROR: AudioRoutingManagerAsyncContext* is Null!");
        return;
    }
    SetDevicesInfo(asyncContext->outDeviceDescriptors, env, result, ARGS_TWO, valueParam);

    napi_get_undefined(env, &result[PARAM0]);
    if (!asyncContext->status) {
        napi_get_undefined(env, &valueParam);
    }
    CommonCallbackRoutine(env, asyncContext, result[PARAM1]);
}

bool AudioRoutingManagerNapi::CheckPreferredOutputDeviceForRendererInfo(napi_env env,
    std::unique_ptr<AudioRoutingManagerAsyncContext>& asyncContext, size_t argc, napi_value* argv)
{
    const int32_t refCount = 1;
    if (argc < ARGS_ONE) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return false;
    }
    for (size_t i = PARAM0; i < argc; i++) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[i], &valueType);

        if (i == PARAM0 && valueType == napi_object) {
            ParseAudioRendererInfo(env, argv[i], &asyncContext->rendererInfo);
        } else if (i == PARAM1) {
            if (valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
            }
            break;
        } else {
            AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
            return false;
        }
    }
    return true;
}

napi_value AudioRoutingManagerNapi::GetPreferOutputDeviceForRendererInfo(napi_env env, napi_callback_info info)
{
    // for api compatibility, leave some time for applications to adapt to new one
    return GetPreferredOutputDeviceForRendererInfo(env, info);
}

napi_value AudioRoutingManagerNapi::GetPreferredOutputDeviceForRendererInfo(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    size_t argc = ARGS_TWO;
    napi_value argv[ARGS_TWO] = {0};
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);

    unique_ptr<AudioRoutingManagerAsyncContext> asyncContext = make_unique<AudioRoutingManagerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (!CheckPreferredOutputDeviceForRendererInfo(env, asyncContext, argc, argv)) {
            return nullptr;
        }
        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetPreferredOutputDeviceForRendererInfo", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRoutingManagerAsyncContext*>(data);
                if (context->status == SUCCESS) {
                    context->status = context->objectInfo->audioRoutingMngr_->GetPreferredOutputDeviceForRendererInfo(
                        context->rendererInfo, context->outDeviceDescriptors);
                }
            }, GetPreferredOutputDeviceForRendererInfoAsyncCallbackComplete,
            static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioRoutingManagerNapi::GetPreferredOutputDeviceForRendererInfoSync(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    void *native = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    if (argc < ARGS_ONE) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return result;
    }

    status = napi_unwrap(env, thisVar, &native);
    auto *audioRoutingManagerNapi = reinterpret_cast<AudioRoutingManagerNapi *>(native);
    if (status != napi_ok || audioRoutingManagerNapi == nullptr) {
        AUDIO_ERR_LOG("GetPreferredOutputDeviceForRendererInfoSync unwrap failure!");
        return result;
    }

    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv[PARAM0], &valueType);
    if (valueType != napi_object) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return result;
    }

    AudioRendererInfo rendererInfo;
    ParseAudioRendererInfo(env, argv[PARAM0], &rendererInfo);

    vector<sptr<AudioDeviceDescriptor>> outDeviceDescriptors;
    audioRoutingManagerNapi->audioRoutingMngr_->GetPreferredOutputDeviceForRendererInfo(
        rendererInfo, outDeviceDescriptors);

    napi_value valueParam = nullptr;
    SetDeviceDescriptors(env, result, valueParam, outDeviceDescriptors);

    return result;
}

static void ParseAudioCapturerInfo(napi_env env, napi_value root, AudioCapturerInfo *capturerInfo)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};

    if (napi_get_named_property(env, root, "source", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        capturerInfo->sourceType = static_cast<SourceType>(intValue);
    }

    if (napi_get_named_property(env, root, "capturerFlags", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        capturerInfo->capturerFlags = intValue;
    }
}

bool AudioRoutingManagerNapi::CheckPreferredInputDeviceForCaptureInfo(napi_env env,
    std::unique_ptr<AudioRoutingManagerAsyncContext> &asyncContext, size_t argc, napi_value *argv)
{
    const int32_t refCount = 1;
    if (argc < ARGS_ONE) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return false;
    }
    for (size_t i = PARAM0; i < argc; i++) {
        napi_valuetype valueType = napi_undefined;
        napi_typeof(env, argv[i], &valueType);

        if (i == PARAM0 && valueType == napi_object) {
            ParseAudioCapturerInfo(env, argv[i], &asyncContext->captureInfo);
        } else if (i == PARAM1) {
            if (valueType == napi_function) {
                napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
            }
            break;
        } else {
            AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
            return false;
        }
    }
    return true;
}

static void GetPreferredInputDeviceForCapturerInfoAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRoutingManagerAsyncContext*>(data);
    napi_value result[ARGS_TWO] = {0};
    napi_value valueParam = nullptr;
    if (asyncContext == nullptr) {
        AUDIO_ERR_LOG("asyncContext is Null!");
        return;
    }
    SetDevicesInfo(asyncContext->inputDeviceDescriptors, env, result, ARGS_TWO, valueParam);

    napi_get_undefined(env, &result[PARAM0]);
    if (!asyncContext->status) {
        napi_get_undefined(env, &valueParam);
    }
    CommonCallbackRoutine(env, asyncContext, result[PARAM1]);
}

napi_value AudioRoutingManagerNapi::GetPreferredInputDeviceForCapturerInfo(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    size_t argc = ARGS_TWO;
    napi_value argv[ARGS_TWO] = {0};
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);

    unique_ptr<AudioRoutingManagerAsyncContext> asyncContext = make_unique<AudioRoutingManagerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (!CheckPreferredInputDeviceForCaptureInfo(env, asyncContext, argc, argv)) {
            return nullptr;
        }
        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "GetPreferredInputDeviceForCapturerInfo", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRoutingManagerAsyncContext*>(data);
                if (context->status == SUCCESS) {
                    context->status = context->objectInfo->audioRoutingMngr_->GetPreferredInputDeviceForCapturerInfo(
                        context->captureInfo, context->inputDeviceDescriptors);
                }
            }, GetPreferredInputDeviceForCapturerInfoAsyncCallbackComplete,
            static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioRoutingManagerNapi::GetPreferredInputDeviceForCapturerInfoSync(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    void *native = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    if (argc < ARGS_ONE) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return result;
    }

    status = napi_unwrap(env, thisVar, &native);
    auto *audioRoutingManagerNapi = reinterpret_cast<AudioRoutingManagerNapi *>(native);
    if (status != napi_ok || audioRoutingManagerNapi == nullptr) {
        AUDIO_ERR_LOG("GetPreferredInputDeviceForCapturerInfoSync unwrap failure!");
        return result;
    }

    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv[PARAM0], &valueType);
    if (valueType != napi_object) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return result;
    }

    AudioCapturerInfo capturerInfo;
    ParseAudioCapturerInfo(env, argv[PARAM0], &capturerInfo);

    vector<sptr<AudioDeviceDescriptor>> outDeviceDescriptors;
    audioRoutingManagerNapi->audioRoutingMngr_->GetPreferredInputDeviceForCapturerInfo(
        capturerInfo, outDeviceDescriptors);

    napi_value valueParam = nullptr;
    SetDeviceDescriptors(env, result, valueParam, outDeviceDescriptors);

    return result;
}

napi_value AudioRoutingManagerNapi::SelectOutputDeviceByFilter(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;
    size_t argc = ARGS_THREE;
    napi_value argv[ARGS_THREE] = {0};
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);

    unique_ptr<AudioRoutingManagerAsyncContext> asyncContext = make_unique<AudioRoutingManagerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc < ARGS_TWO) {
            asyncContext->status = NAPI_ERR_INVALID_PARAM;
        }
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_object) {
                ParseAudioRendererFilter(env, argv[i], asyncContext->audioRendererFilter, asyncContext->bArgTransFlag);
            } else if (i == PARAM1 && valueType == napi_object) {
                ParseAudioDeviceDescriptorVector(env, argv[i], asyncContext->deviceDescriptors,
                    asyncContext->bArgTransFlag);
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
        napi_create_string_utf8(env, "SelectOutputDeviceByFilter", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRoutingManagerAsyncContext*>(data);
                if (context->bArgTransFlag) {
                    context->status = context->objectInfo->audioMngr_->SelectOutputDevice(
                        context->audioRendererFilter, context->deviceDescriptors);
                } else {
                    context->status = context->status ==
                        NAPI_ERR_INVALID_PARAM? NAPI_ERR_INVALID_PARAM : NAPI_ERR_UNSUPPORTED;
                }
            },
            SelectOutputDeviceAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

static void ParseAudioCapturerFilter(napi_env env, napi_value root, sptr<AudioCapturerFilter> &audioCapturerFilter)
{
    napi_value tempValue = nullptr;
    int32_t intValue = {0};

    audioCapturerFilter = new(std::nothrow) AudioCapturerFilter();
    if (napi_get_named_property(env, root, "uid", &tempValue) == napi_ok) {
        napi_get_value_int32(env, tempValue, &intValue);
        audioCapturerFilter->uid = intValue;
    }
}

static void SelectInputDeviceAsyncCallbackComplete(napi_env env, napi_status status, void *data)
{
    auto asyncContext = static_cast<AudioRoutingManagerAsyncContext*>(data);
    napi_value valueParam = nullptr;

    if (asyncContext != nullptr) {
        if (!asyncContext->status) {
            napi_get_undefined(env, &valueParam);
        }
        CommonCallbackRoutine(env, asyncContext, valueParam);
    } else {
        HiLog::Error(LABEL, "ERROR: AudioRoutingManagerAsyncContext* is Null!");
    }
}

napi_value AudioRoutingManagerNapi::SelectInputDevice(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;
    size_t argc = ARGS_TWO;
    napi_value argv[ARGS_TWO] = {0};
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);

    unique_ptr<AudioRoutingManagerAsyncContext> asyncContext = make_unique<AudioRoutingManagerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc < ARGS_ONE) {
            asyncContext->status = NAPI_ERR_INVALID_PARAM;
        }
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);
            if (i == PARAM0 && valueType == napi_object) {
                ParseAudioDeviceDescriptorVector(env, argv[i], asyncContext->deviceDescriptors,
                    asyncContext->bArgTransFlag);
            } else if (i == PARAM1) {
                if (valueType == napi_function) {
                    napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                }
                break;
            } else {
                asyncContext->status = NAPI_ERR_INVALID_PARAM;
            }

            if (!asyncContext->bArgTransFlag) {
                break;
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "SelectInputDevice", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRoutingManagerAsyncContext*>(data);
                if (context->bArgTransFlag) {
                    context->status = context->objectInfo->audioMngr_->SelectInputDevice(context->deviceDescriptors);
                } else {
                    context->status = ERR_INVALID_PARAM;
                }
            },
            SelectInputDeviceAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

napi_value AudioRoutingManagerNapi::SelectInputDeviceByFilter(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    size_t argc = ARGS_THREE;
    napi_value argv[ARGS_TWO] = {0};
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);
    NAPI_ASSERT(env, argc >= ARGS_THREE, "requires 2 parameters minimum");

    unique_ptr<AudioRoutingManagerAsyncContext> asyncContext = make_unique<AudioRoutingManagerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc < ARGS_TWO) {
            asyncContext->status = NAPI_ERR_INVALID_PARAM;
        }
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);
            if (i == PARAM0 && valueType == napi_object) {
                ParseAudioCapturerFilter(env, argv[i], asyncContext->audioCapturerFilter);
            } else if (i == PARAM1 && valueType == napi_object) {
                ParseAudioDeviceDescriptorVector(env, argv[i], asyncContext->deviceDescriptors,
                    asyncContext->bArgTransFlag);
            } else if (i == PARAM2) {
                if (valueType == napi_function) {
                    napi_create_reference(env, argv[i], refCount, &asyncContext->callbackRef);
                }
                break;
            } else {
                asyncContext->status = NAPI_ERR_INVALID_PARAM;
            }

            if (!asyncContext->bArgTransFlag) {
                break;
            }
        }

        if (asyncContext->callbackRef == nullptr) {
            napi_create_promise(env, &asyncContext->deferred, &result);
        } else {
            napi_get_undefined(env, &result);
        }

        napi_value resource = nullptr;
        napi_create_string_utf8(env, "SelectInputDeviceByFilter", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRoutingManagerAsyncContext*>(data);
                if (context->bArgTransFlag) {
                    context->status = context->objectInfo->audioMngr_->SelectInputDevice(
                        context->audioCapturerFilter, context->deviceDescriptors);
                } else {
                    context->status = context->status ==
                        NAPI_ERR_INVALID_PARAM? NAPI_ERR_INVALID_PARAM : NAPI_ERR_UNSUPPORTED;
                }
            },
            SelectInputDeviceAsyncCallbackComplete, static_cast<void*>(asyncContext.get()), &asyncContext->work);
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

void AudioRoutingManagerNapi::RegisterDeviceChangeCallback(napi_env env, size_t argc, napi_value* args,
    const std::string& cbName, AudioRoutingManagerNapi* routingMgrNapi)
{
    int32_t flag = 3;
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, args[PARAM1], &valueType);
    if (valueType == napi_number) {
        napi_get_value_int32(env, args[PARAM1], &flag);
        AUDIO_INFO_LOG("RegisterDeviceChangeCallback:On deviceFlag: %{public}d", flag);
        if (!AudioCommonNapi::IsLegalInputArgumentDeviceFlag(flag)) {
            AudioCommonNapi::throwError(env, NAPI_ERR_INVALID_PARAM);
        }
    } else {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
    }

    napi_valuetype handler = napi_undefined;
    napi_typeof(env, args[PARAM2], &handler);
    if (handler != napi_function) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
    }
    DeviceFlag deviceFlag = DeviceFlag(flag);
    if (!routingMgrNapi->deviceChangeCallbackNapi_) {
        routingMgrNapi->deviceChangeCallbackNapi_= std::make_shared<AudioManagerCallbackNapi>(env);
    }
    CHECK_AND_RETURN_LOG(routingMgrNapi->deviceChangeCallbackNapi_,
        "RegisterDeviceChangeCallback: Memory Allocation Failed !");

    int32_t ret = routingMgrNapi->audioMngr_->SetDeviceChangeCallback(deviceFlag,
        routingMgrNapi->deviceChangeCallbackNapi_);
    if (ret) {
        AUDIO_ERR_LOG("RegisterDeviceChangeCallback: Registering Device Change Callback Failed %{public}d", ret);
        AudioCommonNapi::throwError(env, ret);
        return;
    }

    std::shared_ptr<AudioManagerCallbackNapi> cb =
        std::static_pointer_cast<AudioManagerCallbackNapi>(routingMgrNapi->deviceChangeCallbackNapi_);
    cb->SaveRoutingManagerDeviceChangeCbRef(deviceFlag, args[PARAM2]);
}

void AudioRoutingManagerNapi::RegisterPreferredOutputDeviceChangeCallback(napi_env env, size_t argc, napi_value* args,
    const std::string& cbName, AudioRoutingManagerNapi* routingMgrNapi)
{
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, args[PARAM1], &valueType);
    if (valueType != napi_object) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INVALID_PARAM);
    }

    AudioRendererInfo rendererInfo;
    ParseAudioRendererInfo(env, args[PARAM1], &rendererInfo);
    AudioStreamType streamType = AudioSystemManager::GetStreamType(rendererInfo.contentType,
        rendererInfo.streamUsage);
    
    if (!routingMgrNapi->preferredOutputDeviceCallbackNapi_) {
        routingMgrNapi->preferredOutputDeviceCallbackNapi_ =
            std::make_shared<AudioPreferredOutputDeviceChangeCallbackNapi>(env);
        if (!routingMgrNapi->preferredOutputDeviceCallbackNapi_) {
            AUDIO_ERR_LOG("RegisterPreferredOutputDeviceChangeCallback: Memory Allocation Failed !!");
            return;
        }

        int32_t ret = routingMgrNapi->audioRoutingMngr_->SetPreferredOutputDeviceChangeCallback(
            rendererInfo, routingMgrNapi->preferredOutputDeviceCallbackNapi_);
        if (ret) {
            AUDIO_ERR_LOG("Registering Active Output DeviceChange Callback Failed %{public}d", ret);
            AudioCommonNapi::throwError(env, ret);
            return;
        }
    }

    std::shared_ptr<AudioPreferredOutputDeviceChangeCallbackNapi> cb =
        std::static_pointer_cast<AudioPreferredOutputDeviceChangeCallbackNapi>(
        routingMgrNapi->preferredOutputDeviceCallbackNapi_);
    cb->SaveCallbackReference(streamType, args[PARAM2]);
}

void AudioRoutingManagerNapi::RegisterPreferredInputDeviceChangeCallback(napi_env env, size_t argc, napi_value *args,
    const std::string &cbName, AudioRoutingManagerNapi *routingMgrNapi)
{
    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, args[PARAM1], &valueType);
    if (valueType != napi_object) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INVALID_PARAM);
    }

    AudioCapturerInfo captureInfo;
    ParseAudioCapturerInfo(env, args[PARAM1], &captureInfo);

    if (!routingMgrNapi->preferredInputDeviceCallbackNapi_) {
        routingMgrNapi->preferredInputDeviceCallbackNapi_ =
            std::make_shared<AudioPreferredInputDeviceChangeCallbackNapi>(env);
        if (!routingMgrNapi->preferredInputDeviceCallbackNapi_) {
            AUDIO_ERR_LOG("RegisterPreferredInputDeviceChangeCallback: Memory Allocation Failed !!");
            return;
        }

        int32_t ret = routingMgrNapi->audioRoutingMngr_->SetPreferredInputDeviceChangeCallback(
            captureInfo, routingMgrNapi->preferredInputDeviceCallbackNapi_);
        if (ret) {
            AUDIO_ERR_LOG("Registering Active Input DeviceChange Callback Failed %{public}d", ret);
            AudioCommonNapi::throwError(env, ret);
            return;
        }
    }

    std::shared_ptr<AudioPreferredInputDeviceChangeCallbackNapi> cb =
        std::static_pointer_cast<AudioPreferredInputDeviceChangeCallbackNapi>(
        routingMgrNapi->preferredInputDeviceCallbackNapi_);
    cb->SaveCallbackReference(captureInfo.sourceType, args[PARAM2]);
}

void AudioRoutingManagerNapi::RegisterCallback(napi_env env, napi_value jsThis, size_t argc,
    napi_value* args, const std::string& cbName)
{
    AudioRoutingManagerNapi* routingMgrNapi = nullptr;
    napi_status status = napi_unwrap(env, jsThis, reinterpret_cast<void**>(&routingMgrNapi));
    if ((status != napi_ok) || (routingMgrNapi == nullptr) || (routingMgrNapi->audioMngr_ == nullptr)
        || (routingMgrNapi->audioRoutingMngr_ == nullptr)) {
        AUDIO_ERR_LOG("AudioRoutingMgrNapi::Failed to retrieve stream mgr napi instance.");

        return;
    }

    if (!cbName.compare(DEVICE_CHANGE_CALLBACK_NAME)) {
        RegisterDeviceChangeCallback(env, argc, args, cbName, routingMgrNapi);
    } else if (!cbName.compare(PREFERRED_OUTPUT_DEVICE_CALLBACK_NAME) ||
        !cbName.compare(PREFER_OUTPUT_DEVICE_CALLBACK_NAME)) {
        RegisterPreferredOutputDeviceChangeCallback(env, argc, args, cbName, routingMgrNapi);
    } else if (!cbName.compare(PREFERRED_INPUT_DEVICE_CALLBACK_NAME)) {
        RegisterPreferredInputDeviceChangeCallback(env, argc, args, cbName, routingMgrNapi);
    } else {
        AUDIO_ERR_LOG("AudioRoutingMgrNapi::No such supported");
        AudioCommonNapi::throwError(env, NAPI_ERR_INVALID_PARAM);
    }
}

napi_value AudioRoutingManagerNapi::On(napi_env env, napi_callback_info info)
{
    AUDIO_INFO_LOG("On inter");

    const size_t requireArgc = PARAM2;
    const size_t maxArgc = PARAM3;
    size_t argc = PARAM3;

    napi_value undefinedResult = nullptr;
    napi_get_undefined(env, &undefinedResult);

    napi_value args[requireArgc + 1] = { nullptr, nullptr, nullptr };
    napi_value jsThis = nullptr;
    napi_status status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    bool isArgcCountRight = argc == requireArgc || argc == maxArgc;
    THROW_ERROR_ASSERT(env, status == napi_ok && isArgcCountRight, NAPI_ERR_INPUT_INVALID);

    napi_valuetype eventType = napi_undefined;
    napi_typeof(env, args[0], &eventType);
    THROW_ERROR_ASSERT(env, eventType == napi_string, NAPI_ERR_INPUT_INVALID);
    std::string callbackName = AudioCommonNapi::GetStringArgument(env, args[0]);
    AUDIO_INFO_LOG("On callbackName: %{public}s", callbackName.c_str());

    if (argc == requireArgc) {
        napi_valuetype handler = napi_undefined;
        napi_typeof(env, args[1], &handler);
        THROW_ERROR_ASSERT(env, handler == napi_function, NAPI_ERR_INPUT_INVALID);
    }

    RegisterCallback(env, jsThis, argc, args, callbackName);

    return undefinedResult;
}

void AudioRoutingManagerNapi::UnregisterDeviceChangeCallback(napi_env env, napi_value callback,
    AudioRoutingManagerNapi* routingMgrNapi)
{
    if (routingMgrNapi->deviceChangeCallbackNapi_ != nullptr) {
        std::shared_ptr<AudioManagerCallbackNapi> cb =
            std::static_pointer_cast<AudioManagerCallbackNapi>(
            routingMgrNapi->deviceChangeCallbackNapi_);
        if (callback != nullptr) {
            cb->RemoveRoutingManagerDeviceChangeCbRef(env, callback);
        }
        if (callback == nullptr || cb->GetRoutingManagerDeviceChangeCbListSize() == 0) {
            int32_t ret = routingMgrNapi->audioMngr_->UnsetDeviceChangeCallback(DeviceFlag::ALL_L_D_DEVICES_FLAG);
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "UnsetDeviceChangeCallback Failed");
            routingMgrNapi->deviceChangeCallbackNapi_.reset();
            routingMgrNapi->deviceChangeCallbackNapi_ = nullptr;

            cb->RemoveAllRoutingManagerDeviceChangeCb();
        }
    } else {
        AUDIO_ERR_LOG("UnregisterDeviceChangeCallback: deviceChangeCallbackNapi_ is null");
    }
}

void AudioRoutingManagerNapi::UnregisterPreferredOutputDeviceChangeCallback(napi_env env, napi_value callback,
    AudioRoutingManagerNapi* routingMgrNapi)
{
    if (routingMgrNapi->preferredOutputDeviceCallbackNapi_ != nullptr) {
        std::shared_ptr<AudioPreferredOutputDeviceChangeCallbackNapi> cb =
            std::static_pointer_cast<AudioPreferredOutputDeviceChangeCallbackNapi>(
            routingMgrNapi->preferredOutputDeviceCallbackNapi_);
        if (callback == nullptr) {
            int32_t ret = routingMgrNapi->audioRoutingMngr_->UnsetPreferredOutputDeviceChangeCallback();
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "UnsetPreferredOutputDeviceChangeCallback Failed");

            routingMgrNapi->preferredOutputDeviceCallbackNapi_.reset();
            routingMgrNapi->preferredOutputDeviceCallbackNapi_ = nullptr;
            cb->RemoveAllCallbacks();
            return;
        }
        cb->RemoveCallbackReference(env, callback);
    } else {
        AUDIO_ERR_LOG("UnregisterPreferredOutputDeviceChangeCallback: preferredOutputDeviceCallbackNapi_ is null");
    }
}

void AudioRoutingManagerNapi::UnregisterPreferredInputDeviceChangeCallback(napi_env env, napi_value callback,
    AudioRoutingManagerNapi *routingMgrNapi)
{
    if (routingMgrNapi->preferredInputDeviceCallbackNapi_ != nullptr) {
        std::shared_ptr<AudioPreferredInputDeviceChangeCallbackNapi> cb =
            std::static_pointer_cast<AudioPreferredInputDeviceChangeCallbackNapi>(
            routingMgrNapi->preferredInputDeviceCallbackNapi_);
        if (callback == nullptr) {
            int32_t ret = routingMgrNapi->audioRoutingMngr_->UnsetPreferredInputDeviceChangeCallback();
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "UnsetPreferredInputDeviceChangeCallback Failed");

            routingMgrNapi->preferredInputDeviceCallbackNapi_.reset();
            routingMgrNapi->preferredInputDeviceCallbackNapi_ = nullptr;
            cb->RemoveAllCallbacks();
            return;
        }
        cb->RemoveCallbackReference(env, callback);
    } else {
        AUDIO_ERR_LOG("UnregisterPreferredInputDeviceChangeCallback: preferredInputDeviceCallbackNapi_ is null");
    }
}

napi_value AudioRoutingManagerNapi::UnregisterCallback(napi_env env, napi_value jsThis,
    const std::string& callbackName, napi_value callback)
{
    napi_value undefinedResult = nullptr;
    napi_get_undefined(env, &undefinedResult);
    AudioRoutingManagerNapi* routingMgrNapi = nullptr;
    napi_status status = napi_unwrap(env, jsThis, reinterpret_cast<void **>(&routingMgrNapi));
    NAPI_ASSERT(env, status == napi_ok && routingMgrNapi != nullptr, "Failed to retrieve audio mgr napi instance.");
    NAPI_ASSERT(env, routingMgrNapi->audioMngr_ != nullptr, "audio system mgr instance is null.");

    if (!callbackName.compare(DEVICE_CHANGE_CALLBACK_NAME)) {
        UnregisterDeviceChangeCallback(env, callback, routingMgrNapi);
    } else if (!callbackName.compare(PREFERRED_OUTPUT_DEVICE_CALLBACK_NAME) ||
        !callbackName.compare(PREFER_OUTPUT_DEVICE_CALLBACK_NAME)) {
        UnregisterPreferredOutputDeviceChangeCallback(env, callback, routingMgrNapi);
    } else if (!callbackName.compare(PREFERRED_INPUT_DEVICE_CALLBACK_NAME)) {
        UnregisterPreferredInputDeviceChangeCallback(env, callback, routingMgrNapi);
    } else {
        AUDIO_ERR_LOG("off no such supported");
        AudioCommonNapi::throwError(env, NAPI_ERR_INVALID_PARAM);
    }

    return undefinedResult;
}

napi_value AudioRoutingManagerNapi::Off(napi_env env, napi_callback_info info)
{
    napi_value undefinedResult = nullptr;
    napi_get_undefined(env, &undefinedResult);

    const size_t minArgCount = 1;
    size_t argCount = 2;
    napi_value args[minArgCount + 1] = {nullptr, nullptr};
    napi_value jsThis = nullptr;
    napi_status status = napi_get_cb_info(env, info, &argCount, args, &jsThis, nullptr);
    if (status != napi_ok || argCount < minArgCount) {
        AUDIO_ERR_LOG("Off fail to napi_get_cb_info/Requires min 1 parameters");
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return undefinedResult;
    }

    napi_valuetype eventType = napi_undefined;
    if (napi_typeof(env, args[PARAM0], &eventType) != napi_ok || eventType != napi_string) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return undefinedResult;
    }

    napi_valuetype secondArgsType = napi_undefined;
    if (argCount > minArgCount &&
        (napi_typeof(env, args[PARAM1], &secondArgsType) != napi_ok || secondArgsType != napi_function)) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return undefinedResult;
    }
    std::string callbackName = AudioCommonNapi::GetStringArgument(env, args[0]);

    if (argCount == minArgCount) {
        args[PARAM1] = nullptr;
    }
    AUDIO_INFO_LOG("Off callbackName: %{public}s", callbackName.c_str());

    return UnregisterCallback(env, jsThis, callbackName, args[PARAM1]);
}

napi_value AudioRoutingManagerNapi::SetCommunicationDevice(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_THREE);

    unique_ptr<AudioRoutingManagerAsyncContext> asyncContext = make_unique<AudioRoutingManagerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc < ARGS_TWO) {
            asyncContext->status = NAPI_ERR_INVALID_PARAM;
        }
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_number) {
                napi_get_value_int32(env, argv[i], &asyncContext->deviceType);
                if (!AudioCommonNapi::IsLegalInputArgumentCommunicationDeviceType(asyncContext->deviceType)) {
                asyncContext->status = asyncContext->status ==
                    NAPI_ERR_INVALID_PARAM? NAPI_ERR_INVALID_PARAM : NAPI_ERR_UNSUPPORTED;
                }
            } else if (i == PARAM1 && valueType == napi_boolean) {
                napi_get_value_bool(env, argv[i], &asyncContext->isActive);
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
        napi_create_string_utf8(env, "SetCommunicationDeviceActive", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRoutingManagerAsyncContext*>(data);
                if (context->status == 0) {
                    context->status = context->objectInfo->audioMngr_->SetDeviceActive(
                        static_cast<ActiveDeviceType>(context->deviceType), context->isActive);
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

napi_value AudioRoutingManagerNapi::IsCommunicationDeviceActive(napi_env env, napi_callback_info info)
{
    napi_status status;
    const int32_t refCount = 1;
    napi_value result = nullptr;

    GET_PARAMS(env, info, ARGS_TWO);

    unique_ptr<AudioRoutingManagerAsyncContext> asyncContext = make_unique<AudioRoutingManagerAsyncContext>();

    status = napi_unwrap(env, thisVar, reinterpret_cast<void**>(&asyncContext->objectInfo));
    if (status == napi_ok && asyncContext->objectInfo != nullptr) {
        if (argc < ARGS_ONE) {
            asyncContext->status = NAPI_ERR_INVALID_PARAM;
        }
        for (size_t i = PARAM0; i < argc; i++) {
            napi_valuetype valueType = napi_undefined;
            napi_typeof(env, argv[i], &valueType);

            if (i == PARAM0 && valueType == napi_number) {
                napi_get_value_int32(env, argv[i], &asyncContext->deviceType);
                if (!AudioCommonNapi::IsLegalInputArgumentActiveDeviceType(asyncContext->deviceType)) {
                    asyncContext->status = asyncContext->status ==
                        NAPI_ERR_INVALID_PARAM? NAPI_ERR_INVALID_PARAM : NAPI_ERR_UNSUPPORTED;
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
        napi_create_string_utf8(env, "IsCommunicationDeviceActive", NAPI_AUTO_LENGTH, &resource);

        status = napi_create_async_work(
            env, nullptr, resource,
            [](napi_env env, void *data) {
                auto context = static_cast<AudioRoutingManagerAsyncContext*>(data);
                if (context->status == SUCCESS) {
                    context->isActive = context->objectInfo->audioMngr_->
                        IsDeviceActive(static_cast<ActiveDeviceType>(context->deviceType));
                    context->isTrue = context->isActive;
                    context->status = 0;
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

napi_value AudioRoutingManagerNapi::IsCommunicationDeviceActiveSync(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value result = nullptr;
    void *native = nullptr;

    GET_PARAMS(env, info, ARGS_ONE);

    if (argc < ARGS_ONE) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return result;
    }

    status = napi_unwrap(env, thisVar, &native);
    auto *audioRoutingManagerNapi = reinterpret_cast<AudioRoutingManagerNapi *>(native);
    if (status != napi_ok || audioRoutingManagerNapi == nullptr) {
        AUDIO_ERR_LOG("IsCommunicationDeviceActiveSync unwrap failure!");
        return result;
    }

    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv[PARAM0], &valueType);
    if (valueType != napi_number) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INPUT_INVALID);
        return result;
    }

    int32_t deviceType;
    napi_get_value_int32(env, argv[PARAM0], &deviceType);
    if (!AudioCommonNapi::IsLegalInputArgumentActiveDeviceType(deviceType)) {
        AudioCommonNapi::throwError(env, NAPI_ERR_INVALID_PARAM);
        return result;
    }

    bool isActive = audioRoutingManagerNapi->audioMngr_->IsDeviceActive(static_cast<ActiveDeviceType>(deviceType));
    napi_get_boolean(env, isActive, &result);

    return result;
}

} // namespace AudioStandard
} // namespace OHOS
