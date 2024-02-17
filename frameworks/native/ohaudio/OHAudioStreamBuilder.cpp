/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <memory>
#include "native_audiostreambuilder.h"
#include "OHAudioStreamBuilder.h"
#include "OHAudioRenderer.h"
#include "OHAudioCapturer.h"

using OHOS::AudioStandard::OHAudioStreamBuilder;
using OHOS::AudioStandard::AudioSampleFormat;
using OHOS::AudioStandard::StreamUsage;
using OHOS::AudioStandard::AudioEncodingType;
using OHOS::AudioStandard::ContentType;
using OHOS::AudioStandard::SourceType;

static const int32_t RENDERER_TYPE = 1;
static const int32_t CAPTURER_TYPE = 2;
constexpr int32_t UNDEFINED_SIZE = -1;

static OHOS::AudioStandard::OHAudioStreamBuilder *convertBuilder(OH_AudioStreamBuilder* builder)
{
    return (OHOS::AudioStandard::OHAudioStreamBuilder*) builder;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetSamplingRate(OH_AudioStreamBuilder* builder, int32_t rate)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    return audioStreamBuilder->SetSamplingRate(rate);
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetChannelCount(OH_AudioStreamBuilder* builder, int32_t channelCount)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    return audioStreamBuilder->SetChannelCount(channelCount);
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetSampleFormat(OH_AudioStreamBuilder* builder,
    OH_AudioStream_SampleFormat format)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    AudioSampleFormat sampleFormat = (AudioSampleFormat)format;
    return audioStreamBuilder->SetSampleFormat(sampleFormat);
}


OH_AudioStream_Result OH_AudioStreamBuilder_SetFrameSizeInCallback(OH_AudioStreamBuilder* builder,
    int32_t frameSize)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    return audioStreamBuilder->SetPreferredFrameSize(frameSize);
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetEncodingType(OH_AudioStreamBuilder* builder,
    OH_AudioStream_EncodingType encodingType)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    AudioEncodingType type = (AudioEncodingType)encodingType;
    return audioStreamBuilder->SetEncodingType(type);
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetLatencyMode(OH_AudioStreamBuilder* builder,
    OH_AudioStream_LatencyMode latencyMode)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    int32_t innerLatencyMode = (int32_t)latencyMode;
    return audioStreamBuilder->SetLatencyMode(innerLatencyMode);
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererInfo(OH_AudioStreamBuilder* builder,
    OH_AudioStream_Usage usage)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    StreamUsage streamUsage = (StreamUsage)usage;
    ContentType contentType = ContentType::CONTENT_TYPE_UNKNOWN;
    return audioStreamBuilder->SetRendererInfo(streamUsage, contentType);
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererCallback(OH_AudioStreamBuilder* builder,
    OH_AudioRenderer_Callbacks callbacks, void* userData)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    return audioStreamBuilder->SetRendererCallback(callbacks, userData);
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetCapturerInfo(OH_AudioStreamBuilder* builder,
    OH_AudioStream_SourceType sourceType)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    SourceType type = (SourceType)sourceType;
    return audioStreamBuilder->SetSourceType(type);
}


OH_AudioStream_Result OH_AudioStreamBuilder_Create(OH_AudioStreamBuilder** builder, OH_AudioStream_Type type)
{
    int32_t streamType = type == AUDIOSTREAM_TYPE_RENDERER ? RENDERER_TYPE : CAPTURER_TYPE;
    OHAudioStreamBuilder *streamBuilder = new OHAudioStreamBuilder(streamType);

    *builder = (OH_AudioStreamBuilder*)streamBuilder;

    return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetCapturerCallback(OH_AudioStreamBuilder* builder,
    OH_AudioCapturer_Callbacks callbacks, void* userData)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");

    return audioStreamBuilder->SetCapturerCallback(callbacks, userData);
}

OH_AudioStream_Result OH_AudioStreamBuilder_SetRendererOutputDeviceChangeCallback(OH_AudioStreamBuilder* builder,
    OH_AudioRenderer_OutputDeviceChangeCallback callback, void* userData)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");

    return audioStreamBuilder->SetRendererOutputDeviceChangeCallback(callback, userData);
}

OH_AudioStream_Result OH_AudioStreamBuilder_GenerateRenderer(OH_AudioStreamBuilder* builder,
    OH_AudioRenderer** audioRenderer)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    return audioStreamBuilder->Generate(audioRenderer);
}

OH_AudioStream_Result OH_AudioStreamBuilder_GenerateCapturer(OH_AudioStreamBuilder* builder,
    OH_AudioCapturer** audioCapturer)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    return audioStreamBuilder->Generate(audioCapturer);
}

OH_AudioStream_Result OH_AudioStreamBuilder_Destroy(OH_AudioStreamBuilder* builder)
{
    OHAudioStreamBuilder *audioStreamBuilder = convertBuilder(builder);
    CHECK_AND_RETURN_RET_LOG(audioStreamBuilder != nullptr, AUDIOSTREAM_ERROR_INVALID_PARAM, "convert builder failed");
    if (audioStreamBuilder != nullptr) {
        delete audioStreamBuilder;
        audioStreamBuilder = nullptr;
        return AUDIOSTREAM_SUCCESS;
    }
    return AUDIOSTREAM_ERROR_ILLEGAL_STATE;
}

namespace OHOS {
namespace AudioStandard {

OHAudioStreamBuilder::OHAudioStreamBuilder(const int32_t type) : streamType_(RENDERER_TYPE)
{
    AUDIO_INFO_LOG("OHAudioStreamBuilder created, type is %{public}d", type);
    streamType_ = type;
}

OHAudioStreamBuilder::~OHAudioStreamBuilder()
{
    AUDIO_INFO_LOG("OHAudioStreamBuilder destroyed, type is %{public}d", streamType_);
}

OH_AudioStream_Result OHAudioStreamBuilder::SetSamplingRate(int32_t rate)
{
    switch (rate) {
        case AudioSamplingRate::SAMPLE_RATE_8000:
        case AudioSamplingRate::SAMPLE_RATE_11025:
        case AudioSamplingRate::SAMPLE_RATE_12000:
        case AudioSamplingRate::SAMPLE_RATE_16000:
        case AudioSamplingRate::SAMPLE_RATE_22050:
        case AudioSamplingRate::SAMPLE_RATE_24000:
        case AudioSamplingRate::SAMPLE_RATE_32000:
        case AudioSamplingRate::SAMPLE_RATE_44100:
        case AudioSamplingRate::SAMPLE_RATE_48000:
        case AudioSamplingRate::SAMPLE_RATE_64000:
        case AudioSamplingRate::SAMPLE_RATE_96000:
            AUDIO_DEBUG_LOG("sampleFormat input value is valid");
            break;
        default:
            AUDIO_ERR_LOG("sampleFormat input value is invalid");
            return AUDIOSTREAM_ERROR_INVALID_PARAM;
    }
    samplingRate_ =rate;
    return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OHAudioStreamBuilder::SetChannelCount(int32_t channelCount)
{
    switch (channelCount) {
        case AudioChannel::MONO:
        case AudioChannel::STEREO:
        case AudioChannel::CHANNEL_3:
        case AudioChannel::CHANNEL_4:
        case AudioChannel::CHANNEL_5:
        case AudioChannel::CHANNEL_6:
        case AudioChannel::CHANNEL_7:
        case AudioChannel::CHANNEL_8:
            AUDIO_DEBUG_LOG("channelCount input value is valid");
            break;
        default:
            AUDIO_ERR_LOG("channelCount input value is invalid");
            return AUDIOSTREAM_ERROR_INVALID_PARAM;
    }
    channelCount_ = channelCount;
    return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OHAudioStreamBuilder::SetSampleFormat(AudioSampleFormat sampleFormat)
{
    sampleFormat_ = sampleFormat;
    return AUDIOSTREAM_SUCCESS;
}


OH_AudioStream_Result OHAudioStreamBuilder::SetPreferredFrameSize(int32_t frameSize)
{
    preferredFrameSize_ = frameSize;
    return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OHAudioStreamBuilder::SetRendererInfo(StreamUsage usage, ContentType contentType)
{
    CHECK_AND_RETURN_RET_LOG(streamType_ != CAPTURER_TYPE && usage != StreamUsage::STREAM_USAGE_UNKNOWN,
        AUDIOSTREAM_ERROR_INVALID_PARAM, "Error, invalid type input");
    usage_ = usage;
    contentType_ = contentType;
    return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OHAudioStreamBuilder::SetEncodingType(AudioEncodingType encodingType)
{
    encodingType_ = encodingType;
    return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OHAudioStreamBuilder::SetSourceType(SourceType type)
{
    CHECK_AND_RETURN_RET_LOG(streamType_ != RENDERER_TYPE && type != SOURCE_TYPE_INVALID,
        AUDIOSTREAM_ERROR_INVALID_PARAM, "Error, invalid type input");

    sourceType_ = type;
    return AUDIOSTREAM_SUCCESS;
}


OH_AudioStream_Result OHAudioStreamBuilder::SetLatencyMode(int32_t latencyMode)
{
    latencyMode_ = latencyMode;
    return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OHAudioStreamBuilder::Generate(OH_AudioRenderer** renderer)
{
    AUDIO_INFO_LOG("Generate OHAudioRenderer");
    CHECK_AND_RETURN_RET_LOG(streamType_ == RENDERER_TYPE, AUDIOSTREAM_ERROR_INVALID_PARAM,
        "Error, invalid type input");

    AudioStreamInfo streamInfo = {
        (AudioSamplingRate)samplingRate_,
        encodingType_,
        sampleFormat_,
        (AudioChannel)channelCount_
    };

    AudioRendererInfo rendererInfo = {
        contentType_,
        usage_,
        latencyMode_
    };

    AudioRendererOptions options = {
        streamInfo,
        rendererInfo
    };

    OHAudioRenderer *audioRenderer = new OHAudioRenderer();
    if (audioRenderer->Initialize(options)) {
        audioRenderer->SetRendererCallback(rendererCallbacks_, userData_);
        audioRenderer->SetRendererOutputDeviceChangeCallback(outputDeviceChangecallback_, outputDeviceChangeuserData_);
        *renderer = (OH_AudioRenderer*)audioRenderer;
        if (preferredFrameSize_ != UNDEFINED_SIZE) {
            audioRenderer->SetPreferredFrameSize(preferredFrameSize_);
        }
        return AUDIOSTREAM_SUCCESS;
    }
    AUDIO_ERR_LOG("Create OHAudioRenderer failed");
    delete audioRenderer;
    audioRenderer = nullptr;
    return AUDIOSTREAM_ERROR_INVALID_PARAM;
}

OH_AudioStream_Result OHAudioStreamBuilder::Generate(OH_AudioCapturer** capturer)
{
    AUDIO_INFO_LOG("Generate OHAudioCapturer");
    CHECK_AND_RETURN_RET_LOG(streamType_ == CAPTURER_TYPE, AUDIOSTREAM_ERROR_INVALID_PARAM,
        "Error, invalid type input");
    AudioStreamInfo streamInfo = {
        (AudioSamplingRate)samplingRate_,
        encodingType_,
        sampleFormat_,
        (AudioChannel)channelCount_
    };

    AudioCapturerInfo capturerInfo = {
        sourceType_,
        latencyMode_
    };

    AudioCapturerOptions options = {
        streamInfo,
        capturerInfo
    };

    OHAudioCapturer *audioCapturer = new OHAudioCapturer();
    if (audioCapturer->Initialize(options)) {
        audioCapturer->SetCapturerCallback(capturerCallbacks_, userData_);
        *capturer = (OH_AudioCapturer*)audioCapturer;
        return AUDIOSTREAM_SUCCESS;
    }
    AUDIO_ERR_LOG("Create OHAudioCapturer failed");
    delete audioCapturer;
    audioCapturer = nullptr;
    return AUDIOSTREAM_ERROR_INVALID_PARAM;
}

OH_AudioStream_Result OHAudioStreamBuilder::SetRendererCallback(OH_AudioRenderer_Callbacks callbacks, void* userData)
{
    CHECK_AND_RETURN_RET_LOG(streamType_ != CAPTURER_TYPE, AUDIOSTREAM_ERROR_INVALID_PARAM,
        "SetRendererCallback Error, invalid type input");
    rendererCallbacks_ = callbacks;
    userData_ = userData;
    return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OHAudioStreamBuilder::SetCapturerCallback(OH_AudioCapturer_Callbacks callbacks, void* userData)
{
    CHECK_AND_RETURN_RET_LOG(streamType_ != RENDERER_TYPE, AUDIOSTREAM_ERROR_INVALID_PARAM,
        "SetCapturerCallback Error, invalid type input");
    capturerCallbacks_ = callbacks;
    userData_ = userData;
    return AUDIOSTREAM_SUCCESS;
}

OH_AudioStream_Result OHAudioStreamBuilder::SetRendererOutputDeviceChangeCallback(
    OH_AudioRenderer_OutputDeviceChangeCallback callback, void* userData)
{
    CHECK_AND_RETURN_RET_LOG(streamType_ != CAPTURER_TYPE, AUDIOSTREAM_ERROR_INVALID_PARAM,
        "SetRendererCallback Error, invalid type input");
    outputDeviceChangecallback_ = callback;
    outputDeviceChangeuserData_ = userData;
    return AUDIOSTREAM_SUCCESS;
}
}  // namespace AudioStandard
}  // namespace OHOS
