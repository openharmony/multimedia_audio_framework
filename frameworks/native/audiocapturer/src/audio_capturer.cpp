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

#include "audio_errors.h"
#ifdef OHCORE
#include "audio_capturer_gateway.h"
#else
#include "audio_capturer_private.h"
#include "audio_stream.h"
#endif
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
AudioCapturer::~AudioCapturer() = default;

#ifndef OHCORE
AudioCapturerPrivate::~AudioCapturerPrivate() = default;
#endif

std::unique_ptr<AudioCapturer> AudioCapturer::Create(AudioStreamType audioStreamType)
{
    AppInfo appInfo = {};
    return Create(audioStreamType, appInfo);
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create(AudioStreamType audioStreamType, const AppInfo &appInfo)
{
#ifdef OHCORE
    return std::make_unique<AudioCapturerGateway>(audioStreamType, appInfo);
#else
    return std::make_unique<AudioCapturerPrivate>(audioStreamType, appInfo);
#endif
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create(const AudioCapturerOptions &options)
{
    AppInfo appInfo = {};
    return Create(options, "", appInfo);
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create(const AudioCapturerOptions &options, const AppInfo &appInfo)
{
    return Create(options, "", appInfo);
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create(const AudioCapturerOptions &options, const std::string cachePath)
{
    AppInfo appInfo = {};
    return Create(options, cachePath, appInfo);
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create(const AudioCapturerOptions &capturerOptions,
    const std::string cachePath, const AppInfo &appInfo)
{
    auto sourceType = capturerOptions.capturerInfo.sourceType;
    if (sourceType < SOURCE_TYPE_MIC || sourceType > SOURCE_TYPE_VOICE_COMMUNICATION) {
        return nullptr;
    }

    AudioStreamType audioStreamType = STREAM_MUSIC;
    if (sourceType == SOURCE_TYPE_VOICE_COMMUNICATION) {
        audioStreamType = STREAM_VOICE_CALL;
    }

    AudioCapturerParams params;
    params.audioSampleFormat = capturerOptions.streamInfo.format;
    params.samplingRate = capturerOptions.streamInfo.samplingRate;
    bool isChange = false;
    if (AudioChannel::CHANNEL_3 == capturerOptions.streamInfo.channels) {
        params.audioChannel = AudioChannel::STEREO;
        isChange = true;
    } else {
        params.audioChannel = capturerOptions.streamInfo.channels;
    }
    params.audioEncoding = capturerOptions.streamInfo.encoding;
#ifdef OHCORE
    auto capturer = std::make_unique<AudioCapturerGateway>(audioStreamType, appInfo);
#else
    auto capturer = std::make_unique<AudioCapturerPrivate>(audioStreamType, appInfo);
#endif
    if (capturer == nullptr) {
        return capturer;
    }

    if (!cachePath.empty()) {
        AUDIO_DEBUG_LOG("Set application cache path");
        capturer->SetApplicationCachePath(cachePath);
    }

    capturer->capturerInfo_.sourceType = sourceType;
    capturer->capturerInfo_.capturerFlags = capturerOptions.capturerInfo.capturerFlags;
    if (capturer->SetParams(params) != SUCCESS) {
        capturer = nullptr;
    }
    if (isChange) {
        capturer->isChannelChange_ = true;
    }
    return capturer;
}

#ifndef OHCORE
AudioCapturerPrivate::AudioCapturerPrivate(AudioStreamType audioStreamType, const AppInfo &appInfo)
{
    appInfo_ = appInfo;
    if (!(appInfo_.appPid)) {
        appInfo_.appPid = getpid();
    }

    if (appInfo_.appUid < 0) {
        appInfo_.appUid = static_cast<int32_t>(getuid());
    }

    audioStream_ = std::make_shared<AudioStream>(audioStreamType, AUDIO_MODE_RECORD, appInfo_.appUid);
    if (audioStream_) {
        AUDIO_DEBUG_LOG("AudioCapturerPrivate::Audio stream created");
    }
    capturerProxyObj_ = std::make_shared<AudioCapturerProxyObj>();
    if (!capturerProxyObj_) {
        AUDIO_ERR_LOG("AudioCapturerProxyObj Memory Allocation Failed !!");
    }
}

int32_t AudioCapturerPrivate::GetFrameCount(uint32_t &frameCount) const
{
    return audioStream_->GetFrameCount(frameCount);
}

int32_t AudioCapturerPrivate::SetParams(const AudioCapturerParams params)
{
    if (!audioStream_->VerifyClientPermission(MICROPHONE_PERMISSION, appInfo_.appTokenId, appInfo_.appUid,
        true, AUDIO_PERMISSION_START)) {
        AUDIO_ERR_LOG("MICROPHONE permission denied for %{public}d", appInfo_.appTokenId);
        return ERR_PERMISSION_DENIED;
    }
    const AudioCapturer *capturer = this;
    capturerProxyObj_->SaveCapturerObj(capturer);

    audioStream_->SetCapturerInfo(capturerInfo_);

    AudioStreamParams audioStreamParams;
    audioStreamParams.format = params.audioSampleFormat;
    audioStreamParams.samplingRate = params.samplingRate;
    audioStreamParams.channels = params.audioChannel;
    audioStreamParams.encoding = params.audioEncoding;

    audioStream_->SetClientID(appInfo_.appPid, appInfo_.appUid);

    return audioStream_->SetAudioStreamInfo(audioStreamParams, capturerProxyObj_);
}

int32_t AudioCapturerPrivate::SetCapturerCallback(const std::shared_ptr<AudioCapturerCallback> &callback)
{
    // If the client is using the deprecated SetParams API. SetCapturerCallback must be invoked, after SetParams.
    // In general, callbacks can only be set after the capturer state is  PREPARED.
    CapturerState state = GetStatus();
    if (state == CAPTURER_NEW || state == CAPTURER_RELEASED) {
        AUDIO_DEBUG_LOG("AudioCapturerPrivate::SetCapturerCallback ncorrect state:%{public}d to register cb", state);
        return ERR_ILLEGAL_STATE;
    }
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioCapturerPrivate::SetCapturerCallback callback param is null");
        return ERR_INVALID_PARAM;
    }

    // Save and Set reference for stream callback. Order is important here.
    if (audioStreamCallback_ == nullptr) {
        audioStreamCallback_ = std::make_shared<AudioStreamCallbackCapturer>();
        if (audioStreamCallback_ == nullptr) {
            AUDIO_ERR_LOG("AudioCapturerPrivate::Failed to allocate memory for audioStreamCallback_");
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
        if (this->isChannelChange_) {
            streamInfo.channels = AudioChannel::CHANNEL_3;
        } else {
            streamInfo.channels = static_cast<AudioChannel>(audioStreamParams.channels);
        }
        streamInfo.encoding = static_cast<AudioEncodingType>(audioStreamParams.encoding);
    }

    return result;
}

int32_t AudioCapturerPrivate::SetCapturerPositionCallback(int64_t markPosition,
    const std::shared_ptr<CapturerPositionCallback> &callback)
{
    if ((callback == nullptr) || (markPosition <= 0)) {
        AUDIO_ERR_LOG("AudioCapturerPrivate::SetCapturerPositionCallback input param is invalid");
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
        AUDIO_ERR_LOG("AudioCapturerPrivate::SetCapturerPeriodPositionCallback input param is invalid");
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
    if (!audioStream_->getUsingPemissionFromPrivacy(MICROPHONE_PERMISSION, appInfo_.appTokenId,
        AUDIO_PERMISSION_START)) {
        AUDIO_ERR_LOG("Start monitor permission failed");
    }
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

bool AudioCapturerPrivate::Pause() const
{
    if (!audioStream_->getUsingPemissionFromPrivacy(MICROPHONE_PERMISSION, appInfo_.appTokenId,
        AUDIO_PERMISSION_STOP)) {
        AUDIO_ERR_LOG("Pause monitor permission failed");
    }
    return audioStream_->PauseAudioStream();
}

bool AudioCapturerPrivate::Stop() const
{
    if (!audioStream_->getUsingPemissionFromPrivacy(MICROPHONE_PERMISSION, appInfo_.appTokenId,
        AUDIO_PERMISSION_STOP)) {
        AUDIO_ERR_LOG("Stop monitor permission failed");
    }
    return audioStream_->StopAudioStream();
}

bool AudioCapturerPrivate::Flush() const
{
    return audioStream_->FlushAudioStream();
}

bool AudioCapturerPrivate::Release() const
{
    if (!audioStream_->getUsingPemissionFromPrivacy(MICROPHONE_PERMISSION, appInfo_.appTokenId,
        AUDIO_PERMISSION_STOP)) {
        AUDIO_ERR_LOG("Release monitor permission failed");
    }
    return audioStream_->ReleaseAudioStream();
}

int32_t AudioCapturerPrivate::GetBufferSize(size_t &bufferSize) const
{
    return audioStream_->GetBufferSize(bufferSize);
}

int32_t AudioCapturerPrivate::GetAudioStreamId(uint32_t &sessionID) const
{
    return audioStream_->GetAudioSessionID(sessionID);
}

int32_t AudioCapturerPrivate::SetBufferDuration(uint64_t bufferDuration) const
{
    if (bufferDuration < MINIMUM_BUFFER_SIZE_MSEC || bufferDuration > MAXIMUM_BUFFER_SIZE_MSEC) {
        AUDIO_ERR_LOG("Error: Please set the buffer duration between 5ms ~ 20ms");
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

void AudioStreamCallbackCapturer::OnStateChange(const State state,
    const StateChangeCmdType __attribute__((unused)) cmdType)
{
    std::shared_ptr<AudioCapturerCallback> cb = callback_.lock();
    if (cb == nullptr) {
        AUDIO_ERR_LOG("AudioStreamCallbackCapturer::OnStateChange cb == nullptr.");
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
    return CAPTURER_SUPPORTED_CHANNELS;
}

std::vector<AudioEncodingType> AudioCapturer::GetSupportedEncodingTypes()
{
    return AUDIO_SUPPORTED_ENCODING_TYPES;
}

std::vector<AudioSamplingRate> AudioCapturer::GetSupportedSamplingRates()
{
    return AUDIO_SUPPORTED_SAMPLING_RATES;
}

int32_t AudioCapturerPrivate::SetCaptureMode(AudioCaptureMode captureMode) const
{
    return audioStream_->SetCaptureMode(captureMode);
}

AudioCaptureMode AudioCapturerPrivate::GetCaptureMode() const
{
    return audioStream_->GetCaptureMode();
}

int32_t AudioCapturerPrivate::SetCapturerReadCallback(const std::shared_ptr<AudioCapturerReadCallback> &callback)
{
    return audioStream_->SetCapturerReadCallback(callback);
}

int32_t AudioCapturerPrivate::GetBufferDesc(BufferDesc &bufDesc) const
{
    return audioStream_->GetBufferDesc(bufDesc);
}

int32_t AudioCapturerPrivate::Enqueue(const BufferDesc &bufDesc) const
{
    return audioStream_->Enqueue(bufDesc);
}

int32_t AudioCapturerPrivate::Clear() const
{
    return audioStream_->Clear();
}

int32_t AudioCapturerPrivate::GetBufQueueState(BufferQueueState &bufState) const
{
    return audioStream_->GetBufQueueState(bufState);
}
#endif

}  // namespace AudioStandard
}  // namespace OHOS
