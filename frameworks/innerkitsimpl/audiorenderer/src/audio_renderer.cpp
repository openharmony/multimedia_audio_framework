/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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

#include <vector>

#include "audio_errors.h"
#include "audio_renderer.h"
#include "audio_stream.h"

namespace OHOS {
namespace AudioStandard {

class AudioRendererPrivate : public AudioRenderer {
public:
    int32_t GetFrameCount(uint32_t &frameCount) override;
    int32_t SetParams(const AudioRendererParams params) override;
    int32_t GetParams(AudioRendererParams &params) override;
    bool Start() override;
    int32_t  Write(uint8_t *buffer, size_t bufferSize) override;
    RendererState GetStatus() override;
    bool GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) override;
    bool Drain() override;
    bool Stop() override;
    bool Release() override;
    int32_t GetBufferSize(size_t &bufferSize) override;

    std::unique_ptr<AudioStream> audioRenderer;

    AudioRendererPrivate(AudioStreamType audioStreamType);
    virtual ~AudioRendererPrivate();
};

AudioRenderer::~AudioRenderer() = default;
AudioRendererPrivate::~AudioRendererPrivate() = default;

std::unique_ptr<AudioRenderer> AudioRenderer::Create(AudioStreamType audioStreamType)
{
    return std::make_unique<AudioRendererPrivate>(audioStreamType);
}

AudioRendererPrivate::AudioRendererPrivate(AudioStreamType audioStreamType)
{
    audioRenderer = std::make_unique<AudioStream>(audioStreamType, AUDIO_MODE_PLAYBACK);
}

int32_t AudioRendererPrivate::GetFrameCount(uint32_t &frameCount)
{
    return audioRenderer->GetFrameCount(frameCount);
}

int32_t AudioRendererPrivate::SetParams(const AudioRendererParams params)
{
    AudioStreamParams audioStreamParams;
    audioStreamParams.format = params.sampleFormat;
    audioStreamParams.samplingRate = params.sampleRate;
    audioStreamParams.channels = params.channelCount;
    audioStreamParams.encoding = params.encodingType;

    return audioRenderer->SetAudioStreamInfo(audioStreamParams);
}

int32_t AudioRendererPrivate::GetParams(AudioRendererParams &params)
{
    AudioStreamParams audioStreamParams;
    int32_t result = audioRenderer->GetAudioStreamInfo(audioStreamParams);
    if(!result) {
        params.sampleFormat = static_cast<AudioSampleFormat>(audioStreamParams.format);
        params.sampleRate = static_cast<AudioSamplingRate>(audioStreamParams.samplingRate);
        params.channelCount = static_cast<AudioChannel>(audioStreamParams.channels);
        params.encodingType = static_cast<AudioEncodingType>(audioStreamParams.encoding);
    }

    return result;
}

bool AudioRendererPrivate::Start()
{
    return audioRenderer->StartAudioStream();
}

int32_t AudioRendererPrivate::Write(uint8_t *buffer, size_t bufferSize)
{
    return audioRenderer->Write(buffer, bufferSize);
}

RendererState AudioRendererPrivate::GetStatus()
{
    return static_cast<RendererState>(audioRenderer->GetState());
}

bool AudioRendererPrivate::GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base)
{
    return audioRenderer->GetAudioTime(timestamp,base);
}

bool AudioRendererPrivate::Drain()
{
    return audioRenderer->DrainAudioStream();
}

bool AudioRendererPrivate::Stop()
{
    return audioRenderer->StopAudioStream();
}

bool AudioRendererPrivate::Release()
{
    return audioRenderer->ReleaseAudioStream();
}

int32_t AudioRendererPrivate::GetBufferSize(size_t &bufferSize)
{
    return audioRenderer->GetBufferSize(bufferSize);
}

std::vector<AudioSampleFormat> AudioRenderer::GetSupportedFormats()
{
    return AUDIO_SUPPORTED_FORMATS;
}

std::vector<AudioSamplingRate> AudioRenderer::GetSupportedSamplingRates()
{
    return AUDIO_SUPPORTED_SAMPLING_RATES;
}

std::vector<AudioChannel> AudioRenderer::GetSupportedChannels()
{
    return AUDIO_SUPPORTED_CHANNELS;
}

std::vector<AudioEncodingType> AudioRenderer::GetSupportedEncodingTypes()
{
    return AUDIO_SUPPORTED_ENCODING_TYPES;
}
}  // namespace AudioStandard
}  // namespace OHOS