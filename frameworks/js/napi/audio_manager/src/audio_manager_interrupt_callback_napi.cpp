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
#include <uv.h>

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_manager_interrupt_callback_napi.h"

namespace OHOS {
namespace AudioStandard {
AudioManagerInterruptCallbackNapi::AudioManagerInterruptCallbackNapi(napi_env env)
    : env_(env)
{
    AUDIO_INFO_LOG("AudioManagerInterruptCallbackNapi: instance create");
}

AudioManagerInterruptCallbackNapi::~AudioManagerInterruptCallbackNapi()
{
    AUDIO_INFO_LOG("AudioManagerInterruptCallbackNapi: instance destroy");
}

void AudioManagerInterruptCallbackNapi::SaveCallbackReference(const std::string &callbackName, napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    napi_ref callback = nullptr;
    const int32_t refCount = 1;
    napi_status status = napi_create_reference(env_, args, refCount, &callback);
    CHECK_AND_RETURN_LOG(status == napi_ok && callback != nullptr,
                         "SaveCallbackReference: creating reference for callback fail");
    std::shared_ptr<AutoRef> cb = std::make_shared<AutoRef>(env_, callback);
    if (callbackName == INTERRUPT_CALLBACK_NAME) {
        audioManagerInterruptCallback_ = cb;
    } else {
        AUDIO_ERR_LOG("SaveCallbackReference: Unknown callback type: %{public}s", callbackName.c_str());
    }
}

static void SetValueInt32(const napi_env &env, const std::string &fieldStr, const int intValue, napi_value &result)
{
    napi_value value;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void SetValueBoolean(const napi_env &env, const std::string &fieldStr, const bool boolValue, napi_value &result)
{
    napi_value value;
    napi_get_boolean(env, boolValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void NativeInterruptActionToJsObj(const napi_env &env, napi_value &jsObj,
    const InterruptAction &interruptAction)
{
    napi_create_object(env, &jsObj);
    SetValueInt32(env, "actionType", static_cast<int32_t>(interruptAction.actionType), jsObj);
    SetValueInt32(env, "type", static_cast<int32_t>(interruptAction.interruptType), jsObj);
    SetValueInt32(env, "hint", static_cast<int32_t>(interruptAction.interruptHint), jsObj);
    SetValueBoolean(env, "activated", interruptAction.activated, jsObj);
}

void AudioManagerInterruptCallbackNapi::OnInterrupt(const InterruptAction &interruptAction)
{
    std::lock_guard<std::mutex> lock(mutex_);
    AUDIO_INFO_LOG("OnInterrupt action: %{public}d IntType: %{public}d, IntHint: %{public}d, activated: %{public}d",
        interruptAction.actionType, interruptAction.interruptType, interruptAction.interruptHint,
        interruptAction.activated);
    CHECK_AND_RETURN_LOG(audioManagerInterruptCallback_ != nullptr, "Cannot find the reference of interrupt callback");
    std::unique_ptr<AudioManagerInterruptJsCallback> cb = std::make_unique<AudioManagerInterruptJsCallback>();
    CHECK_AND_RETURN_LOG(cb != nullptr, "No memory");
    cb->callback = audioManagerInterruptCallback_;
    cb->callbackName = INTERRUPT_CALLBACK_NAME;
    cb->interruptAction = interruptAction;
    return OnJsCallbackAudioManagerInterrupt(cb);
}

void AudioManagerInterruptCallbackNapi::OnJsCallbackAudioManagerInterrupt (
    std::unique_ptr<AudioManagerInterruptJsCallback> &jsCb)
{
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        return;
    }

    uv_work_t *work = new(std::nothrow) uv_work_t;
    if (work == nullptr) {
        AUDIO_ERR_LOG("OnJsCallbackAudioManagerInterrupt: No memory");
        return;
    }
    if (jsCb.get() == nullptr) {
        AUDIO_ERR_LOG("OnJsCallbackAudioManagerInterrupt: jsCb.get() is null");
        delete work;
        return;
    }
    work->data = reinterpret_cast<void *>(jsCb.get());

    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, [] (uv_work_t *work, int status) {
        // Js Thread
        AudioManagerInterruptJsCallback *event = reinterpret_cast<AudioManagerInterruptJsCallback *>(work->data);
        std::string request = event->callbackName;
        napi_env env = event->callback->env_;
        napi_ref callback = event->callback->cb_;
        AUDIO_INFO_LOG("OnJsCallbackAudioManagerInterrupt: JsCallBack %{public}s, uv_queue_work start",
            request.c_str());
        do {
            CHECK_AND_BREAK_LOG(status != UV_ECANCELED, "%{public}s canceled", request.c_str());

            napi_value jsCallback = nullptr;
            napi_status nstatus = napi_get_reference_value(env, callback, &jsCallback);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && jsCallback != nullptr, "%{public}s get reference value fail",
                request.c_str());

            // Call back function
            napi_value args[1] = { nullptr };
            NativeInterruptActionToJsObj(env, args[0], event->interruptAction);
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
        AUDIO_ERR_LOG("OnJsCallbackAudioManagerInterrupt: Failed to execute libuv work queue");
        delete work;
    } else {
        jsCb.release();
    }
}
}  // namespace AudioStandard
}  // namespace OHOS
