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

#include "audio_micstatechange_callback_napi.h"

#include <uv.h>

#include "audio_errors.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
AudioManagerMicStateChangeCallbackNapi::AudioManagerMicStateChangeCallbackNapi(napi_env env)
    : env_(env)
{
    AUDIO_DEBUG_LOG("AudioManagerMicStateChangeCallbackNapi: instance create");
}

AudioManagerMicStateChangeCallbackNapi::~AudioManagerMicStateChangeCallbackNapi()
{
    AUDIO_DEBUG_LOG("AudioManagerCallbackNapi: instance destroy");
}

void AudioManagerMicStateChangeCallbackNapi::SaveCallbackReference(const std::string &callbackName, napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    napi_ref callback = nullptr;
    const int32_t refCount = 1;
    napi_status status = napi_create_reference(env_, args, refCount, &callback);
    CHECK_AND_RETURN_LOG(status == napi_ok && callback != nullptr,
        "AudioManagerMicStateChangeCallbackNapi: creating reference for callback fail");

    std::shared_ptr<AutoRef> cb = std::make_shared<AutoRef>(env_, callback);
    if (callbackName == MIC_STATE_CHANGE_CALLBACK_NAME) {
        micStateChangeCallback_ = cb;
    } else {
        AUDIO_ERR_LOG("AudioManagerMicStateChangeCallbackNapi:Unknown callback type:%{public}s", callbackName.c_str());
    }
}

static void SetValueBoolean(const napi_env &env, const std::string &fieldStr, const bool boolValue, napi_value &result)
{
    napi_value value;
    napi_get_boolean(env, boolValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void NativeMicStateChangeToJsObj(const napi_env &env, napi_value &jsObj,
    const  MicStateChangeEvent &micStateChangeEvent)
{
    napi_create_object(env, &jsObj);
    SetValueBoolean(env, "mute", micStateChangeEvent.mute, jsObj);
}

void AudioManagerMicStateChangeCallbackNapi::OnMicStateUpdated(const MicStateChangeEvent &micStateChangeEvent)
{
    std::lock_guard<std::mutex> lock(mutex_);
    CHECK_AND_RETURN_LOG(micStateChangeCallback_ != nullptr, "callback not registered by JS client");

    std::unique_ptr<AudioManagerMicStateChangeJsCallback> cb = std::make_unique<AudioManagerMicStateChangeJsCallback>();
    CHECK_AND_RETURN_LOG(cb != nullptr, "No memory");

    cb->callback = micStateChangeCallback_;
    cb->callbackName = MIC_STATE_CHANGE_CALLBACK_NAME;
    cb->micStateChangeEvent = micStateChangeEvent;
    return OnJsCallbackMicStateChange(cb);
}

void AudioManagerMicStateChangeCallbackNapi::OnJsCallbackMicStateChange
    (std::unique_ptr<AudioManagerMicStateChangeJsCallback> &jsCb)
{
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        return;
    }

    uv_work_t *work = new(std::nothrow) uv_work_t;
    if (work == nullptr) {
        AUDIO_ERR_LOG("AudioManagerCallbackNapi: OnJsCallbackMicStateChange: No memory");
        return;
    }

    if (jsCb.get() == nullptr) {
        AUDIO_ERR_LOG("AudioManagerCallbackNapi: OnJsCallbackMicStateChange: jsCb.get() is null");
        delete work;
        return;
    }

    work->data = reinterpret_cast<void *>(jsCb.get());

    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, [] (uv_work_t *work, int status) {
        // Js Thread
        AudioManagerMicStateChangeJsCallback *event =
            reinterpret_cast<AudioManagerMicStateChangeJsCallback *>(work->data);
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

            NativeMicStateChangeToJsObj(env, args[0], event->micStateChangeEvent);
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
