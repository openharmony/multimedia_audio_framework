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

#include <chrono>
#include <thread>
#include <vector>

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_utils.h"

#include "fast_audio_stream.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
FastAudioStream::FastAudioStream(AudioStreamType eStreamType, AudioMode eMode, int32_t appUid)
    : eStreamType_(eStreamType),
      eMode_(eMode),
      state_(NEW),
      renderMode_(RENDER_MODE_CALLBACK),
      captureMode_(CAPTURE_MODE_CALLBACK)
{
    AUDIO_INFO_LOG("FastAudioStream ctor, appUID = %{public}d", appUid);
    audioStreamTracker_ =  std::make_unique<AudioStreamTracker>(eMode, appUid);
    AUDIO_DEBUG_LOG("AudioStreamTracker created");
}

FastAudioStream::~FastAudioStream()
{
    if (state_ != RELEASED && state_ != NEW) {
        ReleaseAudioStream(false);
    }
}

void FastAudioStream::SetClientID(int32_t clientPid, int32_t clientUid)
{
    AUDIO_INFO_LOG("Set client PID: %{public}d, UID: %{public}d", clientPid, clientUid);
    clientPid_ = clientPid;
    clientUid_ = clientUid;
}

void FastAudioStream::SetRendererInfo(const AudioRendererInfo &rendererInfo)
{
    rendererInfo_ = rendererInfo;
}

void FastAudioStream::SetCapturerInfo(const AudioCapturerInfo &capturerInfo)
{
    capturerInfo_ = capturerInfo;
}

int32_t FastAudioStream::SetAudioStreamInfo(const AudioStreamParams info,
    const std::shared_ptr<AudioClientTracker> &proxyObj)
{
    AUDIO_INFO_LOG("FastAudioStreamInfo, Sampling rate: %{public}d, channels: %{public}d, format: %{public}d,"
        " stream type: %{public}d", info.samplingRate, info.channels, info.format, eStreamType_);
    if (processClient_ != nullptr) {
        AUDIO_ERR_LOG("Process is already inited, reset stream info is not supported.");
        return ERR_INVALID_OPERATION;
    }
    streamInfo_ = info;
    if (state_ != NEW) {
        AUDIO_INFO_LOG("FastAudioStream: State is not new, release existing stream");
        StopAudioStream();
        ReleaseAudioStream(false);
    }
    AudioProcessConfig config;
    config.appInfo.appPid = clientPid_;
    config.appInfo.appUid = clientUid_;
    config.audioMode = eMode_;
    config.streamInfo.channels = static_cast<AudioChannel>(info.channels);
    config.streamInfo.encoding = static_cast<AudioEncodingType>(info.encoding);
    config.streamInfo.format = static_cast<AudioSampleFormat>(info.format);
    config.streamInfo.samplingRate = static_cast<AudioSamplingRate>(info.samplingRate);
    config.streamType = eStreamType_;
    if (eMode_ == AUDIO_MODE_PLAYBACK) {
        AUDIO_DEBUG_LOG("FastAudioStream: Initialize playback");
        config.rendererInfo.contentType = rendererInfo_.contentType;
        config.rendererInfo.streamUsage = rendererInfo_.streamUsage;
        config.rendererInfo.rendererFlags = STREAM_FLAG_FAST;
    } else if (eMode_ == AUDIO_MODE_RECORD) {
        AUDIO_DEBUG_LOG("FastAudioStream: Initialize recording");
        config.capturerInfo.sourceType = capturerInfo_.sourceType;
        config.capturerInfo.capturerFlags = STREAM_FLAG_FAST;
    } else {
        AUDIO_ERR_LOG("FastAudioStream: error eMode.");
        return ERR_INVALID_OPERATION;
    }
    CHECK_AND_RETURN_RET_LOG(AudioProcessInClient::CheckIfSupport(config), ERR_INVALID_PARAM,
        "Stream is not supported.");
    processconfig_ = config;
    processClient_ = AudioProcessInClient::Create(config);
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, ERR_INVALID_PARAM,
        "Client test creat process client fail.");
    state_ = PREPARED;

    if (audioStreamTracker_ != nullptr && audioStreamTracker_.get()) {
        processClient_->GetSessionID(sessionId_);
        AUDIO_DEBUG_LOG("AudioStream:Calling register tracker, sessionid = %{public}d", sessionId_);
        audioStreamTracker_->RegisterTracker(sessionId_, state_, rendererInfo_, capturerInfo_, proxyObj);
    }
    return SUCCESS;
}

int32_t FastAudioStream::GetAudioStreamInfo(AudioStreamParams &audioStreamInfo)
{
    AUDIO_INFO_LOG("GetAudioStreamInfo in");
    audioStreamInfo = streamInfo_;
    return SUCCESS;
}

bool FastAudioStream::CheckRecordingCreate(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid)
{
    AUDIO_INFO_LOG("CheckRecordingCreate in");
    // note: add support later
    return true;
}

bool FastAudioStream::CheckRecordingStateChange(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid,
    AudioPermissionState state)
{
    AUDIO_INFO_LOG("CheckRecordingStateChange in");
    // note: add support later
    return true;
}

int32_t FastAudioStream::GetAudioSessionID(uint32_t &sessionID)
{
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, ERR_OPERATION_FAILED,
        "GetAudioSessionID failed: null process");
    int32_t ret = processClient_->GetSessionID(sessionID);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "GetSessionID error.");
    return ret;
}

State FastAudioStream::GetState()
{
    return state_;
}

bool FastAudioStream::GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base)
{
    CHECK_AND_RETURN_RET_LOG(base == Timestamp::MONOTONIC, false, "GetAudioTime failed: invalid base");

    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, false, "GetAudioTime failed: null process");
    int64_t timeSec = 0;
    int64_t timeNsec = 0;
    int32_t ret = processClient_->GetAudioTime(timestamp.framePosition, timeSec, timeNsec);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, false, "GetBufferSize error.");
    timestamp.time.tv_sec = timeSec;
    timestamp.time.tv_nsec = timeNsec;
    return true;
}

int32_t FastAudioStream::GetBufferSize(size_t &bufferSize)
{
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, ERR_OPERATION_FAILED, "GetBufferSize failed: null process");
    int32_t ret = processClient_->GetBufferSize(bufferSize);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "GetBufferSize error.");
    return ret;
}

int32_t FastAudioStream::GetFrameCount(uint32_t &frameCount)
{
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, ERR_OPERATION_FAILED, "GetFrameCount failed: null process");
    int32_t ret = processClient_->GetFrameCount(frameCount);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "GetFrameCount error.");
    return ret;
}

int32_t FastAudioStream::GetLatency(uint64_t &latency)
{
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, ERR_OPERATION_FAILED, "GetLatency failed: null process");
    int32_t ret = processClient_->GetLatency(latency);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "GetLatency error.");
    return ret;
}

int32_t FastAudioStream::SetAudioStreamType(AudioStreamType audioStreamType)
{
    // Stream type can only be set when create.
    AUDIO_ERR_LOG("Unsupported operation: SetAudioStreamType");
    return ERR_INVALID_OPERATION;
}

int32_t FastAudioStream::SetVolume(float volume)
{
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, ERR_OPERATION_FAILED, "SetVolume failed: null process");
    int32_t ret = processClient_->SetVolume(volume);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "SetVolume error.");
    return ret;
}

float FastAudioStream::GetVolume()
{
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, 1.0f, "SetVolume failed: null process"); // 1.0f for default
    return processClient_->GetVolume();
}

int32_t FastAudioStream::SetRenderRate(AudioRendererRate renderRate)
{
    if (RENDER_RATE_NORMAL == renderRate) {
        return SUCCESS;
    }
    AUDIO_ERR_LOG("Unsupported operation: SetRenderRate");
    return ERR_INVALID_OPERATION;
}

AudioRendererRate FastAudioStream::GetRenderRate()
{
    return renderRate_;
}

int32_t FastAudioStream::SetStreamCallback(const std::shared_ptr<AudioStreamCallback> &callback)
{
    AUDIO_INFO_LOG("SetStreamCallback in");
    // note: need add support
    return SUCCESS;
}

int32_t FastAudioStream::SetRenderMode(AudioRenderMode renderMode)
{
    if (renderMode != RENDER_MODE_CALLBACK || eMode_ != AUDIO_MODE_PLAYBACK) {
        AUDIO_ERR_LOG("SetRenderMode is not supported.");
        return ERR_INVALID_OPERATION;
    }
    return SUCCESS;
}

AudioRenderMode FastAudioStream::GetRenderMode()
{
    AUDIO_INFO_LOG("GetRenderMode in");
    return renderMode_;
}

int32_t FastAudioStream::SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback)
{
    AUDIO_INFO_LOG("SetRendererWriteCallback in.");
    if (!callback || !processClient_ || eMode_ != AUDIO_MODE_PLAYBACK) {
        AUDIO_ERR_LOG("SetRendererWriteCallback callback is nullptr");
        return ERR_INVALID_PARAM;
    }
    spkProcClientCb_ = std::make_shared<FastAudioStreamRenderCallback>(callback);
    int32_t ret = processClient_->SaveDataCallback(spkProcClientCb_);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "Client test save data callback fail, ret %{public}d.", ret);
    return SUCCESS;
}

int32_t FastAudioStream::SetCaptureMode(AudioCaptureMode captureMode)
{
    if (captureMode != CAPTURE_MODE_CALLBACK || eMode_ != AUDIO_MODE_RECORD) {
        AUDIO_ERR_LOG("SetCaptureMode is not supported.");
        return ERR_INVALID_OPERATION;
    }
    return SUCCESS;
}

AudioCaptureMode FastAudioStream::GetCaptureMode()
{
    return captureMode_;
}

int32_t FastAudioStream::SetCapturerReadCallback(const std::shared_ptr<AudioCapturerReadCallback> &callback)
{
    AUDIO_INFO_LOG("SetCapturerReadCallback in.");
    if (!callback || !processClient_ || eMode_ != AUDIO_MODE_RECORD) {
        AUDIO_ERR_LOG("SetCapturerReadCallback, callback or client is nullptr or mode is not record.");
        return ERR_INVALID_PARAM;
    }
    micProcClientCb_ = std::make_shared<FastAudioStreamCaptureCallback>(callback);
    int32_t ret = processClient_->SaveDataCallback(micProcClientCb_);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "Client save data callback fail, ret %{public}d.", ret);
    return SUCCESS;
}

int32_t FastAudioStream::GetBufferDesc(BufferDesc &bufDesc)
{
    AUDIO_DEBUG_LOG("GetBufferDesc in.");
    if (!processClient_) {
        AUDIO_ERR_LOG("spkClient is null.");
        return ERR_INVALID_OPERATION;
    }
    int32_t ret = processClient_->GetBufferDesc(bufDesc);
    if (ret != SUCCESS || bufDesc.buffer == nullptr || bufDesc.bufLength ==0) {
        AUDIO_ERR_LOG("GetBufferDesc failed.");
        return -1;
    }
    return SUCCESS;
}

int32_t FastAudioStream::GetBufQueueState(BufferQueueState &bufState)
{
    AUDIO_INFO_LOG("GetBufQueueState in.");
    // note: add support
    return SUCCESS;
}

int32_t FastAudioStream::Enqueue(const BufferDesc &bufDesc)
{
    AUDIO_DEBUG_LOG("Enqueue in");
    if (!processClient_) {
        AUDIO_ERR_LOG("spkClient is null.");
        return ERR_INVALID_OPERATION;
    }
    int32_t ret = processClient_->Enqueue(bufDesc);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("Enqueue failed.");
        return -1;
    }
    return SUCCESS;
}

int32_t FastAudioStream::Clear()
{
    AUDIO_INFO_LOG("Clear will do nothing.");

    return SUCCESS;
}

int32_t FastAudioStream::SetLowPowerVolume(float volume)
{
    AUDIO_INFO_LOG("SetLowPowerVolume in.");
    return 1.0f;
}

float FastAudioStream::GetLowPowerVolume()
{
    AUDIO_INFO_LOG("GetLowPowerVolume in.");
    return 1.0f;
}

float FastAudioStream::GetSingleStreamVolume()
{
    AUDIO_INFO_LOG("GetSingleStreamVolume in.");
    return 1.0f;
}

AudioEffectMode FastAudioStream::GetAudioEffectMode()
{
    AUDIO_ERR_LOG("GetAudioEffectMode not supported");
    return EFFECT_NONE;
}

int32_t FastAudioStream::SetAudioEffectMode(AudioEffectMode effectMode)
{
    AUDIO_ERR_LOG("SetAudioEffectMode not supported");
    return ERR_NOT_SUPPORTED;
}

int64_t FastAudioStream::GetFramesWritten()
{
    int64_t result = -1; // -1 invalid frame
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, result, "GetFramesWritten failed: null process");
    result = processClient_->GetFramesWritten();
    return result;
}

int64_t FastAudioStream::GetFramesRead()
{
    int64_t result = -1; // -1 invalid frame
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, result, "GetFramesRead failed: null process");
    result = processClient_->GetFramesRead();
    return result;
}

bool FastAudioStream::StartAudioStream(StateChangeCmdType cmdType)
{
    AUDIO_INFO_LOG("StartAudioStream in.");
    if ((state_ != PREPARED) && (state_ != STOPPED) && (state_ != PAUSED)) {
        AUDIO_ERR_LOG("StartAudioStream Illegal state:%{public}u", state_);
        return false;
    }

    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, false, "Start failed, process is null.");
    int32_t ret = ERROR;
    if (state_ == PAUSED) {
        ret = processClient_->Resume();
    } else {
        ret = processClient_->Start();
    }
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, false, "Client test stop fail, ret %{public}d.", ret);
    state_ = RUNNING;

    AUDIO_DEBUG_LOG("StartAudioStream SUCCESS, sessionId: %{public}d", sessionId_);

    if (audioStreamTracker_ != nullptr && audioStreamTracker_.get()) {
        AUDIO_DEBUG_LOG("AudioStream:Calling Update tracker for Running");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    return true;
}

bool FastAudioStream::PauseAudioStream(StateChangeCmdType cmdType)
{
    AUDIO_INFO_LOG("PauseAudioStream in");
    if (state_ != RUNNING) {
        AUDIO_ERR_LOG("PauseAudioStream: State is not RUNNING. Illegal state:%{public}u", state_);
        return false;
    }
    State oldState = state_;

    state_ = PAUSED;
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, false, "Pause failed, process is null.");
    int32_t ret = processClient_->Pause();
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("StreamPause fail,ret:%{public}d", ret);
        state_ = oldState;
        return false;
    }

    AUDIO_DEBUG_LOG("PauseAudioStream SUCCESS, sessionId: %{public}d", sessionId_);
    if (audioStreamTracker_ != nullptr && audioStreamTracker_.get()) {
        AUDIO_DEBUG_LOG("AudioStream:Calling Update tracker for Pause");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    return true;
}

bool FastAudioStream::StopAudioStream()
{
    if ((state_ != RUNNING) && (state_ != PAUSED)) {
        AUDIO_ERR_LOG("StopAudioStream: State is not RUNNING. Illegal state:%{public}u", state_);
        return false;
    }
    State oldState = state_;
    state_ = STOPPED; // Set it before stopping as Read/Write and Stop can be called from different threads

    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, false, "Stop failed, process is null.");
    int32_t ret = processClient_->Stop();
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("StreamStop fail,ret:%{public}d", ret);
        state_ = oldState;
        return false;
    }

    AUDIO_INFO_LOG("StopAudioStream SUCCESS, sessionId: %{public}d", sessionId_);
    if (audioStreamTracker_ != nullptr && audioStreamTracker_.get()) {
        AUDIO_DEBUG_LOG("AudioStream:Calling Update tracker for stop");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    return true;
}

bool FastAudioStream::FlushAudioStream()
{
    AUDIO_INFO_LOG("FlushAudioStream in.");
    return true;
}

bool FastAudioStream::DrainAudioStream()
{
    AUDIO_INFO_LOG("Drain stream SUCCESS");
    return true;
}

bool FastAudioStream::ReleaseAudioStream(bool releaseRunner)
{
    if (state_ == RELEASED || state_ == NEW) {
        AUDIO_ERR_LOG("Illegal state: state = %{public}u", state_);
        return false;
    }
    // If state_ is RUNNING try to Stop it first and Release
    if (state_ == RUNNING) {
        StopAudioStream();
    }

    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, false, "Release failed, process is null.");
    processClient_->Release();
    state_ = RELEASED;
    AUDIO_INFO_LOG("ReleaseAudiostream SUCCESS, sessionId: %{public}d", sessionId_);
    if (audioStreamTracker_ != nullptr && audioStreamTracker_.get()) {
        AUDIO_DEBUG_LOG("AudioStream:Calling Update tracker for release");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    return true;
}

int32_t FastAudioStream::Read(uint8_t &buffer, size_t userSize, bool isBlockingRead)
{
    AUDIO_ERR_LOG("Unsupported operation: read");
    return ERR_INVALID_OPERATION;
}

size_t FastAudioStream::Write(uint8_t *buffer, size_t buffer_size)
{
    AUDIO_ERR_LOG("Unsupported operation: Write");
    return ERR_INVALID_OPERATION;
}

uint32_t FastAudioStream::GetUnderflowCount()
{
    AUDIO_INFO_LOG("GetUnderflowCount in.");
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, 0, "process client is null.");
    underflowCount_ = processClient_->GetUnderflowCount();
    return underflowCount_;
}

void FastAudioStream::SetRendererPositionCallback(int64_t markPosition,
    const std::shared_ptr<RendererPositionCallback> &callback)
{
    AUDIO_INFO_LOG("Registering render frame position callback mark position");
    // note: need support
}

void FastAudioStream::UnsetRendererPositionCallback()
{
    AUDIO_INFO_LOG("Unregistering render frame position callback");
    // note: need support
}

void FastAudioStream::SetRendererPeriodPositionCallback(int64_t periodPosition,
    const std::shared_ptr<RendererPeriodPositionCallback> &callback)
{
    AUDIO_INFO_LOG("Registering render period position callback");
}

void FastAudioStream::UnsetRendererPeriodPositionCallback()
{
    AUDIO_INFO_LOG("Unregistering render period position callback");
}

void FastAudioStream::SetCapturerPositionCallback(int64_t markPosition,
    const std::shared_ptr<CapturerPositionCallback> &callback)
{
    AUDIO_INFO_LOG("Registering capture frame position callback, mark position");
}

void FastAudioStream::UnsetCapturerPositionCallback()
{
    AUDIO_INFO_LOG("Unregistering capture frame position callback");
}

void FastAudioStream::SetCapturerPeriodPositionCallback(int64_t periodPosition,
    const std::shared_ptr<CapturerPeriodPositionCallback> &callback)
{
    AUDIO_INFO_LOG("Registering period position callback");
}

void FastAudioStream::UnsetCapturerPeriodPositionCallback()
{
    AUDIO_INFO_LOG("Unregistering period position callback");
}

int32_t FastAudioStream::SetRendererSamplingRate(uint32_t sampleRate)
{
    AUDIO_ERR_LOG("SetRendererSamplingRate  is not supported");

    return ERR_OPERATION_FAILED;
}

uint32_t FastAudioStream::GetRendererSamplingRate()
{
    AUDIO_INFO_LOG("GetRendererSamplingRate in");
    return streamInfo_.samplingRate;
}

int32_t FastAudioStream::SetBufferSizeInMsec(int32_t bufferSizeInMsec)
{
    AUDIO_ERR_LOG("SetBufferSizeInMsec is not supported");
    // note: add support
    return ERR_NOT_SUPPORTED;
}

void FastAudioStream::SetApplicationCachePath(const std::string cachePath)
{
    AUDIO_INFO_LOG("SetApplicationCachePath to %{public}s", cachePath.c_str());

    cachePath_ = cachePath;
}
void FastAudioStream::SetInnerCapturerState(bool isInnerCapturer)
{
    AUDIO_ERR_LOG("SetInnerCapturerState is not supported");
}

void FastAudioStream::SetWakeupCapturerState(bool isWakeupCapturer)
{
    AUDIO_ERR_LOG("SetWakeupCapturerState is not supported");
}

void FastAudioStream::SetPrivacyType(AudioPrivacyType privacyType)
{
    AUDIO_ERR_LOG("SetPrivacyType is not supported");
}

IAudioStream::StreamClass FastAudioStream::GetStreamClass()
{
    return IAudioStream::StreamClass::FAST_STREAM;
}

void FastAudioStream::SetStreamTrackerState(bool trackerRegisteredState)
{
    streamTrackerRegistered_ = trackerRegisteredState;
}

void FastAudioStream::GetSwitchInfo(IAudioStream::SwitchInfo& info)
{
    GetAudioStreamInfo(info.params);
    info.rendererInfo = rendererInfo_;
    info.capturerInfo = capturerInfo_;
    info.eStreamType = eStreamType_;
    info.state = state_;
    info.sessionId = sessionId_;
    info.cachePath = cachePath_;

    info.clientPid = clientPid_;
    info.clientUid = clientUid_;

    info.volume = GetVolume();
    info.effectMode = GetAudioEffectMode();
    info.renderMode = renderMode_;
    info.captureMode = captureMode_;
    info.renderRate = renderRate_;

    if (spkProcClientCb_) {
        info.rendererWriteCallback = spkProcClientCb_->GetRendererWriteCallback();
    }
}

void FastAudioStreamRenderCallback::OnHandleData(size_t length)
{
    CHECK_AND_RETURN_LOG(rendererWriteCallback_!= nullptr, "OnHandleData failed: rendererWriteCallback_ is null.");
    rendererWriteCallback_->OnWriteData(length);
}

std::shared_ptr<AudioRendererWriteCallback> FastAudioStreamRenderCallback::GetRendererWriteCallback() const
{
    return rendererWriteCallback_;
}

void FastAudioStreamCaptureCallback::OnHandleData(size_t length)
{
    CHECK_AND_RETURN_LOG(captureCallback_!= nullptr, "OnHandleData failed: captureCallback_ is null.");
    captureCallback_->OnReadData(length);
}
} // namespace AudioStandard
} // namespace OHOS
