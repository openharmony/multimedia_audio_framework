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

#include "audio_renderer_callback_napi.h"

#include <uv.h>

#include "audio_errors.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
AudioRendererCallbackNapi::AudioRendererCallbackNapi(napi_env env)
    : env_(env)
{
    AUDIO_DEBUG_LOG("AudioRendererCallbackNapi: instance create");
}

AudioRendererCallbackNapi::~AudioRendererCallbackNapi()
{
    AUDIO_DEBUG_LOG("AudioRendererCallbackNapi: instance destroy");
}

void AudioRendererCallbackNapi::SaveCallbackReference(const std::string &callbackName, napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    napi_ref callback = nullptr;
    const int32_t refCount = 1;
    napi_status status = napi_create_reference(env_, args, refCount, &callback);
    CHECK_AND_RETURN_LOG(status == napi_ok && callback != nullptr,
                         "AudioRendererCallbackNapi: creating reference for callback fail");

    std::shared_ptr<AutoRef> cb = std::make_shared<AutoRef>(env_, callback);
    if (callbackName == INTERRUPT_CALLBACK_NAME || callbackName == AUDIO_INTERRUPT_CALLBACK_NAME) {
        interruptCallback_ = cb;
    } else if (callbackName == STATE_CHANGE_CALLBACK_NAME) {
        stateChangeCallback_ = cb;
    } else {
        AUDIO_ERR_LOG("AudioRendererCallbackNapi: Unknown callback type: %{public}s", callbackName.c_str());
    }
}

static void SetValueInt32(const napi_env& env, const std::string& fieldStr, const int intValue, napi_value& result)
{
    napi_value value = nullptr;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void NativeInterruptEventToJsObj(const napi_env& env, napi_value& jsObj,
    const InterruptEvent& interruptEvent)
{
        napi_create_object(env, &jsObj);
        SetValueInt32(env, "eventType", static_cast<int32_t>(interruptEvent.eventType), jsObj);
        SetValueInt32(env, "forceType", static_cast<int32_t>(interruptEvent.forceType), jsObj);
        SetValueInt32(env, "hintType", static_cast<int32_t>(interruptEvent.hintType), jsObj);
}

void AudioRendererCallbackNapi::OnInterrupt(const InterruptEvent &interruptEvent)
{
    std::lock_guard<std::mutex> lock(mutex_);
    AUDIO_DEBUG_LOG("AudioRendererCallbackNapi: OnInterrupt is called");
    AUDIO_DEBUG_LOG("AudioRendererCallbackNapi: hintType: %{public}d", interruptEvent.hintType);
    CHECK_AND_RETURN_LOG(interruptCallback_ != nullptr, "Cannot find the reference of interrupt callback");

    std::unique_ptr<AudioRendererJsCallback> cb = std::make_unique<AudioRendererJsCallback>();
    CHECK_AND_RETURN_LOG(cb != nullptr, "No memory");
    cb->callback = interruptCallback_;
    cb->callbackName = INTERRUPT_CALLBACK_NAME;
    cb->interruptEvent = interruptEvent;
    return OnJsCallbackInterrupt(cb);
}

void AudioRendererCallbackNapi::OnJsCallbackInterrupt(std::unique_ptr<AudioRendererJsCallback> &jsCb)
{
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        return;
    }

    uv_work_t *work = new(std::nothrow) uv_work_t;
    if (work == nullptr) {
        AUDIO_ERR_LOG("AudioRendererCallbackNapi: OnJsCallBackInterrupt: No memory");
        return;
    }
    if (jsCb.get() == nullptr) {
        AUDIO_ERR_LOG("AudioRendererCallbackNapi: OnJsCallBackInterrupt: jsCb.get() is null");
        delete work;
        return;
    }
    work->data = reinterpret_cast<void *>(jsCb.get());

    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, [] (uv_work_t *work, int status) {
        // Js Thread
        AudioRendererJsCallback *event = reinterpret_cast<AudioRendererJsCallback *>(work->data);
        std::string request = event->callbackName;
        napi_env env = event->callback->env_;
        napi_ref callback = event->callback->cb_;
        AUDIO_DEBUG_LOG("AudioRendererCallbackNapi: JsCallBack %{public}s, uv_queue_work start", request.c_str());
        do {
            CHECK_AND_BREAK_LOG(status != UV_ECANCELED, "%{public}s cancelled", request.c_str());

            napi_value jsCallback = nullptr;
            napi_status nstatus = napi_get_reference_value(env, callback, &jsCallback);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && jsCallback != nullptr, "%{public}s get reference value fail",
                request.c_str());

            // Call back function
            napi_value args[1] = { nullptr };
            NativeInterruptEventToJsObj(env, args[0], event->interruptEvent);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && args[0] != nullptr,
                "%{public}s fail to create Interrupt callback", request.c_str());

            const size_t argCount = 1;
            napi_value result = nullptr;
            nstatus = napi_call_function(env, nullptr, jsCallback, argCount, args, &result);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok, "%{public}s fail to call Interrupt callback", request.c_str());
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

void AudioRendererCallbackNapi::OnStateChange(const RendererState state,
    const StateChangeCmdType __attribute__((unused)) cmdType)
{
    std::lock_guard<std::mutex> lock(mutex_);
    AUDIO_DEBUG_LOG("AudioRendererCallbackNapi: OnStateChange is called");
    AUDIO_DEBUG_LOG("AudioRendererCallbackNapi: state: %{public}d", state);
    CHECK_AND_RETURN_LOG(stateChangeCallback_ != nullptr, "Cannot find the reference of stateChange callback");

    std::unique_ptr<AudioRendererJsCallback> cb = std::make_unique<AudioRendererJsCallback>();
    CHECK_AND_RETURN_LOG(cb != nullptr, "No memory");
    cb->callback = stateChangeCallback_;
    cb->callbackName = STATE_CHANGE_CALLBACK_NAME;
    cb->state = state;
    return OnJsCallbackStateChange(cb);
}

void AudioRendererCallbackNapi::OnJsCallbackStateChange(std::unique_ptr<AudioRendererJsCallback> &jsCb)
{
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        return;
    }

    uv_work_t *work = new(std::nothrow) uv_work_t;
    if (work == nullptr) {
        AUDIO_ERR_LOG("AudioRendererCallbackNapi: OnJsCallbackStateChange: No memory");
        return;
    }
    if (jsCb.get() == nullptr) {
        AUDIO_ERR_LOG("AudioRendererCallbackNapi: OnJsCallbackStateChange: jsCb.get() is null");
        delete work;
        return;
    }
    work->data = reinterpret_cast<void *>(jsCb.get());

    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, [] (uv_work_t *work, int status) {
        // Js Thread
        AudioRendererJsCallback *event = reinterpret_cast<AudioRendererJsCallback *>(work->data);
        std::string request = event->callbackName;
        napi_env env = event->callback->env_;
        napi_ref callback = event->callback->cb_;
        AUDIO_DEBUG_LOG("AudioRendererCallbackNapi: JsCallBack %{public}s, uv_queue_work start", request.c_str());
        do {
            CHECK_AND_BREAK_LOG(status != UV_ECANCELED, "%{public}s cancelled", request.c_str());

            napi_value jsCallback = nullptr;
            napi_status nstatus = napi_get_reference_value(env, callback, &jsCallback);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && jsCallback != nullptr, "%{public}s get reference value fail",
                request.c_str());

            // Call back function
            napi_value args[1] = { nullptr };
            nstatus = napi_create_int32(env, event->state, &args[0]);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && args[0] != nullptr,
                "%{public}s fail to create Interrupt callback", request.c_str());

            const size_t argCount = 1;
            napi_value result = nullptr;
            nstatus = napi_call_function(env, nullptr, jsCallback, argCount, args, &result);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok, "%{public}s fail to call Interrupt callback", request.c_str());
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
