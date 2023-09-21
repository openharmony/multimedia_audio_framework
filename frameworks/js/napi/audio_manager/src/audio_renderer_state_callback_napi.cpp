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

#include "audio_renderer_state_callback_napi.h"

#include <uv.h>

#include "audio_errors.h"
#include "audio_log.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
AudioRendererStateCallbackNapi::AudioRendererStateCallbackNapi(napi_env env)
    : env_(env)
{
    AUDIO_DEBUG_LOG("AudioRendererStateCallbackNapi: instance create");
}

AudioRendererStateCallbackNapi::~AudioRendererStateCallbackNapi()
{
    AUDIO_DEBUG_LOG("AudioRendererStateCallbackNapi: instance destroy");
}

void AudioRendererStateCallbackNapi::RemoveCallbackReference()
{
    std::lock_guard<std::mutex> lock(mutex_);
    rendererStateCallback_.reset();
}

void AudioRendererStateCallbackNapi::SaveCallbackReference(napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    napi_ref callback = nullptr;
    const int32_t refCount = 1;
    napi_status status = napi_create_reference(env_, args, refCount, &callback);
    CHECK_AND_RETURN_LOG(status == napi_ok && callback != nullptr,
                         "AudioRendererStateCallbackNapi: creating reference for callback fail");

    std::shared_ptr<AutoRef> cb = std::make_shared<AutoRef>(env_, callback);
    CHECK_AND_RETURN_LOG(cb != nullptr, "AudioRendererStateCallbackNapi: creating callback failed");

    rendererStateCallback_ = cb;
}

void AudioRendererStateCallbackNapi::OnRendererStateChange(
    const std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos)
{
    AUDIO_INFO_LOG("OnRendererStateChange entered");

    std::lock_guard<std::mutex> lock(mutex_);

    CHECK_AND_RETURN_LOG(rendererStateCallback_ != nullptr, "rendererStateCallback_ is nullptr!");

    std::unique_ptr<AudioRendererStateJsCallback> cb = std::make_unique<AudioRendererStateJsCallback>();
    CHECK_AND_RETURN_LOG(cb != nullptr, "No memory!!");

    std::vector<std::unique_ptr<AudioRendererChangeInfo>> rendererChangeInfos;
    for (const auto &changeInfo : audioRendererChangeInfos) {
        rendererChangeInfos.push_back(std::make_unique<AudioRendererChangeInfo>(*changeInfo));
    }

    cb->callback = rendererStateCallback_;
    cb->changeInfos = move(rendererChangeInfos);

    return OnJsCallbackRendererState(cb);
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
    SetValueString(env, "displayName", deviceInfo.displayName, valueParam);
    SetValueString(env, "networkId", deviceInfo.networkId, valueParam);
    SetValueInt32(env, "interruptGroupId", static_cast<int32_t>(deviceInfo.interruptGroupId), valueParam);
    SetValueInt32(env, "volumeGroupId", static_cast<int32_t>(deviceInfo.volumeGroupId), valueParam);

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

static void NativeRendererChangeInfoToJsObj(const napi_env &env, napi_value &jsArrayChangeInfoObj,
    const vector<unique_ptr<AudioRendererChangeInfo>> &changeInfos)
{
    napi_value jsChangeInfoObj = nullptr;
    napi_value jsRenInfoObj = nullptr;

    size_t size = changeInfos.size();
    int32_t position = 0;
    napi_create_array_with_length(env, size, &jsArrayChangeInfoObj);
    for (const unique_ptr<AudioRendererChangeInfo> &changeInfo: changeInfos) {
        napi_create_object(env, &jsChangeInfoObj);
        SetValueInt32(env, "streamId", static_cast<int32_t>(changeInfo->sessionId), jsChangeInfoObj);
        SetValueInt32(env, "rendererState", static_cast<int32_t>(changeInfo->rendererState), jsChangeInfoObj);
        SetValueInt32(env, "clientUid", static_cast<int32_t>(changeInfo->clientUID), jsChangeInfoObj);
        napi_create_object(env, &jsRenInfoObj);
        SetValueInt32(env, "content", static_cast<int32_t>(changeInfo->rendererInfo.contentType), jsRenInfoObj);
        SetValueInt32(env, "usage", static_cast<int32_t>(changeInfo->rendererInfo.streamUsage), jsRenInfoObj);
        SetValueInt32(env, "rendererFlags", changeInfo->rendererInfo.rendererFlags, jsRenInfoObj);
        napi_set_named_property(env, jsChangeInfoObj, "rendererInfo", jsRenInfoObj);
        SetDeviceDescriptors(env, jsChangeInfoObj, changeInfo->outputDeviceInfo);

        napi_set_element(env, jsArrayChangeInfoObj, position, jsChangeInfoObj);
        position++;
    }
}

void AudioRendererStateCallbackNapi::OnJsCallbackRendererState(std::unique_ptr<AudioRendererStateJsCallback> &jsCb)
{
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        return;
    }

    uv_work_t *work = new(std::nothrow) uv_work_t;
    if (work == nullptr) {
        AUDIO_ERR_LOG("AudioRendererStateCallbackNapi: OnJsCallbackRendererState: No memory");
        return;
    }

    work->data = reinterpret_cast<void *>(jsCb.get());

    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, [] (uv_work_t *work, int status) {
        // Js Thread
        AudioRendererStateJsCallback *event = reinterpret_cast<AudioRendererStateJsCallback *>(work->data);
        if (event == nullptr || event->callback == nullptr) {
            AUDIO_ERR_LOG("AudioRendererStateCallbackNapi: OnJsCallbackRendererState: No memory");
            return;
        }
        napi_env env = event->callback->env_;
        napi_ref callback = event->callback->cb_;

        do {
            napi_value jsCallback = nullptr;
            napi_status nstatus = napi_get_reference_value(env, callback, &jsCallback);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && jsCallback != nullptr, "callback get reference value fail");

            // Call back function
            napi_value args[1] = { nullptr };
            NativeRendererChangeInfoToJsObj(env, args[0], event->changeInfos);

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
