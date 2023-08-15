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
#ifdef OHCORE
#include "audio_capturer_gateway.h"
#endif
#include "audio_log.h"
#include "audio_policy_manager.h"

namespace OHOS {
namespace AudioStandard {
AudioCapturer::~AudioCapturer() = default;
AudioCapturerPrivate::~AudioCapturerPrivate() = default;

std::unique_ptr<AudioCapturer> AudioCapturer::Create(AudioStreamType audioStreamType)
{
    AppInfo appInfo = {};
    return Create(audioStreamType, appInfo);
}

std::unique_ptr<AudioCapturer> AudioCapturer::Create(AudioStreamType audioStreamType, const AppInfo &appInfo)
{
#ifdef OHCORE
    return std::make_unique<AudioCapturerGateway>(audioStreamType);
#else
    return std::make_unique<AudioCapturerPrivate>(audioStreamType, appInfo, true);
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
    if (sourceType < SOURCE_TYPE_MIC || sourceType > SOURCE_TYPE_ULTRASONIC) {
        return nullptr;
    }

    AudioStreamType audioStreamType = FindStreamTypeBySourceType(sourceType);

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
    auto capturer = std::make_unique<AudioCapturerGateway>(audioStreamType);
#else
    auto capturer = std::make_unique<AudioCapturerPrivate>(audioStreamType, appInfo, false);
#endif
    if (capturer == nullptr) {
        return capturer;
    }

    if (!cachePath.empty()) {
        AUDIO_DEBUG_LOG("Set application cache path");
        capturer->cachePath_ = cachePath;
    }

    if (capturer->InitPlaybackCapturer(sourceType, capturerOptions.playbackCaptureConfig) != SUCCESS) {
        return nullptr;
    }

    capturer->capturerInfo_.sourceType = sourceType;
    capturer->capturerInfo_.capturerFlags = capturerOptions.capturerInfo.capturerFlags;
    if (capturer->SetParams(params) != SUCCESS) {
        capturer = nullptr;
    }

    if (capturer != nullptr && isChange) {
        capturer->isChannelChange_ = true;
    }

    return capturer;
}

AudioCapturerPrivate::AudioCapturerPrivate(AudioStreamType audioStreamType, const AppInfo &appInfo, bool createStream)
{
    if (audioStreamType < STREAM_VOICE_CALL || audioStreamType > STREAM_ALL) {
        AUDIO_ERR_LOG("AudioCapturerPrivate audioStreamType is invalid!");
    }
    audioStreamType_ = audioStreamType;
    auto iter = streamToSource_.find(audioStreamType);
    if (iter != streamToSource_.end()) {
        capturerInfo_.sourceType = iter->second;
    }
    appInfo_ = appInfo;
    if (!(appInfo_.appPid)) {
        appInfo_.appPid = getpid();
    }

    if (appInfo_.appUid < 0) {
        appInfo_.appUid = static_cast<int32_t>(getuid());
    }
    if (createStream) {
        AudioStreamParams tempParams = {};
        audioStream_ = IAudioStream::GetRecordStream(IAudioStream::PA_STREAM, tempParams, audioStreamType_,
            appInfo_.appUid);
        AUDIO_INFO_LOG("AudioCapturerPrivate create normal stream for old mode.");
    }

    capturerProxyObj_ = std::make_shared<AudioCapturerProxyObj>();
    if (!capturerProxyObj_) {
        AUDIO_ERR_LOG("AudioCapturerProxyObj Memory Allocation Failed !!");
    }
}

int32_t AudioCapturerPrivate::InitPlaybackCapturer(int32_t type, const AudioPlaybackCaptureConfig &config)
{
    if (type != SOURCE_TYPE_PLAYBACK_CAPTURE) {
        return SUCCESS;
    }
    return AudioPolicyManager::GetInstance().SetPlaybackCapturerFilterInfos(config, appInfo_.appTokenId);
}

int32_t AudioCapturerPrivate::GetFrameCount(uint32_t &frameCount) const
{
    return audioStream_->GetFrameCount(frameCount);
}

int32_t AudioCapturerPrivate::SetParams(const AudioCapturerParams params)
{
    AUDIO_INFO_LOG("AudioCapturer::SetParams");
    AudioStreamParams audioStreamParams = ConvertToAudioStreamParams(params);

    IAudioStream::StreamClass streamClass = IAudioStream::PA_STREAM;
    if (capturerInfo_.capturerFlags == STREAM_FLAG_FAST) {
        streamClass = IAudioStream::FAST_STREAM;
    }

    // check AudioStreamParams for fast stream
    if (audioStream_ == nullptr) {
        audioStream_ = IAudioStream::GetRecordStream(streamClass, audioStreamParams, audioStreamType_,
            appInfo_.appUid);
        CHECK_AND_RETURN_RET_LOG(audioStream_ != nullptr, ERR_INVALID_PARAM, "SetParams GetRecordStream faied.");
        AUDIO_INFO_LOG("IAudioStream::GetStream success");
        audioStream_->SetApplicationCachePath(cachePath_);
    }

    if (!audioStream_->CheckRecordingCreate(appInfo_.appTokenId, appInfo_.appFullTokenId, appInfo_.appUid)) {
        AUDIO_ERR_LOG("recording create check failed");
        return ERR_PERMISSION_DENIED;
    }

    const AudioCapturer *capturer = this;
    capturerProxyObj_->SaveCapturerObj(capturer);

    audioStream_->SetCapturerInfo(capturerInfo_);

    audioStream_->SetClientID(appInfo_.appPid, appInfo_.appUid);

    if (capturerInfo_.sourceType == SOURCE_TYPE_PLAYBACK_CAPTURE) {
        audioStream_->SetInnerCapturerState(true);
    }

    if (capturerInfo_.sourceType == SourceType::SOURCE_TYPE_WAKEUP) {
        audioStream_->SetWakeupCapturerState(true);
    }

    int32_t ret = audioStream_->SetAudioStreamInfo(audioStreamParams, capturerProxyObj_);
    if (ret) {
        AUDIO_ERR_LOG("AudioCapturerPrivate::SetParams SetAudioStreamInfo Failed");
        return ret;
    }
    if (audioStream_->GetAudioSessionID(sessionID_) != 0) {
        AUDIO_ERR_LOG("InitAudioInterruptCallback::GetAudioSessionID failed for INDEPENDENT_MODE");
        return ERR_INVALID_INDEX;
    }
    audioInterrupt_.sessionID = sessionID_;
    audioInterrupt_.audioFocusType.sourceType = capturerInfo_.sourceType;
    if (audioInterruptCallback_ == nullptr) {
        audioInterruptCallback_ = std::make_shared<AudioCapturerInterruptCallbackImpl>(audioStream_);
        if (audioInterruptCallback_ == nullptr) {
            AUDIO_ERR_LOG("AudioCapturerPrivate::Failed to allocate memory for audioInterruptCallback_");
            return ERROR;
        }
    }
    return AudioPolicyManager::GetInstance().SetAudioInterruptCallback(sessionID_, audioInterruptCallback_);
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

    // Save reference for interrupt callback
    if (audioInterruptCallback_ == nullptr) {
        AUDIO_ERR_LOG("AudioCapturerPrivate::SetCapturerCallback audioInterruptCallback_ == nullptr");
        return ERROR;
    }
    std::shared_ptr<AudioCapturerInterruptCallbackImpl> cbInterrupt =
        std::static_pointer_cast<AudioCapturerInterruptCallbackImpl>(audioInterruptCallback_);
    cbInterrupt->SaveCallback(callback);

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
    AUDIO_INFO_LOG("AudioCapturer::Start");
    if (!audioStream_->CheckRecordingStateChange(appInfo_.appTokenId, appInfo_.appFullTokenId,
        appInfo_.appUid, AUDIO_PERMISSION_START)) {
        AUDIO_ERR_LOG("recording start check failed");
        return false;
    }

    if (audioInterrupt_.audioFocusType.sourceType == SOURCE_TYPE_INVALID ||
        audioInterrupt_.sessionID == INVALID_SESSION_ID) {
        return false;
    }

    int32_t ret = AudioPolicyManager::GetInstance().ActivateAudioInterrupt(audioInterrupt_);
    if (ret != 0) {
        AUDIO_ERR_LOG("AudioCapturerPrivate::ActivateAudioInterrupt Failed");
        return false;
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
    AUDIO_INFO_LOG("AudioCapturer::Pause");
    if (!audioStream_->CheckRecordingStateChange(appInfo_.appTokenId, appInfo_.appFullTokenId,
        appInfo_.appUid, AUDIO_PERMISSION_STOP)) {
        AUDIO_ERR_LOG("Pause monitor permission failed");
    }

    // When user is intentionally pausing , Deactivate to remove from audio focus info list
    int32_t ret = AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt_);
    if (ret != 0) {
        AUDIO_ERR_LOG("AudioRenderer: DeactivateAudioInterrupt Failed");
    }

    return audioStream_->PauseAudioStream();
}

bool AudioCapturerPrivate::Stop() const
{
    AUDIO_INFO_LOG("AudioCapturer::Stop");
    if (!audioStream_->CheckRecordingStateChange(appInfo_.appTokenId, appInfo_.appFullTokenId,
        appInfo_.appUid, AUDIO_PERMISSION_STOP)) {
        AUDIO_ERR_LOG("Stop monitor permission failed");
    }

    int32_t ret = AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt_);
    if (ret != 0) {
        AUDIO_ERR_LOG("AudioCapturer: DeactivateAudioInterrupt Failed");
    }

    return audioStream_->StopAudioStream();
}

bool AudioCapturerPrivate::Flush() const
{
    AUDIO_INFO_LOG("AudioCapturer::Flush");
    return audioStream_->FlushAudioStream();
}

bool AudioCapturerPrivate::Release()
{
    AUDIO_INFO_LOG("AudioCapturer::Release");

    std::lock_guard<std::mutex> lock(lock_);
    if (!isValid_) {
        AUDIO_ERR_LOG("Release when capturer invalid");
        return false;
    }

    if (!audioStream_->CheckRecordingStateChange(appInfo_.appTokenId, appInfo_.appFullTokenId,
        appInfo_.appUid, AUDIO_PERMISSION_STOP)) {
        AUDIO_ERR_LOG("Release monitor permission failed");
    }

    (void)AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt_);

    // Unregister the callaback in policy server
    (void)AudioPolicyManager::GetInstance().UnsetAudioInterruptCallback(sessionID_);

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
    cachePath_ = cachePath;
    if (audioStream_ != nullptr) {
        audioStream_->SetApplicationCachePath(cachePath_);
    } else {
        AUDIO_WARNING_LOG("AudioCapturer SetApplicationCachePath while stream is null");
    }
}

AudioCapturerInterruptCallbackImpl::AudioCapturerInterruptCallbackImpl(const std::shared_ptr<IAudioStream> &audioStream)
    : audioStream_(audioStream)
{
    AUDIO_INFO_LOG("AudioCapturerInterruptCallbackImpl constructor");
}

AudioCapturerInterruptCallbackImpl::~AudioCapturerInterruptCallbackImpl()
{
    AUDIO_DEBUG_LOG("AudioCapturerInterruptCallbackImpl: instance destroy");
}

void AudioCapturerInterruptCallbackImpl::SaveCallback(const std::weak_ptr<AudioCapturerCallback> &callback)
{
    callback_ = callback;
}

void AudioCapturerInterruptCallbackImpl::NotifyEvent(const InterruptEvent &interruptEvent)
{
    AUDIO_INFO_LOG("AudioCapturerInterruptCallbackImpl: NotifyEvent: Hint: %{public}d, eventType: %{public}d",
        interruptEvent.hintType, interruptEvent.eventType);

    if (cb_ != nullptr) {
        cb_->OnInterrupt(interruptEvent);
        AUDIO_DEBUG_LOG("AudioCapturerInterruptCallbackImpl: OnInterrupt : NotifyEvent to app complete");
    } else {
        AUDIO_DEBUG_LOG("AudioCapturerInterruptCallbackImpl: cb_ == nullptr cannont NotifyEvent to app");
    }
}

void AudioCapturerInterruptCallbackImpl::NotifyForcePausedToResume(const InterruptEventInternal &interruptEvent)
{
    // Change InterruptForceType to Share, Since app will take care of resuming
    InterruptEvent interruptEventResume {interruptEvent.eventType, INTERRUPT_SHARE,
                                         interruptEvent.hintType};
    NotifyEvent(interruptEventResume);
}

void AudioCapturerInterruptCallbackImpl::HandleAndNotifyForcedEvent(const InterruptEventInternal &interruptEvent)
{
    InterruptHint hintType = interruptEvent.hintType;
    AUDIO_DEBUG_LOG("AudioCapturerPrivate HandleAndNotifyForcedEvent: Force handle the event and notify the app,\
        Hint: %{public}d eventType: %{public}d", interruptEvent.hintType, interruptEvent.eventType);

    switch (hintType) {
        case INTERRUPT_HINT_RESUME:
            if (audioStream_->GetState() != PAUSED || !isForcePaused_) {
                AUDIO_DEBUG_LOG("AudioRendererPrivate::OnInterrupt state is not paused or not forced paused");
                return;
            }
            isForcePaused_ = false;
            NotifyForcePausedToResume(interruptEvent);
            return;
        case INTERRUPT_HINT_PAUSE:
            if (audioStream_->GetState() != RUNNING) {
                AUDIO_DEBUG_LOG("AudioCapturerPrivate::OnInterrupt state is not running no need to pause");
                return;
            }
            (void)audioStream_->PauseAudioStream(); // Just Pause, do not deactivate here
            isForcePaused_ = true;
            break;
        case INTERRUPT_HINT_STOP:
            (void)audioStream_->StopAudioStream();
            break;
        default:
            break;
    }
    // Notify valid forced event callbacks to app
    InterruptEvent interruptEventForced {interruptEvent.eventType, interruptEvent.forceType, interruptEvent.hintType};
    NotifyEvent(interruptEventForced);
}

void AudioCapturerInterruptCallbackImpl::OnInterrupt(const InterruptEventInternal &interruptEvent)
{
    cb_ = callback_.lock();
    InterruptForceType forceType = interruptEvent.forceType;
    AUDIO_DEBUG_LOG("AudioCapturerInterruptCallbackImpl::OnInterrupt InterruptForceType: %{public}d", forceType);

    if (forceType != INTERRUPT_FORCE) { // INTERRUPT_SHARE
        AUDIO_DEBUG_LOG("AudioCapturerPrivate ForceType: INTERRUPT_SHARE. Let app handle the event");
        InterruptEvent interruptEventShared {interruptEvent.eventType, interruptEvent.forceType,
                                             interruptEvent.hintType};
        NotifyEvent(interruptEventShared);
        return;
    }

    if (audioStream_ == nullptr) {
        AUDIO_DEBUG_LOG("AudioCapturerInterruptCallbackImpl::Stream is not alive. No need to take forced action");
        return;
    }

    HandleAndNotifyForcedEvent(interruptEvent);
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

AudioStreamType AudioCapturer::FindStreamTypeBySourceType(SourceType sourceType)
{
    switch (sourceType) {
        case SOURCE_TYPE_VOICE_COMMUNICATION:
            return STREAM_VOICE_CALL;
        case SOURCE_TYPE_WAKEUP:
            return STREAM_WAKEUP;
        default:
            return STREAM_MUSIC;
    }
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

void AudioCapturerPrivate::SetValid(bool valid)
{
    std::lock_guard<std::mutex> lock(lock_);
    isValid_ = valid;
}

int64_t AudioCapturerPrivate::GetFramesRead() const
{
    return audioStream_->GetFramesRead();
}
}  // namespace AudioStandard
}  // namespace OHOS
