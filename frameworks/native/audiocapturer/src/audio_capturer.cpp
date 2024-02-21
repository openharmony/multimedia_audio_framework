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
static constexpr uid_t UID_MSDP_SA = 6699;
std::map<AudioStreamType, SourceType> AudioCapturerPrivate::streamToSource_ = {
    {AudioStreamType::STREAM_MUSIC, SourceType::SOURCE_TYPE_MIC},
    {AudioStreamType::STREAM_MEDIA, SourceType::SOURCE_TYPE_MIC},
    {AudioStreamType::STREAM_VOICE_CALL, SourceType::SOURCE_TYPE_VOICE_COMMUNICATION},
    {AudioStreamType::STREAM_ULTRASONIC, SourceType::SOURCE_TYPE_ULTRASONIC},
    {AudioStreamType::STREAM_WAKEUP, SourceType::SOURCE_TYPE_WAKEUP},
    {AudioStreamType::STREAM_SOURCE_VOICE_CALL, SourceType::SOURCE_TYPE_VOICE_CALL},
};

AudioCapturer::~AudioCapturer() = default;

AudioCapturerPrivate::~AudioCapturerPrivate()
{
    AUDIO_INFO_LOG("~AudioCapturerPrivate");
    if (audioStream_ != nullptr) {
        audioStream_->ReleaseAudioStream(true);
        audioStream_ = nullptr;
    }
    if (audioStateChangeCallback_ != nullptr) {
        audioStateChangeCallback_->HandleCapturerDestructor();
    }
}

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
    CHECK_AND_RETURN_RET_LOG(sourceType >= SOURCE_TYPE_MIC && sourceType <= SOURCE_TYPE_MAX, nullptr,
        "Invalid source type %{public}d!", sourceType);

    CHECK_AND_RETURN_RET_LOG(sourceType != SOURCE_TYPE_ULTRASONIC || getuid() == UID_MSDP_SA, nullptr,
        "Create failed: SOURCE_TYPE_ULTRASONIC can only be used by MSDP");

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
        AUDIO_WARNING_LOG("audioStreamType is invalid!");
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
        AUDIO_INFO_LOG("create normal stream for old mode.");
    }

    capturerProxyObj_ = std::make_shared<AudioCapturerProxyObj>();
    if (!capturerProxyObj_) {
        AUDIO_WARNING_LOG("AudioCapturerProxyObj Memory Allocation Failed !!");
    }
}

int32_t AudioCapturerPrivate::InitPlaybackCapturer(int32_t type, const AudioPlaybackCaptureConfig &config)
{
    if (type != SOURCE_TYPE_PLAYBACK_CAPTURE) {
        return SUCCESS;
    }
    return AudioPolicyManager::GetInstance().SetPlaybackCapturerFilterInfos(config, appInfo_.appTokenId);
}

int32_t AudioCapturerPrivate::SetCaptureSilentState(bool state)
{
    return AudioPolicyManager::GetInstance().SetCaptureSilentState(state);
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

    bool checkRecordingCreate = audioStream_->CheckRecordingCreate(appInfo_.appTokenId, appInfo_.appFullTokenId,
        appInfo_.appUid, capturerInfo_.sourceType);
    CHECK_AND_RETURN_RET_LOG((capturerInfo_.sourceType == SOURCE_TYPE_VIRTUAL_CAPTURE) ||
        checkRecordingCreate, ERR_PERMISSION_DENIED, "recording create check failed");

    const AudioCapturer *capturer = this;
    capturerProxyObj_->SaveCapturerObj(capturer);

    audioStream_->SetCapturerInfo(capturerInfo_);

    audioStream_->SetClientID(appInfo_.appPid, appInfo_.appUid, appInfo_.appTokenId);

    if (capturerInfo_.sourceType == SOURCE_TYPE_PLAYBACK_CAPTURE) {
        audioStream_->SetInnerCapturerState(true);
    } else if (capturerInfo_.sourceType == SourceType::SOURCE_TYPE_WAKEUP) {
        audioStream_->SetWakeupCapturerState(true);
    }

    audioStream_->SetCapturerSource(capturerInfo_.sourceType);

    int32_t ret = audioStream_->SetAudioStreamInfo(audioStreamParams, capturerProxyObj_);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "SetAudioStreamInfo Failed");

    RegisterCapturerPolicyServiceDiedCallback();

    return InitAudioInterruptCallback();
}

int32_t AudioCapturerPrivate::InitAudioInterruptCallback()
{
    if (audioStream_->GetAudioSessionID(sessionID_) != 0) {
        AUDIO_ERR_LOG("GetAudioSessionID failed for INDEPENDENT_MODE");
        return ERR_INVALID_INDEX;
    }
    audioInterrupt_.sessionID = sessionID_;
    audioInterrupt_.pid = appInfo_.appPid;
    audioInterrupt_.audioFocusType.sourceType = capturerInfo_.sourceType;
    if (audioInterrupt_.audioFocusType.sourceType == SOURCE_TYPE_VIRTUAL_CAPTURE) {
        isVoiceCallCapturer_ = true;
        audioInterrupt_.audioFocusType.sourceType = SOURCE_TYPE_VOICE_COMMUNICATION;
    }
    if (audioInterruptCallback_ == nullptr) {
        audioInterruptCallback_ = std::make_shared<AudioCapturerInterruptCallbackImpl>(audioStream_);
        CHECK_AND_RETURN_RET_LOG(audioInterruptCallback_ != nullptr, ERROR,
            "Failed to allocate memory for audioInterruptCallback_");
    }
    return AudioPolicyManager::GetInstance().SetAudioInterruptCallback(sessionID_, audioInterruptCallback_);
}

int32_t AudioCapturerPrivate::SetCapturerCallback(const std::shared_ptr<AudioCapturerCallback> &callback)
{
    // If the client is using the deprecated SetParams API. SetCapturerCallback must be invoked, after SetParams.
    // In general, callbacks can only be set after the capturer state is  PREPARED.
    CapturerState state = GetStatus();
    CHECK_AND_RETURN_RET_LOG(state != CAPTURER_NEW && state != CAPTURER_RELEASED, ERR_ILLEGAL_STATE,
        "SetCapturerCallback ncorrect state:%{public}d to register cb", state);
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM,
        "SetCapturerCallback callback param is null");

    // Save reference for interrupt callback
    CHECK_AND_RETURN_RET_LOG(audioInterruptCallback_ != nullptr, ERROR,
        "SetCapturerCallback audioInterruptCallback_ == nullptr");
    std::shared_ptr<AudioCapturerInterruptCallbackImpl> cbInterrupt =
        std::static_pointer_cast<AudioCapturerInterruptCallbackImpl>(audioInterruptCallback_);
    cbInterrupt->SaveCallback(callback);

    // Save and Set reference for stream callback. Order is important here.
    if (audioStreamCallback_ == nullptr) {
        audioStreamCallback_ = std::make_shared<AudioStreamCallbackCapturer>();
        CHECK_AND_RETURN_RET_LOG(audioStreamCallback_ != nullptr, ERROR,
            "Failed to allocate memory for audioStreamCallback_");
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
    CHECK_AND_RETURN_RET_LOG((callback != nullptr) && (markPosition > 0), ERR_INVALID_PARAM,
        "input param is invalid");

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
    CHECK_AND_RETURN_RET_LOG((callback != nullptr) && (frameNumber > 0), ERR_INVALID_PARAM,
        "input param is invalid");

    audioStream_->SetCapturerPeriodPositionCallback(frameNumber, callback);

    return SUCCESS;
}

void AudioCapturerPrivate::UnsetCapturerPeriodPositionCallback()
{
    audioStream_->UnsetCapturerPeriodPositionCallback();
}

bool AudioCapturerPrivate::Start() const
{
    AUDIO_INFO_LOG("AudioCapturer::Start %{public}u", sessionID_);
    bool recordingStateChange = audioStream_->CheckRecordingStateChange(appInfo_.appTokenId, appInfo_.appFullTokenId,
        appInfo_.appUid, AUDIO_PERMISSION_START);
    CHECK_AND_RETURN_RET_LOG(recordingStateChange, false, "recording start check failed");

    CHECK_AND_RETURN_RET(audioInterrupt_.audioFocusType.sourceType != SOURCE_TYPE_INVALID &&
        audioInterrupt_.sessionID != INVALID_SESSION_ID, false);

    AUDIO_INFO_LOG("sourceType: %{public}d, sessionID: %{public}d",
        audioInterrupt_.audioFocusType.sourceType, audioInterrupt_.sessionID);
    int32_t ret = AudioPolicyManager::GetInstance().ActivateAudioInterrupt(audioInterrupt_);
    CHECK_AND_RETURN_RET_LOG(ret == 0, false, "ActivateAudioInterrupt Failed");

    // When the cellular call stream is starting, only need to activate audio interrupt.
    CHECK_AND_RETURN_RET(!isVoiceCallCapturer_, true);

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
    AUDIO_INFO_LOG("AudioCapturer::Pause %{public}u", sessionID_);
    if (!audioStream_->CheckRecordingStateChange(appInfo_.appTokenId, appInfo_.appFullTokenId,
        appInfo_.appUid, AUDIO_PERMISSION_STOP)) {
        AUDIO_WARNING_LOG("Pause monitor permission failed");
    }

    // When user is intentionally pausing , Deactivate to remove from audio focus info list
    int32_t ret = AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt_);
    if (ret != 0) {
        AUDIO_WARNING_LOG("AudioRenderer: DeactivateAudioInterrupt Failed");
    }

    // When the cellular call stream is pausing, only need to deactivate audio interrupt.
    CHECK_AND_RETURN_RET(!isVoiceCallCapturer_, true);
    return audioStream_->PauseAudioStream();
}

bool AudioCapturerPrivate::Stop() const
{
    AUDIO_INFO_LOG("AudioCapturer::Stop %{public}u", sessionID_);
    if (!audioStream_->CheckRecordingStateChange(appInfo_.appTokenId, appInfo_.appFullTokenId,
        appInfo_.appUid, AUDIO_PERMISSION_STOP)) {
        AUDIO_WARNING_LOG("Stop monitor permission failed");
    }

    int32_t ret = AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt_);
    if (ret != 0) {
        AUDIO_WARNING_LOG("AudioCapturer: DeactivateAudioInterrupt Failed");
    }

    CHECK_AND_RETURN_RET(isVoiceCallCapturer_ != true, true);

    return audioStream_->StopAudioStream();
}

bool AudioCapturerPrivate::Flush() const
{
    AUDIO_INFO_LOG("AudioCapturer::Flush");
    return audioStream_->FlushAudioStream();
}

bool AudioCapturerPrivate::Release()
{
    AUDIO_INFO_LOG("AudioCapturer::Release %{public}u", sessionID_);
    abortRestore_ = true;
    std::lock_guard<std::mutex> lock(lock_);
    CHECK_AND_RETURN_RET_LOG(isValid_, false, "Release when capturer invalid");

    if (!audioStream_->CheckRecordingStateChange(appInfo_.appTokenId, appInfo_.appFullTokenId,
        appInfo_.appUid, AUDIO_PERMISSION_STOP)) {
        AUDIO_WARNING_LOG("Release monitor permission failed");
    }

    (void)AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt_);

    // Unregister the callaback in policy server
    (void)AudioPolicyManager::GetInstance().UnsetAudioInterruptCallback(sessionID_);

    RemoveCapturerPolicyServiceDiedCallback();

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
    CHECK_AND_RETURN_RET_LOG(bufferDuration >= MINIMUM_BUFFER_SIZE_MSEC && bufferDuration <= MAXIMUM_BUFFER_SIZE_MSEC,
        ERR_INVALID_PARAM, "Error: Please set the buffer duration between 5ms ~ 20ms");
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
    AUDIO_DEBUG_LOG("AudioCapturerInterruptCallbackImpl constructor");
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
    AUDIO_INFO_LOG("NotifyEvent: Hint: %{public}d, eventType: %{public}d",
        interruptEvent.hintType, interruptEvent.eventType);

    if (cb_ != nullptr) {
        cb_->OnInterrupt(interruptEvent);
        AUDIO_DEBUG_LOG("OnInterrupt : NotifyEvent to app complete");
    } else {
        AUDIO_DEBUG_LOG("cb_ == nullptr cannont NotifyEvent to app");
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
    AUDIO_DEBUG_LOG("Force handle the event and notify the app,\
        Hint: %{public}d eventType: %{public}d", interruptEvent.hintType, interruptEvent.eventType);

    switch (hintType) {
        case INTERRUPT_HINT_RESUME:
            CHECK_AND_RETURN_LOG(audioStream_->GetState() == PAUSED && isForcePaused_ == true,
                "OnInterrupt state is not paused or not forced paused");
            isForcePaused_ = false;
            NotifyForcePausedToResume(interruptEvent);
            return;
        case INTERRUPT_HINT_PAUSE:
            CHECK_AND_RETURN_LOG(audioStream_->GetState() == RUNNING,
                "OnInterrupt state is not running no need to pause");
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
    AUDIO_DEBUG_LOG("InterruptForceType: %{public}d", forceType);

    if (forceType != INTERRUPT_FORCE) { // INTERRUPT_SHARE
        AUDIO_DEBUG_LOG("AudioCapturerPrivate ForceType: INTERRUPT_SHARE. Let app handle the event");
        InterruptEvent interruptEventShared {interruptEvent.eventType, interruptEvent.forceType,
                                             interruptEvent.hintType};
        NotifyEvent(interruptEventShared);
        return;
    }

    CHECK_AND_RETURN_LOG(audioStream_ != nullptr,
        "Stream is not alive. No need to take forced action");

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

    CHECK_AND_RETURN_LOG(cb != nullptr, "AudioStreamCallbackCapturer::OnStateChange cb == nullptr.");

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
        case SOURCE_TYPE_VIRTUAL_CAPTURE:
            return STREAM_VOICE_CALL;
        case SOURCE_TYPE_WAKEUP:
            return STREAM_WAKEUP;
        case SOURCE_TYPE_VOICE_CALL:
            return STREAM_SOURCE_VOICE_CALL;
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

int32_t AudioCapturerPrivate::GetCurrentInputDevices(DeviceInfo &deviceInfo) const
{
    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
    uint32_t sessionId = static_cast<uint32_t>(-1);
    int32_t ret = GetAudioStreamId(sessionId);
    CHECK_AND_RETURN_RET_LOG(!ret, ret, "Get sessionId failed");

    ret = AudioPolicyManager::GetInstance().GetCurrentCapturerChangeInfos(audioCapturerChangeInfos);
    CHECK_AND_RETURN_RET_LOG(!ret, ret, "Get current capturer devices failed");

    for (auto it = audioCapturerChangeInfos.begin(); it != audioCapturerChangeInfos.end(); it++) {
        if ((*it)->sessionId == static_cast<int32_t>(sessionId)) {
            deviceInfo = (*it)->inputDeviceInfo;
        }
    }
    return SUCCESS;
}

int32_t AudioCapturerPrivate::GetCurrentCapturerChangeInfo(AudioCapturerChangeInfo &changeInfo) const
{
    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
    uint32_t sessionId = static_cast<uint32_t>(-1);
    int32_t ret = GetAudioStreamId(sessionId);
    CHECK_AND_RETURN_RET_LOG(!ret, ret, "Get sessionId failed");

    ret = AudioPolicyManager::GetInstance().GetCurrentCapturerChangeInfos(audioCapturerChangeInfos);
    CHECK_AND_RETURN_RET_LOG(!ret, ret, "Get current capturer devices failed");

    for (auto it = audioCapturerChangeInfos.begin(); it != audioCapturerChangeInfos.end(); it++) {
        if ((*it)->sessionId == static_cast<int32_t>(sessionId)) {
            changeInfo = *(*it);
        }
    }
    return SUCCESS;
}

std::vector<sptr<MicrophoneDescriptor>> AudioCapturerPrivate::GetCurrentMicrophones() const
{
    uint32_t sessionId = static_cast<uint32_t>(-1);
    GetAudioStreamId(sessionId);
    return AudioPolicyManager::GetInstance().GetAudioCapturerMicrophoneDescriptors(sessionId);
}

int32_t AudioCapturerPrivate::SetAudioCapturerDeviceChangeCallback(
    const std::shared_ptr<AudioCapturerDeviceChangeCallback> &callback)
{
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERROR, "Callback is null");

    if (RegisterAudioCapturerEventListener() != SUCCESS) {
        return ERROR;
    }

    CHECK_AND_RETURN_RET_LOG(audioStateChangeCallback_ != nullptr, ERROR, "audioStateChangeCallback_ is null");
    audioStateChangeCallback_->SaveDeviceChangeCallback(callback);
    return SUCCESS;
}

int32_t AudioCapturerPrivate::RemoveAudioCapturerDeviceChangeCallback(
    const std::shared_ptr<AudioCapturerDeviceChangeCallback> &callback)
{
    CHECK_AND_RETURN_RET_LOG(audioStateChangeCallback_ != nullptr, ERROR, "audioStateChangeCallback_ is null");

    audioStateChangeCallback_->RemoveDeviceChangeCallback(callback);
    if (UnregisterAudioCapturerEventListener() != SUCCESS) {
        return ERROR;
    }
    return SUCCESS;
}

bool AudioCapturerPrivate::IsDeviceChanged(DeviceInfo &newDeviceInfo)
{
    bool deviceUpdated = false;
    DeviceInfo deviceInfo = {};

    CHECK_AND_RETURN_RET_LOG(GetCurrentInputDevices(deviceInfo) == SUCCESS, deviceUpdated,
        "GetCurrentInputDevices failed");

    if (currentDeviceInfo_.deviceType != deviceInfo.deviceType) {
        currentDeviceInfo_ = deviceInfo;
        newDeviceInfo = currentDeviceInfo_;
        deviceUpdated = true;
    }
    return deviceUpdated;
}

void AudioCapturerPrivate::GetAudioInterrupt(AudioInterrupt &audioInterrupt)
{
    audioInterrupt = audioInterrupt_;
}

int32_t AudioCapturerPrivate::RegisterAudioCapturerEventListener()
{
    if (!audioStateChangeCallback_) {
        audioStateChangeCallback_ = std::make_shared<AudioCapturerStateChangeCallbackImpl>();
        CHECK_AND_RETURN_RET_LOG(audioStateChangeCallback_, ERROR, "Memory allocation failed!!");

        int32_t ret =
            AudioPolicyManager::GetInstance().RegisterAudioCapturerEventListener(getpid(), audioStateChangeCallback_);
        CHECK_AND_RETURN_RET_LOG(ret == 0, ERROR, "RegisterAudioCapturerEventListener failed");
        audioStateChangeCallback_->setAudioCapturerObj(this);
    }
    return SUCCESS;
}

int32_t AudioCapturerPrivate::UnregisterAudioCapturerEventListener()
{
    CHECK_AND_RETURN_RET_LOG(audioStateChangeCallback_ != nullptr, ERROR, "audioStateChangeCallback_ is null");
    if (audioStateChangeCallback_->DeviceChangeCallbackArraySize() == 0 &&
        audioStateChangeCallback_->GetCapturerInfoChangeCallbackArraySize() == 0) {
        int32_t ret =
            AudioPolicyManager::GetInstance().UnregisterAudioCapturerEventListener(getpid());
        CHECK_AND_RETURN_RET_LOG(ret == 0, ERROR, "failed");
        audioStateChangeCallback_ = nullptr;
    }
    return SUCCESS;
}

int32_t AudioCapturerPrivate::SetAudioCapturerInfoChangeCallback(
    const std::shared_ptr<AudioCapturerInfoChangeCallback> &callback)
{
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM, "Callback is null");

    CHECK_AND_RETURN_RET(RegisterAudioCapturerEventListener() == SUCCESS, ERROR);

    CHECK_AND_RETURN_RET_LOG(audioStateChangeCallback_ != nullptr, ERROR, "audioStateChangeCallback_ is null");
    audioStateChangeCallback_->SaveCapturerInfoChangeCallback(callback);
    return SUCCESS;
}

int32_t AudioCapturerPrivate::RemoveAudioCapturerInfoChangeCallback(
    const std::shared_ptr<AudioCapturerInfoChangeCallback> &callback)
{
    CHECK_AND_RETURN_RET_LOG(audioStateChangeCallback_ != nullptr, ERROR, "audioStateChangeCallback_ is null");
    audioStateChangeCallback_->RemoveCapturerInfoChangeCallback(callback);
    CHECK_AND_RETURN_RET(UnregisterAudioCapturerEventListener() == SUCCESS, ERROR);
    return SUCCESS;
}

int32_t AudioCapturerPrivate::RegisterCapturerPolicyServiceDiedCallback()
{
    AUDIO_DEBUG_LOG("AudioCapturerPrivate::SetCapturerPolicyServiceDiedCallback");
    if (!audioPolicyServiceDiedCallback_) {
        audioPolicyServiceDiedCallback_ = std::make_shared<CapturerPolicyServiceDiedCallback>();
        if (!audioPolicyServiceDiedCallback_) {
            AUDIO_ERR_LOG("Memory allocation failed!!");
            return ERROR;
        }
        audioStream_->RegisterRendererOrCapturerPolicyServiceDiedCB(audioPolicyServiceDiedCallback_);
        audioPolicyServiceDiedCallback_->SetAudioCapturerObj(this);
        audioPolicyServiceDiedCallback_->SetAudioInterrupt(audioInterrupt_);
    }
    return SUCCESS;
}

int32_t AudioCapturerPrivate::RemoveCapturerPolicyServiceDiedCallback()
{
    AUDIO_DEBUG_LOG("AudioCapturerPrivate::RemoveCapturerPolicyServiceDiedCallback");
    if (audioPolicyServiceDiedCallback_) {
        int32_t ret = audioStream_->RemoveRendererOrCapturerPolicyServiceDiedCB();
        if (ret != 0) {
            AUDIO_ERR_LOG("RemoveCapturerPolicyServiceDiedCallback failed");
            return ERROR;
        }
    }
    return SUCCESS;
}

AudioCapturerStateChangeCallbackImpl::AudioCapturerStateChangeCallbackImpl()
{
    AUDIO_DEBUG_LOG("AudioCapturerStateChangeCallbackImpl instance create");
}

AudioCapturerStateChangeCallbackImpl::~AudioCapturerStateChangeCallbackImpl()
{
    AUDIO_DEBUG_LOG("AudioCapturerStateChangeCallbackImpl instance destory");
}

void AudioCapturerStateChangeCallbackImpl::SaveCapturerInfoChangeCallback(
    const std::shared_ptr<AudioCapturerInfoChangeCallback> &callback)
{
    auto iter = find(capturerInfoChangeCallbacklist_.begin(), capturerInfoChangeCallbacklist_.end(), callback);
    if (iter == capturerInfoChangeCallbacklist_.end()) {
        capturerInfoChangeCallbacklist_.emplace_back(callback);
    }
}

void AudioCapturerStateChangeCallbackImpl::RemoveCapturerInfoChangeCallback(
    const std::shared_ptr<AudioCapturerInfoChangeCallback> &callback)
{
    if (callback == nullptr) {
        capturerInfoChangeCallbacklist_.clear();
        return;
    }

    auto iter = find(capturerInfoChangeCallbacklist_.begin(), capturerInfoChangeCallbacklist_.end(), callback);
    if (iter != capturerInfoChangeCallbacklist_.end()) {
        capturerInfoChangeCallbacklist_.erase(iter);
    }
}

int32_t AudioCapturerStateChangeCallbackImpl::GetCapturerInfoChangeCallbackArraySize()
{
    return capturerInfoChangeCallbacklist_.size();
}

void AudioCapturerStateChangeCallbackImpl::SaveDeviceChangeCallback(
    const std::shared_ptr<AudioCapturerDeviceChangeCallback> &callback)
{
    auto iter = find(deviceChangeCallbacklist_.begin(), deviceChangeCallbacklist_.end(), callback);
    if (iter == deviceChangeCallbacklist_.end()) {
        deviceChangeCallbacklist_.emplace_back(callback);
    }
}

void AudioCapturerStateChangeCallbackImpl::RemoveDeviceChangeCallback(
    const std::shared_ptr<AudioCapturerDeviceChangeCallback> &callback)
{
    if (callback == nullptr) {
        deviceChangeCallbacklist_.clear();
        return;
    }

    auto iter = find(deviceChangeCallbacklist_.begin(), deviceChangeCallbacklist_.end(), callback);
    if (iter != deviceChangeCallbacklist_.end()) {
        deviceChangeCallbacklist_.erase(iter);
    }
}

int32_t AudioCapturerStateChangeCallbackImpl::DeviceChangeCallbackArraySize()
{
    return deviceChangeCallbacklist_.size();
}

void AudioCapturerStateChangeCallbackImpl::setAudioCapturerObj(AudioCapturerPrivate *capturerObj)
{
    std::lock_guard<std::mutex> lock(capturerMutex_);
    capturer_ = capturerObj;
}

void AudioCapturerStateChangeCallbackImpl::NotifyAudioCapturerInfoChange(
    const std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    uint32_t sessionId = static_cast<uint32_t>(-1);
    bool found = false;
    AudioCapturerChangeInfo capturerChangeInfo;

    {
        std::lock_guard<std::mutex> lock(capturerMutex_);
        CHECK_AND_RETURN_LOG(capturer_ != nullptr, "Bare pointer capturer_ is nullptr");
        int32_t ret = capturer_->GetAudioStreamId(sessionId);
        CHECK_AND_RETURN_LOG(!ret, "Get sessionId failed");
    }

    for (auto it = audioCapturerChangeInfos.begin(); it != audioCapturerChangeInfos.end(); it++) {
        if ((*it)->sessionId == static_cast<int32_t>(sessionId)) {
            capturerChangeInfo = *(*it);
            found = true;
        }
    }

    if (found) {
        for (auto it = capturerInfoChangeCallbacklist_.begin(); it != capturerInfoChangeCallbacklist_.end(); ++it) {
            if (*it != nullptr) {
                (*it)->OnStateChange(capturerChangeInfo);
            }
        }
    }
}

void AudioCapturerStateChangeCallbackImpl::NotifyAudioCapturerDeviceChange(
    const std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    DeviceInfo deviceInfo = {};
    {
        std::lock_guard<std::mutex> lock(capturerMutex_);
        CHECK_AND_RETURN_LOG(capturer_ != nullptr, "Bare pointer capturer_ is nullptr");
        CHECK_AND_RETURN_LOG(capturer_->IsDeviceChanged(deviceInfo), "Device not change, no need callback.");
    }

    for (auto it = deviceChangeCallbacklist_.begin(); it != deviceChangeCallbacklist_.end(); ++it) {
        if (*it != nullptr) {
            (*it)->OnStateChange(deviceInfo);
        }
    }
}

void AudioCapturerStateChangeCallbackImpl::OnCapturerStateChange(
    const std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    if (deviceChangeCallbacklist_.size() != 0) {
        NotifyAudioCapturerDeviceChange(audioCapturerChangeInfos);
    }

    if (capturerInfoChangeCallbacklist_.size() != 0) {
        NotifyAudioCapturerInfoChange(audioCapturerChangeInfos);
    }
}

void AudioCapturerStateChangeCallbackImpl::HandleCapturerDestructor()
{
    std::lock_guard<std::mutex> lock(capturerMutex_);
    capturer_ = nullptr;
}

CapturerPolicyServiceDiedCallback::CapturerPolicyServiceDiedCallback()
{
    AUDIO_DEBUG_LOG("CapturerPolicyServiceDiedCallback create");
}

CapturerPolicyServiceDiedCallback::~CapturerPolicyServiceDiedCallback()
{
    AUDIO_DEBUG_LOG("CapturerPolicyServiceDiedCallback destroy");
    if (restoreThread_ != nullptr && restoreThread_->joinable()) {
        restoreThread_->join();
        restoreThread_.reset();
        restoreThread_ = nullptr;
    }
}

void CapturerPolicyServiceDiedCallback::SetAudioCapturerObj(AudioCapturerPrivate *capturerObj)
{
    capturer_ = capturerObj;
}

void CapturerPolicyServiceDiedCallback::SetAudioInterrupt(AudioInterrupt &audioInterrupt)
{
    audioInterrupt_ = audioInterrupt;
}

void CapturerPolicyServiceDiedCallback::OnAudioPolicyServiceDied()
{
    AUDIO_INFO_LOG("CapturerPolicyServiceDiedCallback OnAudioPolicyServiceDied");
    if (restoreThread_ != nullptr) {
        restoreThread_->detach();
    }
    restoreThread_ = std::make_unique<std::thread>(&CapturerPolicyServiceDiedCallback::RestoreTheadLoop, this);
    pthread_setname_np(restoreThread_->native_handle(), "OS_ACPSRestore");
}

void CapturerPolicyServiceDiedCallback::RestoreTheadLoop()
{
    int32_t tryCounter = 5;
    uint32_t sleepTime = 500000;
    bool result = false;
    int32_t ret = -1;
    while (!result && tryCounter-- > 0) {
        usleep(sleepTime);
        if (capturer_== nullptr || capturer_->audioStream_== nullptr ||
            capturer_->abortRestore_) {
            AUDIO_INFO_LOG("CapturerPolicyServiceDiedCallback RestoreTheadLoop abort restore");
            break;
        }
        result = capturer_->audioStream_->RestoreAudioStream();
        if (!result) {
            AUDIO_ERR_LOG("RestoreAudioStream Failed, %{public}d attempts remaining", tryCounter);
            continue;
        } else {
            capturer_->abortRestore_ = false;
        }

        if (capturer_->GetStatus() == CAPTURER_RUNNING) {
            capturer_->GetAudioInterrupt(audioInterrupt_);
            ret = AudioPolicyManager::GetInstance().ActivateAudioInterrupt(audioInterrupt_);
            if (ret != SUCCESS) {
                AUDIO_ERR_LOG("RestoreTheadLoop ActivateAudioInterrupt Failed");
            }
        }
    }
}
}  // namespace AudioStandard
}  // namespace OHOS
