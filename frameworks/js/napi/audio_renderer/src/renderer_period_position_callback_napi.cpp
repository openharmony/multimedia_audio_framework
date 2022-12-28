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

#include "renderer_period_position_callback_napi.h"

#include <uv.h>

#include "audio_errors.h"
#include "audio_log.h"

namespace {
    const std::string RENDERER_PERIOD_POSITION_CALLBACK_NAME = "periodReach";
}

namespace OHOS {
namespace AudioStandard {
RendererPeriodPositionCallbackNapi::RendererPeriodPositionCallbackNapi(napi_env env)
    : env_(env)
{
    AUDIO_DEBUG_LOG("RendererPeriodPositionCallbackNapi: instance create");
}

RendererPeriodPositionCallbackNapi::~RendererPeriodPositionCallbackNapi()
{
    AUDIO_DEBUG_LOG("RendererPeriodPositionCallbackNapi: instance destroy");
}

void RendererPeriodPositionCallbackNapi::SaveCallbackReference(const std::string &callbackName, napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    napi_ref callback = nullptr;
    const int32_t refCount = 1;
    napi_status status = napi_create_reference(env_, args, refCount, &callback);
    CHECK_AND_RETURN_LOG(status == napi_ok && callback != nullptr,
                         "RendererPeriodPositionCallbackNapi: creating reference for callback fail");

    std::shared_ptr<AutoRef> cb = std::make_shared<AutoRef>(env_, callback);
    if (callbackName == RENDERER_PERIOD_POSITION_CALLBACK_NAME) {
        renderPeriodPositionCallback_ = cb;
    } else {
        AUDIO_ERR_LOG("RendererPeriodPositionCallbackNapi: Unknown callback type: %{public}s", callbackName.c_str());
    }
}

void RendererPeriodPositionCallbackNapi::OnPeriodReached(const int64_t &frameNumber)
{
    std::lock_guard<std::mutex> lock(mutex_);
    AUDIO_DEBUG_LOG("RendererPeriodPositionCallbackNapi: period reached");
    CHECK_AND_RETURN_LOG(renderPeriodPositionCallback_ != nullptr, "Cannot find the reference of position callback");

    std::unique_ptr<RendererPeriodPositionJsCallback> cb = std::make_unique<RendererPeriodPositionJsCallback>();
    CHECK_AND_RETURN_LOG(cb != nullptr, "No memory");
    cb->callback = renderPeriodPositionCallback_;
    cb->callbackName = RENDERER_PERIOD_POSITION_CALLBACK_NAME;
    cb->position = frameNumber;
    return OnJsRendererPeriodPositionCallback(cb);
}

void RendererPeriodPositionCallbackNapi::OnJsRendererPeriodPositionCallback(
    std::unique_ptr<RendererPeriodPositionJsCallback> &jsCb)
{
    uv_loop_s *loop = nullptr;
    CHECK_AND_RETURN_LOG(env_ != nullptr,
                         "RendererPeriodPositionCallbackNapi: OnJsRendererPeriodPositionCallback: env_ is null");
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        return;
    }
    uv_work_t *work = new(std::nothrow) uv_work_t;
    CHECK_AND_RETURN_LOG(work != nullptr,
                         "RendererPeriodPositionCallbackNapi: OnJsRendererPeriodPositionCallback: No memory");
    if (jsCb.get() == nullptr) {
        AUDIO_ERR_LOG("RendererPeriodPositionCallbackNapi: OnJsRendererPeriodPositionCallback: jsCb.get() is null");
        delete work;
        return;
    }
    work->data = reinterpret_cast<void *>(jsCb.get());

    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, [] (uv_work_t *work, int status) {
        // Js Thread
        RendererPeriodPositionJsCallback *event = reinterpret_cast<RendererPeriodPositionJsCallback *>(work->data);
        std::string request = event->callbackName;
        napi_env env = event->callback->env_;
        napi_ref callback = event->callback->cb_;
        AUDIO_DEBUG_LOG("RendererPeriodPositionCallbackNapi: JsCallBack %{public}s, uv_queue_work start",
            request.c_str());
        do {
            CHECK_AND_BREAK_LOG(status != UV_ECANCELED, "%{public}s canceled", request.c_str());

            napi_value jsCallback = nullptr;
            napi_status nstatus = napi_get_reference_value(env, callback, &jsCallback);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && jsCallback != nullptr, "%{public}s get reference value fail",
                request.c_str());

            // Call back function
            napi_value args[1] = { nullptr };
            napi_create_int64(env, event->position, &args[0]);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && args[0] != nullptr,
                "%{public}s fail to create position callback", request.c_str());

            const size_t argCount = 1;
            napi_value result = nullptr;
            nstatus = napi_call_function(env, nullptr, jsCallback, argCount, args, &result);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok, "%{public}s fail to call position callback", request.c_str());
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
