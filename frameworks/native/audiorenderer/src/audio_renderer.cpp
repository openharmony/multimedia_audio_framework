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

#include "audio_renderer.h"

#include "audio_log.h"
#include "audio_errors.h"
#include "audio_policy_manager.h"
#ifdef OHCORE
#include "audio_renderer_gateway.h"
#include "audio_container_stream_base.h"
#else
#include "audio_stream.h"
#include "audio_renderer_private.h"
#endif

namespace OHOS {
namespace AudioStandard {
#ifndef OHCORE
std::map<pid_t, std::map<AudioStreamType, AudioInterrupt>> AudioRendererPrivate::sharedInterrupts_;
#endif

AudioRenderer::~AudioRenderer() = default;

#ifndef OHCORE
AudioRendererPrivate::~AudioRendererPrivate()
{
    RendererState state = GetStatus();
    if (state != RENDERER_RELEASED && state != RENDERER_NEW) {
        Release();
    }
}
#endif

std::unique_ptr<AudioRenderer> AudioRenderer::Create(AudioStreamType audioStreamType)
{
    AppInfo appInfo = {};
    return Create(audioStreamType, appInfo);
}

std::unique_ptr<AudioRenderer> AudioRenderer::Create(AudioStreamType audioStreamType, const AppInfo &appInfo)
{
#ifdef OHCORE
    return std::make_unique<AudioRendererGateway>(audioStreamType, appInfo);
#else
    return std::make_unique<AudioRendererPrivate>(audioStreamType, appInfo);
#endif
}

std::unique_ptr<AudioRenderer> AudioRenderer::Create(const AudioRendererOptions &rendererOptions)
{
    AppInfo appInfo = {};
    return Create("", rendererOptions, appInfo);
}

std::unique_ptr<AudioRenderer> AudioRenderer::Create(const AudioRendererOptions &rendererOptions,
    const AppInfo &appInfo)
{
    return Create("", rendererOptions, appInfo);
}

std::unique_ptr<AudioRenderer> AudioRenderer::Create(const std::string cachePath,
    const AudioRendererOptions &rendererOptions)
{
    AppInfo appInfo = {};
    return Create(cachePath, rendererOptions, appInfo);
}

std::unique_ptr<AudioRenderer> AudioRenderer::Create(const std::string cachePath,
    const AudioRendererOptions &rendererOptions, const AppInfo &appInfo)
{
    ContentType contentType = rendererOptions.rendererInfo.contentType;
    CHECK_AND_RETURN_RET_LOG(contentType >= CONTENT_TYPE_UNKNOWN && contentType <= CONTENT_TYPE_RINGTONE, nullptr,
                             "Invalid content type");

    StreamUsage streamUsage = rendererOptions.rendererInfo.streamUsage;
    CHECK_AND_RETURN_RET_LOG(streamUsage >= STREAM_USAGE_UNKNOWN && streamUsage <= STREAM_USAGE_NOTIFICATION_RINGTONE,
                             nullptr, "Invalid stream usage");

    AudioStreamType audioStreamType;
#ifdef OHCORE
    audioStreamType = AudioContainerRenderStream::GetStreamType(contentType, streamUsage);
    auto audioRenderer = std::make_unique<AudioRendererGateway>(audioStreamType, appInfo);
#else
    audioStreamType = AudioStream::GetStreamType(contentType, streamUsage);
    auto audioRenderer = std::make_unique<AudioRendererPrivate>(audioStreamType, appInfo);
#endif
    CHECK_AND_RETURN_RET_LOG(audioRenderer != nullptr, nullptr, "Failed to create renderer object");
    if (!cachePath.empty()) {
        AUDIO_DEBUG_LOG("Set application cache path");
        audioRenderer->SetApplicationCachePath(cachePath);
    }

    audioRenderer->rendererInfo_.contentType = contentType;
    audioRenderer->rendererInfo_.streamUsage = streamUsage;
    audioRenderer->rendererInfo_.rendererFlags = rendererOptions.rendererInfo.rendererFlags;

    AudioRendererParams params;
    params.sampleFormat = rendererOptions.streamInfo.format;
    params.sampleRate = rendererOptions.streamInfo.samplingRate;
    params.channelCount = rendererOptions.streamInfo.channels;
    params.encodingType = rendererOptions.streamInfo.encoding;

    if (audioRenderer->SetParams(params) != SUCCESS) {
        AUDIO_ERR_LOG("SetParams failed in renderer");
        audioRenderer = nullptr;
        return nullptr;
    }

    return audioRenderer;
}

#ifndef OHCORE
AudioRendererPrivate::AudioRendererPrivate(AudioStreamType audioStreamType, const AppInfo &appInfo)
{
    appInfo_ = appInfo;
    if (!(appInfo_.appPid)) {
        appInfo_.appPid = getpid();
    }

    if (appInfo_.appUid < 0) {
        appInfo_.appUid = static_cast<int32_t>(getuid());
    }

    audioStream_ = std::make_shared<AudioStream>(audioStreamType, AUDIO_MODE_PLAYBACK, appInfo_.appUid);
    if (audioStream_) {
        AUDIO_DEBUG_LOG("AudioRendererPrivate::Audio stream created");
        // Initializing with default values
        rendererInfo_.contentType = CONTENT_TYPE_MUSIC;
        rendererInfo_.streamUsage = STREAM_USAGE_MEDIA;
    }

    rendererProxyObj_ = std::make_shared<AudioRendererProxyObj>();
    if (!rendererProxyObj_) {
        AUDIO_ERR_LOG("AudioRendererProxyObj Memory Allocation Failed !!");
    }

    audioInterrupt_.streamType = audioStreamType;
    sharedInterrupt_.streamType = audioStreamType;
}

int32_t AudioRendererPrivate::InitAudioInterruptCallback()
{
    AUDIO_INFO_LOG("AudioRendererPrivate::InitAudioInterruptCallback in");
    AudioInterrupt interrupt;
    switch (mode_) {
        case InterruptMode::SHARE_MODE:
            if (InitSharedInterrupt() != 0) {
                AUDIO_ERR_LOG("InitAudioInterruptCallback::GetAudioSessionID failed for SHARE_MODE");
                return ERR_INVALID_INDEX;
            }
            interrupt = sharedInterrupt_;
            break;
        case InterruptMode::INDEPENDENT_MODE:
            if (audioStream_->GetAudioSessionID(audioInterrupt_.sessionID) != 0) {
                AUDIO_ERR_LOG("InitAudioInterruptCallback::GetAudioSessionID failed for INDEPENDENT_MODE");
                return ERR_INVALID_INDEX;
            }
            interrupt = audioInterrupt_;
            break;
        default:
            AUDIO_ERR_LOG("InitAudioInterruptCallback::Invalid interrupt mode!");
            return ERR_INVALID_PARAM;
    }
    sessionID_ = interrupt.sessionID;

    AUDIO_INFO_LOG("InitAudioInterruptCallback::interruptMode %{public}d, streamType %{public}d, sessionID %{public}d",
        mode_, interrupt.streamType, interrupt.sessionID);

    if (audioInterruptCallback_ == nullptr) {
        audioInterruptCallback_ = std::make_shared<AudioInterruptCallbackImpl>(audioStream_, interrupt);
        if (audioInterruptCallback_ == nullptr) {
            AUDIO_ERR_LOG("InitAudioInterruptCallback::Failed to allocate memory for audioInterruptCallback_");
            return ERROR;
        }
    }
    return AudioPolicyManager::GetInstance().SetAudioInterruptCallback(sessionID_, audioInterruptCallback_);
}

int32_t AudioRendererPrivate::InitSharedInterrupt()
{
    if (AudioRendererPrivate::sharedInterrupts_.find(appInfo_.appPid) ==
        AudioRendererPrivate::sharedInterrupts_.end()) {
        AUDIO_INFO_LOG("InitSharedInterrupt: appInfo_.appPid %{public}d create new sharedInterrupt", appInfo_.appPid);
        std::map<AudioStreamType, AudioInterrupt> interrupts;
        std::vector<AudioStreamType> types;
        types.push_back(AudioStreamType::STREAM_DEFAULT);
        types.push_back(AudioStreamType::STREAM_VOICE_CALL);
        types.push_back(AudioStreamType::STREAM_MUSIC);
        types.push_back(AudioStreamType::STREAM_RING);
        types.push_back(AudioStreamType::STREAM_MEDIA);
        types.push_back(AudioStreamType::STREAM_VOICE_ASSISTANT);
        types.push_back(AudioStreamType::STREAM_SYSTEM);
        types.push_back(AudioStreamType::STREAM_ALARM);
        types.push_back(AudioStreamType::STREAM_NOTIFICATION);
        types.push_back(AudioStreamType::STREAM_BLUETOOTH_SCO);
        types.push_back(AudioStreamType::STREAM_ENFORCED_AUDIBLE);
        types.push_back(AudioStreamType::STREAM_DTMF);
        types.push_back(AudioStreamType::STREAM_TTS);
        types.push_back(AudioStreamType::STREAM_ACCESSIBILITY);
        for (auto type : types) {
            uint32_t interruptId;
            if (audioStream_->GetAudioSessionID(interruptId) != 0) {
                AUDIO_ERR_LOG("AudioRendererPrivate::GetAudioSessionID interruptId Failed");
                return ERR_INVALID_INDEX;
            }
            AudioInterrupt interrupt = {STREAM_USAGE_UNKNOWN, CONTENT_TYPE_UNKNOWN, sharedInterrupt_.streamType,
                interruptId};
            interrupts.insert(std::make_pair(type, interrupt));
        }
        AudioRendererPrivate::sharedInterrupts_.insert(std::make_pair(appInfo_.appPid, interrupts));
    } else {
        AUDIO_INFO_LOG("InitSharedInterrupt: sharedInterrupt of appInfo_.appPid %{public}d existed", appInfo_.appPid);
    }

    sharedInterrupt_ = AudioRendererPrivate::sharedInterrupts_.find(appInfo_.appPid)
        ->second.find(sharedInterrupt_.streamType)->second;
    return SUCCESS;
}

int32_t AudioRendererPrivate::GetFrameCount(uint32_t &frameCount) const
{
    return audioStream_->GetFrameCount(frameCount);
}

int32_t AudioRendererPrivate::GetLatency(uint64_t &latency) const
{
    return audioStream_->GetLatency(latency);
}

int32_t AudioRendererPrivate::SetParams(const AudioRendererParams params)
{
    AudioStreamParams audioStreamParams;
    AudioRenderer *renderer = this;
    rendererProxyObj_->SaveRendererObj(renderer);
    audioStream_->SetRendererInfo(rendererInfo_);

    audioStreamParams.format = params.sampleFormat;
    audioStreamParams.samplingRate = params.sampleRate;
    audioStreamParams.channels = params.channelCount;
    audioStreamParams.encoding = params.encodingType;

    audioStream_->SetClientID(appInfo_.appPid, appInfo_.appUid);

    int32_t ret = audioStream_->SetAudioStreamInfo(audioStreamParams, rendererProxyObj_);
    if (ret) {
        AUDIO_ERR_LOG("AudioRendererPrivate::SetParams SetAudioStreamInfo Failed");
        return ret;
    }
    AUDIO_INFO_LOG("AudioRendererPrivate::SetParams SetAudioStreamInfo Succeeded");

    return InitAudioInterruptCallback();
}

int32_t AudioRendererPrivate::GetParams(AudioRendererParams &params) const
{
    AudioStreamParams audioStreamParams;
    int32_t result = audioStream_->GetAudioStreamInfo(audioStreamParams);
    if (!result) {
        params.sampleFormat = static_cast<AudioSampleFormat>(audioStreamParams.format);
        params.sampleRate = static_cast<AudioSamplingRate>(audioStreamParams.samplingRate);
        params.channelCount = static_cast<AudioChannel>(audioStreamParams.channels);
        params.encodingType = static_cast<AudioEncodingType>(audioStreamParams.encoding);
    }

    return result;
}

int32_t AudioRendererPrivate::GetRendererInfo(AudioRendererInfo &rendererInfo) const
{
    rendererInfo = rendererInfo_;

    return SUCCESS;
}

int32_t AudioRendererPrivate::GetStreamInfo(AudioStreamInfo &streamInfo) const
{
    AudioStreamParams audioStreamParams;
    int32_t result = audioStream_->GetAudioStreamInfo(audioStreamParams);
    if (!result) {
        streamInfo.format = static_cast<AudioSampleFormat>(audioStreamParams.format);
        streamInfo.samplingRate = static_cast<AudioSamplingRate>(audioStreamParams.samplingRate);
        streamInfo.channels = static_cast<AudioChannel>(audioStreamParams.channels);
        streamInfo.encoding = static_cast<AudioEncodingType>(audioStreamParams.encoding);
    }

    return result;
}

int32_t AudioRendererPrivate::SetRendererCallback(const std::shared_ptr<AudioRendererCallback> &callback)
{
    // If the client is using the deprecated SetParams API. SetRendererCallback must be invoked, after SetParams.
    // In general, callbacks can only be set after the renderer state is PREPARED.
    RendererState state = GetStatus();
    if (state == RENDERER_NEW || state == RENDERER_RELEASED) {
        AUDIO_DEBUG_LOG("AudioRendererPrivate::SetRendererCallback incorrect state:%{public}d to register cb", state);
        return ERR_ILLEGAL_STATE;
    }
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioRendererPrivate::SetRendererCallback callback param is null");
        return ERR_INVALID_PARAM;
    }

    // Save reference for interrupt callback
    if (audioInterruptCallback_ == nullptr) {
        AUDIO_ERR_LOG("AudioRendererPrivate::SetRendererCallback audioInterruptCallback_ == nullptr");
        return ERROR;
    }
    std::shared_ptr<AudioInterruptCallbackImpl> cbInterrupt =
        std::static_pointer_cast<AudioInterruptCallbackImpl>(audioInterruptCallback_);
    cbInterrupt->SaveCallback(callback);

    // Save and Set reference for stream callback. Order is important here.
    if (audioStreamCallback_ == nullptr) {
        audioStreamCallback_ = std::make_shared<AudioStreamCallbackRenderer>();
        if (audioStreamCallback_ == nullptr) {
            AUDIO_ERR_LOG("AudioRendererPrivate::Failed to allocate memory for audioStreamCallback_");
            return ERROR;
        }
    }
    std::shared_ptr<AudioStreamCallbackRenderer> cbStream =
        std::static_pointer_cast<AudioStreamCallbackRenderer>(audioStreamCallback_);
    cbStream->SaveCallback(callback);
    (void)audioStream_->SetStreamCallback(audioStreamCallback_);

    return SUCCESS;
}

int32_t AudioRendererPrivate::SetRendererPositionCallback(int64_t markPosition,
    const std::shared_ptr<RendererPositionCallback> &callback)
{
    if ((callback == nullptr) || (markPosition <= 0)) {
        AUDIO_ERR_LOG("AudioRendererPrivate::SetRendererPositionCallback input param is invalid");
        return ERR_INVALID_PARAM;
    }

    audioStream_->SetRendererPositionCallback(markPosition, callback);

    return SUCCESS;
}

void AudioRendererPrivate::UnsetRendererPositionCallback()
{
    audioStream_->UnsetRendererPositionCallback();
}

int32_t AudioRendererPrivate::SetRendererPeriodPositionCallback(int64_t frameNumber,
    const std::shared_ptr<RendererPeriodPositionCallback> &callback)
{
    if ((callback == nullptr) || (frameNumber <= 0)) {
        AUDIO_ERR_LOG("AudioRendererPrivate::SetRendererPeriodPositionCallback input param is invalid");
        return ERR_INVALID_PARAM;
    }

    audioStream_->SetRendererPeriodPositionCallback(frameNumber, callback);

    return SUCCESS;
}

void AudioRendererPrivate::UnsetRendererPeriodPositionCallback()
{
    audioStream_->UnsetRendererPeriodPositionCallback();
}

bool AudioRendererPrivate::Start(StateChangeCmdType cmdType) const
{
    AUDIO_INFO_LOG("AudioRenderer::Start");
    RendererState state = GetStatus();
    if ((state != RENDERER_PREPARED) && (state != RENDERER_STOPPED) && (state != RENDERER_PAUSED)) {
        AUDIO_ERR_LOG("AudioRendererPrivate::Start() Illegal state:%{public}u, Start failed", state);
        return false;
    }

    AudioInterrupt audioInterrupt;
    switch (mode_) {
        case InterruptMode::SHARE_MODE:
            audioInterrupt = sharedInterrupt_;
            break;
        case InterruptMode::INDEPENDENT_MODE:
            audioInterrupt = audioInterrupt_;
            break;
        default:
            break;
    }
    AUDIO_INFO_LOG("AudioRenderer::Start::interruptMode: %{public}d, streamType: %{public}d, sessionID: %{public}d",
        mode_, audioInterrupt.streamType, audioInterrupt.sessionID);

    if (audioInterrupt.streamType == STREAM_DEFAULT || audioInterrupt.sessionID == INVALID_SESSION_ID) {
        return false;
    }

    int32_t ret = AudioPolicyManager::GetInstance().ActivateAudioInterrupt(audioInterrupt);
    if (ret != 0) {
        AUDIO_ERR_LOG("AudioRendererPrivate::ActivateAudioInterrupt Failed");
        return false;
    }

    return audioStream_->StartAudioStream(cmdType);
}

int32_t AudioRendererPrivate::Write(uint8_t *buffer, size_t bufferSize)
{
    return audioStream_->Write(buffer, bufferSize);
}

RendererState AudioRendererPrivate::GetStatus() const
{
    return static_cast<RendererState>(audioStream_->GetState());
}

bool AudioRendererPrivate::GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) const
{
    return audioStream_->GetAudioTime(timestamp, base);
}

bool AudioRendererPrivate::Drain() const
{
    return audioStream_->DrainAudioStream();
}

bool AudioRendererPrivate::Flush() const
{
    return audioStream_->FlushAudioStream();
}

bool AudioRendererPrivate::Pause(StateChangeCmdType cmdType) const
{
    AUDIO_INFO_LOG("AudioRenderer::Pause");
    bool result = audioStream_->PauseAudioStream(cmdType);
    AudioInterrupt audioInterrupt;
    switch (mode_) {
        case InterruptMode::SHARE_MODE:
            audioInterrupt = sharedInterrupt_;
            break;
        case InterruptMode::INDEPENDENT_MODE:
            audioInterrupt = audioInterrupt_;
            break;
        default:
            break;
    }
    // When user is intentionally pausing , Deactivate to remove from active/pending owners list
    int32_t ret = AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt);
    if (ret != 0) {
        AUDIO_ERR_LOG("AudioRenderer: DeactivateAudioInterrupt Failed");
    }

    return result;
}

bool AudioRendererPrivate::Stop() const
{
    AUDIO_INFO_LOG("AudioRenderer::Stop");
    bool result = audioStream_->StopAudioStream();
    AudioInterrupt audioInterrupt;
    switch (mode_) {
        case InterruptMode::SHARE_MODE:
            audioInterrupt = sharedInterrupt_;
            break;
        case InterruptMode::INDEPENDENT_MODE:
            audioInterrupt = audioInterrupt_;
            break;
        default:
            break;
    }
    int32_t ret = AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt);
    if (ret != 0) {
        AUDIO_ERR_LOG("AudioRenderer: DeactivateAudioInterrupt Failed");
    }

    return result;
}

bool AudioRendererPrivate::Release() const
{
    AUDIO_INFO_LOG("AudioRenderer::Release");
    // If Stop call was skipped, Release to take care of Deactivation
    (void)AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt_);

    // Unregister the callaback in policy server
    (void)AudioPolicyManager::GetInstance().UnsetAudioInterruptCallback(sessionID_);

    return audioStream_->ReleaseAudioStream();
}

int32_t AudioRendererPrivate::GetBufferSize(size_t &bufferSize) const
{
    return audioStream_->GetBufferSize(bufferSize);
}

int32_t AudioRendererPrivate::GetAudioStreamId(uint32_t &sessionID) const
{
    return audioStream_->GetAudioSessionID(sessionID);
}

int32_t AudioRendererPrivate::SetAudioRendererDesc(AudioRendererDesc audioRendererDesc) const
{
    ContentType contentType = audioRendererDesc.contentType;
    StreamUsage streamUsage = audioRendererDesc.streamUsage;
    AudioStreamType audioStreamType = audioStream_->GetStreamType(contentType, streamUsage);
    return audioStream_->SetAudioStreamType(audioStreamType);
}

int32_t AudioRendererPrivate::SetStreamType(AudioStreamType audioStreamType) const
{
    return audioStream_->SetAudioStreamType(audioStreamType);
}

int32_t AudioRendererPrivate::SetVolume(float volume) const
{
    return audioStream_->SetVolume(volume);
}

float AudioRendererPrivate::GetVolume() const
{
    return audioStream_->GetVolume();
}

int32_t AudioRendererPrivate::SetRenderRate(AudioRendererRate renderRate) const
{
    return audioStream_->SetRenderRate(renderRate);
}

AudioRendererRate AudioRendererPrivate::GetRenderRate() const
{
    return audioStream_->GetRenderRate();
}

int32_t AudioRendererPrivate::SetBufferDuration(uint64_t bufferDuration) const
{
    if (bufferDuration < MINIMUM_BUFFER_SIZE_MSEC || bufferDuration > MAXIMUM_BUFFER_SIZE_MSEC) {
        AUDIO_ERR_LOG("Error: Please set the buffer duration between 5ms ~ 20ms");
        return ERR_INVALID_PARAM;
    }

    return audioStream_->SetBufferSizeInMsec(bufferDuration);
}

AudioInterruptCallbackImpl::AudioInterruptCallbackImpl(const std::shared_ptr<AudioStream> &audioStream,
    const AudioInterrupt &audioInterrupt)
    : audioStream_(audioStream), audioInterrupt_(audioInterrupt)
{
    AUDIO_INFO_LOG("AudioInterruptCallbackImpl constructor");
}

AudioInterruptCallbackImpl::~AudioInterruptCallbackImpl()
{
    AUDIO_DEBUG_LOG("AudioInterruptCallbackImpl: instance destroy");
}

void AudioInterruptCallbackImpl::SaveCallback(const std::weak_ptr<AudioRendererCallback> &callback)
{
    callback_ = callback;
}

void AudioInterruptCallbackImpl::NotifyEvent(const InterruptEvent &interruptEvent)
{
    AUDIO_DEBUG_LOG("AudioRendererPrivate: NotifyEvent: Hint: %{public}d", interruptEvent.hintType);
    AUDIO_DEBUG_LOG("AudioRendererPrivate: NotifyEvent: eventType: %{public}d", interruptEvent.eventType);

    if (cb_ != nullptr) {
        cb_->OnInterrupt(interruptEvent);
        AUDIO_DEBUG_LOG("AudioRendererPrivate: OnInterrupt : NotifyEvent to app complete");
    } else {
        AUDIO_DEBUG_LOG("AudioRendererPrivate: cb_ == nullptr cannont NotifyEvent to app");
    }
}

bool AudioInterruptCallbackImpl::HandleForceDucking(const InterruptEventInternal &interruptEvent)
{
    float streamVolume = AudioPolicyManager::GetInstance().GetStreamVolume(audioInterrupt_.streamType);
    float duckVolume = interruptEvent.duckVolume;
    int32_t ret = 0;

    if (streamVolume <= duckVolume || FLOAT_COMPARE_EQ(streamVolume, 0.0f)) {
        AUDIO_INFO_LOG("AudioRendererPrivate: StreamVolume: %{public}f <= duckVolume: %{public}f",
                       streamVolume, duckVolume);
        AUDIO_INFO_LOG("AudioRendererPrivate: No need to duck further return");
        return false;
    }

    instanceVolBeforeDucking_ = audioStream_->GetVolume();
    float duckInstanceVolume = duckVolume / streamVolume;
    if (FLOAT_COMPARE_EQ(instanceVolBeforeDucking_, 0.0f) || instanceVolBeforeDucking_ < duckInstanceVolume) {
        AUDIO_INFO_LOG("AudioRendererPrivate: No need to duck further return");
        return false;
    }

    ret = audioStream_->SetVolume(duckInstanceVolume);
    if (ret) {
        AUDIO_DEBUG_LOG("AudioRendererPrivate: set duckVolume(instance) %{pubic}f failed", duckInstanceVolume);
        return false;
    }

    AUDIO_DEBUG_LOG("AudioRendererPrivate: set duckVolume(instance) %{pubic}f succeeded", duckInstanceVolume);
    return true;
}

void AudioInterruptCallbackImpl::NotifyForcePausedToResume(const InterruptEventInternal &interruptEvent)
{
    // Change InterruptForceType to Share, Since app will take care of resuming
    InterruptEvent interruptEventResume {interruptEvent.eventType, INTERRUPT_SHARE,
                                         interruptEvent.hintType};
    NotifyEvent(interruptEventResume);
}

void AudioInterruptCallbackImpl::HandleAndNotifyForcedEvent(const InterruptEventInternal &interruptEvent)
{
    InterruptHint hintType = interruptEvent.hintType;
    AUDIO_DEBUG_LOG("AudioRendererPrivate ForceType: INTERRUPT_FORCE, Force handle the event and notify the app");
    AUDIO_DEBUG_LOG("AudioRendererPrivate: HandleAndNotifyForcedEvent: Hint: %{public}d eventType: %{public}d",
        interruptEvent.hintType, interruptEvent.eventType);

    switch (hintType) {
        case INTERRUPT_HINT_PAUSE:
            if (audioStream_->GetState() != RUNNING) {
                AUDIO_DEBUG_LOG("AudioRendererPrivate::OnInterrupt state is not running no need to pause");
                return;
            }
            (void)audioStream_->PauseAudioStream(); // Just Pause, do not deactivate here
            isForcePaused_ = true;
            break;
        case INTERRUPT_HINT_RESUME:
            if (audioStream_->GetState() != PAUSED || !isForcePaused_) {
                AUDIO_DEBUG_LOG("AudioRendererPrivate::OnInterrupt state is not paused or not forced paused");
                return;
            }
            isForcePaused_ = false;
            NotifyForcePausedToResume(interruptEvent);
            return; // return, sending callback is taken care in NotifyForcePausedToResume
        case INTERRUPT_HINT_STOP:
            (void)audioStream_->StopAudioStream();
            break;
        case INTERRUPT_HINT_DUCK:
            if (!HandleForceDucking(interruptEvent)) {
                AUDIO_DEBUG_LOG("AudioRendererPrivate:: It is not forced ducked, no need notify app, return");
                return;
            }
            isForceDucked_ = true;
            break;
        case INTERRUPT_HINT_UNDUCK:
            if (!isForceDucked_) {
                AUDIO_DEBUG_LOG("AudioRendererPrivate:: It is not forced ducked, no need to unduck or notify app");
                return;
            }
            (void)audioStream_->SetVolume(instanceVolBeforeDucking_);
            AUDIO_DEBUG_LOG("AudioRendererPrivate: unduck Volume(instance) complete: %{public}f",
                            instanceVolBeforeDucking_);
            isForceDucked_ = false;
            break;
        default:
            break;
    }
    // Notify valid forced event callbacks to app
    InterruptEvent interruptEventForced {interruptEvent.eventType, interruptEvent.forceType, interruptEvent.hintType};
    NotifyEvent(interruptEventForced);
}

void AudioInterruptCallbackImpl::OnInterrupt(const InterruptEventInternal &interruptEvent)
{
    cb_ = callback_.lock();
    InterruptForceType forceType = interruptEvent.forceType;
    AUDIO_DEBUG_LOG("AudioRendererPrivate: OnInterrupt InterruptForceType: %{public}d", forceType);

    if (forceType != INTERRUPT_FORCE) { // INTERRUPT_SHARE
        AUDIO_DEBUG_LOG("AudioRendererPrivate ForceType: INTERRUPT_SHARE. Let app handle the event");
        InterruptEvent interruptEventShared {interruptEvent.eventType, interruptEvent.forceType,
                                             interruptEvent.hintType};
        NotifyEvent(interruptEventShared);
        return;
    }

    if (audioStream_ == nullptr) {
        AUDIO_DEBUG_LOG("AudioInterruptCallbackImpl::OnInterrupt stream is not alive. No need to take forced action");
        return;
    }

    HandleAndNotifyForcedEvent(interruptEvent);
}

void AudioStreamCallbackRenderer::SaveCallback(const std::weak_ptr<AudioRendererCallback> &callback)
{
    callback_ = callback;
}

void AudioStreamCallbackRenderer::OnStateChange(const State state, const StateChangeCmdType cmdType)
{
    std::shared_ptr<AudioRendererCallback> cb = callback_.lock();
    if (cb == nullptr) {
        AUDIO_ERR_LOG("AudioStreamCallbackRenderer::OnStateChange cb == nullptr.");
        return;
    }

    cb->OnStateChange(static_cast<RendererState>(state), cmdType);
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
    return RENDERER_SUPPORTED_CHANNELS;
}

std::vector<AudioEncodingType> AudioRenderer::GetSupportedEncodingTypes()
{
    return AUDIO_SUPPORTED_ENCODING_TYPES;
}

int32_t AudioRendererPrivate::SetRenderMode(AudioRenderMode renderMode) const
{
    return audioStream_->SetRenderMode(renderMode);
}

AudioRenderMode AudioRendererPrivate::GetRenderMode() const
{
    return audioStream_->GetRenderMode();
}

int32_t AudioRendererPrivate::GetBufferDesc(BufferDesc &bufDesc) const
{
    return audioStream_->GetBufferDesc(bufDesc);
}

int32_t AudioRendererPrivate::Enqueue(const BufferDesc &bufDesc) const
{
    return audioStream_->Enqueue(bufDesc);
}

int32_t AudioRendererPrivate::Clear() const
{
    return audioStream_->Clear();
}

int32_t AudioRendererPrivate::GetBufQueueState(BufferQueueState &bufState) const
{
    return audioStream_->GetBufQueueState(bufState);
}

void AudioRendererPrivate::SetApplicationCachePath(const std::string cachePath)
{
    audioStream_->SetApplicationCachePath(cachePath);
}

int32_t AudioRendererPrivate::SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback)
{
    return audioStream_->SetRendererWriteCallback(callback);
}

void AudioRendererPrivate::SetInterruptMode(InterruptMode mode)
{
    AUDIO_INFO_LOG("AudioRendererPrivate::SetInterruptMode: InterruptMode %{pubilc}d", mode);
    if (mode_ == mode) {
        return;
    } else if (mode != SHARE_MODE && mode != INDEPENDENT_MODE) {
        AUDIO_ERR_LOG("AudioRendererPrivate::SetInterruptMode: Invalid interrupt mode!");
        return;
    }
    mode_ = mode;

    if (AudioPolicyManager::GetInstance().UnsetAudioInterruptCallback(sessionID_) != 0) {
        AUDIO_ERR_LOG("AudioRendererPrivate::SetInterruptMode: UnsetAudioInterruptCallback failed!");
        return;
    }
    if (InitAudioInterruptCallback() != 0) {
        AUDIO_ERR_LOG("AudioRendererPrivate::SetInterruptMode: InitAudioInterruptCallback failed!");
        return;
    }
}

int32_t AudioRendererPrivate::SetLowPowerVolume(float volume) const
{
    return audioStream_->SetLowPowerVolume(volume);
}

float AudioRendererPrivate::GetLowPowerVolume() const
{
    return audioStream_->GetLowPowerVolume();
}

float AudioRendererPrivate::GetSingleStreamVolume() const
{
    return audioStream_->GetSingleStreamVolume();
}
#endif

}  // namespace AudioStandard
}  // namespace OHOS
