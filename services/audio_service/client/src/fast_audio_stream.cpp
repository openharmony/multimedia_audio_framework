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
#ifndef LOG_TAG
#define LOG_TAG "FastAudioStream"
#endif

#include <chrono>
#include <thread>
#include <vector>

#include "audio_errors.h"
#include "audio_service_log.h"
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
    audioStreamTracker_ = std::make_unique<AudioStreamTracker>(eMode, appUid);
    AUDIO_DEBUG_LOG("AudioStreamTracker created");
}

FastAudioStream::~FastAudioStream()
{
    if (state_ != RELEASED && state_ != NEW) {
        ReleaseAudioStream(false);
    }
}

void FastAudioStream::SetClientID(int32_t clientPid, int32_t clientUid, uint32_t appTokenId, uint64_t fullTokenId)
{
    AUDIO_INFO_LOG("Set client PID: %{public}d, UID: %{public}d", clientPid, clientUid);
    clientPid_ = clientPid;
    clientUid_ = clientUid;
    appTokenId_ = appTokenId;
    fullTokenId_ = fullTokenId;
}

int32_t FastAudioStream::UpdatePlaybackCaptureConfig(const AudioPlaybackCaptureConfig &config)
{
    AUDIO_ERR_LOG("Unsupported operation!");
    return ERR_NOT_SUPPORTED;
}

void FastAudioStream::SetRendererInfo(const AudioRendererInfo &rendererInfo)
{
    rendererInfo_ = rendererInfo;
    rendererInfo_.samplingRate = static_cast<AudioSamplingRate>(streamInfo_.samplingRate);
}

void FastAudioStream::SetCapturerInfo(const AudioCapturerInfo &capturerInfo)
{
    capturerInfo_ = capturerInfo;
    capturerInfo_.samplingRate = static_cast<AudioSamplingRate>(streamInfo_.samplingRate);
}

int32_t FastAudioStream::InitializeAudioProcessConfig(AudioProcessConfig &config, const AudioStreamParams &info)
{
    config.appInfo.appPid = clientPid_;
    config.appInfo.appUid = clientUid_;
    config.audioMode = eMode_;
    config.streamInfo.channels = static_cast<AudioChannel>(info.channels);
    config.streamInfo.encoding = static_cast<AudioEncodingType>(info.encoding);
    config.streamInfo.format = static_cast<AudioSampleFormat>(info.format);
    config.streamInfo.samplingRate = static_cast<AudioSamplingRate>(info.samplingRate);
    config.streamType = eStreamType_;
    config.originalSessionId = info.originalSessionId;
    if (eMode_ == AUDIO_MODE_PLAYBACK) {
        AUDIO_DEBUG_LOG("FastAudioStream: Initialize playback");
        config.rendererInfo.contentType = rendererInfo_.contentType;
        config.rendererInfo.streamUsage = rendererInfo_.streamUsage;
        config.rendererInfo.rendererFlags = STREAM_FLAG_FAST;
        config.rendererInfo.originalFlag = rendererInfo_.originalFlag;
    } else if (eMode_ == AUDIO_MODE_RECORD) {
        AUDIO_DEBUG_LOG("FastAudioStream: Initialize recording");
        config.capturerInfo.sourceType = capturerInfo_.sourceType;
        config.capturerInfo.capturerFlags = STREAM_FLAG_FAST;
        config.capturerInfo.originalFlag = capturerInfo_.originalFlag;
    } else {
        return ERR_INVALID_OPERATION;
    }
    return SUCCESS;
}

int32_t FastAudioStream::SetAudioStreamInfo(const AudioStreamParams info,
    const std::shared_ptr<AudioClientTracker> &proxyObj)
{
    AUDIO_INFO_LOG("FastAudioStreamInfo, Sampling rate: %{public}d, channels: %{public}d, format: %{public}d,"
        " stream type: %{public}d", info.samplingRate, info.channels, info.format, eStreamType_);
    CHECK_AND_RETURN_RET_LOG(processClient_ == nullptr, ERR_INVALID_OPERATION,
        "Process is already inited, reset stream info is not supported.");
    streamInfo_ = info;
    if (state_ != NEW) {
        AUDIO_INFO_LOG("FastAudioStream: State is not new, release existing stream");
        StopAudioStream();
        ReleaseAudioStream(false);
    }
    AudioProcessConfig config;
    int32_t ret = InitializeAudioProcessConfig(config, info);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "Initialize failed.");
    CHECK_AND_RETURN_RET_LOG(AudioProcessInClient::CheckIfSupport(config), ERR_INVALID_PARAM,
        "Stream is not supported.");
    processconfig_ = config;
    processClient_ = AudioProcessInClient::Create(config);
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, ERR_INVALID_PARAM,
        "Client test creat process client fail.");
    state_ = PREPARED;
    proxyObj_ = proxyObj;

    if (audioStreamTracker_ != nullptr && audioStreamTracker_.get()) {
        processClient_->GetSessionID(sessionId_);

        AudioRegisterTrackerInfo registerTrackerInfo;
        UpdateRegisterTrackerInfo(registerTrackerInfo);
        audioStreamTracker_->RegisterTracker(registerTrackerInfo, proxyObj);
    }
    return SUCCESS;
}

int32_t FastAudioStream::GetAudioStreamInfo(AudioStreamParams &audioStreamInfo)
{
    AUDIO_INFO_LOG("GetAudioStreamInfo enter.");
    audioStreamInfo = streamInfo_;
    return SUCCESS;
}

bool FastAudioStream::CheckRecordingCreate(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid,
    SourceType sourceType)
{
    AUDIO_ERR_LOG("Not supported operation");
    return false;
}

bool FastAudioStream::CheckRecordingStateChange(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid,
    AudioPermissionState state)
{
    AUDIO_ERR_LOG("Not supported operation");
    return false;
}

int32_t FastAudioStream::GetAudioSessionID(uint32_t &sessionID)
{
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, ERR_OPERATION_FAILED,
        "GetAudioSessionID failed: null process");
    int32_t ret = processClient_->GetSessionID(sessionID);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "GetSessionID error.");
    return ret;
}

void FastAudioStream::GetAudioPipeType(AudioPipeType &pipeType)
{
    pipeType = eMode_ == AUDIO_MODE_PLAYBACK ? rendererInfo_.pipeType : capturerInfo_.pipeType;
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
    bool ret = processClient_->GetAudioTime(timestamp.framePosition, timeSec, timeNsec);
    CHECK_AND_RETURN_RET_LOG(ret, false, "GetBufferSize error.");
    timestamp.time.tv_sec = timeSec;
    timestamp.time.tv_nsec = timeNsec;
    return true;
}

bool FastAudioStream::GetAudioPosition(Timestamp &timestamp, Timestamp::Timestampbase base)
{
    return GetAudioTime(timestamp, base);
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
    int32_t ret = SUCCESS;
    cacheVolume_ = volume;
    if (!silentModeAndMixWithOthers_) {
        ret = processClient_->SetVolume(volume);
        CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "SetVolume error.");
    }
    return ret;
}

float FastAudioStream::GetVolume()
{
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, 1.0f, "SetVolume failed: null process"); // 1.0f for default
    if (silentModeAndMixWithOthers_) {
        return cacheVolume_;
    } else {
        return processClient_->GetVolume();
    }
}

int32_t FastAudioStream::SetDuckVolume(float volume)
{
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, ERR_OPERATION_FAILED, "SetDuckVolume failed: null process");
    int32_t ret = processClient_->SetDuckVolume(volume);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "SetDuckVolume error.");
    return ret;
}

void FastAudioStream::SetSilentModeAndMixWithOthers(bool on)
{
    if (!silentModeAndMixWithOthers_ && on) {
        SetVolume(0.0);
    } else if (silentModeAndMixWithOthers_ && !on) {
        SetVolume(cacheVolume_);
    }
    silentModeAndMixWithOthers_ = on;
}

bool FastAudioStream::GetSilentModeAndMixWithOthers()
{
    return silentModeAndMixWithOthers_;
}

int32_t FastAudioStream::SetRenderRate(AudioRendererRate renderRate)
{
    CHECK_AND_RETURN_RET(RENDER_RATE_NORMAL != renderRate, SUCCESS);
    AUDIO_ERR_LOG("Unsupported operation: SetRenderRate");
    return ERR_INVALID_OPERATION;
}

AudioRendererRate FastAudioStream::GetRenderRate()
{
    return renderRate_;
}

int32_t FastAudioStream::SetStreamCallback(const std::shared_ptr<AudioStreamCallback> &callback)
{
    AUDIO_INFO_LOG("SetStreamCallback enter.");
    // note: need add support
    return SUCCESS;
}

int32_t FastAudioStream::SetRenderMode(AudioRenderMode renderMode)
{
    CHECK_AND_RETURN_RET_LOG(renderMode == RENDER_MODE_CALLBACK && eMode_ == AUDIO_MODE_PLAYBACK,
        ERR_INVALID_OPERATION, "SetRenderMode is not supported.");
    return SUCCESS;
}

AudioRenderMode FastAudioStream::GetRenderMode()
{
    AUDIO_INFO_LOG("GetRenderMode enter.");
    return renderMode_;
}

int32_t FastAudioStream::SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback)
{
    AUDIO_INFO_LOG("SetRendererWriteCallback enter.");
    CHECK_AND_RETURN_RET_LOG(callback && processClient_ && eMode_ == AUDIO_MODE_PLAYBACK,
        ERR_INVALID_PARAM, "callback is nullptr");
    spkProcClientCb_ = std::make_shared<FastAudioStreamRenderCallback>(callback, *this);
    int32_t ret = processClient_->SaveDataCallback(spkProcClientCb_);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "Client test save data callback fail, ret %{public}d.", ret);
    return SUCCESS;
}

int32_t FastAudioStream::SetRendererFirstFrameWritingCallback(
    const std::shared_ptr<AudioRendererFirstFrameWritingCallback> &callback)
{
    AUDIO_INFO_LOG("SetRendererFirstFrameWritingCallback in.");
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM, "callback is nullptr");
    firstFrameWritingCb_ = callback;
    return SUCCESS;
}

int32_t FastAudioStream::SetCaptureMode(AudioCaptureMode captureMode)
{
    CHECK_AND_RETURN_RET_LOG(captureMode == CAPTURE_MODE_CALLBACK && eMode_ == AUDIO_MODE_RECORD,
        ERR_INVALID_OPERATION, "SetCaptureMode is not supported.");
    return SUCCESS;
}

AudioCaptureMode FastAudioStream::GetCaptureMode()
{
    return captureMode_;
}

int32_t FastAudioStream::SetCapturerReadCallback(const std::shared_ptr<AudioCapturerReadCallback> &callback)
{
    AUDIO_INFO_LOG("SetCapturerReadCallback enter.");
    CHECK_AND_RETURN_RET_LOG(callback && processClient_ && eMode_ == AUDIO_MODE_RECORD,
        ERR_INVALID_PARAM, "callback or client is nullptr or mode is not record.");
    micProcClientCb_ = std::make_shared<FastAudioStreamCaptureCallback>(callback);
    int32_t ret = processClient_->SaveDataCallback(micProcClientCb_);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "Client save data callback fail, ret %{public}d.", ret);
    return SUCCESS;
}

int32_t FastAudioStream::GetBufferDesc(BufferDesc &bufDesc)
{
    AUDIO_DEBUG_LOG("GetBufferDesc enter.");
    CHECK_AND_RETURN_RET_LOG(processClient_, ERR_INVALID_OPERATION, "spkClient is null.");
    int32_t ret = processClient_->GetBufferDesc(bufDesc);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS && bufDesc.buffer != nullptr && bufDesc.bufLength != 0,
        -1, "GetBufferDesc failed.");
    return SUCCESS;
}

int32_t FastAudioStream::GetBufQueueState(BufferQueueState &bufState)
{
    AUDIO_INFO_LOG("GetBufQueueState enter.");
    // note: add support
    return SUCCESS;
}

int32_t FastAudioStream::Enqueue(const BufferDesc &bufDesc)
{
    AUDIO_DEBUG_LOG("Enqueue enter.");
    CHECK_AND_RETURN_RET_LOG(processClient_, ERR_INVALID_OPERATION,
        "spkClient is null.");
    int32_t ret = processClient_->Enqueue(bufDesc);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, -1, "Enqueue failed.");
    return SUCCESS;
}

void FastAudioStream::SetPreferredFrameSize(int32_t frameSize)
{
    std::lock_guard<std::mutex> lockSetPreferredFrameSize(setPreferredFrameSizeMutex_);
    userSettedPreferredFrameSize_ = frameSize;
    CHECK_AND_RETURN_LOG(processClient_ != nullptr, "process client is null.");
    processClient_->SetPreferredFrameSize(frameSize);
}

void FastAudioStream::UpdateLatencyTimestamp(std::string &timestamp, bool isRenderer)
{
    CHECK_AND_RETURN_LOG(processClient_ != nullptr, "process client is null.");
    processClient_->UpdateLatencyTimestamp(timestamp, isRenderer);
}

int32_t FastAudioStream::Clear()
{
    AUDIO_INFO_LOG("Clear will do nothing.");

    return SUCCESS;
}

int32_t FastAudioStream::SetLowPowerVolume(float volume)
{
    AUDIO_INFO_LOG("SetLowPowerVolume enter.");
    return SUCCESS;
}

float FastAudioStream::GetLowPowerVolume()
{
    AUDIO_INFO_LOG("GetLowPowerVolume enter.");
    return 1.0f;
}

int32_t FastAudioStream::SetOffloadMode(int32_t state, bool isAppBack)
{
    AUDIO_WARNING_LOG("SetOffloadMode enter.");
    return ERR_NOT_SUPPORTED;
}

int32_t FastAudioStream::UnsetOffloadMode()
{
    AUDIO_WARNING_LOG("UnsetOffloadMode enter.");
    return ERR_NOT_SUPPORTED;
}

float FastAudioStream::GetSingleStreamVolume()
{
    AUDIO_INFO_LOG("GetSingleStreamVolume enter.");
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

int32_t FastAudioStream::SetSpeed(float speed)
{
    AUDIO_ERR_LOG("SetSpeed is not supported");
    return ERR_OPERATION_FAILED;
}

float FastAudioStream::GetSpeed()
{
    AUDIO_ERR_LOG("GetSpeed is not supported");
    return static_cast<float>(ERROR);
}

int32_t FastAudioStream::ChangeSpeed(uint8_t *buffer, int32_t bufferSize,
    std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize)
{
    AUDIO_ERR_LOG("ChangeSpeed is not supported");
    return ERR_OPERATION_FAILED;
}

bool FastAudioStream::StartAudioStream(StateChangeCmdType cmdType,
    AudioStreamDeviceChangeReasonExt reason)
{
    AUDIO_PRERELEASE_LOGI("StartAudioStream enter.");
    CHECK_AND_RETURN_RET_LOG((state_ == PREPARED) || (state_ == STOPPED) || (state_ == PAUSED),
        false, "Illegal state:%{public}u", state_);

    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, false, "Start failed, process is null.");
    if (spkProcClientCb_ != nullptr) {
        AUDIO_DEBUG_LOG("StartAudioStream: reset the first frame state before starting");
        spkProcClientCb_->ResetFirstFrameState();
    }
    if (audioStreamTracker_ != nullptr && audioStreamTracker_.get()) {
        audioStreamTracker_->FetchOutputDeviceForTrack(sessionId_, RUNNING, clientPid_, rendererInfo_, reason);
        audioStreamTracker_->FetchInputDeviceForTrack(sessionId_, RUNNING, clientPid_, capturerInfo_);
    }
    int32_t ret = ERROR;
    if (state_ == PAUSED || state_ == STOPPED) {
        ret = processClient_->Resume();
    } else {
        ret = processClient_->Start();
    }
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, false, "Client test stop fail, ret %{public}d.", ret);
    state_ = RUNNING;

    AUDIO_DEBUG_LOG("StartAudioStream SUCCESS, sessionId: %{public}d", sessionId_);

    if (audioStreamTracker_ != nullptr && audioStreamTracker_.get()) {
        AUDIO_DEBUG_LOG("AudioStream:Calling Update tracker for Running");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, clientPid_, rendererInfo_, capturerInfo_);
    }

    return true;
}

bool FastAudioStream::PauseAudioStream(StateChangeCmdType cmdType)
{
    AUDIO_PRERELEASE_LOGI("PauseAudioStream enter.");
    CHECK_AND_RETURN_RET_LOG(state_ == RUNNING, false,
        "state is not RUNNING. Illegal state:%{public}u", state_);
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
        audioStreamTracker_->UpdateTracker(sessionId_, state_, clientPid_, rendererInfo_, capturerInfo_);
    }
    return true;
}

bool FastAudioStream::StopAudioStream()
{
    CHECK_AND_RETURN_RET_LOG((state_ == RUNNING) || (state_ == PAUSED), false,
        "State is not RUNNING. Illegal state:%{public}u", state_);
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
        audioStreamTracker_->UpdateTracker(sessionId_, state_, clientPid_, rendererInfo_, capturerInfo_);
    }
    return true;
}

bool FastAudioStream::FlushAudioStream()
{
    AUDIO_PRERELEASE_LOGI("FlushAudioStream enter.");
    return true;
}

bool FastAudioStream::DrainAudioStream(bool stopFlag)
{
    AUDIO_INFO_LOG("Drain stream SUCCESS");
    return true;
}

bool FastAudioStream::ReleaseAudioStream(bool releaseRunner, bool isSwitchStream)
{
    CHECK_AND_RETURN_RET_LOG(state_ != RELEASED && state_ != NEW,
        false, "Illegal state: state = %{public}u", state_);
    // If state_ is RUNNING try to Stop it first and Release
    if (state_ == RUNNING) {
        StopAudioStream();
    }

    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, false, "Release failed, process is null.");
    processClient_->Release(isSwitchStream);
    state_ = RELEASED;
    AUDIO_INFO_LOG("ReleaseAudiostream SUCCESS, sessionId: %{public}d", sessionId_);
    if (audioStreamTracker_ != nullptr && audioStreamTracker_.get()) {
        AUDIO_DEBUG_LOG("AudioStream:Calling Update tracker for release");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, clientPid_, rendererInfo_, capturerInfo_);
    }
    return true;
}

int32_t FastAudioStream::Read(uint8_t &buffer, size_t userSize, bool isBlockingRead)
{
    AUDIO_ERR_LOG("Unsupported operation: read");
    return ERR_INVALID_OPERATION;
}

int32_t FastAudioStream::Write(uint8_t *buffer, size_t buffer_size)
{
    AUDIO_ERR_LOG("Unsupported operation: Write");
    return ERR_INVALID_OPERATION;
}

int32_t FastAudioStream::Write(uint8_t *pcmBuffer, size_t pcmBufferSize, uint8_t *metaBuffer, size_t metaBufferSize)
{
    AUDIO_ERR_LOG("Unsupported operation: Write");
    return ERR_INVALID_OPERATION;
}

uint32_t FastAudioStream::GetUnderflowCount()
{
    AUDIO_INFO_LOG("GetUnderflowCount enter.");
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, 0, "process client is null.");
    underflowCount_ = processClient_->GetUnderflowCount();
    return underflowCount_;
}

uint32_t FastAudioStream::GetOverflowCount()
{
    AUDIO_INFO_LOG("enter.");
    CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, 0, "process client is null.");
    overflowCount_ = processClient_->GetOverflowCount();
    return overflowCount_;
}

void FastAudioStream::SetUnderflowCount(uint32_t underflowCount)
{
    CHECK_AND_RETURN_LOG(processClient_ != nullptr, "process client is null.");
    processClient_->SetUnderflowCount(underflowCount);
}

void FastAudioStream::SetOverflowCount(uint32_t overflowCount)
{
    CHECK_AND_RETURN_LOG(processClient_ != nullptr, "process client is null.");
    processClient_->SetOverflowCount(overflowCount);
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
    AUDIO_INFO_LOG("GetRendererSamplingRate enter.");
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

void FastAudioStream::SetCapturerSource(int capturerSource)
{
    AUDIO_ERR_LOG("SetCapturerSource is not supported");
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

    info.underFlowCount = GetUnderflowCount();
    info.overFlowCount = GetOverflowCount();

    info.silentModeAndMixWithOthers = silentModeAndMixWithOthers_;

    {
        std::lock_guard<std::mutex> lock(setPreferredFrameSizeMutex_);
        info.userSettedPreferredFrameSize = userSettedPreferredFrameSize_;
    }

    if (spkProcClientCb_) {
        info.rendererWriteCallback = spkProcClientCb_->GetRendererWriteCallback();
    }
    if (micProcClientCb_) {
        info.capturerReadCallback = micProcClientCb_->GetCapturerReadCallback();
    }
    if (firstFrameWritingCb_) {
        info.rendererFirstFrameWritingCallback = firstFrameWritingCb_;
    }
}

void FastAudioStream::OnFirstFrameWriting()
{
    CHECK_AND_RETURN_LOG(firstFrameWritingCb_!= nullptr, "firstFrameWritingCb_ is null.");
    uint64_t latency = 0;
    this->GetLatency(latency);
    firstFrameWritingCb_->OnFirstFrameWriting(latency);
}

void FastAudioStreamRenderCallback::OnHandleData(size_t length)
{
    CHECK_AND_RETURN_LOG(rendererWriteCallback_!= nullptr, "OnHandleData failed: rendererWriteCallback_ is null.");
    if (!hasFirstFrameWrited_) {
        AUDIO_DEBUG_LOG("OnHandleData: send the first frame writing event to audio haptic player");
        audioStreamImpl_.OnFirstFrameWriting();
        hasFirstFrameWrited_ = true;
    }
    rendererWriteCallback_->OnWriteData(length);
}

void FastAudioStreamRenderCallback::ResetFirstFrameState()
{
    AUDIO_DEBUG_LOG("ResetFirstFrameState: set the hasFirstFrameWrited_ to false");
    hasFirstFrameWrited_ = false;
}

std::shared_ptr<AudioRendererWriteCallback> FastAudioStreamRenderCallback::GetRendererWriteCallback() const
{
    return rendererWriteCallback_;
}

std::shared_ptr<AudioCapturerReadCallback> FastAudioStreamCaptureCallback::GetCapturerReadCallback() const
{
    return captureCallback_;
}

void FastAudioStreamCaptureCallback::OnHandleData(size_t length)
{
    CHECK_AND_RETURN_LOG(captureCallback_!= nullptr, "OnHandleData failed: captureCallback_ is null.");
    captureCallback_->OnReadData(length);
}

int32_t FastAudioStream::SetChannelBlendMode(ChannelBlendMode blendMode)
{
    AUDIO_ERR_LOG("SetChannelBlendMode is not supported");
    return SUCCESS;
}

int32_t FastAudioStream::SetVolumeWithRamp(float volume, int32_t duration)
{
    AUDIO_ERR_LOG("SetVolumeWithRamp is not supported");
    return SUCCESS;
}

void FastAudioStream::UpdateRegisterTrackerInfo(AudioRegisterTrackerInfo &registerTrackerInfo)
{
    rendererInfo_.samplingRate = static_cast<AudioSamplingRate>(streamInfo_.samplingRate);
    capturerInfo_.samplingRate = static_cast<AudioSamplingRate>(streamInfo_.samplingRate);

    registerTrackerInfo.sessionId = sessionId_;
    registerTrackerInfo.clientPid = clientPid_;
    registerTrackerInfo.state = state_;
    registerTrackerInfo.rendererInfo = rendererInfo_;
    registerTrackerInfo.capturerInfo = capturerInfo_;
}

bool FastAudioStream::RestoreAudioStream(bool needStoreState)
{
    CHECK_AND_RETURN_RET_LOG(proxyObj_ != nullptr, false, "proxyObj_ is null");
    CHECK_AND_RETURN_RET_LOG(state_ != NEW && state_ != INVALID && state_ != RELEASED, true,
        "state_ is %{public}d, no need for restore", state_);
    bool result = false;
    State oldState = state_;
    state_ = NEW;
    SetStreamTrackerState(false);
    if (processClient_ != nullptr) {
        processClient_->Stop();
        processClient_->Release();
        processClient_ = nullptr;
    }
    int32_t ret = SetAudioStreamInfo(streamInfo_, proxyObj_);
    if (ret != SUCCESS) {
        goto error;
    }
    switch (oldState) {
        case RUNNING:
            CHECK_AND_RETURN_RET_LOG(processClient_ != nullptr, false, "processClient_ is null");
            ret = processClient_->SaveDataCallback(spkProcClientCb_);
            if (ret != SUCCESS) {
                goto error;
            }
            result = StartAudioStream();
            break;
        case PAUSED:
            result = StartAudioStream() && PauseAudioStream();
            break;
        case STOPPED:
        case STOPPING:
            result = StartAudioStream() && StopAudioStream();
            break;
        default:
            break;
    }
    if (!result) {
        goto error;
    }
    return result;

error:
    AUDIO_ERR_LOG("RestoreAudioStream failed");
    state_ = oldState;
    return false;
}

bool FastAudioStream::GetOffloadEnable()
{
    AUDIO_WARNING_LOG("not supported in fast audio stream");
    return false;
}

bool FastAudioStream::GetSpatializationEnabled()
{
    AUDIO_WARNING_LOG("not supported in fast audio stream");
    return false;
}

bool FastAudioStream::GetHighResolutionEnabled()
{
    AUDIO_WARNING_LOG("not supported in fast audio stream");
    return false;
}
} // namespace AudioStandard
} // namespace OHOS
