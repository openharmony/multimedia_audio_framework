/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "audio_renderer_device_change_callback_napi.h"
#include <uv.h>
#include "audio_errors.h"
#include "audio_log.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
AudioRendererDeviceChangeCallbackNapi::AudioRendererDeviceChangeCallbackNapi(napi_env env)
    : env_(env)
{
    AUDIO_INFO_LOG("instance create");
}

AudioRendererDeviceChangeCallbackNapi::~AudioRendererDeviceChangeCallbackNapi()
{
    AUDIO_INFO_LOG("instance destroy");
}

void AudioRendererDeviceChangeCallbackNapi::AddCallbackReference(napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    napi_ref callback = nullptr;
    const int32_t refCount = 1;
    bool isEquals = false;
    napi_value copyValue = nullptr;

    for (auto ref = callbacks_.begin(); ref != callbacks_.end(); ++ref) {
        napi_get_reference_value(env_, *ref, &copyValue);
        if (napi_ok != napi_strict_equals(env_, copyValue, args, &isEquals)) {
            AUDIO_ERR_LOG("get napi_strict_equals failed");
            return;
        }
        if (isEquals == true) {
            AUDIO_INFO_LOG("js Callback already exist");
            return;
        }
    }

    napi_status status = napi_create_reference(env_, args, refCount, &callback);
    CHECK_AND_RETURN_LOG(status == napi_ok && callback != nullptr,
        "AudioRendererDeviceChangeCallbackNapi: creating reference for callback fail");

    callbacks_.push_back(callback);
    AUDIO_INFO_LOG("AddCallbackReference successful");
}

void AudioRendererDeviceChangeCallbackNapi::RemoveCallbackReference(napi_env env, napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    bool isEquals = false;
    napi_value copyValue = nullptr;

    if (args == nullptr) {
        for (auto ref = callbacks_.begin(); ref != callbacks_.end(); ++ref) {
            napi_status ret = napi_delete_reference(env, *ref);
            CHECK_AND_RETURN_LOG(napi_ok == ret, "delete callback reference failed");
        }
        callbacks_.clear();
        AUDIO_INFO_LOG("Remove all JS Callback");
        return;
    }

    for (auto ref = callbacks_.begin(); ref != callbacks_.end(); ++ref) {
        napi_get_reference_value(env, *ref, &copyValue);
        if (copyValue == nullptr) {
            AUDIO_ERR_LOG("copyValue is nullptr");
            return;
        }
        if (napi_ok != napi_strict_equals(env, args, copyValue, &isEquals)) {
            AUDIO_ERR_LOG("get napi_strict_equals failed");
            return;
        }

        if (isEquals == true) {
            AUDIO_INFO_LOG("found JS Callback, delete it!");
            callbacks_.remove(*ref);
            napi_status status = napi_delete_reference(env, *ref);
            CHECK_AND_RETURN_LOG(status == napi_ok,
                "deleting reference for callback fail");
            return;
        }
    }

    AUDIO_INFO_LOG("RemoveCallbackReference success");
}

void AudioRendererDeviceChangeCallbackNapi::RemoveAllCallbacks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto ref = callbacks_.begin(); ref != callbacks_.end(); ++ref) {
        napi_status ret = napi_delete_reference(env_, *ref);
        CHECK_AND_RETURN_LOG(napi_ok == ret, "delete callback reference failed");
    }
    callbacks_.clear();
    AUDIO_INFO_LOG("RemoveAllCallbacks successful");
}

void AudioRendererDeviceChangeCallbackNapi::OnStateChange(const DeviceInfo &deviceInfo)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto ref = callbacks_.begin(); ref != callbacks_.end(); ++ref) {
        OnJsCallbackRendererDeviceInfo(*ref, deviceInfo);
    }
}

static void SetValueInt32(const napi_env& env, const std::string& fieldStr, const int intValue, napi_value& obj)
{
    napi_value value = nullptr;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, obj, fieldStr.c_str(), value);
}

static void SetValueString(const napi_env &env, const std::string &fieldStr, const std::string &stringValue,
    napi_value &result)
{
    napi_value value = nullptr;
    napi_create_string_utf8(env, stringValue.c_str(), NAPI_AUTO_LENGTH, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void SetDeviceDescriptors(const napi_env& env, napi_value &jsChangeInfoObj, const DeviceInfo &deviceInfo)
{
    SetValueInt32(env, "deviceRole", static_cast<int32_t>(deviceInfo.deviceRole), jsChangeInfoObj);
    SetValueInt32(env, "deviceType", static_cast<int32_t>(deviceInfo.deviceType), jsChangeInfoObj);
    SetValueInt32(env, "id", static_cast<int32_t>(deviceInfo.deviceId), jsChangeInfoObj);
    SetValueString(env, "name", deviceInfo.deviceName, jsChangeInfoObj);
    SetValueString(env, "address", deviceInfo.macAddress, jsChangeInfoObj);

    napi_value value = nullptr;
    napi_value sampleRates;
    napi_create_array_with_length(env, 1, &sampleRates);
    napi_create_int32(env, deviceInfo.audioStreamInfo.samplingRate, &value);
    napi_set_element(env, sampleRates, 0, value);
    napi_set_named_property(env, jsChangeInfoObj, "sampleRates", sampleRates);

    napi_value channelCounts;
    napi_create_array_with_length(env, 1, &channelCounts);
    napi_create_int32(env, deviceInfo.audioStreamInfo.channels, &value);
    napi_set_element(env, channelCounts, 0, value);
    napi_set_named_property(env, jsChangeInfoObj, "channelCounts", channelCounts);

    napi_value channelMasks;
    napi_create_array_with_length(env, 1, &channelMasks);
    napi_create_int32(env, deviceInfo.channelMasks, &value);
    napi_set_element(env, channelMasks, 0, value);
    napi_set_named_property(env, jsChangeInfoObj, "channelMasks", channelMasks);
}

static void NativeRendererChangeInfoToJsObj(const napi_env &env, napi_value &jsChangeDeviceInfosObj,
    const DeviceInfo &devInfo)
{
    napi_value valueParam = nullptr;
    napi_create_array_with_length(env, 1, &jsChangeDeviceInfosObj);
    napi_create_object(env, &valueParam);
    SetDeviceDescriptors(env, valueParam, devInfo);
    napi_set_element(env, jsChangeDeviceInfosObj, 0, valueParam);
}

void AudioRendererDeviceChangeCallbackNapi::WorkCallbackCompleted(uv_work_t *work, int status)
{
    // Js Thread
    std::shared_ptr<AudioRendererDeviceChangeJsCallback> context(
        static_cast<AudioRendererDeviceChangeJsCallback*>(work->data),
        [work](AudioRendererDeviceChangeJsCallback* ptr) {
            delete ptr;
            delete work;
    });

    AudioRendererDeviceChangeJsCallback *event = reinterpret_cast<AudioRendererDeviceChangeJsCallback*>(work->data);
    if (event == nullptr || event->callback_ == nullptr) {
        AUDIO_ERR_LOG("OnJsCallbackRendererDeviceInfo: No memory");
        return;
    }
    napi_env env = event->env_;
    napi_ref callback = event->callback_;

    do {
        napi_value jsCallback = nullptr;
        napi_status nstatus = napi_get_reference_value(env, callback, &jsCallback);
        CHECK_AND_BREAK_LOG(nstatus == napi_ok && jsCallback != nullptr, "callback get reference value fail");
        // Call back function
        napi_value args[1] = { nullptr };
        NativeRendererChangeInfoToJsObj(env, args[0], event->deviceInfo_);
        CHECK_AND_BREAK_LOG(nstatus == napi_ok && args[0] != nullptr,
            " fail to convert to jsobj");

        const size_t argCount = 1;
        napi_value result = nullptr;
        nstatus = napi_call_function(env, nullptr, jsCallback, argCount, args, &result);
        CHECK_AND_BREAK_LOG(nstatus == napi_ok, "Fail to call devicechange callback");
    } while (0);
}

void AudioRendererDeviceChangeCallbackNapi::OnJsCallbackRendererDeviceInfo(napi_ref method,
    const DeviceInfo &deviceInfo)
{
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        AUDIO_ERR_LOG("OnJsCallbackRendererDeviceInfo loop_ is nullptr");
        return;
    }

    if (method == nullptr) {
        AUDIO_ERR_LOG("OnJsCallbackRendererDeviceInfo method is nullptr");
        return;
    }

    uv_work_t *work = new(std::nothrow) uv_work_t;
    if (work == nullptr) {
        AUDIO_ERR_LOG("OnJsCallbackRendererDeviceInfo: No memory");
    }

    work->data = new AudioRendererDeviceChangeJsCallback {method, env_, deviceInfo};
    if (work->data == nullptr) {
        AUDIO_ERR_LOG("OnJsCallbackRendererDeviceInfo failed: No memory");
    }
    AUDIO_ERR_LOG("OnJsCallbackRendererDeviceInfo");
    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, WorkCallbackCompleted);
    if (ret != 0) {
        AUDIO_ERR_LOG("Failed to execute libuv work queue");
        delete work;
    }
}

int32_t AudioRendererDeviceChangeCallbackNapi::GetCallbackListSize() const
{
    return callbacks_.size();
}
}  // namespace AudioStandard
}  // namespace OHOS
