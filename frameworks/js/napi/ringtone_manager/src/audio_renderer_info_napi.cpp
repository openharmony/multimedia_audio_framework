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

#include "audio_renderer_info_napi.h"
#include "hilog/log.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
napi_ref AudioRendererInfoNapi::sConstructor_ = nullptr;
unique_ptr<AudioRendererInfo> AudioRendererInfoNapi::sAudioRendererInfo_ = nullptr;

namespace {
    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "AudioRendererInfoNapi"};
}

AudioRendererInfoNapi::AudioRendererInfoNapi()
    : env_(nullptr) {
}

AudioRendererInfoNapi::~AudioRendererInfoNapi() = default;

void AudioRendererInfoNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<AudioRendererInfoNapi *>(nativeObject);
        delete obj;
        obj = nullptr;
    }
}

napi_value AudioRendererInfoNapi::Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    int32_t refCount = 1;

    napi_property_descriptor audio_renderer_info_properties[] = {
        DECLARE_NAPI_GETTER_SETTER("content", GetContentType, SetContentType),
        DECLARE_NAPI_GETTER_SETTER("usage", GetStreamUsage, SetStreamUsage),
        DECLARE_NAPI_GETTER_SETTER("rendererFlags", GetRendererFlags, SetRendererFlags)
    };

    status = napi_define_class(env, AUDIO_RENDERER_INFO_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct,
        nullptr, sizeof(audio_renderer_info_properties) / sizeof(audio_renderer_info_properties[0]),
        audio_renderer_info_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }

    status = napi_create_reference(env, constructor, refCount, &sConstructor_);
    if (status == napi_ok) {
        status = napi_set_named_property(env, exports, AUDIO_RENDERER_INFO_NAPI_CLASS_NAME.c_str(),
                                         constructor);
        if (status == napi_ok) {
            return exports;
        }
    }
    HiLog::Error(LABEL, "Failure in AudioRendererInfoNapi::Init()");

    return result;
}

napi_value AudioRendererInfoNapi::Construct(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value jsThis = nullptr;
    size_t argCount = 0;

    status = napi_get_cb_info(env, info, &argCount, nullptr, &jsThis, nullptr);
    if (status == napi_ok) {
        unique_ptr<AudioRendererInfoNapi> obj = make_unique<AudioRendererInfoNapi>();
        if (obj != nullptr) {
            obj->env_ = env;
            obj->audioRendererInfo_ = move(sAudioRendererInfo_);
            status = napi_wrap(env, jsThis, static_cast<void *>(obj.get()),
                               AudioRendererInfoNapi::Destructor, nullptr, nullptr);
            if (status == napi_ok) {
                obj.release();
                return jsThis;
            }
        }
    }
    HiLog::Error(LABEL, "Failed in AudioRendererInfoNapi::Construct()!");
    napi_get_undefined(env, &jsThis);

    return jsThis;
}

napi_value AudioRendererInfoNapi::CreateAudioRendererInfoWrapper(napi_env env,
    unique_ptr<AudioRendererInfo> &audioRendererInfo)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value constructor;

    status = napi_get_reference_value(env, sConstructor_, &constructor);
    if (status == napi_ok) {
        sAudioRendererInfo_ = move(audioRendererInfo);
        status = napi_new_instance(env, constructor, 0, nullptr, &result);
        sAudioRendererInfo_.release();
        if (status == napi_ok) {
            return result;
        }
    }
    HiLog::Error(LABEL, "Failed in CreateAudioRendererInfoWrapper, %{public}d", status);

    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioRendererInfoNapi::GetContentType(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioRendererInfoNapi *audioRendererInfoNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    ContentType contentType;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get content type fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioRendererInfoNapi);
    if (status == napi_ok) {
        contentType = audioRendererInfoNapi->audioRendererInfo_->contentType;
        status = napi_create_int32(env, contentType, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioRendererInfoNapi::SetContentType(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioRendererInfoNapi *audioRendererInfoNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    int32_t contentType;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr || args[0] == nullptr) {
        HiLog::Error(LABEL, "set content type fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioRendererInfoNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "set content type fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_int32(env, args[0], &contentType);
    if (status == napi_ok) {
        audioRendererInfoNapi->audioRendererInfo_->contentType
            = static_cast<ContentType>(contentType);
    }

    return jsResult;
}

napi_value AudioRendererInfoNapi::GetStreamUsage(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioRendererInfoNapi *audioRendererInfoNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    StreamUsage usage;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get stream usage fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioRendererInfoNapi);
    if (status == napi_ok) {
        usage = audioRendererInfoNapi->audioRendererInfo_->streamUsage;
        status = napi_create_int32(env, usage, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioRendererInfoNapi::SetStreamUsage(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioRendererInfoNapi *audioRendererInfoNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    int32_t usage;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr || args[0] == nullptr) {
        HiLog::Error(LABEL, "set stream usage fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioRendererInfoNapi);
    if (status != napi_ok) {
        HiLog::Error(LABEL, "Napi unwrap failed");
        return jsResult;
    }

    napi_valuetype valueType = napi_undefined;
    if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
        HiLog::Error(LABEL, "set stream usage fail: wrong data type");
        return jsResult;
    }

    status = napi_get_value_int32(env, args[0], &usage);
    if (status == napi_ok) {
        audioRendererInfoNapi->audioRendererInfo_->streamUsage = static_cast<StreamUsage>(usage);
    }

    return jsResult;
}

napi_value AudioRendererInfoNapi::GetRendererFlags(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioRendererInfoNapi *audioRendererInfoNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get renderer flag fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioRendererInfoNapi);
    if (status == napi_ok) {
        status = napi_create_int32(env, audioRendererInfoNapi->audioRendererInfo_->rendererFlags,
                                   &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioRendererInfoNapi::SetRendererFlags(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioRendererInfoNapi *audioRendererInfoNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    int32_t rendererFlags;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr || args[0] == nullptr) {
        HiLog::Error(LABEL, "get renderer flag fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioRendererInfoNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "get renderer flag fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_int32(env, args[0], &rendererFlags);
    if (status == napi_ok) {
        audioRendererInfoNapi->audioRendererInfo_->rendererFlags = rendererFlags;
    }

    return jsResult;
}
}  // namespace AudioStandard
}  // namespace OHOS
