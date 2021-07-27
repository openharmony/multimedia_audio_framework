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
#include "audio_recorder.h"
#include "audio_recorder_private.h"
#include "audio_stream.h"

namespace OHOS {
namespace AudioStandard {
AudioRecorder::~AudioRecorder() = default;
AudioRecorderPrivate::~AudioRecorderPrivate() = default;

std::unique_ptr<AudioRecorder> AudioRecorder::Create(AudioStreamType audioStreamType)
{
    return std::make_unique<AudioRecorderPrivate>(audioStreamType);
}

AudioRecorderPrivate::AudioRecorderPrivate(AudioStreamType audioStreamType)
{
    audioRecorder = std::make_unique<AudioStream>(audioStreamType, AUDIO_MODE_RECORD);
}

int32_t AudioRecorderPrivate::GetFrameCount(uint32_t &frameCount) const
{
    return audioRecorder->GetFrameCount(frameCount);
}

int32_t AudioRecorderPrivate::SetParams(const AudioRecorderParams params) const
{
    AudioStreamParams audioStreamParams;
    audioStreamParams.format = params.audioSampleFormat;
    audioStreamParams.samplingRate = params.samplingRate;
    audioStreamParams.channels = params.audioChannel;
    audioStreamParams.encoding = params.audioEncoding;

    return audioRecorder->SetAudioStreamInfo(audioStreamParams);
}

int32_t AudioRecorderPrivate::GetParams(AudioRecorderParams &params) const
{
    AudioStreamParams audioStreamParams;
    int32_t result = audioRecorder->GetAudioStreamInfo(audioStreamParams);
    if (SUCCESS == result) {
        params.audioSampleFormat = static_cast<AudioSampleFormat>(audioStreamParams.format);
        params.samplingRate =  static_cast<AudioSamplingRate>(audioStreamParams.samplingRate);
        params.audioChannel = static_cast<AudioChannel>(audioStreamParams.channels);
        params.audioEncoding = static_cast<AudioEncodingType>(audioStreamParams.encoding);
    }

    return result;
}

bool AudioRecorderPrivate::Start() const
{
    return audioRecorder->StartAudioStream();
}

int32_t AudioRecorderPrivate::Read(uint8_t &buffer, size_t userSize, bool isBlockingRead) const
{
    return audioRecorder->Read(buffer, userSize, isBlockingRead);
}

RecorderState AudioRecorderPrivate::GetStatus() const
{
    return (RecorderState)audioRecorder->GetState();
}

bool AudioRecorderPrivate::GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) const
{
    return audioRecorder->GetAudioTime(timestamp, base);
}

bool AudioRecorderPrivate::Stop() const
{
    return audioRecorder->StopAudioStream();
}

bool AudioRecorderPrivate::Flush() const
{
    return audioRecorder->FlushAudioStream();
}

bool AudioRecorderPrivate::Release() const
{
    return audioRecorder->ReleaseAudioStream();
}

int32_t AudioRecorderPrivate::GetBufferSize(size_t &bufferSize) const
{
    return audioRecorder->GetBufferSize(bufferSize);
}

std::vector<AudioSampleFormat> AudioRecorder::GetSupportedFormats()
{
    return AUDIO_SUPPORTED_FORMATS;
}

std::vector<AudioChannel> AudioRecorder::GetSupportedChannels()
{
    return AUDIO_SUPPORTED_CHANNELS;
}

std::vector<AudioEncodingType> AudioRecorder::GetSupportedEncodingTypes()
{
    return AUDIO_SUPPORTED_ENCODING_TYPES;
}

std::vector<AudioSamplingRate> AudioRecorder::GetSupportedSamplingRates()
{
    return AUDIO_SUPPORTED_SAMPLING_RATES;
}
}  // namespace AudioStandard
}  // namespace OHOS