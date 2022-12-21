/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd.
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

#include "audio_errors.h"
#include "audio_policy_manager.h"
#include "audio_renderer_gateway.h"
#include "audio_container_stream_base.h"
#include "audio_log.h"
#include "audio_renderer.h"

namespace OHOS {
namespace AudioStandard {
std::map<pid_t, std::map<AudioStreamType, AudioInterrupt>> AudioRendererGateway::sharedInterrupts_;

AudioRenderer::~AudioRenderer() = default;
AudioRendererGateway::~AudioRendererGateway()
{
    RendererState state = GetStatus();
    if (state != RENDERER_RELEASED && state != RENDERER_NEW) {
        Release();
    }
}

AudioRendererGateway::AudioRendererGateway(AudioStreamType audioStreamType)
{
    audioStream_ = std::make_shared<AudioContainerRenderStream>(audioStreamType, AUDIO_MODE_PLAYBACK);
    audioInterrupt_.streamType = audioStreamType;
}

int32_t AudioRendererGateway::GetFrameCount(uint32_t &frameCount) const
{
    return audioStream_->GetFrameCount(frameCount);
}

int32_t AudioRendererGateway::GetLatency(uint64_t &latency) const
{
    return audioStream_->GetLatency(latency);
}

int32_t AudioRendererGateway::SetParams(const AudioRendererParams params)
{
    AudioStreamParams audioStreamParams;
    audioStreamParams.format = params.sampleFormat;
    audioStreamParams.samplingRate = params.sampleRate;
    audioStreamParams.channels = params.channelCount;
    audioStreamParams.encoding = params.encodingType;

    int32_t ret = audioStream_->SetAudioStreamInfo(audioStreamParams);

    AUDIO_INFO_LOG("AudioRendererGateway::SetParams SetAudioStreamInfo Success");
    if (ret) {
        AUDIO_ERR_LOG("AudioRendererGateway::SetParams SetAudioStreamInfo Failed");
        return ret;
    }
    
    if (audioStream_->GetAudioSessionID(sessionID_) != 0) {
        AUDIO_ERR_LOG("AudioRendererGateway::SetParams GetAudioSessionID Failed");
        return ERR_INVALID_INDEX;
    }
    audioInterrupt_.sessionID = sessionID_;

    if (audioInterruptCallback_ == nullptr) {
        audioInterruptCallback_ = std::make_shared<AudioInterruptCallbackGateway>(audioStream_, audioInterrupt_);
        if (audioInterruptCallback_ == nullptr) {
            AUDIO_ERR_LOG("AudioRendererGateway::Failed to allocate memory for audioInterruptCallback_");
            return ERROR;
        }
    }

    return AudioPolicyManager::GetInstance().SetAudioInterruptCallback(sessionID_, audioInterruptCallback_);
}

int32_t AudioRendererGateway::GetParams(AudioRendererParams &params) const
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

int32_t AudioRendererGateway::GetRendererInfo(AudioRendererInfo &rendererInfo) const
{
    rendererInfo = rendererInfo_;

    return SUCCESS;
}

int32_t AudioRendererGateway::GetStreamInfo(AudioStreamInfo &streamInfo) const
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

int32_t AudioRendererGateway::SetRendererCallback(const std::shared_ptr<AudioRendererCallback> &callback)
{
    // If the client is using the deprecated SetParams API. SetRendererCallback must be invoked, after SetParams.
    // In general, callbacks can only be set after the renderer state is PREPARED.
    RendererState state = GetStatus();
    if (state == RENDERER_NEW || state == RENDERER_RELEASED) {
        AUDIO_DEBUG_LOG("AudioRendererGateway::SetRendererCallback incorrect state:%{public}d to register cb", state);
        return ERR_ILLEGAL_STATE;
    }
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioRendererGateway::SetRendererCallback callback param is null");
        return ERR_INVALID_PARAM;
    }

    // Save reference for interrupt callback
    if (audioInterruptCallback_ == nullptr) {
        AUDIO_ERR_LOG("AudioRendererGateway::SetRendererCallback audioInterruptCallback_ == nullptr");
        return ERROR;
    }
    std::shared_ptr<AudioInterruptCallbackGateway> cbInterrupt =
        std::static_pointer_cast<AudioInterruptCallbackGateway>(audioInterruptCallback_);
    cbInterrupt->SaveCallback(callback);

    // Save and Set reference for stream callback. Order is important here.
    if (audioStreamCallback_ == nullptr) {
        audioStreamCallback_ = std::make_shared<AudioStreamRenderCallback>();
        if (audioStreamCallback_ == nullptr) {
            AUDIO_ERR_LOG("AudioRendererGateway::Failed to allocate memory for audioStreamCallback_");
            return ERROR;
        }
    }

    std::shared_ptr<AudioStreamRenderCallback> cbStream =
        std::static_pointer_cast<AudioStreamRenderCallback>(audioStreamCallback_);
    cbStream->SaveCallback(callback);
    AUDIO_INFO_LOG("AudioRendererGateway::SetRendererCallback callback %p audioStreamCallback_ %p", callback.get(),
        audioStreamCallback_.get());
    (void)audioStream_->SetStreamCallback(audioStreamCallback_);

    return SUCCESS;
}

int32_t AudioRendererGateway::SetRendererPositionCallback(int64_t markPosition,
    const std::shared_ptr<RendererPositionCallback> &callback)
{
    if ((callback == nullptr) || (markPosition <= 0)) {
        AUDIO_ERR_LOG("AudioRendererGateway::SetRendererPositionCallback input param is invalid");
        return ERR_INVALID_PARAM;
    }

    audioStream_->SetRendererPositionCallback(markPosition, callback);

    return SUCCESS;
}

void AudioRendererGateway::UnsetRendererPositionCallback()
{
    audioStream_->UnsetRendererPositionCallback();
}

int32_t AudioRendererGateway::SetRendererPeriodPositionCallback(int64_t frameNumber,
    const std::shared_ptr<RendererPeriodPositionCallback> &callback)
{
    if ((callback == nullptr) || (frameNumber <= 0)) {
        AUDIO_ERR_LOG("AudioRendererGateway::SetRendererPeriodPositionCallback input param is invalid");
        return ERR_INVALID_PARAM;
    }

    audioStream_->SetRendererPeriodPositionCallback(frameNumber, callback);

    return SUCCESS;
}

void AudioRendererGateway::UnsetRendererPeriodPositionCallback()
{
    audioStream_->UnsetRendererPeriodPositionCallback();
}

bool AudioRendererGateway::Start(StateChangeCmdType cmdType) const
{
    AUDIO_INFO_LOG("AudioRendererGateway::Start()");
    RendererState state = GetStatus();
    if ((state != RENDERER_PREPARED) && (state != RENDERER_STOPPED) && (state != RENDERER_PAUSED)) {
        AUDIO_ERR_LOG("AudioRendererGateway::Start() Illegal state:%{public}u, Start failed", state);
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

    if (audioInterrupt_.streamType == STREAM_DEFAULT || audioInterrupt_.sessionID == INVALID_SESSION) {
        return false;
    }

    return audioStream_->StartAudioStream();
}

int32_t AudioRendererGateway::Write(uint8_t *buffer, size_t bufferSize)
{
    return audioStream_->Write(buffer, bufferSize);
}

RendererState AudioRendererGateway::GetStatus() const
{
    return static_cast<RendererState>(audioStream_->GetState());
}

bool AudioRendererGateway::GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) const
{
    return audioStream_->GetAudioTime(timestamp, base);
}

bool AudioRendererGateway::Drain() const
{
    return audioStream_->DrainAudioStream();
}

bool AudioRendererGateway::Flush() const
{
    return audioStream_->FlushAudioStream();
}

bool AudioRendererGateway::Pause(StateChangeCmdType cmdType) const
{
    bool result = audioStream_->PauseAudioStream();
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
        AUDIO_ERR_LOG("AudioRendererGateway: DeactivateAudioInterrupt Failed");
    }

    return result;
}

bool AudioRendererGateway::Stop() const
{
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
        AUDIO_ERR_LOG("AudioRendererGateway: DeactivateAudioInterrupt Failed");
    }

    return result;
}

bool AudioRendererGateway::Release() const
{
    // If Stop call was skipped, Release to take care of Deactivation
    (void)AudioPolicyManager::GetInstance().DeactivateAudioInterrupt(audioInterrupt_);

    // Unregister the callaback in policy server
    (void)AudioPolicyManager::GetInstance().UnsetAudioInterruptCallback(sessionID_);

    return audioStream_->ReleaseAudioStream();
}

int32_t AudioRendererGateway::GetBufferSize(size_t &bufferSize) const
{
    return audioStream_->GetBufferSize(bufferSize);
}

int32_t AudioRendererGateway::GetAudioStreamId(uint32_t &sessionID) const
{
    return audioStream_->GetAudioSessionID(sessionID);
}

int32_t AudioRendererGateway::SetAudioRendererDesc(AudioRendererDesc audioRendererDesc) const
{
    ContentType contentType = audioRendererDesc.contentType;
    StreamUsage streamUsage = audioRendererDesc.streamUsage;
    AudioStreamType audioStreamType = audioStream_->GetStreamType(contentType, streamUsage);
    return audioStream_->SetAudioStreamType(audioStreamType);
}

int32_t AudioRendererGateway::SetStreamType(AudioStreamType audioStreamType) const
{
    return audioStream_->SetAudioStreamType(audioStreamType);
}

int32_t AudioRendererGateway::SetVolume(float volume) const
{
    return audioStream_->SetVolume(volume);
}

float AudioRendererGateway::GetVolume() const
{
    return audioStream_->GetVolume();
}

int32_t AudioRendererGateway::SetRenderRate(AudioRendererRate renderRate) const
{
    return audioStream_->SetRenderRate(renderRate);
}

AudioRendererRate AudioRendererGateway::GetRenderRate() const
{
    return audioStream_->GetRenderRate();
}

int32_t AudioRendererGateway::SetBufferDuration(uint64_t bufferDuration) const
{
    if (bufferDuration < MINIMUM_BUFFER_SIZE_MSEC || bufferDuration > MAXIMUM_BUFFER_SIZE_MSEC) {
        AUDIO_ERR_LOG("Error: Please set the buffer duration between 5ms ~ 20ms");
        return ERR_INVALID_PARAM;
    }

    return audioStream_->SetBufferSizeInMsec(bufferDuration);
}

AudioInterruptCallbackGateway::AudioInterruptCallbackGateway(
    const std::shared_ptr<AudioContainerRenderStream> &audioStream, const AudioInterrupt &audioInterrupt)
    : audioStream_(audioStream), audioInterrupt_(audioInterrupt)
{
    AUDIO_INFO_LOG("AudioInterruptCallbackGateway constructor");
}

AudioInterruptCallbackGateway::~AudioInterruptCallbackGateway()
{
    AUDIO_DEBUG_LOG("AudioInterruptCallbackGateway: instance destroy");
}

void AudioInterruptCallbackGateway::SaveCallback(const std::weak_ptr<AudioRendererCallback> &callback)
{
    callback_ = callback;
}

void AudioInterruptCallbackGateway::NotifyEvent(const InterruptEvent &interruptEvent)
{
    AUDIO_DEBUG_LOG("AudioRendererGateway: NotifyEvent: Hint: %{public}d", interruptEvent.hintType);
    AUDIO_DEBUG_LOG("AudioRendererGateway: NotifyEvent: eventType: %{public}d", interruptEvent.eventType);

    if (cb_ != nullptr) {
        cb_->OnInterrupt(interruptEvent);
        AUDIO_DEBUG_LOG("AudioRendererGateway: OnInterrupt : NotifyEvent to app complete");
    } else {
        AUDIO_DEBUG_LOG("AudioRendererGateway: cb_ == nullptr cannont NotifyEvent to app");
    }
}

bool AudioInterruptCallbackGateway::HandleForceDucking(const InterruptEventInternal &interruptEvent)
{
    float streamVolume = AudioPolicyManager::GetInstance().GetStreamVolume(audioInterrupt_.streamType);
    float duckVolume = interruptEvent.duckVolume;
    int32_t ret = 0;

    if (streamVolume <= duckVolume || FLOAT_COMPARE_EQ(streamVolume, 0.0f)) {
        AUDIO_INFO_LOG("AudioRendererGateway: StreamVolume: %{public}f <= duckVolume: %{public}f",
                       streamVolume, duckVolume);
        AUDIO_INFO_LOG("AudioRendererGateway: No need to duck further return");
        return false;
    }

    instanceVolBeforeDucking_ = audioStream_->GetVolume();
    float duckInstanceVolume = duckVolume / streamVolume;
    if (FLOAT_COMPARE_EQ(instanceVolBeforeDucking_, 0.0f) || instanceVolBeforeDucking_ < duckInstanceVolume) {
        AUDIO_INFO_LOG("AudioRendererGateway: No need to duck further return");
        return false;
    }

    ret = audioStream_->SetVolume(duckInstanceVolume);
    if (ret) {
        AUDIO_DEBUG_LOG("AudioRendererGateway: set duckVolume(instance) %{pubic}f failed", duckInstanceVolume);
        return false;
    }

    AUDIO_DEBUG_LOG("AudioRendererGateway: set duckVolume(instance) %{pubic}f succeeded", duckInstanceVolume);
    return true;
}

void AudioInterruptCallbackGateway::NotifyForcePausedToResume(const InterruptEventInternal &interruptEvent)
{
    // Change InterruptForceType to Share, Since app will take care of resuming
    InterruptEvent interruptEventResume {interruptEvent.eventType, INTERRUPT_SHARE,
                                         interruptEvent.hintType};
    NotifyEvent(interruptEventResume);
}

void AudioInterruptCallbackGateway::HandleAndNotifyForcedEvent(const InterruptEventInternal &interruptEvent)
{
    InterruptHint hintType = interruptEvent.hintType;
    AUDIO_DEBUG_LOG("AudioRendererGateway ForceType: INTERRUPT_FORCE, Force handle the event and notify the app");
    AUDIO_DEBUG_LOG("AudioRendererGateway: HandleAndNotifyForcedEvent: Hint: %{public}d eventType: %{public}d",
        interruptEvent.hintType, interruptEvent.eventType);

    switch (hintType) {
        case INTERRUPT_HINT_PAUSE:
            if (audioStream_->GetState() != RUNNING) {
                AUDIO_DEBUG_LOG("AudioRendererGateway::OnInterrupt state is not running no need to pause");
                return;
            }
            (void)audioStream_->PauseAudioStream(); // Just Pause, do not deactivate here
            isForcePaused_ = true;
            break;
        case INTERRUPT_HINT_RESUME:
            if (audioStream_->GetState() != PAUSED || !isForcePaused_) {
                AUDIO_DEBUG_LOG("AudioRendererGateway::OnInterrupt state is not paused or not forced paused");
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
                AUDIO_DEBUG_LOG("AudioRendererGateway:: It is not forced ducked, no need notify app, return");
                return;
            }
            isForceDucked_ = true;
            break;
        case INTERRUPT_HINT_UNDUCK:
            if (!isForceDucked_) {
                AUDIO_DEBUG_LOG("AudioRendererGateway:: It is not forced ducked, no need to unduck or notify app");
                return;
            }
            (void)audioStream_->SetVolume(instanceVolBeforeDucking_);
            AUDIO_DEBUG_LOG("AudioRendererGateway: unduck Volume(instance) complete: %{public}f",
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

void AudioInterruptCallbackGateway::OnInterrupt(const InterruptEventInternal &interruptEvent)
{
    cb_ = callback_.lock();
    InterruptForceType forceType = interruptEvent.forceType;
    AUDIO_DEBUG_LOG("AudioRendererGateway: OnInterrupt InterruptForceType: %{public}d", forceType);

    if (forceType != INTERRUPT_FORCE) { // INTERRUPT_SHARE
        AUDIO_DEBUG_LOG("AudioRendererGateway ForceType: INTERRUPT_SHARE. Let app handle the event");
        InterruptEvent interruptEventShared {interruptEvent.eventType, interruptEvent.forceType,
                                             interruptEvent.hintType};
        NotifyEvent(interruptEventShared);
        return;
    }

    if (audioStream_ == nullptr) {
        AUDIO_DEBUG_LOG("AudioInterruptCallbackGateway::OnInterrupt: stream is null. No need to take forced action");
        return;
    }

    HandleAndNotifyForcedEvent(interruptEvent);
}

void AudioStreamRenderCallback::SaveCallback(const std::weak_ptr<AudioRendererCallback> &callback)
{
    std::shared_ptr<AudioRendererCallback> cb = callback.lock();
    AUDIO_ERR_LOG("AudioStreamRenderCallback::SaveCallback cb %p", cb.get());
    callback_ = callback;
}

void AudioStreamRenderCallback::OnStateChange(const State state, StateChangeCmdType cmdType)
{
    std::shared_ptr<AudioRendererCallback> cb = callback_.lock();
    if (cb == nullptr) {
        AUDIO_ERR_LOG("AudioStreamRenderCallback::OnStateChange cb == nullptr.");
        return;
    }
    AUDIO_ERR_LOG("AudioStreamRenderCallback::OnStateChange cb %p", cb.get());
    cb->OnStateChange(static_cast<RendererState>(state));
}

int32_t AudioRendererGateway::SetRenderMode(AudioRenderMode renderMode) const
{
    return audioStream_->SetRenderMode(renderMode);
}

AudioRenderMode AudioRendererGateway::GetRenderMode() const
{
    return audioStream_->GetRenderMode();
}

int32_t AudioRendererGateway::GetBufferDesc(BufferDesc &bufDesc) const
{
    return audioStream_->GetBufferDesc(bufDesc);
}

int32_t AudioRendererGateway::Enqueue(const BufferDesc &bufDesc) const
{
    return audioStream_->Enqueue(bufDesc);
}

int32_t AudioRendererGateway::Clear() const
{
    return audioStream_->Clear();
}

int32_t AudioRendererGateway::GetBufQueueState(BufferQueueState &bufState) const
{
    return audioStream_->GetBufQueueState(bufState);
}

void AudioRendererGateway::SetApplicationCachePath(const std::string cachePath)
{
    audioStream_->SetApplicationCachePath(cachePath);
}

int32_t AudioRendererGateway::SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback)
{
    return audioStream_->SetRendererWriteCallback(callback);
}

void AudioRendererGateway::SetInterruptMode(InterruptMode mode)
{
    AUDIO_INFO_LOG("AudioRendererGateway: SetInterruptMode: InterruptMode %{pubilc}d", mode);
    mode_ = mode;
}

int32_t AudioRendererGateway::SetLowPowerVolume(float volume) const
{
    return 0;
}

float AudioRendererGateway::GetLowPowerVolume() const
{
    return 0;
}

float AudioRendererGateway::GetSingleStreamVolume() const
{
    return 0;
}
}  // namespace AudioStandard
}  // namespace OHOS
