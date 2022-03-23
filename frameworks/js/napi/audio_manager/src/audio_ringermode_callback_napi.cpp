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

#include "audio_ringermode_callback_napi.h"

#include <uv.h>

#include "audio_errors.h"
#include "media_log.h"

namespace {
    const std::string RINGERMODE_CALLBACK_NAME = "ringerModeChange";
}

namespace OHOS {
namespace AudioStandard {
AudioRingerModeCallbackNapi::AudioRingerModeCallbackNapi(napi_env env)
    : env_(env)
{
    MEDIA_DEBUG_LOG("AudioRingerModeCallbackNapi: instance create");
}

AudioRingerModeCallbackNapi::~AudioRingerModeCallbackNapi()
{
    MEDIA_DEBUG_LOG("AudioRingerModeCallbackNapi: instance destroy");
}

void AudioRingerModeCallbackNapi::SaveCallbackReference(const std::string &callbackName, napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    napi_ref callback = nullptr;
    const int32_t refCount = 1;
    napi_status status = napi_create_reference(env_, args, refCount, &callback);
    CHECK_AND_RETURN_LOG(status == napi_ok && callback != nullptr,
                         "AudioRingerModeCallbackNapi: creating reference for callback fail");

    std::shared_ptr<AutoRef> cb = std::make_shared<AutoRef>(env_, callback);
    if (callbackName == RINGERMODE_CALLBACK_NAME) {
        ringerModeCallback_ = cb;
    } else {
        MEDIA_ERR_LOG("AudioRingerModeCallbackNapi: Unknown callback type: %{public}s", callbackName.c_str());
    }
}

void AudioRingerModeCallbackNapi::OnRingerModeUpdated(const AudioRingerMode &ringerMode)
{
    std::lock_guard<std::mutex> lock(mutex_);
    MEDIA_DEBUG_LOG("AudioRingerModeCallbackNapi: OnRingerModeUpdated is called");
    MEDIA_DEBUG_LOG("AudioRingerModeCallbackNapi: ringer mode: %{public}d", ringerMode);
    CHECK_AND_RETURN_LOG(ringerModeCallback_ != nullptr, "Cannot find the reference of ringer mode callback");

    std::unique_ptr<AudioRingerModeJsCallback> cb = std::make_unique<AudioRingerModeJsCallback>();
    CHECK_AND_RETURN_LOG(cb != nullptr, "No memory");
    cb->callback = ringerModeCallback_;
    cb->callbackName = RINGERMODE_CALLBACK_NAME;
    cb->ringerMode = ringerMode;
    return OnJsCallbackRingerMode(cb);
}

static AudioManagerNapi::AudioRingMode GetJsAudioRingMode(int32_t ringerMode)
{
    AudioManagerNapi::AudioRingMode result;

    switch (ringerMode) {
        case RINGER_MODE_SILENT:
            result = AudioManagerNapi::RINGER_MODE_SILENT;
            break;
        case RINGER_MODE_VIBRATE:
            result = AudioManagerNapi::RINGER_MODE_VIBRATE;
            break;
        case RINGER_MODE_NORMAL:
            result = AudioManagerNapi::RINGER_MODE_NORMAL;
            break;
        default:
            result = AudioManagerNapi::RINGER_MODE_NORMAL;
            break;
    }

    return result;
}

void AudioRingerModeCallbackNapi::OnJsCallbackRingerMode(std::unique_ptr<AudioRingerModeJsCallback> &jsCb)
{
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        return;
    }

    uv_work_t *work = new(std::nothrow) uv_work_t;
    if (work == nullptr) {
        MEDIA_ERR_LOG("AudioRingerModeCallbackNapi: OnJsCallbackRingerMode: No memory");
        return;
    }
    if (jsCb.get() == nullptr) {
        MEDIA_ERR_LOG("AudioRingerModeCallbackNapi: OnJsCallbackRingerMode: jsCb.get() is null");
        delete work;
        return;
    }
    work->data = reinterpret_cast<void *>(jsCb.get());

    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, [] (uv_work_t *work, int status) {
        // Js Thread
        AudioRingerModeJsCallback *event = reinterpret_cast<AudioRingerModeJsCallback *>(work->data);
        std::string request = event->callbackName;
        napi_env env = event->callback->env_;
        napi_ref callback = event->callback->cb_;
        MEDIA_DEBUG_LOG("AudioRingerModeCallbackNapi: JsCallBack %{public}s, uv_queue_work start", request.c_str());
        do {
            CHECK_AND_BREAK_LOG(status != UV_ECANCELED, "%{public}s canceled", request.c_str());

            napi_value jsCallback = nullptr;
            napi_status nstatus = napi_get_reference_value(env, callback, &jsCallback);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && jsCallback != nullptr, "%{public}s get reference value fail",
                request.c_str());

            // Call back function
            napi_value args[1] = { nullptr };
            napi_create_int32(env, GetJsAudioRingMode(event->ringerMode), &args[0]);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && args[0] != nullptr,
                "%{public}s fail to create ringer mode callback", request.c_str());

            const size_t argCount = 1;
            napi_value result = nullptr;
            nstatus = napi_call_function(env, nullptr, jsCallback, argCount, args, &result);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok, "%{public}s fail to call ringer mode callback", request.c_str());
        } while (0);
        delete event;
        delete work;
    });
    if (ret != 0) {
        MEDIA_ERR_LOG("Failed to execute libuv work queue");
        delete work;
    } else {
        jsCb.release();
    }
}
}  // namespace AudioStandard
}  // namespace OHOS
