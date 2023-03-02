/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "audio_info.h"
#include "audio_log.h"
#include "audio_container_stream_base.h"
#include "audio_policy_manager.h"
#include "audio_container_client_base.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
const unsigned long long TIME_CONVERSION_US_S = 1000000ULL; /* us to s */
const unsigned long long TIME_CONVERSION_NS_US = 1000ULL; /* ns to us */
const unsigned long long TIME_CONVERSION_NS_S = 1000000000ULL; /* ns to s */
constexpr int32_t WRITE_RETRY_DELAY_IN_US = 500;
constexpr int32_t READ_WRITE_WAIT_TIME_IN_US = 500;
constexpr int32_t CB_WRITE_BUFFERS_WAIT_IN_US = 500;
constexpr int32_t CB_READ_BUFFERS_WAIT_IN_US = 500;
constexpr int32_t AUDIO_RENDERER_SERVICE_GATEWAY_ID = 13011;
constexpr int32_t AUDIO_CAPTURER_SERVICE_GATEWAY_ID = 13013;


const map<pair<ContentType, StreamUsage>, AudioStreamType> AudioContainerStreamBase::streamTypeMap_ =
    AudioContainerStreamBase::CreateStreamMap();

map<pair<ContentType, StreamUsage>, AudioStreamType> AudioContainerStreamBase::CreateStreamMap()
{
    map<pair<ContentType, StreamUsage>, AudioStreamType> streamMap;

    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_UNKNOWN)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_MEDIA)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_UNKNOWN)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_MEDIA)] = STREAM_VOICE_ASSISTANT;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_VOICE_CALL;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_VOICE_ASSISTANT;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_UNKNOWN)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_MEDIA)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_VOICE_ASSISTANT;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_RING;

    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_UNKNOWN)] = STREAM_MEDIA;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_MEDIA)] = STREAM_MEDIA;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_UNKNOWN)] = STREAM_NOTIFICATION;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_MEDIA)] = STREAM_NOTIFICATION;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_UNKNOWN)] = STREAM_RING;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_MEDIA)] = STREAM_RING;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_RING;

    return streamMap;
}

AudioContainerStreamBase::AudioContainerStreamBase(AudioStreamType eStreamType, AudioMode eMode, int32_t appUid)
    : eStreamType_(eStreamType),
      eMode_(eMode),
      state_(NEW),
      isReadInProgress_(false),
      isWriteInProgress_(false),
      resetTime_(false),
      resetTimestamp_(0),
      renderMode_(RENDER_MODE_NORMAL),
      captureMode_(CAPTURE_MODE_NORMAL),
      isReadyToWrite_(false),
      isReadyToRead_(false),
      isFirstRead_(false)
{
    audioStreamTracker_ =  std::make_unique<AudioStreamTracker>(eMode, appUid);
    AUDIO_DEBUG_LOG("AudioContainerStreamBase::AudioContainerStreamBase");
}

AudioContainerStreamBase::~AudioContainerStreamBase()
{
    isReadyToWrite_ = false;
    isReadyToRead_ = false;

    if (writeThread_ && writeThread_->joinable()) {
        writeThread_->join();
    }

    if (readThread_ && readThread_->joinable()) {
        readThread_->join();
    }

    if (state_ != RELEASED && state_ != NEW) {
        ReleaseAudioStream();
    }
    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerStreamBase:~AudioContainerStreamBase:Calling update tracker");
        AudioRendererInfo rendererInfo = {};
        AudioCapturerInfo capturerInfo = {};
        state_ = RELEASED;
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo, capturerInfo);
    }
}
void AudioContainerStreamBase::SetRendererInfo(const AudioRendererInfo &rendererInfo)
{
    rendererInfo_ = rendererInfo;
}

void AudioContainerStreamBase::SetCapturerInfo(const AudioCapturerInfo &capturerInfo)
{
    capturerInfo_ = capturerInfo;
}

State AudioContainerStreamBase::GetState()
{
    return state_;
}

int32_t AudioContainerStreamBase::GetAudioSessionID(uint32_t &sessionID) const
{
    if ((state_ == RELEASED) || (state_ == NEW)) {
        return ERR_ILLEGAL_STATE;
    }

    if (GetSessionID(sessionID, trackId_) != 0) {
        return ERR_INVALID_INDEX;
    }

    return SUCCESS;
}

bool AudioContainerStreamBase::GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base)
{
    uint64_t paTimeStamp = 0;
    if (GetCurrentTimeStamp(paTimeStamp, trackId_) == SUCCESS) {
        if (resetTime_) {
            AUDIO_INFO_LOG("AudioContainerStreamBase::GetAudioTime resetTime_ %{public}d", resetTime_);
            resetTime_ = false;
            resetTimestamp_ = paTimeStamp;
        }

        timestamp.time.tv_sec = static_cast<time_t>((paTimeStamp - resetTimestamp_) / TIME_CONVERSION_US_S);
        timestamp.time.tv_nsec = static_cast<time_t>(((paTimeStamp - resetTimestamp_) -
            (timestamp.time.tv_sec * TIME_CONVERSION_US_S)) * TIME_CONVERSION_NS_US);
        timestamp.time.tv_sec += baseTimestamp_.tv_sec;
        timestamp.time.tv_nsec += baseTimestamp_.tv_nsec;
        timestamp.time.tv_sec += (timestamp.time.tv_nsec / TIME_CONVERSION_NS_S);
        timestamp.time.tv_nsec = (timestamp.time.tv_nsec % TIME_CONVERSION_NS_S);

        return true;
    }
    return false;
}

int32_t AudioContainerStreamBase::GetBufferSize(size_t &bufferSize) const
{
    if (GetMinimumBufferSize(bufferSize, trackId_) != 0) {
        return ERR_OPERATION_FAILED;
    }
    return SUCCESS;
}

int32_t AudioContainerStreamBase::GetFrameCount(uint32_t &frameCount) const
{
    AUDIO_INFO_LOG("AudioContainerStreamBase: Get frame count");
    if (GetMinimumFrameCount(frameCount, trackId_) != 0) {
        return ERR_OPERATION_FAILED;
    }

    return SUCCESS;
}

int32_t AudioContainerStreamBase::GetLatency(uint64_t &latency) const
{
    if (GetAudioLatency(latency, trackId_) != SUCCESS) {
        return ERR_OPERATION_FAILED;
    } else {
        return SUCCESS;
    }
}

vector<AudioSampleFormat> AudioContainerStreamBase::GetSupportedFormats() const
{
    return AUDIO_SUPPORTED_FORMATS;
}

vector<AudioEncodingType> AudioContainerStreamBase::GetSupportedEncodingTypes() const
{
    return AUDIO_SUPPORTED_ENCODING_TYPES;
}

vector<AudioSamplingRate> AudioContainerStreamBase::GetSupportedSamplingRates() const
{
    return AUDIO_SUPPORTED_SAMPLING_RATES;
}

bool IsFormatValid(uint8_t format)
{
    bool isValidFormat = (find(AUDIO_SUPPORTED_FORMATS.begin(), AUDIO_SUPPORTED_FORMATS.end(), format)
                          != AUDIO_SUPPORTED_FORMATS.end());
    AUDIO_DEBUG_LOG("AudioContainerStreamBase: IsFormatValid: %{public}s", isValidFormat ? "true" : "false");
    return isValidFormat;
}

bool IsRendererChannelValid(uint8_t channel)
{
    bool isValidChannel = (find(RENDERER_SUPPORTED_CHANNELS.begin(), RENDERER_SUPPORTED_CHANNELS.end(), channel)
                           != RENDERER_SUPPORTED_CHANNELS.end());
    AUDIO_DEBUG_LOG("AudioContainerStreamBase: IsChannelValid: %{public}s", isValidChannel ? "true" : "false");
    return isValidChannel;
}

bool IsCapturerChannelValid(uint8_t channel)
{
    bool isValidChannel = (find(CAPTURER_SUPPORTED_CHANNELS.begin(), CAPTURER_SUPPORTED_CHANNELS.end(), channel)
                           != CAPTURER_SUPPORTED_CHANNELS.end());
    AUDIO_DEBUG_LOG("AudioContainerStreamBase: IsChannelValid: %{public}s", isValidChannel ? "true" : "false");
    return isValidChannel;
}

bool IsEncodingTypeValid(uint8_t encodingType)
{
    bool isValidEncodingType = (find(AUDIO_SUPPORTED_ENCODING_TYPES.begin(), AUDIO_SUPPORTED_ENCODING_TYPES.end(),
        encodingType) != AUDIO_SUPPORTED_ENCODING_TYPES.end());
    AUDIO_DEBUG_LOG("AudioContainerStreamBase: IsEncodingTypeValid: %{public}s",
                    isValidEncodingType ? "true" : "false");
    return isValidEncodingType;
}

bool IsSamplingRateValid(uint32_t samplingRate)
{
    bool isValidSamplingRate = (find(AUDIO_SUPPORTED_SAMPLING_RATES.begin(), AUDIO_SUPPORTED_SAMPLING_RATES.end(),
        samplingRate) != AUDIO_SUPPORTED_SAMPLING_RATES.end());
    AUDIO_DEBUG_LOG("AudioContainerStreamBase: IsSamplingRateValid: %{public}s",
                    isValidSamplingRate ? "true" : "false");
    return isValidSamplingRate;
}

int32_t AudioContainerStreamBase::GetAudioStreamInfo(AudioStreamParams &audioStreamInfo)
{
    if (GetAudioStreamParams(audioStreamInfo, trackId_) != 0) {
        return ERR_OPERATION_FAILED;
    }

    return SUCCESS;
}

bool AudioContainerStreamBase::VerifyClientPermission(const std::string &permissionName, uint32_t appTokenId,
    int32_t appUid, bool privacyFlag, AudioPermissionState state)
{
    if (!AudioPolicyManager::GetInstance().VerifyClientPermission(permissionName, appTokenId, appUid,
        privacyFlag, state)) {
            AUDIO_ERR_LOG("AudioContainerStreamBase: Client doesn't have MICROPHONE permission");
            return false;
        }
    return true;
}

int32_t AudioContainerStreamBase::SetAudioStreamInfo(const AudioStreamParams info)
{
    if (!IsFormatValid(info.format) || !IsSamplingRateValid(info.samplingRate) || !IsEncodingTypeValid(info.encoding)) {
        AUDIO_ERR_LOG("AudioContainerStreamBase: Unsupported audio parameter");
        return ERR_NOT_SUPPORTED;
    }
    sample_rate_ = info.samplingRate;
    format_ = info.format;
    channelCount_ = info.channels;
    if (state_ != NEW) {
        AUDIO_DEBUG_LOG("AudioContainerStreamBase: State is not new, release existing stream");
        StopAudioStream();
        ReleaseAudioStream();
    }

    int32_t ret = 0;
    switch (eMode_) {
        case AUDIO_MODE_PLAYBACK:
            AUDIO_ERR_LOG("AudioContainerStreamBase: Initialize playback");
            if (!IsRendererChannelValid(info.channels)) {
                AUDIO_ERR_LOG("AudioContainerStreamBase: Invalid sink channel %{public}d", info.channels);
                return ERR_NOT_SUPPORTED;
            }
            ret = Initialize(AUDIO_SERVICE_CLIENT_PLAYBACK, AUDIO_RENDERER_SERVICE_GATEWAY_ID);
            break;
        case AUDIO_MODE_RECORD:
            AUDIO_ERR_LOG("AudioContainerStreamBase: Initialize recording");
            if (!IsCapturerChannelValid(info.channels)) {
                AUDIO_ERR_LOG("AudioContainerStreamBase: Invalid source channel %{public}d", info.channels);
                return ERR_NOT_SUPPORTED;
            }
            ret = Initialize(AUDIO_SERVICE_CLIENT_RECORD, AUDIO_CAPTURER_SERVICE_GATEWAY_ID);
            break;
        default:
            return ERR_INVALID_OPERATION;
    }

    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("AudioContainerStreamBase: Error initializing!");
        return ret;
    }
    trackId_ = CreateStream(info, eStreamType_);
    if (trackId_ < 0) {
        AUDIO_ERR_LOG("AudioContainerStreamBase: Create stream failed");
        return ERROR;
    }

    state_ = PREPARED;
    if (audioStreamTracker_) {
        (void)GetAudioSessionID(sessionId_);
        AUDIO_DEBUG_LOG("AudioContainerStreamBase:Calling register tracker, sessionid = %{public}d", sessionId_);
        audioStreamTracker_->RegisterTracker(sessionId_, state_, rendererInfo_, capturerInfo_, nullptr);
    }
    AUDIO_INFO_LOG("AudioContainerStreamBase: CreateStream trackId_ %{public}d", trackId_);
    return SUCCESS;
}

bool AudioContainerStreamBase::StartAudioStream()
{
    if ((state_ != PREPARED) && (state_ != STOPPED) && (state_ != PAUSED)) {
        AUDIO_ERR_LOG("StartAudioStream Illegal state:%{public}u", state_);
        return false;
    }

    int32_t ret = StartStream(trackId_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("StartStream Start failed:%{public}d", ret);
        return false;
    }

    resetTime_ = true;
    int32_t retCode = clock_gettime(CLOCK_MONOTONIC, &baseTimestamp_);
    if (retCode != 0) {
        AUDIO_ERR_LOG("AudioContainerStreamBase::StartAudioStream get system elapsed time failed: %d", retCode);
    }

    if (renderMode_ == RENDER_MODE_CALLBACK) {
        isReadyToWrite_ = true;
        if (writeThread_ == nullptr) {
            writeThread_ = std::make_unique<std::thread>(&AudioContainerStreamBase::WriteBuffers, this);
        }
    } else if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        isReadyToRead_ = true;
        readThread_ = std::make_unique<std::thread>(&AudioContainerStreamBase::ReadBuffers, this);
    }

    isFirstRead_ = true;
    state_ = RUNNING;
    pthread_cond_signal(&writeCondition_);
    AUDIO_INFO_LOG("AudioContainerStreamBase::StartAudioStream SUCCESS trackId_ %{public}d", trackId_);
    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerStreamBase:Calling Update tracker for Running");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    return true;
}

int32_t AudioContainerStreamBase::Read(uint8_t &buffer, size_t userSize, bool isBlockingRead)
{
    if (userSize <= 0) {
        AUDIO_ERR_LOG("Invalid userSize:%{public}zu", userSize);
        return ERR_INVALID_PARAM;
    }

    if (state_ != RUNNING) {
        AUDIO_ERR_LOG("Read: State is not RUNNNIG. Illegal  state:%{public}u", state_);
        return ERR_ILLEGAL_STATE;
    }

    if (isFirstRead_) {
        FlushAudioStream();
        isFirstRead_ = false;
    }

    StreamBuffer stream;
    stream.buffer = &buffer;
    stream.bufferLen = userSize;
    isReadInProgress_ = true;
    int32_t readLen = ReadStream(stream, isBlockingRead, trackId_);
    isReadInProgress_ = false;
    if (readLen < 0) {
        AUDIO_ERR_LOG("ReadStream fail,ret:%{public}d", readLen);
        return ERR_INVALID_READ;
    }

    return readLen;
}

size_t AudioContainerStreamBase::Write(uint8_t *buffer, size_t buffer_size)
{
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        AUDIO_ERR_LOG("AudioContainerStreamBase::Write not supported. RenderMode is callback");
        return ERR_INCORRECT_MODE;
    }

    if ((buffer == nullptr) || (buffer_size <= 0)) {
        AUDIO_ERR_LOG("Invalid buffer size:%{public}zu", buffer_size);
        return ERR_INVALID_PARAM;
    }

    if (state_ != RUNNING) {
        // To allow context switch for APIs running in different thread contexts
        std::this_thread::sleep_for(std::chrono::microseconds(WRITE_RETRY_DELAY_IN_US));
        return ERR_ILLEGAL_STATE;
    }

    int32_t writeError;
    StreamBuffer stream;
    stream.buffer = buffer;
    stream.bufferLen = buffer_size;
    isWriteInProgress_ = true;
    size_t bytesWritten = WriteStream(stream, writeError, trackId_);
    isWriteInProgress_ = false;
    if (writeError != 0) {
        AUDIO_ERR_LOG("WriteStream fail,writeError:%{public}d", writeError);
        return ERR_WRITE_FAILED;
    }
    return bytesWritten;
}

bool AudioContainerStreamBase::PauseAudioStream()
{
    if (state_ != RUNNING) {
        AUDIO_ERR_LOG("PauseAudioStream: State is not RUNNING. Illegal state:%{public}u", state_);
        return false;
    }
    State oldState = state_;
    state_ = PAUSED; // Set it before stopping as Read/Write and Stop can be called from different threads
    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        isReadyToRead_ = false;
        if (readThread_ && readThread_->joinable()) {
            readThread_->join();
        }
    }
    while (isReadInProgress_ || isWriteInProgress_) {
        std::this_thread::sleep_for(std::chrono::microseconds(READ_WRITE_WAIT_TIME_IN_US));
    }

    int32_t ret = PauseStream(trackId_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("StreamPause fail, ret:%{public}d", ret);
        state_ = oldState;
        return false;
    }
    AUDIO_DEBUG_LOG("PauseAudioStream SUCCESS");

    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerStreamBase:Calling Update tracker for Pause");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    return true;
}

bool AudioContainerStreamBase::StopAudioStream()
{
    if (state_ == PAUSED) {
        state_ = STOPPED;
        AUDIO_INFO_LOG("StopAudioStream SUCCESS");
        return true;
    }

    if (state_ != RUNNING) {
        AUDIO_ERR_LOG("StopAudioStream: State is not RUNNING. Illegal state:%{public}u", state_);
        return false;
    }
    State oldState = state_;
    state_ = STOPPED; // Set it before stopping as Read/Write and Stop can be called from different threads
    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        isReadyToRead_ = false;
        if (readThread_ && readThread_->joinable()) {
            readThread_->join();
        }
    }
    while (isReadInProgress_ || isWriteInProgress_) {
        std::this_thread::sleep_for(std::chrono::microseconds(READ_WRITE_WAIT_TIME_IN_US));
    }

    int32_t ret = StopStream(trackId_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("StreamStop fail, ret:%{public}d", ret);
        state_ = oldState;
        return false;
    }

    // Ends the WriteBuffers thread
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        isReadyToWrite_ = false;
        pthread_cond_signal(&writeCondition_);
    }

    AUDIO_DEBUG_LOG("StopAudioStream SUCCESS");

    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerStreamBase:Calling Update tracker for stop");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    return true;
}

bool AudioContainerStreamBase::FlushAudioStream()
{
    if ((state_ != RUNNING) && (state_ != PAUSED) && (state_ != STOPPED)) {
        AUDIO_ERR_LOG("FlushAudioStream: State is not RUNNING. Illegal state:%{public}u", state_);
        return false;
    }

    int32_t ret = FlushStream(trackId_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("Flush stream fail,ret:%{public}d", ret);
        return false;
    }

    AUDIO_DEBUG_LOG("Flush stream SUCCESS");
    return true;
}

bool AudioContainerStreamBase::DrainAudioStream()
{
    if (state_ != RUNNING) {
        AUDIO_ERR_LOG("DrainAudioStream: State is not RUNNING. Illegal  state:%{public}u", state_);
        return false;
    }

    int32_t ret = DrainStream(trackId_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("Drain stream fail,ret:%{public}d", ret);
        return false;
    }

    AUDIO_DEBUG_LOG("Drain stream SUCCESS");
    return true;
}

bool AudioContainerStreamBase::ReleaseAudioStream()
{
    if (state_ == RELEASED || state_ == NEW) {
        AUDIO_ERR_LOG("Illegal state: state = %{public}u", state_);
        return false;
    }
    // If state_ is RUNNING try to Stop it first and Release
    if (state_ == RUNNING || state_ == PAUSED) {
        StopAudioStream();
    }

    ReleaseStream(trackId_);
    state_ = RELEASED;
    AUDIO_DEBUG_LOG("ReleaseAudiostream SUCCESS");
    pthread_cond_signal(&writeCondition_);

    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerStreamBase:Calling Update tracker for release");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    return true;
}

AudioStreamType AudioContainerStreamBase::GetStreamType(ContentType contentType, StreamUsage streamUsage)
{
    AudioStreamType streamType = STREAM_MUSIC;
    auto pos = streamTypeMap_.find(make_pair(contentType, streamUsage));
    if (pos != streamTypeMap_.end()) {
        streamType = pos->second;
    }

    if (streamType == STREAM_MEDIA) {
        streamType = STREAM_MUSIC;
    }

    return streamType;
}

int32_t AudioContainerStreamBase::SetAudioStreamType(AudioStreamType audioStreamType)
{
    return SetStreamType(audioStreamType, trackId_);
}

int32_t AudioContainerStreamBase::SetVolume(float volume)
{
    return SetStreamVolume(volume, trackId_);
}

float AudioContainerStreamBase::GetVolume()
{
    return GetStreamVolume(trackId_);
}

int32_t AudioContainerStreamBase::SetRenderRate(AudioRendererRate renderRate)
{
    switch (renderRate) {
        case RENDER_RATE_NORMAL:
        case RENDER_RATE_DOUBLE:
        case RENDER_RATE_HALF:
            break;
        default:
            return AUDIO_CLIENT_INVALID_PARAMS_ERR;
    }
    renderRate_ = renderRate;
    return SetStreamRenderRate(renderRate, trackId_);
}

AudioRendererRate AudioContainerStreamBase::GetRenderRate()
{
    return GetStreamRenderRate();
}

int32_t AudioContainerStreamBase::SetStreamCallback(const std::shared_ptr<AudioStreamCallback> &callback)
{
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioContainerStreamBase::SetStreamCallback failed. callback == nullptr");
        return ERR_INVALID_PARAM;
    }

    SaveStreamCallback(callback);

    return SUCCESS;
}

int32_t AudioContainerStreamBase::SetRenderMode(AudioRenderMode renderMode)
{
    int32_t ret = SetAudioRenderMode(renderMode, trackId_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("AudioContainerStreamBase::SetRenderMode: renderMode: %{public}d failed", renderMode);
        return ERR_OPERATION_FAILED;
    }
    renderMode_ = renderMode;

    for (int32_t i = 0; i < MAX_LEN_BUFFERS; ++i) {
        size_t length;
        GetMinimumBufferSize(length, trackId_);
        AUDIO_DEBUG_LOG("AudioContainerStreamBase::GetMinimumBufferSize: %{public}zu", length);

        bufferPool_[i] = std::make_unique<uint8_t[]>(length);
        if (bufferPool_[i] == nullptr) {
            AUDIO_ERR_LOG("GetBufferDescriptor bufferPool_[i]==nullptr. Allocate memory failed.");
            return ERR_OPERATION_FAILED;
        }

        BufferDesc bufDesc {};
        bufDesc.buffer = bufferPool_[i].get();
        bufDesc.bufLength = length;
        freeBufferQ_.emplace(bufDesc);
    }

    return SUCCESS;
}

AudioRenderMode AudioContainerStreamBase::GetRenderMode()
{
    return GetAudioRenderMode();
}

int32_t AudioContainerStreamBase::SetCaptureMode(AudioCaptureMode captureMode)
{
    int32_t ret = SetAudioCaptureMode(captureMode);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("AudioContainerStreamBase::SetCaptureMode: captureMode: %{public}d failed", captureMode);
        return ERR_OPERATION_FAILED;
    }
    captureMode_ = captureMode;

    for (int32_t i = 0; i < MAX_LEN_BUFFERS; ++i) {
        size_t length;
        GetMinimumBufferSize(length, trackId_);
        AUDIO_DEBUG_LOG("AudioContainerStreamBase::SetCaptureMode: length %{public}zu", length);

        bufferPool_[i] = std::make_unique<uint8_t[]>(length);
        if (bufferPool_[i] == nullptr) {
            AUDIO_ERR_LOG("AudioContainerStreamBase::SetCaptureMode bufferPool_[i]==nullptr. Allocate memory failed.");
            return ERR_OPERATION_FAILED;
        }

        BufferDesc bufDesc {};
        bufDesc.buffer = bufferPool_[i].get();
        bufDesc.bufLength = length;
        freeBufferQ_.emplace(bufDesc);
    }

    return SUCCESS;
}

AudioCaptureMode AudioContainerStreamBase::GetCaptureMode()
{
    return GetAudioCaptureMode();
}

int32_t AudioContainerStreamBase::SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback)
{
    if (renderMode_ != RENDER_MODE_CALLBACK) {
        AUDIO_ERR_LOG("AudioContainerStreamBase::SetRendererWriteCallback not supported. Render mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    callback_ = callback;
    return SUCCESS;
}

int32_t AudioContainerStreamBase::SetCapturerReadCallback(const std::shared_ptr<AudioCapturerReadCallback> &callback)
{
    if (captureMode_ != CAPTURE_MODE_CALLBACK) {
        AUDIO_ERR_LOG("AudioContainerStreamBase::SetCapturerReadCallback not supported. Capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    int32_t ret = SaveReadCallback(callback);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("AudioContainerStreamBase::SetCapturerReadCallback: failed.");
        return ERR_INVALID_PARAM;
    }

    return SUCCESS;
}

int32_t AudioContainerStreamBase::GetBufferDesc(BufferDesc &bufDesc)
{
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("GetBufferDesc not supported. Render or Capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    AUDIO_DEBUG_LOG("freeBufferQ_ count %{public}zu", freeBufferQ_.size());
    AUDIO_DEBUG_LOG("filledBufferQ_ count %{public}zu", filledBufferQ_.size());

    lock_guard<mutex> lock(mBufferQueueLock);
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        if (!freeBufferQ_.empty()) {
            bufDesc.buffer = freeBufferQ_.front().buffer;
            bufDesc.bufLength = freeBufferQ_.front().bufLength;
            freeBufferQ_.pop();
        } else {
            bufDesc.buffer = nullptr;
            AUDIO_ERR_LOG("GetBufferDesc freeBufferQ_.empty()");
            return ERR_OPERATION_FAILED;
        }
    }

    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        if (!filledBufferQ_.empty()) {
            bufDesc.buffer = filledBufferQ_.front().buffer;
            bufDesc.bufLength = filledBufferQ_.front().bufLength;
            bufDesc.dataLength = filledBufferQ_.front().dataLength;
            filledBufferQ_.pop();
        } else {
            bufDesc.buffer = nullptr;
            AUDIO_ERR_LOG("GetBufferDesc filledBufferQ_.empty()");
            return ERR_OPERATION_FAILED;
        }
    }
    return SUCCESS;
}

int32_t AudioContainerStreamBase::GetBufQueueState(BufferQueueState &bufState)
{
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("GetBufQueueState not supported. Render or Capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    lock_guard<mutex> lock(mBufferQueueLock);
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        AUDIO_DEBUG_LOG("GetBufQueueState: bufferlength: %{public}zu.", filledBufferQ_.size());
        bufState.numBuffers = filledBufferQ_.size();
    }

    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        bufState.numBuffers = freeBufferQ_.size();
    }

    return SUCCESS;
}

int32_t AudioContainerStreamBase::Enqueue(const BufferDesc &bufDesc)
{
    AUDIO_DEBUG_LOG("AudioContainerStreamBase::Enqueue");
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("Enqueue not supported. Render or capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    if (bufDesc.buffer == nullptr) {
        AUDIO_ERR_LOG("Enqueue: failed. bufDesc.buffer == nullptr.");
        return ERR_INVALID_PARAM;
    }

    lock_guard<mutex> lock(mBufferQueueLock);
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        AUDIO_INFO_LOG("Enqueue: filledBuffer length: %{public}zu.", bufDesc.bufLength);
        filledBufferQ_.emplace(bufDesc);
    }

    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        AUDIO_INFO_LOG("Enqueue: freeBuffer length: %{public}zu.", bufDesc.bufLength);
        freeBufferQ_.emplace(bufDesc);
    }

    return SUCCESS;
}

int32_t AudioContainerStreamBase::Clear()
{
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("Clear not supported. Render or capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    lock_guard<mutex> lock(mBufferQueueLock);
    while (!filledBufferQ_.empty()) {
        freeBufferQ_.emplace(filledBufferQ_.front());
        filledBufferQ_.pop();
    }

    return SUCCESS;
}

void AudioContainerStreamBase::WriteBuffers()
{
    StreamBuffer stream;
    size_t bytesWritten;
    int32_t writeError;
    while (isReadyToWrite_) {
        size_t callback_size;
        GetMinimumBufferSize(callback_size, trackId_);
        for (size_t i = 0; i < freeBufferQ_.size(); ++i) {
            callback_->OnWriteData(callback_size);
        }
        while (!filledBufferQ_.empty()) {
            if (state_ != RUNNING) {
                AUDIO_ERR_LOG("Write: Illegal state:%{public}u", state_);
                break;
            }
            if ((state_ != STOPPED) && (state_ != PAUSED)) {
                stream.buffer = filledBufferQ_.front().buffer;
                stream.bufferLen = filledBufferQ_.front().bufLength;
                if (stream.buffer == nullptr) {
                    AUDIO_ERR_LOG("WriteBuffers stream.buffer == nullptr return");
                    return;
                }
                bytesWritten = WriteStreamInCb(stream, writeError, trackId_);
                if (writeError != 0) {
                    AUDIO_ERR_LOG("WriteStreamInCb fail, writeError:%{public}d", writeError);
                } else {
                    AUDIO_INFO_LOG("WriteBuffers WriteStream, bytesWritten:%{public}zu", bytesWritten);
                    freeBufferQ_.emplace(filledBufferQ_.front());
                    filledBufferQ_.pop();
                }
                callback_->OnWriteData(callback_size);
            } else {
                pthread_cond_wait(&writeCondition_, &writeLock_);
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(CB_WRITE_BUFFERS_WAIT_IN_US));
    }
}

void AudioContainerStreamBase::ReadBuffers()
{
    AUDIO_INFO_LOG("ReadBuffers thread start");
    StreamBuffer stream;
    int32_t readLen;
    bool isBlockingRead = true;

    while (isReadyToRead_) {
        while (!freeBufferQ_.empty()) {
            if (state_ != RUNNING) {
                AUDIO_ERR_LOG("ReadBuffers Read: Illegal state:%{public}u", state_);
                isReadyToRead_ = false;
                return;
            }
            AUDIO_DEBUG_LOG("ReadBuffers !freeBufferQ_.empty()");
            stream.buffer = freeBufferQ_.front().buffer;
            stream.bufferLen = freeBufferQ_.front().bufLength;
            AUDIO_DEBUG_LOG("ReadBuffers requested stream.bufferLen:%{public}d", stream.bufferLen);
            if (stream.buffer == nullptr) {
                AUDIO_ERR_LOG("ReadBuffers stream.buffer == nullptr return");
                return;
            }
            readLen = ReadStream(stream, isBlockingRead, trackId_);
            if (readLen < 0) {
                AUDIO_ERR_LOG("ReadBuffers ReadStream fail, ret: %{public}d", readLen);
            } else {
                AUDIO_DEBUG_LOG("ReadBuffers ReadStream, bytesRead:%{public}d", readLen);
                freeBufferQ_.front().dataLength = readLen;
                filledBufferQ_.emplace(freeBufferQ_.front());
                freeBufferQ_.pop();
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(CB_READ_BUFFERS_WAIT_IN_US));
    }

    AUDIO_INFO_LOG("ReadBuffers thread end");
}

void AudioContainerStreamBase::SetApplicationCachePath(const std::string cachePath)
{
    SetAppCachePath(cachePath);
}

void AudioContainerStreamBase::SetClientID(int32_t clientPid, int32_t clientUid)
{
    AUDIO_DEBUG_LOG("Set client PID: %{public}d, UID: %{public}d", clientPid, clientUid);
}

AudioContainerCaptureStream::AudioContainerCaptureStream(AudioStreamType eStreamType, AudioMode eMode, int32_t appUid)
    : AudioContainerStreamBase(eStreamType, eMode, appUid)
{
    AUDIO_INFO_LOG("AudioContainerCaptureStream::AudioContainerCaptureStream");
}

AudioContainerCaptureStream::~AudioContainerCaptureStream()
{
    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerCaptureStream:~AudioContainerCaptureStream:Calling update tracker");
        AudioRendererInfo rendererInfo = {};
        AudioCapturerInfo capturerInfo = {};
        state_ = RELEASED;
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo, capturerInfo);
    }
}

bool AudioContainerCaptureStream::StartAudioStream()
{
    if ((state_ != PREPARED) && (state_ != STOPPED) && (state_ != PAUSED)) {
        AUDIO_ERR_LOG("StartAudioStream Illegal state:%{public}u", state_);
        return false;
    }

    pthread_cond_signal(&writeCondition_);
    int32_t ret = StartStream(trackId_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("StartStream Start failed:%{public}d", ret);
        return false;
    }

    resetTime_ = true;
    int32_t retCode = clock_gettime(CLOCK_MONOTONIC, &baseTimestamp_);
    if (retCode != 0) {
        AUDIO_ERR_LOG("AudioContainerCaptureStream::StartAudioStream get system elapsed time failed: %d", retCode);
    }

    if (renderMode_ == RENDER_MODE_CALLBACK) {
        isReadyToWrite_ = true;
        if (writeThread_ == nullptr) {
            writeThread_ = std::make_unique<std::thread>(&AudioContainerStreamBase::WriteBuffers, this);
        }
    } else if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        isReadyToRead_ = true;
        readThread_ = std::make_unique<std::thread>(&AudioContainerStreamBase::ReadBuffers, this);
    }

    isFirstRead_ = true;
    state_ = RUNNING;
    AUDIO_INFO_LOG("AudioContainerCaptureStream::StartAudioStream SUCCESS trackId_ %{public}d", trackId_);
    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerCaptureStream:Calling Update tracker for Running");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    return true;
}

bool AudioContainerCaptureStream::StopAudioStream()
{
    if (state_ == PAUSED) {
        state_ = STOPPED;
        AUDIO_INFO_LOG("StopAudioStream SUCCESS");
        return true;
    }

    if (state_ != RUNNING) {
        AUDIO_ERR_LOG("StopAudioStream: State is not RUNNING. Illegal state:%{public}u", state_);
        return false;
    }
    State oldState = state_;
    state_ = STOPPED; // Set it before stopping as Read/Write and Stop can be called from different threads
    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        isReadyToRead_ = false;
        if (readThread_ && readThread_->joinable()) {
            readThread_->join();
        }
        readThread_ = nullptr;
    }
    while (isReadInProgress_ || isWriteInProgress_) {
        std::this_thread::sleep_for(std::chrono::microseconds(READ_WRITE_WAIT_TIME_IN_US));
    }

    int32_t ret = StopStream(trackId_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("StreamStop fail, ret:%{public}d", ret);
        state_ = oldState;
        return false;
    }

    // Ends the WriteBuffers thread
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        isReadyToWrite_ = false;
    }

    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerCaptureStream:Calling Update tracker for stop");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    AUDIO_DEBUG_LOG("StopAudioStream SUCCESS");

    return true;
}

int32_t AudioContainerCaptureStream::GetBufferDesc(BufferDesc & bufDesc)
{
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("GetBufferDesc not supported. Render or Capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    AUDIO_DEBUG_LOG("AudioContainerStreamBase::freeBufferQ_ count %{public}zu", freeBufferQ_.size());
    AUDIO_DEBUG_LOG("AudioContainerStreamBase::filledBufferQ_ count %{public}zu", filledBufferQ_.size());

    if (renderMode_ == RENDER_MODE_CALLBACK) {
        if (!freeBufferQ_.empty()) {
            bufDesc.buffer = freeBufferQ_.front().buffer;
            bufDesc.bufLength = freeBufferQ_.front().bufLength;
            bufDesc.dataLength = freeBufferQ_.front().dataLength;
            freeBufferQ_.pop();
        } else {
            bufDesc.buffer = nullptr;
            AUDIO_ERR_LOG("AudioContainerCaptureStream::GetBufferDesc freeBufferQ_.empty()");
            return ERR_OPERATION_FAILED;
        }
    }

    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        if (!filledBufferQ_.empty()) {
            bufDesc.buffer = filledBufferQ_.front().buffer;
            bufDesc.bufLength = filledBufferQ_.front().bufLength;
            bufDesc.dataLength = filledBufferQ_.front().dataLength;
            filledBufferQ_.pop();
        } else {
            bufDesc.buffer = nullptr;
            AUDIO_ERR_LOG("AudioContainerCaptureStream::GetBufferDesc filledBufferQ_.empty()");
            return ERR_OPERATION_FAILED;
        }
    }
    return SUCCESS;
}

int32_t AudioContainerCaptureStream::GetBufQueueState(BufferQueueState &bufState)
{
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("GetBufQueueState not supported. Render or Capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    if (renderMode_ == RENDER_MODE_CALLBACK) {
        bufState.numBuffers = filledBufferQ_.size();
    }

    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        bufState.numBuffers = freeBufferQ_.size();
    }

    return SUCCESS;
}

int32_t AudioContainerCaptureStream::Enqueue(const BufferDesc &bufDesc)
{
    AUDIO_DEBUG_LOG("AudioContainerCaptureStream::Enqueue");
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("AudioContainerCaptureStream::Enqueue not supported. Render or capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    if (bufDesc.buffer == nullptr) {
        AUDIO_ERR_LOG("AudioContainerCaptureStream::Enqueue: failed. bufDesc.buffer == nullptr.");
        return ERR_INVALID_PARAM;
    }

    if (renderMode_ == RENDER_MODE_CALLBACK) {
        AUDIO_INFO_LOG("AudioContainerCaptureStream::Enqueue: filledBuffer length: %{public}zu.", bufDesc.bufLength);
        filledBufferQ_.emplace(bufDesc);
    }

    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        AUDIO_INFO_LOG("AudioContainerCaptureStream::Enqueue: freeBuffer length: %{public}zu.", bufDesc.bufLength);
        freeBufferQ_.emplace(bufDesc);
    }

    return SUCCESS;
}

int32_t AudioContainerCaptureStream::Clear()
{
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("AudioContainerCaptureStream::Clear not supported. Render or capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    while (!filledBufferQ_.empty()) {
        freeBufferQ_.emplace(filledBufferQ_.front());
        filledBufferQ_.pop();
    }

    return SUCCESS;
}

AudioContainerRenderStream::AudioContainerRenderStream(AudioStreamType eStreamType, AudioMode eMode, int32_t appUid)
    : AudioContainerStreamBase(eStreamType, eMode, appUid)
{
    AUDIO_INFO_LOG("AudioContainerRenderStream::AudioContainerRenderStream");
}

AudioContainerRenderStream::~AudioContainerRenderStream()
{
    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerRenderStream:~AudioContainerRenderStream:Calling update tracker");
        AudioRendererInfo rendererInfo = {};
        AudioCapturerInfo capturerInfo = {};
        state_ = RELEASED;
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo, capturerInfo);
    }
}

bool AudioContainerRenderStream::StartAudioStream()
{
    AUDIO_INFO_LOG("AudioContainerRenderStream::StartAudioStream");
    if ((state_ != PREPARED) && (state_ != STOPPED) && (state_ != PAUSED)) {
        AUDIO_ERR_LOG("StartAudioStream Illegal state:%{public}u", state_);
        return false;
    }

    int32_t ret = StartStream(trackId_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("StartStream Start failed:%{public}d", ret);
        return false;
    }

    resetTime_ = true;
    int32_t retCode = clock_gettime(CLOCK_MONOTONIC, &baseTimestamp_);
    if (retCode != 0) {
        AUDIO_ERR_LOG("AudioContainerRenderStream::StartAudioStream get system elapsed time failed: %d", retCode);
    }

    if (renderMode_ == RENDER_MODE_CALLBACK) {
        isReadyToWrite_ = true;
        if (writeThread_ == nullptr) {
            writeThread_ = std::make_unique<std::thread>(&AudioContainerStreamBase::WriteBuffers, this);
        }
    } else if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        isReadyToRead_ = true;
        readThread_ = std::make_unique<std::thread>(&AudioContainerStreamBase::ReadBuffers, this);
    }

    isFirstRead_ = true;
    state_ = RUNNING;
    pthread_cond_signal(&writeCondition_);
    AUDIO_INFO_LOG("AudioContainerRenderStream::StartAudioStream SUCCESS trackId_ %{public}d", trackId_);
    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerRenderStream:Calling Update tracker for Running");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }
    return true;
}

bool AudioContainerRenderStream::StopAudioStream()
{
    if (state_ != RUNNING && state_ != PAUSED) {
        AUDIO_ERR_LOG("StopAudioStream: State is not RUNNING. Illegal state:%{public}u", state_);
        return false;
    }
    State oldState = state_;
    state_ = STOPPED; // Set it before stopping as Read/Write and Stop can be called from different threads
    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        isReadyToRead_ = false;
        if (readThread_ && readThread_->joinable()) {
            readThread_->join();
        }
    }
    while (isReadInProgress_ || isWriteInProgress_) {
        std::this_thread::sleep_for(std::chrono::microseconds(READ_WRITE_WAIT_TIME_IN_US));
    }

    int32_t ret = StopStream(trackId_);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("AudioContainerRenderStream::StreamStop fail, ret:%{public}d", ret);
        state_ = oldState;
        return false;
    }

    // Ends the WriteBuffers thread
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        isReadyToWrite_ = false;
        pthread_cond_signal(&writeCondition_);
        if (writeThread_ && writeThread_->joinable()) {
            writeThread_->join();
        }
        writeThread_ = nullptr;
    }

    AUDIO_DEBUG_LOG("AudioContainerRenderStream::StopAudioStream SUCCESS");
    if (audioStreamTracker_) {
        AUDIO_DEBUG_LOG("AudioContainerRenderStream:Calling Update tracker for stop");
        audioStreamTracker_->UpdateTracker(sessionId_, state_, rendererInfo_, capturerInfo_);
    }

    return true;
}

int32_t AudioContainerRenderStream::GetBufferDesc(BufferDesc &bufDesc)
{
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("GetBufferDesc not supported. Render or Capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    AUDIO_DEBUG_LOG("AudioContainerStreamBase::freeBufferQ_ count %{public}zu", freeBufferQ_.size());
    AUDIO_DEBUG_LOG("AudioContainerStreamBase::filledBufferQ_ count %{public}zu", filledBufferQ_.size());

    lock_guard<mutex> lock(mBufferQueueLock);
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        if (!freeBufferQ_.empty()) {
            bufDesc.buffer = freeBufferQ_.front().buffer;
            bufDesc.bufLength = freeBufferQ_.front().bufLength;
            freeBufferQ_.pop();
        } else {
            bufDesc.buffer = nullptr;
            AUDIO_ERR_LOG("AudioContainerRenderStream::GetBufferDesc freeBufferQ_.empty()");
            return ERR_OPERATION_FAILED;
        }
    }

    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        if (!filledBufferQ_.empty()) {
            bufDesc.buffer = filledBufferQ_.front().buffer;
            bufDesc.bufLength = filledBufferQ_.front().bufLength;
            bufDesc.dataLength = filledBufferQ_.front().dataLength;
            filledBufferQ_.pop();
        } else {
            bufDesc.buffer = nullptr;
            AUDIO_ERR_LOG("AudioContainerRenderStream::GetBufferDesc filledBufferQ_.empty()");
            return ERR_OPERATION_FAILED;
        }
    }
    return SUCCESS;
}

int32_t AudioContainerRenderStream::GetBufQueueState(BufferQueueState &bufState)
{
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("GetBufQueueState not supported. Render or Capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    lock_guard<mutex> lock(mBufferQueueLock);
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        AUDIO_DEBUG_LOG("GetBufQueueState: bufferlength: %{public}zu.", filledBufferQ_.size());
        bufState.numBuffers = filledBufferQ_.size();
    }

    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        bufState.numBuffers = freeBufferQ_.size();
    }

    return SUCCESS;
}

int32_t AudioContainerRenderStream::Enqueue(const BufferDesc &bufDesc)
{
    AUDIO_DEBUG_LOG("AudioContainerRenderStream::Enqueue");
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("AudioContainerRenderStream::Enqueue not supported. Render or capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    if (bufDesc.buffer == nullptr) {
        AUDIO_ERR_LOG("AudioContainerRenderStream::Enqueue: failed. bufDesc.buffer == nullptr.");
        return ERR_INVALID_PARAM;
    }

    lock_guard<mutex> lock(mBufferQueueLock);
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        AUDIO_INFO_LOG("AudioContainerRenderStream::Enqueue: filledBuffer length: %{public}zu.", bufDesc.bufLength);
        filledBufferQ_.emplace(bufDesc);
    }

    if (captureMode_ == CAPTURE_MODE_CALLBACK) {
        AUDIO_INFO_LOG("AudioContainerRenderStream::Enqueue: freeBuffer length: %{public}zu.", bufDesc.bufLength);
        freeBufferQ_.emplace(bufDesc);
    }

    return SUCCESS;
}

int32_t AudioContainerRenderStream::Clear()
{
    if ((renderMode_ != RENDER_MODE_CALLBACK) && (captureMode_ != CAPTURE_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("AudioContainerRenderStream::Clear not supported. Render or capture mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    lock_guard<mutex> lock(mBufferQueueLock);
    while (!filledBufferQ_.empty()) {
        freeBufferQ_.emplace(filledBufferQ_.front());
        filledBufferQ_.pop();
    }

    return SUCCESS;
}
} // namespace AudioStandard
} // namespace OHOS
