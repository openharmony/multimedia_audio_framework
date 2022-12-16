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

#include "ringtone_options_napi.h"
#include "hilog/log.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
napi_ref RingtoneOptionsNapi::sConstructor_ = nullptr;

float RingtoneOptionsNapi::sVolume_ = 1;
bool RingtoneOptionsNapi::sLoop_ = true;

namespace {
    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "RingtoneOptionsNapi"};
}

RingtoneOptionsNapi::RingtoneOptionsNapi()
    : env_(nullptr) {
}

RingtoneOptionsNapi::~RingtoneOptionsNapi() = default;

void RingtoneOptionsNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<RingtoneOptionsNapi *>(nativeObject);
        delete obj;
        obj = nullptr;
    }
}

napi_value RingtoneOptionsNapi::Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    napi_property_descriptor ringtone_options_properties[] = {
        DECLARE_NAPI_GETTER_SETTER("volume", GetVolume, SetVolume),
        DECLARE_NAPI_GETTER_SETTER("loop", GetLoop, SetLoop)
    };

    status = napi_define_class(env, RINGTONE_OPTIONS_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct,
                               nullptr, sizeof(ringtone_options_properties) / sizeof(ringtone_options_properties[0]),
                               ringtone_options_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }

    status = napi_create_reference(env, constructor, 1, &sConstructor_);
    if (status == napi_ok) {
        status = napi_set_named_property(env, exports, RINGTONE_OPTIONS_NAPI_CLASS_NAME.c_str(),
                                         constructor);
        if (status == napi_ok) {
            return exports;
        }
    }
    HiLog::Error(LABEL, "Failure in RingtoneOptionsNapi::Init()");

    return result;
}

napi_value RingtoneOptionsNapi::Construct(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value jsThis = nullptr;
    size_t argCount = 0;

    status = napi_get_cb_info(env, info, &argCount, nullptr, &jsThis, nullptr);
    if (status == napi_ok) {
        unique_ptr<RingtoneOptionsNapi> obj = make_unique<RingtoneOptionsNapi>();
        if (obj != nullptr) {
            obj->env_ = env;
            obj->volume_ = sVolume_;
            obj->loop_ = sLoop_;
            status = napi_wrap(env, jsThis, static_cast<void*>(obj.get()),
                               RingtoneOptionsNapi::Destructor, nullptr, nullptr);
            if (status == napi_ok) {
                obj.release();
                return jsThis;
            }
        }
    }

    HiLog::Error(LABEL, "Failed in RingtoneOptionsNapi::Construct()!");
    napi_get_undefined(env, &jsThis);

    return jsThis;
}

napi_value RingtoneOptionsNapi::CreateRingtoneOptionsWrapper(napi_env env, float volume, bool loop)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value constructor;

    status = napi_get_reference_value(env, sConstructor_, &constructor);
    if (status == napi_ok) {
        sVolume_ = volume;
        sLoop_ = loop;
        status = napi_new_instance(env, constructor, 0, nullptr, &result);
        if (status == napi_ok) {
            return result;
        }
    }
    HiLog::Error(LABEL, "Failed in CreateRingtoneOptionsWrapper, %{public}d", status);

    napi_get_undefined(env, &result);

    return result;
}

napi_value RingtoneOptionsNapi::GetVolume(napi_env env, napi_callback_info info)
{
    napi_status status;
    RingtoneOptionsNapi *ringtoneOptionsNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get volume fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&ringtoneOptionsNapi);
    if (status == napi_ok) {
        status = napi_create_double(env, ringtoneOptionsNapi->volume_, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value RingtoneOptionsNapi::SetVolume(napi_env env, napi_callback_info info)
{
    napi_status status;
    RingtoneOptionsNapi *ringtoneOptionsNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    double volume;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr || args[0] == nullptr) {
        HiLog::Error(LABEL, "set volume fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&ringtoneOptionsNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "set volume fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_double(env, args[0], &volume);
    if (status == napi_ok) {
        ringtoneOptionsNapi->volume_ = (float)volume;
    }

    return jsResult;
}

napi_value RingtoneOptionsNapi::GetLoop(napi_env env, napi_callback_info info)
{
    napi_status status;
    RingtoneOptionsNapi *ringtoneOptionsNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get volume fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&ringtoneOptionsNapi);
    if (status == napi_ok) {
        status = napi_get_boolean(env, ringtoneOptionsNapi->loop_, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value RingtoneOptionsNapi::SetLoop(napi_env env, napi_callback_info info)
{
    napi_status status;
    RingtoneOptionsNapi *ringtoneOptionsNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    bool loop = false;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr || args[0] == nullptr) {
        HiLog::Error(LABEL, "set loop fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&ringtoneOptionsNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_boolean) {
            HiLog::Error(LABEL, "set volume fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_bool(env, args[0], &loop);
    if (status == napi_ok) {
        ringtoneOptionsNapi->loop_ = loop;
    }

    return jsResult;
}
}  // namespace AudioStandard
}  // namespace OHOS
