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

#include "audio_capturer.h"

#include "audio_capturer_private.h"
#include "audio_errors.h"
#include "audio_stream.h"
#include "media_log.h"

namespace OHOS {
namespace AudioStandard {
AudioCapturer::~AudioCapturer() = default;
AudioCapturerPrivate::~AudioCapturerPrivate() = default;

std::unique_ptr<AudioCapturer> AudioCapturer::Create(AudioStreamType audioStreamType)
{
    return std::make_unique<AudioCapturerPrivate>(audioStreamType);
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create(const AudioCapturerOptions &capturerOptions)
{
    return Create("", capturerOptions);
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create(const std::string cachePath,
    const AudioCapturerOptions &capturerOptions)
{
    auto sourceType = capturerOptions.capturerInfo.sourceType;
    if (sourceType < SOURCE_TYPE_MIC || sourceType > SOURCE_TYPE_VOICE_CALL) {
        return nullptr;
    }

    AudioStreamType audioStreamType = STREAM_MUSIC;
    if (sourceType == SOURCE_TYPE_VOICE_CALL) {
        audioStreamType = STREAM_VOICE_CALL;
    }

    AudioCapturerParams params;
    params.audioSampleFormat = capturerOptions.streamInfo.format;
    params.samplingRate = capturerOptions.streamInfo.samplingRate;
    params.audioChannel = capturerOptions.streamInfo.channels;
    params.audioEncoding = capturerOptions.streamInfo.encoding;

    auto capturer = std::make_unique<AudioCapturerPrivate>(audioStreamType);
    if (capturer == nullptr) {
        return capturer;
    }

    if (!cachePath.empty()) {
        MEDIA_DEBUG_LOG("Set application cache path");
        capturer->SetApplicationCachePath(cachePath);
    }

    if (capturer->SetParams(params) != SUCCESS) {
        capturer = nullptr;
        return nullptr;
    }

    capturer->capturerInfo_.sourceType = sourceType;
    capturer->capturerInfo_.capturerFlags = capturerOptions.capturerInfo.capturerFlags;

    return capturer;
}

AudioCapturerPrivate::AudioCapturerPrivate(AudioStreamType audioStreamType)
{
    audioStream_ = std::make_shared<AudioStream>(audioStreamType, AUDIO_MODE_RECORD);
}

int32_t AudioCapturerPrivate::GetFrameCount(uint32_t &frameCount) const
{
    return audioStream_->GetFrameCount(frameCount);
}

int32_t AudioCapturerPrivate::SetParams(const AudioCapturerParams params) const
{
    AudioStreamParams audioStreamParams;
    audioStreamParams.format = params.audioSampleFormat;
    audioStreamParams.samplingRate = params.samplingRate;
    audioStreamParams.channels = params.audioChannel;
    audioStreamParams.encoding = params.audioEncoding;

    return audioStream_->SetAudioStreamInfo(audioStreamParams);
}

int32_t AudioCapturerPrivate::SetCapturerCallback(const std::shared_ptr<AudioCapturerCallback> &callback)
{
    // If the client is using the deprecated SetParams API. SetCapturerCallback must be invoked, after SetParams.
    // In general, callbacks can only be set after the capturer state is  PREPARED.
    CapturerState state = GetStatus();
    if (state == CAPTURER_NEW || state == CAPTURER_RELEASED) {
        MEDIA_DEBUG_LOG("AudioCapturerPrivate::SetCapturerCallback ncorrect state:%{public}d to register cb", state);
        return ERR_ILLEGAL_STATE;
    }
    if (callback == nullptr) {
        MEDIA_ERR_LOG("AudioCapturerPrivate::SetCapturerCallback callback param is null");
        return ERR_INVALID_PARAM;
    }

    // Save and Set reference for stream callback. Order is important here.
    if (audioStreamCallback_ == nullptr) {
        audioStreamCallback_ = std::make_shared<AudioStreamCallbackCapturer>();
        if (audioStreamCallback_ == nullptr) {
            MEDIA_ERR_LOG("AudioCapturerPrivate::Failed to allocate memory for audioStreamCallback_");
            return ERROR;
        }
    }
    std::shared_ptr<AudioStreamCallbackCapturer> cbStream =
        std::static_pointer_cast<AudioStreamCallbackCapturer>(audioStreamCallback_);
    cbStream->SaveCallback(callback);
    (void)audioStream_->SetStreamCallback(audioStreamCallback_);

    return SUCCESS;
}

int32_t AudioCapturerPrivate::GetParams(AudioCapturerParams &params) const
{
    AudioStreamParams audioStreamParams;
    int32_t result = audioStream_->GetAudioStreamInfo(audioStreamParams);
    if (SUCCESS == result) {
        params.audioSampleFormat = static_cast<AudioSampleFormat>(audioStreamParams.format);
        params.samplingRate = static_cast<AudioSamplingRate>(audioStreamParams.samplingRate);
        params.audioChannel = static_cast<AudioChannel>(audioStreamParams.channels);
        params.audioEncoding = static_cast<AudioEncodingType>(audioStreamParams.encoding);
    }

    return result;
}

int32_t AudioCapturerPrivate::GetCapturerInfo(AudioCapturerInfo &capturerInfo) const
{
    capturerInfo = capturerInfo_;

    return SUCCESS;
}

int32_t AudioCapturerPrivate::GetStreamInfo(AudioStreamInfo &streamInfo) const
{
    AudioStreamParams audioStreamParams;
    int32_t result = audioStream_->GetAudioStreamInfo(audioStreamParams);
    if (SUCCESS == result) {
        streamInfo.format = static_cast<AudioSampleFormat>(audioStreamParams.format);
        streamInfo.samplingRate = static_cast<AudioSamplingRate>(audioStreamParams.samplingRate);
        streamInfo.channels = static_cast<AudioChannel>(audioStreamParams.channels);
        streamInfo.encoding = static_cast<AudioEncodingType>(audioStreamParams.encoding);
    }

    return result;
}

int32_t AudioCapturerPrivate::SetCapturerPositionCallback(int64_t markPosition,
    const std::shared_ptr<CapturerPositionCallback> &callback)
{
    if ((callback == nullptr) || (markPosition <= 0)) {
        MEDIA_ERR_LOG("AudioCapturerPrivate::SetCapturerPositionCallback input param is invalid");
        return ERR_INVALID_PARAM;
    }

    audioStream_->SetCapturerPositionCallback(markPosition, callback);

    return SUCCESS;
}

void AudioCapturerPrivate::UnsetCapturerPositionCallback()
{
    audioStream_->UnsetCapturerPositionCallback();
}

int32_t AudioCapturerPrivate::SetCapturerPeriodPositionCallback(int64_t frameNumber,
    const std::shared_ptr<CapturerPeriodPositionCallback> &callback)
{
    if ((callback == nullptr) || (frameNumber <= 0)) {
        MEDIA_ERR_LOG("AudioCapturerPrivate::SetCapturerPeriodPositionCallback input param is invalid");
        return ERR_INVALID_PARAM;
    }

    audioStream_->SetCapturerPeriodPositionCallback(frameNumber, callback);

    return SUCCESS;
}

void AudioCapturerPrivate::UnsetCapturerPeriodPositionCallback()
{
    audioStream_->UnsetCapturerPeriodPositionCallback();
}

bool AudioCapturerPrivate::Start() const
{
    return audioStream_->StartAudioStream();
}

int32_t AudioCapturerPrivate::Read(uint8_t &buffer, size_t userSize, bool isBlockingRead) const
{
    return audioStream_->Read(buffer, userSize, isBlockingRead);
}

CapturerState AudioCapturerPrivate::GetStatus() const
{
    return (CapturerState)audioStream_->GetState();
}

bool AudioCapturerPrivate::GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) const
{
    return audioStream_->GetAudioTime(timestamp, base);
}

bool AudioCapturerPrivate::Stop() const
{
    return audioStream_->StopAudioStream();
}

bool AudioCapturerPrivate::Flush() const
{
    return audioStream_->FlushAudioStream();
}

bool AudioCapturerPrivate::Release() const
{
    return audioStream_->ReleaseAudioStream();
}

int32_t AudioCapturerPrivate::GetBufferSize(size_t &bufferSize) const
{
    return audioStream_->GetBufferSize(bufferSize);
}

int32_t AudioCapturerPrivate::SetBufferDuration(uint64_t bufferDuration) const
{
    if (bufferDuration < MINIMUM_BUFFER_SIZE_MSEC || bufferDuration > MAXIMUM_BUFFER_SIZE_MSEC) {
        MEDIA_ERR_LOG("Error: Please set the buffer duration between 5ms ~ 20ms");
        return ERR_INVALID_PARAM;
    }
    return audioStream_->SetBufferSizeInMsec(bufferDuration);
}

void AudioCapturerPrivate::SetApplicationCachePath(const std::string cachePath)
{
    audioStream_->SetApplicationCachePath(cachePath);
}

void AudioStreamCallbackCapturer::SaveCallback(const std::weak_ptr<AudioCapturerCallback> &callback)
{
    callback_ = callback;
}

void AudioStreamCallbackCapturer::OnStateChange(const State state)
{
    std::shared_ptr<AudioCapturerCallback> cb = callback_.lock();
    if (cb == nullptr) {
        MEDIA_ERR_LOG("AudioStreamCallbackCapturer::OnStateChange cb == nullptr.");
        return;
    }

    cb->OnStateChange(static_cast<CapturerState>(state));
}

std::vector<AudioSampleFormat> AudioCapturer::GetSupportedFormats()
{
    return AUDIO_SUPPORTED_FORMATS;
}

std::vector<AudioChannel> AudioCapturer::GetSupportedChannels()
{
    return AUDIO_SUPPORTED_CHANNELS;
}

std::vector<AudioEncodingType> AudioCapturer::GetSupportedEncodingTypes()
{
    return AUDIO_SUPPORTED_ENCODING_TYPES;
}

std::vector<AudioSamplingRate> AudioCapturer::GetSupportedSamplingRates()
{
    return AUDIO_SUPPORTED_SAMPLING_RATES;
}
}  // namespace AudioStandard
}  // namespace OHOS
