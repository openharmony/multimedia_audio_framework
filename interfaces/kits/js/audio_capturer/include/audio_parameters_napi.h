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

#ifndef AUDIO_PARAMETERS_NAPI_H_
#define AUDIO_PARAMETERS_NAPI_H_

#include <iostream>
#include <map>

#include "audio_info.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
static const std::string AUDIO_PARAMETERS_NAPI_CLASS_NAME = "AudioParameters";

static const std::int32_t REFERENCE_CREATION_COUNT = 1;

static const std::map<std::string, AudioChannel> audioChannelMap = {
    {"CHANNEL_1", MONO},
    {"CHANNEL_2", STEREO}
};

static const std::map<std::string, AudioSamplingRate> samplingRateMap = {
    {"SAMPLE_RATE_8000", SAMPLE_RATE_8000},
    {"SAMPLE_RATE_11025", SAMPLE_RATE_11025},
    {"SAMPLE_RATE_12000", SAMPLE_RATE_12000},
    {"SAMPLE_RATE_16000", SAMPLE_RATE_16000},
    {"SAMPLE_RATE_22050", SAMPLE_RATE_22050},
    {"SAMPLE_RATE_24000", SAMPLE_RATE_24000},
    {"SAMPLE_RATE_32000", SAMPLE_RATE_32000},
    {"SAMPLE_RATE_44100", SAMPLE_RATE_44100},
    {"SAMPLE_RATE_48000", SAMPLE_RATE_48000},
    {"SAMPLE_RATE_64000", SAMPLE_RATE_64000},
    {"SAMPLE_RATE_96000", SAMPLE_RATE_96000}
};

static const std::map<std::string, AudioEncodingType> encodingTypeMap = {
    {"ENCODING_TYPE_INVALID", ENCODING_INVALID},
    {"ENCODING_TYPE_RAW", ENCODING_PCM}
};

static const std::map<std::string, ContentType> contentTypeMap = {
    {"CONTENT_TYPE_UNKNOWN", CONTENT_TYPE_UNKNOWN},
    {"CONTENT_TYPE_SPEECH", CONTENT_TYPE_SPEECH},
    {"CONTENT_TYPE_MUSIC", CONTENT_TYPE_MUSIC},
    {"CONTENT_TYPE_MOVIE", CONTENT_TYPE_MOVIE},
    {"CONTENT_TYPE_SONIFICATION", CONTENT_TYPE_SONIFICATION},
    {"CONTENT_TYPE_RINGTONE", CONTENT_TYPE_RINGTONE}
};

static const std::map<std::string, StreamUsage> streamUsageMap = {
    {"STREAM_USAGE_UNKNOWN", STREAM_USAGE_UNKNOWN},
    {"STREAM_USAGE_MEDIA", STREAM_USAGE_MEDIA},
    {"STREAM_USAGE_VOICE_ASSISTANT", STREAM_USAGE_VOICE_ASSISTANT},
    {"STREAM_USAGE_VOICE_COMMUNICATION", STREAM_USAGE_VOICE_COMMUNICATION},
    {"STREAM_USAGE_NOTIFICATION_RINGTONE", STREAM_USAGE_NOTIFICATION_RINGTONE}
};

static const std::map<std::string, DeviceRole> deviceRoleMap = {
    {"DEVICE_ROLE_NONE", DEVICE_ROLE_NONE},
    {"INPUT_DEVICE", INPUT_DEVICE},
    {"OUTPUT_DEVICE", OUTPUT_DEVICE},
    {"DEVICE_ROLE_MAX", DEVICE_ROLE_MAX}
};

static const std::map<std::string, DeviceType> deviceTypeMap = {
    {"NONE", DEVICE_TYPE_NONE},
    {"INVALID", DEVICE_TYPE_INVALID},
    {"EARPIECE", DEVICE_TYPE_EARPIECE},
    {"SPEAKER", DEVICE_TYPE_SPEAKER},
    {"WIRED_HEADSET", DEVICE_TYPE_WIRED_HEADSET},
    {"WIRED_HEADPHONES", DEVICE_TYPE_WIRED_HEADPHONES},
    {"BLUETOOTH_SCO", DEVICE_TYPE_BLUETOOTH_SCO},
    {"BLUETOOTH_A2DP", DEVICE_TYPE_BLUETOOTH_A2DP},
    {"MIC", DEVICE_TYPE_MIC},
    {"USB_HEADSET", DEVICE_TYPE_USB_HEADSET},
    {"DEFAULT", DEVICE_TYPE_DEFAULT},
    {"MAX", DEVICE_TYPE_MAX},
};

static const std::map<std::string, SourceType> sourceTypeMap = {
    {"SOURCE_TYPE_INVALID", SOURCE_TYPE_INVALID},
    {"SOURCE_TYPE_MIC", SOURCE_TYPE_MIC},
    {"SOURCE_TYPE_VOICE_RECOGNITION", SOURCE_TYPE_VOICE_RECOGNITION},
    {"SOURCE_TYPE_VOICE_COMMUNICATION", SOURCE_TYPE_VOICE_COMMUNICATION}
};

class AudioParametersNapi {
public:
    AudioParametersNapi();
    ~AudioParametersNapi();

    static napi_value Init(napi_env env, napi_value exports);
    static napi_value CreateAudioParametersWrapper(napi_env env, std::unique_ptr<AudioParameters> &audioParameters);

private:
    static void Destructor(napi_env env, void *nativeObject, void *finalize_hint);
    static napi_value Construct(napi_env env, napi_callback_info info);
    static napi_value GetAudioSampleFormat(napi_env env, napi_callback_info info);
    static napi_value SetAudioSampleFormat(napi_env env, napi_callback_info info);
    static napi_value GetAudioChannel(napi_env env, napi_callback_info info);
    static napi_value SetAudioChannel(napi_env env, napi_callback_info info);
    static napi_value GetAudioSamplingRate(napi_env env, napi_callback_info info);
    static napi_value SetAudioSamplingRate(napi_env env, napi_callback_info info);
    static napi_value GetAudioEncodingType(napi_env env, napi_callback_info info);
    static napi_value SetAudioEncodingType(napi_env env, napi_callback_info info);
    static napi_value GetContentType(napi_env env, napi_callback_info info);
    static napi_value SetContentType(napi_env env, napi_callback_info info);
    static napi_value GetStreamUsage(napi_env env, napi_callback_info info);
    static napi_value SetStreamUsage(napi_env env, napi_callback_info info);
    static napi_value GetDeviceRole(napi_env env, napi_callback_info info);
    static napi_value SetDeviceRole(napi_env env, napi_callback_info info);
    static napi_value GetDeviceType(napi_env env, napi_callback_info info);
    static napi_value SetDeviceType(napi_env env, napi_callback_info info);

    static napi_status AddNamedProperty(napi_env env, napi_value object, const std::string name, int32_t enumValue);
    static napi_value CreateAudioSampleFormatObject(napi_env env);
    static napi_value CreateAudioChannelObject(napi_env env);
    static napi_value CreateSamplingRateObject(napi_env env);
    static napi_value CreateEncodingTypeObject(napi_env env);
    static napi_value CreateContentTypeObject(napi_env env);
    static napi_value CreateStreamUsageObject(napi_env env);
    static napi_value CreateDeviceRoleObject(napi_env env);
    static napi_value CreateDeviceTypeObject(napi_env env);
    static napi_value CreateSourceTypeObject(napi_env env);

    static napi_ref sConstructor_;
    static napi_ref audioChannel_;
    static napi_ref samplingRate_;
    static napi_ref encodingType_;
    static napi_ref contentType_;
    static napi_ref streamUsage_;
    static napi_ref deviceRole_;
    static napi_ref deviceType_;
    static napi_ref sourceType_;

    static std::unique_ptr<AudioParameters> sAudioParameters_;

    std::unique_ptr<AudioParameters> audioParameters_;
    napi_env env_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* AUDIO_PARAMETERS_NAPI_H_ */
