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

#include "audio_parameters_napi.h"
#include "hilog/log.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
napi_ref AudioParametersNapi::sConstructor_ = nullptr;
unique_ptr<AudioParameters> AudioParametersNapi::sAudioParameters_ = nullptr;

napi_ref AudioParametersNapi::audioChannel_ = nullptr;
napi_ref AudioParametersNapi::samplingRate_ = nullptr;
napi_ref AudioParametersNapi::encodingType_ = nullptr;
napi_ref AudioParametersNapi::contentType_ = nullptr;
napi_ref AudioParametersNapi::streamUsage_ = nullptr;
napi_ref AudioParametersNapi::deviceRole_ = nullptr;
napi_ref AudioParametersNapi::deviceType_ = nullptr;
napi_ref AudioParametersNapi::sourceType_ = nullptr;

namespace {
    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "AudioParametersNapi"};
}

AudioParametersNapi::AudioParametersNapi()
    : env_(nullptr) {
}

AudioParametersNapi::~AudioParametersNapi()
{
    audioParameters_ = nullptr;
}

void AudioParametersNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<AudioParametersNapi *>(nativeObject);
        delete obj;
    }
}

napi_status AudioParametersNapi::AddNamedProperty(napi_env env, napi_value object,
                                                  const std::string name, int32_t enumValue)
{
    napi_status status;
    napi_value enumNapiValue;

    status = napi_create_int32(env, enumValue, &enumNapiValue);
    if (status == napi_ok) {
        status = napi_set_named_property(env, object, name.c_str(), enumNapiValue);
    }

    return status;
}

napi_value AudioParametersNapi::CreateAudioChannelObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: audioChannelMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &audioChannel_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateAudioChannelObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioParametersNapi::CreateSamplingRateObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: samplingRateMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &samplingRate_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateSamplingRateObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioParametersNapi::CreateEncodingTypeObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: encodingTypeMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &encodingType_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateEncodingTypeObject is failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioParametersNapi::CreateContentTypeObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: contentTypeMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &contentType_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateContentTypeObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioParametersNapi::CreateStreamUsageObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: streamUsageMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &streamUsage_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateStreamUsageObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioParametersNapi::CreateDeviceRoleObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: deviceRoleMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &deviceRole_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateDeviceRoleObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioParametersNapi::CreateDeviceTypeObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: deviceTypeMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop!");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &deviceType_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateDeviceRoleObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioParametersNapi::CreateSourceTypeObject(napi_env env)
{
    napi_value result = nullptr;
    napi_status status;
    std::string propName;

    status = napi_create_object(env, &result);
    if (status == napi_ok) {
        for (auto &iter: sourceTypeMap) {
            propName = iter.first;
            status = AddNamedProperty(env, result, propName, iter.second);
            if (status != napi_ok) {
                HiLog::Error(LABEL, "Failed to add named prop! in CreateSourceTypeObject");
                break;
            }
            propName.clear();
        }
        if (status == napi_ok) {
            status = napi_create_reference(env, result, REFERENCE_CREATION_COUNT, &sourceType_);
            if (status == napi_ok) {
                return result;
            }
        }
    }
    HiLog::Error(LABEL, "CreateSourceTypeObject is Failed!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioParametersNapi::Init(napi_env env, napi_value exports)
{
    HiLog::Info(LABEL, "AudioParametersNapi::Init()");
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    napi_property_descriptor audio_parameters_properties[] = {
        DECLARE_NAPI_GETTER_SETTER("format", GetAudioSampleFormat, SetAudioSampleFormat),
        DECLARE_NAPI_GETTER_SETTER("channels", GetAudioChannel, SetAudioChannel),
        DECLARE_NAPI_GETTER_SETTER("samplingRate", GetAudioSamplingRate, SetAudioSamplingRate),
        DECLARE_NAPI_GETTER_SETTER("encoding", GetAudioEncodingType, SetAudioEncodingType),
        DECLARE_NAPI_GETTER_SETTER("contentType", GetContentType, SetContentType),
        DECLARE_NAPI_GETTER_SETTER("usage", GetStreamUsage, SetStreamUsage),
        DECLARE_NAPI_GETTER_SETTER("deviceRole", GetDeviceRole, SetDeviceRole),
        DECLARE_NAPI_GETTER_SETTER("deviceType", GetDeviceType, SetDeviceType)
    };

    napi_property_descriptor static_prop[] = {
        DECLARE_NAPI_PROPERTY("AudioChannel", CreateAudioChannelObject(env)),
        DECLARE_NAPI_PROPERTY("AudioSamplingRate", CreateSamplingRateObject(env)),
        DECLARE_NAPI_PROPERTY("AudioEncodingType", CreateEncodingTypeObject(env)),
        DECLARE_NAPI_PROPERTY("ContentType", CreateContentTypeObject(env)),
        DECLARE_NAPI_PROPERTY("StreamUsage", CreateStreamUsageObject(env)),
        DECLARE_NAPI_PROPERTY("DeviceRole", CreateDeviceRoleObject(env)),
        DECLARE_NAPI_PROPERTY("DeviceType", CreateDeviceTypeObject(env)),
        DECLARE_NAPI_PROPERTY("SourceType", CreateSourceTypeObject(env))
    };

    status = napi_define_class(env, AUDIO_PARAMETERS_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct,
                               nullptr, sizeof(audio_parameters_properties) / sizeof(audio_parameters_properties[0]),
                               audio_parameters_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }

    status = napi_create_reference(env, constructor, REFERENCE_CREATION_COUNT, &sConstructor_);
    if (status == napi_ok) {
        status = napi_set_named_property(env, exports, AUDIO_PARAMETERS_NAPI_CLASS_NAME.c_str(),
                                         constructor);
        if (status == napi_ok) {
            status = napi_define_properties(env, exports,
                                            sizeof(static_prop) / sizeof(static_prop[0]), static_prop);
            if (status == napi_ok) {
                return exports;
            }
        }
    }
    HiLog::Error(LABEL, "Failure in AudioParametersNapi::Init()");

    return result;
}

napi_value AudioParametersNapi::Construct(napi_env env, napi_callback_info info)
{
    napi_status status;
    napi_value jsThis = nullptr;
    size_t argCount = 0;

    status = napi_get_cb_info(env, info, &argCount, nullptr, &jsThis, nullptr);
    if (status == napi_ok) {
        unique_ptr<AudioParametersNapi> obj = make_unique<AudioParametersNapi>();
        if (obj != nullptr) {
            obj->env_ = env;
            obj->audioParameters_ = move(sAudioParameters_);
            status = napi_wrap(env, jsThis, static_cast<void*>(obj.get()),
                               AudioParametersNapi::Destructor, nullptr, nullptr);
            if (status == napi_ok) {
                obj.release();
                return jsThis;
            }
        }
    }
    HiLog::Error(LABEL, "Failed in AudioParametersNapi::Construct()!");
    napi_get_undefined(env, &jsThis);

    return jsThis;
}

napi_value AudioParametersNapi::GetAudioSampleFormat(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    AudioSampleFormat audioSampleFormat;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get audio sample format fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        if (!((audioParametersNapi != nullptr) && (audioParametersNapi->audioParameters_ != nullptr))) {
            HiLog::Error(LABEL, "Get audio sample format fail to napi_unwrap");
            return jsResult;
        }
        audioSampleFormat = audioParametersNapi->audioParameters_->format;
        status = napi_create_int32(env, audioSampleFormat, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioParametersNapi::SetAudioSampleFormat(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    int32_t audioSampleFormat;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr || args[0] == nullptr) {
        HiLog::Error(LABEL, "set sample format fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "set sample format fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_int32(env, args[0], &audioSampleFormat);
    if (status == napi_ok) {
        audioParametersNapi->audioParameters_->format = static_cast<AudioSampleFormat>(audioSampleFormat);
    }

    return jsResult;
}

napi_value AudioParametersNapi::GetAudioChannel(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    AudioChannel audioChannel;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get audio channels fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        audioChannel = audioParametersNapi->audioParameters_->channels;
        status = napi_create_int32(env, audioChannel, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioParametersNapi::SetAudioChannel(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    int32_t audioChannel;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr || args[0] == nullptr) {
        HiLog::Error(LABEL, "set audio channel fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "set audio channel fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_int32(env, args[0], &audioChannel);
    if (status == napi_ok) {
        audioParametersNapi->audioParameters_->channels = static_cast<AudioChannel>(audioChannel);
    }

    return jsResult;
}

napi_value AudioParametersNapi::GetAudioSamplingRate(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    AudioSamplingRate samplingRate;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get sampling rate fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        samplingRate = audioParametersNapi->audioParameters_->samplingRate;
        status = napi_create_int32(env, samplingRate, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioParametersNapi::SetAudioSamplingRate(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    int32_t samplingRate;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr || args[0] == nullptr) {
        HiLog::Error(LABEL, "set sampling rate fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "set sampling rate fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_int32(env, args[0], &samplingRate);
    if (status == napi_ok) {
        audioParametersNapi->audioParameters_->samplingRate = static_cast<AudioSamplingRate>(samplingRate);
    }

    return jsResult;
}

napi_value AudioParametersNapi::GetAudioEncodingType(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    AudioEncodingType encodingType;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get encoding type fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        encodingType = audioParametersNapi->audioParameters_->encoding;
        status = napi_create_int32(env, encodingType, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioParametersNapi::SetAudioEncodingType(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    int32_t encodingType;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr || args[0] == nullptr) {
        HiLog::Error(LABEL, "set audio encoding type fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "set audio encoding type fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_int32(env, args[0], &encodingType);
    if (status == napi_ok) {
        audioParametersNapi->audioParameters_->encoding = static_cast<AudioEncodingType>(encodingType);
    }

    return jsResult;
}

napi_value AudioParametersNapi::GetContentType(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
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

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        contentType = audioParametersNapi->audioParameters_->contentType;
        status = napi_create_int32(env, contentType, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioParametersNapi::SetContentType(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
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

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "set content type fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_int32(env, args[0], &contentType);
    if (status == napi_ok) {
        audioParametersNapi->audioParameters_->contentType = static_cast<ContentType>(contentType);
    }

    return jsResult;
}

napi_value AudioParametersNapi::GetStreamUsage(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
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

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        usage = audioParametersNapi->audioParameters_->usage;
        status = napi_create_int32(env, usage, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioParametersNapi::SetStreamUsage(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
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

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "set stream usage fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_int32(env, args[0], &usage);
    if (status == napi_ok) {
        audioParametersNapi->audioParameters_->usage = static_cast<StreamUsage>(usage);
    }

    return jsResult;
}

napi_value AudioParametersNapi::GetDeviceRole(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    DeviceRole deviceRole;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get device role fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        deviceRole = audioParametersNapi->audioParameters_->deviceRole;
        status = napi_create_int32(env, deviceRole, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioParametersNapi::SetDeviceRole(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    int32_t deviceRole;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr || args[0] == nullptr) {
        HiLog::Error(LABEL, "set device role fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "set device role fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_int32(env, args[0], &deviceRole);
    if (status == napi_ok) {
        audioParametersNapi->audioParameters_->deviceRole = static_cast<DeviceRole>(deviceRole);
    }

    return jsResult;
}

napi_value AudioParametersNapi::GetDeviceType(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 0;
    napi_value jsThis = nullptr;
    DeviceType deviceType;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, nullptr, &jsThis, nullptr);
    if (status != napi_ok || jsThis == nullptr) {
        HiLog::Error(LABEL, "Get device type fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        deviceType = audioParametersNapi->audioParameters_->deviceType;
        HiLog::Info(LABEL, "get device type: %d", deviceType);
        status = napi_create_int32(env, deviceType, &jsResult);
        if (status == napi_ok) {
            return jsResult;
        }
    }

    return jsResult;
}

napi_value AudioParametersNapi::SetDeviceType(napi_env env, napi_callback_info info)
{
    napi_status status;
    AudioParametersNapi *audioParametersNapi = nullptr;
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_value jsThis = nullptr;
    int32_t deviceType;
    napi_value jsResult = nullptr;
    napi_get_undefined(env, &jsResult);

    status = napi_get_cb_info(env, info, &argc, args, &jsThis, nullptr);
    if ((status != napi_ok) || (jsThis == nullptr) || (args[0] == nullptr)) {
        HiLog::Error(LABEL, "set device type fail to napi_get_cb_info");
        return jsResult;
    }

    status = napi_unwrap(env, jsThis, (void **)&audioParametersNapi);
    if (status == napi_ok) {
        napi_valuetype valueType = napi_undefined;
        if (napi_typeof(env, args[0], &valueType) != napi_ok || valueType != napi_number) {
            HiLog::Error(LABEL, "set device type fail: wrong data type");
            return jsResult;
        }
    }

    status = napi_get_value_int32(env, args[0], &deviceType);
    if (status == napi_ok) {
        audioParametersNapi->audioParameters_->deviceType = static_cast<DeviceType>(deviceType);
        HiLog::Info(LABEL, "set device type: %d", audioParametersNapi->audioParameters_->deviceType);
    }

    return jsResult;
}
}  // namespace AudioStandard
}  // namespace OHOS
