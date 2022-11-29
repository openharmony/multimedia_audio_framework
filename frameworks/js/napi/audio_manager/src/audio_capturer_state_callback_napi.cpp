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

#include "audio_capturer_state_callback_napi.h"

#include <uv.h>

#include "audio_errors.h"
#include "audio_log.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
AudioCapturerStateCallbackNapi::AudioCapturerStateCallbackNapi(napi_env env)
    : env_(env)
{
    AUDIO_DEBUG_LOG("AudioCapturerStateCallbackNapi: instance create");
}

AudioCapturerStateCallbackNapi::~AudioCapturerStateCallbackNapi()
{
    AUDIO_DEBUG_LOG("AudioCapturerStateCallbackNapi: instance destroy");
}

void AudioCapturerStateCallbackNapi::SaveCallbackReference(napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    napi_ref callback = nullptr;
    const int32_t refCount = 1;
    napi_status status = napi_create_reference(env_, args, refCount, &callback);
    CHECK_AND_RETURN_LOG(status == napi_ok && callback != nullptr,
                         "AudioCapturerStateCallbackNapi: creating reference for callback fail");

    std::shared_ptr<AutoRef> cb = std::make_shared<AutoRef>(env_, callback);
    CHECK_AND_RETURN_LOG(cb != nullptr, "AudioCapturerStateCallbackNapi: creating callback failed");

    capturerStateCallback_ = cb;
}

void AudioCapturerStateCallbackNapi::OnCapturerStateChange(
    const std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    AUDIO_INFO_LOG("AudioCapturerStateCallbackNapi: OnCapturerStateChange is called");

    std::lock_guard<std::mutex> lock(mutex_);
    std::unique_ptr<AudioCapturerStateJsCallback> cb = std::make_unique<AudioCapturerStateJsCallback>();
    CHECK_AND_RETURN_LOG(cb != nullptr, "No memory!!");

    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> capturerChangeInfos;
    for (const auto &changeInfo : audioCapturerChangeInfos) {
        capturerChangeInfos.push_back(std::make_unique<AudioCapturerChangeInfo>(*changeInfo));
    }

    cb->callback = capturerStateCallback_;
    cb->changeInfos = move(capturerChangeInfos);

    return OnJsCallbackCapturerState(cb);
}

static void SetValueInt32(const napi_env& env, const std::string& fieldStr, const int intValue, napi_value& obj)
{
    napi_value value = nullptr;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, obj, fieldStr.c_str(), value);
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

static void NativeCapturerChangeInfoToJsObj(const napi_env &env, napi_value &jsArrayChangeInfoObj,
    const vector<unique_ptr<AudioCapturerChangeInfo>> &changeInfos)
{
    napi_value jsChangeInfoObj = nullptr;
    napi_value jsCapInfoObj = nullptr;

    size_t size = changeInfos.size();
    int32_t position = 0;

    napi_create_array_with_length(env, size, &jsArrayChangeInfoObj);
    for (const unique_ptr<AudioCapturerChangeInfo> &changeInfo : changeInfos) {
        AUDIO_DEBUG_LOG("AudioCapturerStateCallbackNapi: ChangeInfo to jsobj called");
        napi_create_object(env, &jsChangeInfoObj);
        SetValueInt32(env, "streamId", static_cast<int32_t>(changeInfo->sessionId), jsChangeInfoObj);
        SetValueInt32(env, "capturerState", static_cast<int32_t>(changeInfo->capturerState), jsChangeInfoObj);
        SetValueInt32(env, "clientUid", static_cast<int32_t>(changeInfo->clientUID), jsChangeInfoObj);

        napi_create_object(env, &jsCapInfoObj);
        SetValueInt32(env, "source", static_cast<int32_t>(changeInfo->capturerInfo.sourceType), jsCapInfoObj);
        SetValueInt32(env, "capturerFlags", changeInfo->capturerInfo.capturerFlags, jsCapInfoObj);
        napi_set_named_property(env, jsChangeInfoObj, "capturerInfo", jsCapInfoObj);
        SetDeviceDescriptors(env, jsChangeInfoObj, changeInfo->inputDeviceInfo);

        napi_set_element(env, jsArrayChangeInfoObj, position, jsChangeInfoObj);
        position++;
    }
}

void AudioCapturerStateCallbackNapi::OnJsCallbackCapturerState(std::unique_ptr<AudioCapturerStateJsCallback> &jsCb)
{
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        return;
    }

    uv_work_t *work = new(std::nothrow) uv_work_t;
    if (work == nullptr) {
        AUDIO_ERR_LOG("AudioCapturerStateCallbackNapi: OnJsCallbackCapturerState: No memory");
        return;
    }

    work->data = reinterpret_cast<void *>(jsCb.get());

    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, [] (uv_work_t *work, int status) {
        // Js Thread
        AudioCapturerStateJsCallback *event = reinterpret_cast<AudioCapturerStateJsCallback *>(work->data);
        if (event == nullptr || event->callback == nullptr) {
            AUDIO_ERR_LOG("AudioCapturerStateCallbackNapi: OnJsCallbackCapturerState: data or callback is null.");
            delete event;
            delete work;
            return;
        }
        napi_env env = event->callback->env_;
        napi_ref callback = event->callback->cb_;

        do {
            napi_value jsCallback = nullptr;
            napi_status nstatus = napi_get_reference_value(env, callback, &jsCallback);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && jsCallback != nullptr,
                "callback get reference value fail");
            // Call back function
            napi_value args[1] = { nullptr };
            NativeCapturerChangeInfoToJsObj(env, args[0], event->changeInfos);

            CHECK_AND_BREAK_LOG(nstatus == napi_ok && args[0] != nullptr,
                " fail to convert to jsobj");

            const size_t argCount = 1;
            napi_value result = nullptr;
            nstatus = napi_call_function(env, nullptr, jsCallback, argCount, args, &result);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok, "Fail to call renderstate callback");
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
