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

#include "audio_manager_callback_napi.h"

#include <uv.h>

#include "audio_errors.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
AudioManagerCallbackNapi::AudioManagerCallbackNapi(napi_env env)
    : env_(env)
{
    AUDIO_DEBUG_LOG("AudioManagerCallbackNapi: instance create");
}

AudioManagerCallbackNapi::~AudioManagerCallbackNapi()
{
    AUDIO_DEBUG_LOG("AudioManagerCallbackNapi: instance destroy");
}

void AudioManagerCallbackNapi::SaveCallbackReference(const std::string &callbackName, napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    napi_ref callback = nullptr;
    const int32_t refCount = 1;
    napi_status status = napi_create_reference(env_, args, refCount, &callback);
    CHECK_AND_RETURN_LOG(status == napi_ok && callback != nullptr,
                         "AudioManagerCallbackNapi: creating reference for callback fail");

    std::shared_ptr<AutoRef> cb = std::make_shared<AutoRef>(env_, callback);
    if (callbackName == DEVICE_CHANGE_CALLBACK_NAME) {
        deviceChangeCallback_ = cb;
    } else {
        AUDIO_ERR_LOG("AudioManagerCallbackNapi: Unknown callback type: %{public}s", callbackName.c_str());
    }
}

static void SetValueInt32(const napi_env& env, const std::string& fieldStr, const int intValue, napi_value& result)
{
    napi_value value = nullptr;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void SetValueString(const napi_env &env, const std::string &fieldStr, const std::string &stringValue,
    napi_value &result)
{
    napi_value value = nullptr;
    napi_create_string_utf8(env, stringValue.c_str(), NAPI_AUTO_LENGTH, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void NativeDeviceChangeActionToJsObj(const napi_env& env, napi_value& jsObj, const DeviceChangeAction &action)
{
    napi_create_object(env, &jsObj);
    SetValueInt32(env, "type", static_cast<int32_t>(action.type), jsObj);

    napi_value value = nullptr;
    napi_value subValue = nullptr;
    napi_value jsArray;
    size_t size = action.deviceDescriptors.size();
    napi_create_array_with_length(env, size, &jsArray);

    for (size_t i = 0; i < size; i++) {
        if (action.deviceDescriptors[i] != nullptr) {
            (void)napi_create_object(env, &value);
            SetValueInt32(env, "deviceRole", static_cast<int32_t>(action.deviceDescriptors[i]->deviceRole_), value);
            SetValueInt32(env, "deviceType", static_cast<int32_t>(action.deviceDescriptors[i]->deviceType_), value);
            SetValueInt32(env, "id", static_cast<int32_t>(action.deviceDescriptors[i]->deviceId_), value);
            SetValueString(env, "name", action.deviceDescriptors[i]->deviceName_, value);
            SetValueString(env, "address", action.deviceDescriptors[i]->macAddress_, value);

            napi_value sampleRates;
            napi_create_array_with_length(env, 1, &sampleRates);
            napi_create_int32(env, action.deviceDescriptors[i]->audioStreamInfo_.samplingRate, &subValue);
            napi_set_element(env, sampleRates, 0, subValue);
            napi_set_named_property(env, value, "sampleRates", sampleRates);

            napi_value channelCounts;
            napi_create_array_with_length(env, 1, &channelCounts);
            napi_create_int32(env, action.deviceDescriptors[i]->audioStreamInfo_.channels, &subValue);
            napi_set_element(env, channelCounts, 0, subValue);
            napi_set_named_property(env, value, "channelCounts", channelCounts);

            napi_value channelMasks;
            napi_create_array_with_length(env, 1, &channelMasks);
            napi_create_int32(env, action.deviceDescriptors[i]->channelMasks_, &subValue);
            napi_set_element(env, channelMasks, 0, subValue);
            napi_set_named_property(env, value, "channelMasks", channelMasks);

            napi_set_element(env, jsArray, i, value);
        }
    }

    napi_set_named_property(env, jsObj, "deviceDescriptors", jsArray);
}

void AudioManagerCallbackNapi::OnDeviceChange(const DeviceChangeAction &deviceChangeAction)
{
    std::lock_guard<std::mutex> lock(mutex_);
    AUDIO_DEBUG_LOG("OnDeviceChange: type[%{public}d]", deviceChangeAction.type);
    CHECK_AND_RETURN_LOG(deviceChangeCallback_ != nullptr, "callback not registered by JS client");

    std::unique_ptr<AudioManagerJsCallback> cb = std::make_unique<AudioManagerJsCallback>();
    CHECK_AND_RETURN_LOG(cb != nullptr, "No memory");

    cb->callback = deviceChangeCallback_;
    cb->callbackName = DEVICE_CHANGE_CALLBACK_NAME;
    cb->deviceChangeAction = deviceChangeAction;
    return OnJsCallbackDeviceChange(cb);
}

void AudioManagerCallbackNapi::OnJsCallbackDeviceChange(std::unique_ptr<AudioManagerJsCallback> &jsCb)
{
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        return;
    }

    uv_work_t *work = new(std::nothrow) uv_work_t;
    if (work == nullptr) {
        AUDIO_ERR_LOG("AudioManagerCallbackNapi: OnJsCallbackDeviceChange: No memory");
        return;
    }

    if (jsCb.get() == nullptr) {
        AUDIO_ERR_LOG("AudioManagerCallbackNapi: OnJsCallbackDeviceChange: jsCb.get() is null");
        delete work;
        return;
    }

    work->data = reinterpret_cast<void *>(jsCb.get());

    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, [] (uv_work_t *work, int status) {
        // Js Thread
        AudioManagerJsCallback *event = reinterpret_cast<AudioManagerJsCallback *>(work->data);
        std::string request = event->callbackName;
        napi_env env = event->callback->env_;
        napi_ref callback = event->callback->cb_;
        AUDIO_DEBUG_LOG("AudioManagerCallbackNapi: JsCallBack %{public}s, uv_queue_work start", request.c_str());
        do {
            CHECK_AND_BREAK_LOG(status != UV_ECANCELED, "%{public}s canceled", request.c_str());

            napi_value jsCallback = nullptr;
            napi_status nstatus = napi_get_reference_value(env, callback, &jsCallback);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && jsCallback != nullptr, "%{public}s get reference value fail",
                request.c_str());

            // Call back function
            napi_value args[1] = { nullptr };
            NativeDeviceChangeActionToJsObj(env, args[0], event->deviceChangeAction);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && args[0] != nullptr,
                "%{public}s fail to create DeviceChange callback", request.c_str());

            const size_t argCount = 1;
            napi_value result = nullptr;
            nstatus = napi_call_function(env, nullptr, jsCallback, argCount, args, &result);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok, "%{public}s fail to call DeviceChange callback", request.c_str());
        } while (0);
        delete event;
        delete work;
    });
    if (ret != 0) {
        AUDIO_ERR_LOG("Failed to execute libuv work queue");
        delete work;
    } else {
        jsCb.release();
    }
}
}  // namespace AudioStandard
}  // namespace OHOS
