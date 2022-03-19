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

#include "media_log.h"

#include "audio_volume_key_event_napi.h"

using namespace std;
namespace {
    const std::string VOLUME_KEY_EVENT_CALLBACK_NAME = "volumeChange";
}
namespace OHOS {
namespace AudioStandard {
constexpr int32_t JS_MEDIA = 3;
constexpr int32_t JS_VOICE_CALL = 0;
constexpr int32_t JS_RINGTONE = 2;
constexpr int32_t JS_STREAM_ERROR = -1;

AudioVolumeKeyEventNapi::AudioVolumeKeyEventNapi(napi_env env)
    :env_(env)
{
    MEDIA_DEBUG_LOG("AudioVolumeKeyEventNapi::Constructor");
}

AudioVolumeKeyEventNapi::~AudioVolumeKeyEventNapi()
{
    MEDIA_DEBUG_LOG("AudioVolumeKeyEventNapi::Destructor");
}

void AudioVolumeKeyEventNapi::OnVolumeKeyEvent(AudioStreamType streamType, int32_t volumeLevel, bool isUpdateUi)
{
    std::lock_guard<std::mutex> lock(mutex_);
    MEDIA_DEBUG_LOG("AudioVolumeKeyEventNapi: OnVolumeKeyEvent is called volumeLevel=%{public}d", volumeLevel);
    MEDIA_DEBUG_LOG("AudioVolumeKeyEventNapi: isUpdateUi is called isUpdateUi=%{public}d", isUpdateUi);
    if (audioVolumeKeyEventJsCallback_ == nullptr) {
        MEDIA_DEBUG_LOG("AudioManagerCallbackNapi:No JS callback registered return");
        return;
    }
    std::unique_ptr<AudioVolumeKeyEventJsCallback> cb = std::make_unique<AudioVolumeKeyEventJsCallback>();
    CHECK_AND_RETURN_LOG(cb != nullptr, "No memory");
    cb->callback = audioVolumeKeyEventJsCallback_;
    cb->callbackName = VOLUME_KEY_EVENT_CALLBACK_NAME;
    cb->volumeEvent.volumeType = streamType;
    cb->volumeEvent.volume = volumeLevel;
    cb->volumeEvent.updateUi = isUpdateUi;

    return OnJsCallbackVolumeEvent(cb);
}

void AudioVolumeKeyEventNapi::SaveCallbackReference(const std::string &callbackName, napi_value args)
{
    std::lock_guard<std::mutex> lock(mutex_);
    napi_ref callback = nullptr;
    const int32_t refCount = 1;
    napi_status status = napi_create_reference(env_, args, refCount, &callback);
    CHECK_AND_RETURN_LOG(status == napi_ok && callback != nullptr,
                         "AudioVolumeKeyEventNapi: creating reference for callback fail");
    std::shared_ptr<VolumeKeyEventAutoRef> cb = std::make_shared<VolumeKeyEventAutoRef>(env_, callback);
    if (callbackName == VOLUME_KEY_EVENT_CALLBACK_NAME) {
        audioVolumeKeyEventJsCallback_ = cb;
    } else {
        MEDIA_ERR_LOG("AudioVolumeKeyEventNapi: Unknown callback type: %{public}s", callbackName.c_str());
    }
}

static void SetValueInt32(const napi_env& env, const std::string& fieldStr, const int intValue, napi_value& result)
{
    napi_value value = nullptr;
    napi_create_int32(env, intValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static void SetValueBoolean(const napi_env& env, const std::string& fieldStr, const bool boolValue, napi_value& result)
{
    napi_value value = nullptr;
    napi_get_boolean(env, boolValue, &value);
    napi_set_named_property(env, result, fieldStr.c_str(), value);
}

static int32_t GetJsAudioVolumeType(AudioStreamType streamType)
{
    int32_t nativeStreamType = static_cast<int32_t>(streamType);
    int32_t result = JS_STREAM_ERROR;
    switch (nativeStreamType) {
        case AudioStreamType::STREAM_MUSIC:
            result = JS_MEDIA;
            break;
        case AudioStreamType::STREAM_VOICE_CALL:
            result = JS_VOICE_CALL;
            break;
        case AudioStreamType::STREAM_RING:
            result = JS_RINGTONE;
            break;
        default:
            result = JS_STREAM_ERROR;
            break;
    }
    return result;
}

static void NativeVolumeEventToJsObj(const napi_env& env, napi_value& jsObj,
    const VolumeEvent& volumeEvent)
{
    napi_create_object(env, &jsObj);
    SetValueInt32(env, "volumeType", GetJsAudioVolumeType(volumeEvent.volumeType), jsObj);
    SetValueInt32(env, "volume", static_cast<int32_t>(volumeEvent.volume), jsObj);
    SetValueBoolean(env, "updateUi", volumeEvent.updateUi, jsObj);
}

void AudioVolumeKeyEventNapi::OnJsCallbackVolumeEvent(std::unique_ptr<AudioVolumeKeyEventJsCallback> &jsCb)
{
    MEDIA_ERR_LOG("AudioVolumeKeyEventNapi:OnJsCallbackVolumeEvent");
    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(env_, &loop);
    if (loop == nullptr) {
        return;
    }

    uv_work_t *work = new(std::nothrow) uv_work_t;
    if (work == nullptr) {
        MEDIA_ERR_LOG("AudioVolumeKeyEventNapi: OnJsCallBackInterrupt: No memory");
        return;
    }
    work->data = reinterpret_cast<void *>(jsCb.get());

    int ret = uv_queue_work(loop, work, [] (uv_work_t *work) {}, [] (uv_work_t *work, int status) {
        // Js Thread
        AudioVolumeKeyEventJsCallback *event = reinterpret_cast<AudioVolumeKeyEventJsCallback *>(work->data);
        std::string request = event->callbackName;
        napi_env env = event->callback->env_;
        napi_ref callback = event->callback->cb_;
        MEDIA_DEBUG_LOG("AudioVolumeKeyEventNapi: JsCallBack %{public}s, uv_queue_work start", request.c_str());
        do {
            CHECK_AND_BREAK_LOG(status != UV_ECANCELED, "%{public}s canceled", request.c_str());

            napi_value jsCallback = nullptr;
            napi_status nstatus = napi_get_reference_value(env, callback, &jsCallback);
            CHECK_AND_BREAK_LOG(nstatus == napi_ok && jsCallback != nullptr, "%{public}s get reference value fail",
                request.c_str());

            // Call back function
            napi_value args[1] = { nullptr };
            NativeVolumeEventToJsObj(env, args[0], event->volumeEvent);
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
        MEDIA_ERR_LOG("Failed to execute libuv work queue");
        delete work;
    } else {
        jsCb.release();
    }
}
} // namespace AudioStandard
} // namespace OHOS
