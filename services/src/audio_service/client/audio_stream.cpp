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

#include <chrono>
#include <thread>
#include <vector>

#include "audio_errors.h"
#include "audio_info.h"
#include "media_log.h"

#include "audio_stream.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
const unsigned long long TIME_CONVERSION_US_S = 1000000ULL; /* us to s */
const unsigned long long TIME_CONVERSION_NS_US = 1000ULL; /* ns to us */
const unsigned long long TIME_CONVERSION_NS_S = 1000000000ULL; /* ns to s */
constexpr int32_t WRITE_RETRY_DELAY_IN_US = 500;
constexpr int32_t READ_WRITE_WAIT_TIME_IN_US = 500;
constexpr int32_t CB_WRITE_BUFFERS_WAIT_IN_US = 500;

const map<pair<ContentType, StreamUsage>, AudioStreamType> AudioStream::streamTypeMap_ = AudioStream::CreateStreamMap();

map<pair<ContentType, StreamUsage>, AudioStreamType> AudioStream::CreateStreamMap()
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

AudioStream::AudioStream(AudioStreamType eStreamType, AudioMode eMode) : eStreamType_(eStreamType),
                                                                         eMode_(eMode),
                                                                         state_(NEW),
                                                                         isReadInProgress_(false),
                                                                         isWriteInProgress_(false),
                                                                         resetTime_(false),
                                                                         resetTimestamp_(0),
                                                                         renderMode_(RENDER_MODE_NORMAL),
                                                                         isReadyToWrite_(false),
                                                                         isFirstRead_(false)
{
    MEDIA_DEBUG_LOG("AudioStream ctor");
}

AudioStream::~AudioStream()
{
    isReadyToWrite_ = false;
    if (writeThread_ && writeThread_->joinable()) {
        writeThread_->join();
    }

    if (state_ != RELEASED && state_ != NEW) {
        ReleaseAudioStream();
    }
}

State AudioStream::GetState()
{
    return state_;
}

int32_t AudioStream::GetAudioSessionID(uint32_t &sessionID) const
{
    if ((state_ == RELEASED) || (state_ == NEW)) {
        return ERR_ILLEGAL_STATE;
    }

    if (GetSessionID(sessionID) != 0) {
        return ERR_INVALID_INDEX;
    }

    return SUCCESS;
}

bool AudioStream::GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base)
{
    if (state_ == STOPPED) {
        return false;
    }
    uint64_t paTimeStamp = 0;
    if (GetCurrentTimeStamp(paTimeStamp) == SUCCESS) {
        if (resetTime_) {
            MEDIA_INFO_LOG("AudioStream::GetAudioTime resetTime_ %{public}d", resetTime_);
            resetTime_ = false;
            resetTimestamp_ = paTimeStamp;
        }

        timestamp.time.tv_sec = static_cast<time_t>((paTimeStamp - resetTimestamp_) / TIME_CONVERSION_US_S);
        timestamp.time.tv_nsec
            = static_cast<time_t>(((paTimeStamp - resetTimestamp_) - (timestamp.time.tv_sec * TIME_CONVERSION_US_S))
                                  * TIME_CONVERSION_NS_US);
        timestamp.time.tv_sec += baseTimestamp_.tv_sec;
        timestamp.time.tv_nsec += baseTimestamp_.tv_nsec;
        timestamp.time.tv_sec += (timestamp.time.tv_nsec / TIME_CONVERSION_NS_S);
        timestamp.time.tv_nsec = (timestamp.time.tv_nsec % TIME_CONVERSION_NS_S);

        return true;
    }
    return false;
}

int32_t AudioStream::GetBufferSize(size_t &bufferSize) const
{
    MEDIA_INFO_LOG("AudioStream: Get Buffer size");
    if (GetMinimumBufferSize(bufferSize) != 0) {
        return ERR_OPERATION_FAILED;
    }

    return SUCCESS;
}

int32_t AudioStream::GetFrameCount(uint32_t &frameCount) const
{
    MEDIA_INFO_LOG("AudioStream: Get frame count");
    if (GetMinimumFrameCount(frameCount) != 0) {
        return ERR_OPERATION_FAILED;
    }

    return SUCCESS;
}

int32_t AudioStream::GetLatency(uint64_t &latency) const
{
    if (GetAudioLatency(latency) != SUCCESS) {
        return ERR_OPERATION_FAILED;
    } else {
        return SUCCESS;
    }
}

vector<AudioSampleFormat> AudioStream::GetSupportedFormats() const
{
    return AUDIO_SUPPORTED_FORMATS;
}

vector<AudioChannel> AudioStream::GetSupportedChannels() const
{
    return AUDIO_SUPPORTED_CHANNELS;
}

vector<AudioEncodingType> AudioStream::GetSupportedEncodingTypes() const
{
    return AUDIO_SUPPORTED_ENCODING_TYPES;
}

vector<AudioSamplingRate> AudioStream::GetSupportedSamplingRates() const
{
    return AUDIO_SUPPORTED_SAMPLING_RATES;
}

bool IsFormatValid(uint8_t format)
{
    bool isValidFormat = (find(AUDIO_SUPPORTED_FORMATS.begin(), AUDIO_SUPPORTED_FORMATS.end(), format)
                          != AUDIO_SUPPORTED_FORMATS.end());
    MEDIA_DEBUG_LOG("AudioStream: IsFormatValid: %{public}s", isValidFormat ? "true" : "false");
    return isValidFormat;
}

bool IsChannelValid(uint8_t channel)
{
    bool isValidChannel = (find(AUDIO_SUPPORTED_CHANNELS.begin(), AUDIO_SUPPORTED_CHANNELS.end(), channel)
                           != AUDIO_SUPPORTED_CHANNELS.end());
    MEDIA_DEBUG_LOG("AudioStream: IsChannelValid: %{public}s", isValidChannel ? "true" : "false");
    return isValidChannel;
}

bool IsEncodingTypeValid(uint8_t encodingType)
{
    bool isValidEncodingType
            = (find(AUDIO_SUPPORTED_ENCODING_TYPES.begin(), AUDIO_SUPPORTED_ENCODING_TYPES.end(), encodingType)
               != AUDIO_SUPPORTED_ENCODING_TYPES.end());
    MEDIA_DEBUG_LOG("AudioStream: IsEncodingTypeValid: %{public}s",
                    isValidEncodingType ? "true" : "false");
    return isValidEncodingType;
}

bool IsSamplingRateValid(uint32_t samplingRate)
{
    bool isValidSamplingRate
            = (find(AUDIO_SUPPORTED_SAMPLING_RATES.begin(), AUDIO_SUPPORTED_SAMPLING_RATES.end(), samplingRate)
               != AUDIO_SUPPORTED_SAMPLING_RATES.end());
    MEDIA_DEBUG_LOG("AudioStream: IsSamplingRateValid: %{public}s",
                    isValidSamplingRate ? "true" : "false");
    return isValidSamplingRate;
}

int32_t AudioStream::GetAudioStreamInfo(AudioStreamParams &audioStreamInfo)
{
    MEDIA_INFO_LOG("AudioStream: GetAudioStreamInfo");
    if (GetAudioStreamParams(audioStreamInfo) != 0) {
        return ERR_OPERATION_FAILED;
    }

    return SUCCESS;
}

int32_t AudioStream::SetAudioStreamInfo(const AudioStreamParams info)
{
    MEDIA_INFO_LOG("AudioStream: SetAudioParams");

    MEDIA_DEBUG_LOG("AudioStream: Sampling rate: %{public}d", info.samplingRate);
    MEDIA_DEBUG_LOG("AudioStream: channels: %{public}d", info.channels);
    MEDIA_DEBUG_LOG("AudioStream: format: %{public}d", info.format);
    MEDIA_DEBUG_LOG("AudioStream: stream type: %{public}d", eStreamType_);

    if (!IsFormatValid(info.format) || !IsChannelValid(info.channels)
        || !IsSamplingRateValid(info.samplingRate) || !IsEncodingTypeValid(info.encoding)) {
        MEDIA_ERR_LOG("AudioStream: Unsupported audio parameter");
        return ERR_NOT_SUPPORTED;
    }
    if (state_ != NEW) {
        MEDIA_DEBUG_LOG("AudioStream: State is not new, release existing stream");
        StopAudioStream();
        ReleaseAudioStream();
    }

    int32_t ret = 0;
    switch (eMode_) {
        case AUDIO_MODE_PLAYBACK:
            MEDIA_DEBUG_LOG("AudioStream: Initialize playback");
            ret = Initialize(AUDIO_SERVICE_CLIENT_PLAYBACK);
            break;
        case AUDIO_MODE_RECORD:
            MEDIA_DEBUG_LOG("AudioStream: Initialize recording");
            ret = Initialize(AUDIO_SERVICE_CLIENT_RECORD);
            break;
        default:
            return ERR_INVALID_OPERATION;
    }

    if (ret) {
        MEDIA_DEBUG_LOG("AudioStream: Error initializing!");
        return ret;
    }

    if (CreateStream(info, eStreamType_) != SUCCESS) {
        MEDIA_ERR_LOG("AudioStream:Create stream failed");
        return ERROR;
    }

    state_ = PREPARED;
    MEDIA_INFO_LOG("AudioStream:Set stream Info SUCCESS");
    return SUCCESS;
}

bool AudioStream::StartAudioStream()
{
    if ((state_ != PREPARED) && (state_ != STOPPED) && (state_ != PAUSED)) {
        MEDIA_ERR_LOG("StartAudioStream Illegal state:%{public}u", state_);
        return false;
    }

    int32_t ret = StartStream();
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("StartStream Start failed:%{public}d", ret);
        return false;
    }

    resetTime_ = true;
    int32_t retCode = clock_gettime(CLOCK_MONOTONIC, &baseTimestamp_);
    if (retCode != 0) {
        MEDIA_ERR_LOG("AudioStream::StartAudioStream get system elapsed time failed: %d", retCode);
    }

    if (renderMode_ == RENDER_MODE_CALLBACK) {
        isReadyToWrite_ = true;
        writeThread_ = std::make_unique<std::thread>(&AudioStream::WriteBuffers, this);
    }

    isFirstRead_ = true;
    state_ = RUNNING;
    MEDIA_INFO_LOG("StartAudioStream SUCCESS");
    return true;
}

int32_t AudioStream::Read(uint8_t &buffer, size_t userSize, bool isBlockingRead)
{
    if (userSize <= 0) {
        MEDIA_ERR_LOG("Invalid userSize:%{public}zu", userSize);
        return ERR_INVALID_PARAM;
    }

    if (state_ != RUNNING) {
        MEDIA_ERR_LOG("Read: State is not RUNNNIG. Illegal  state:%{public}u", state_);
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
    int32_t readLen = ReadStream(stream, isBlockingRead);
    isReadInProgress_ = false;
    if (readLen < 0) {
        MEDIA_ERR_LOG("ReadStream fail,ret:%{public}d", readLen);
        return ERR_INVALID_READ;
    }

    return readLen;
}

size_t AudioStream::Write(uint8_t *buffer, size_t buffer_size)
{
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        MEDIA_ERR_LOG("AudioStream::Write not supported. RenderMode is callback");
        return ERR_INCORRECT_MODE;
    }

    if ((buffer == nullptr) || (buffer_size <= 0)) {
        MEDIA_ERR_LOG("Invalid buffer size:%{public}zu", buffer_size);
        return ERR_INVALID_PARAM;
    }

    if (state_ != RUNNING) {
        MEDIA_ERR_LOG("Write: Illegal  state:%{public}u", state_);
        // To allow context switch for APIs running in different thread contexts
        std::this_thread::sleep_for(std::chrono::microseconds(WRITE_RETRY_DELAY_IN_US));
        return ERR_ILLEGAL_STATE;
    }

    int32_t writeError;
    StreamBuffer stream;
    stream.buffer = buffer;
    stream.bufferLen = buffer_size;
    isWriteInProgress_ = true;
    size_t bytesWritten = WriteStream(stream, writeError);
    isWriteInProgress_ = false;
    if (writeError != 0) {
        MEDIA_ERR_LOG("WriteStream fail,writeError:%{public}d", writeError);
        return ERR_WRITE_FAILED;
    }
    if (bytesWritten < 0) {
        MEDIA_ERR_LOG("WriteStream fail,bytesWritten:%{public}zu", bytesWritten);
        return ERR_INVALID_WRITE;
    }

    return bytesWritten;
}

bool AudioStream::PauseAudioStream()
{
    if (state_ != RUNNING) {
        MEDIA_ERR_LOG("PauseAudioStream: State is not RUNNING. Illegal state:%{public}u", state_);
        return false;
    }
    State oldState = state_;
    state_ = PAUSED; // Set it before stopping as Read/Write and Stop can be called from different threads
    while (isReadInProgress_ || isWriteInProgress_) {
        std::this_thread::sleep_for(std::chrono::microseconds(READ_WRITE_WAIT_TIME_IN_US));
    }

    int32_t ret = PauseStream();
    if (ret != SUCCESS) {
        MEDIA_DEBUG_LOG("StreamPause fail,ret:%{public}d", ret);
        state_ = oldState;
        return false;
    }

    // Ends the WriteBuffers thread
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        isReadyToWrite_ = false;
    }

    MEDIA_INFO_LOG("PauseAudioStream SUCCESS");

    return true;
}

bool AudioStream::StopAudioStream()
{
    MEDIA_INFO_LOG("AudioStream: StopAudioStream begin");
    if (state_ == PAUSED) {
        state_ = STOPPED;
        MEDIA_INFO_LOG("StopAudioStream SUCCESS");
        return true;
    }

    if (state_ != RUNNING) {
        MEDIA_ERR_LOG("StopAudioStream: State is not RUNNING. Illegal state:%{public}u", state_);
        return false;
    }
    State oldState = state_;
    state_ = STOPPED; // Set it before stopping as Read/Write and Stop can be called from different threads
    while (isReadInProgress_ || isWriteInProgress_) {
        std::this_thread::sleep_for(std::chrono::microseconds(READ_WRITE_WAIT_TIME_IN_US));
    }

    int32_t ret = StopStream();
    if (ret != SUCCESS) {
        MEDIA_DEBUG_LOG("StreamStop fail,ret:%{public}d", ret);
        state_ = oldState;
        return false;
    }

    // Ends the WriteBuffers thread
    if (renderMode_ == RENDER_MODE_CALLBACK) {
        isReadyToWrite_ = false;
    }

    MEDIA_INFO_LOG("StopAudioStream SUCCESS");

    return true;
}

bool AudioStream::FlushAudioStream()
{
    if ((state_ != RUNNING) && (state_ != PAUSED) && (state_ != STOPPED)) {
        MEDIA_ERR_LOG("FlushAudioStream: State is not RUNNING. Illegal state:%{public}u", state_);
        return false;
    }

    int32_t ret = FlushStream();
    if (ret != SUCCESS) {
        MEDIA_DEBUG_LOG("Flush stream fail,ret:%{public}d", ret);
        return false;
    }

    MEDIA_INFO_LOG("Flush stream SUCCESS");
    return true;
}

bool AudioStream::DrainAudioStream()
{
    if (state_ != RUNNING) {
        MEDIA_ERR_LOG("DrainAudioStream: State is not RUNNING. Illegal  state:%{public}u", state_);
        return false;
    }

    int32_t ret = DrainStream();
    if (ret != SUCCESS) {
        MEDIA_DEBUG_LOG("Drain stream fail,ret:%{public}d", ret);
        return false;
    }

    MEDIA_INFO_LOG("Drain stream SUCCESS");
    return true;
}

bool AudioStream::ReleaseAudioStream()
{
    if (state_ == RELEASED || state_ == NEW) {
        MEDIA_ERR_LOG("Illegal state: state = %{public}u", state_);
        return false;
    }
    // If state_ is RUNNING try to Stop it first and Release
    if (state_ == RUNNING) {
        StopAudioStream();
    }

    ReleaseStream();
    state_ = RELEASED;
    MEDIA_INFO_LOG("ReleaseAudiostream SUCCESS");

    return true;
}

AudioStreamType AudioStream::GetStreamType(ContentType contentType, StreamUsage streamUsage)
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

int32_t AudioStream::SetAudioStreamType(AudioStreamType audioStreamType)
{
    return SetStreamType(audioStreamType);
}

int32_t AudioStream::SetVolume(float volume)
{
    return SetStreamVolume(volume);
}

float AudioStream::GetVolume()
{
    return GetStreamVolume();
}

int32_t AudioStream::SetRenderRate(AudioRendererRate renderRate)
{
    return SetStreamRenderRate(renderRate);
}

AudioRendererRate AudioStream::GetRenderRate()
{
    return GetStreamRenderRate();
}

int32_t AudioStream::SetStreamCallback(const std::shared_ptr<AudioStreamCallback> &callback)
{
    if (callback == nullptr) {
        MEDIA_ERR_LOG("AudioStream::SetStreamCallback failed. callback == nullptr");
        return ERR_INVALID_PARAM;
    }

    SaveStreamCallback(callback);

    return SUCCESS;
}

int32_t AudioStream::SetRenderMode(AudioRenderMode renderMode)
{
    int32_t ret = SetAudioRenderMode(renderMode);
    if (ret) {
        MEDIA_ERR_LOG("AudioStream::SetRenderMode: renderMode: %{public}d failed", renderMode);
        return ERR_OPERATION_FAILED;
    }
    renderMode_ = renderMode;

    for (int32_t i = 0; i < MAX_NUM_BUFFERS; ++i) {
        size_t length;
        GetMinimumBufferSize(length);
        MEDIA_INFO_LOG("AudioServiceClient:: GetMinimumBufferSize: %{public}zu", length);

        bufferPool_[i] = std::make_unique<uint8_t[]>(length);
        if (bufferPool_[i] == nullptr) {
            MEDIA_INFO_LOG("AudioServiceClient::GetBufferDescriptor bufferPool_[i]==nullptr. Allocate memory failed.");
            return ERR_OPERATION_FAILED;
        }

        BufferDesc bufDesc {};
        bufDesc.buffer = bufferPool_[i].get();
        bufDesc.bufLength = length;
        freeBufferQ_.emplace(bufDesc);
    }

    return SUCCESS;
}

AudioRenderMode AudioStream::GetRenderMode()
{
    return GetAudioRenderMode();
}

int32_t AudioStream::SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback)
{
    if (renderMode_ != RENDER_MODE_CALLBACK) {
        MEDIA_ERR_LOG("AudioStream::SetRendererWriteCallback not supported. Render mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    int32_t ret = SaveWriteCallback(callback);
    if (ret) {
        MEDIA_ERR_LOG("AudioStream::SetRendererWriteCallback: failed");
        return ERR_INVALID_PARAM;
    }

    return SUCCESS;
}

int32_t AudioStream::GetBufferDesc(BufferDesc &bufDesc)
{
    if (renderMode_ != RENDER_MODE_CALLBACK) {
        MEDIA_ERR_LOG("AudioStream::GetBufferDesc not supported. Render mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    MEDIA_INFO_LOG("AudioStream::freeBufferQ_ count %{public}zu", freeBufferQ_.size());
    MEDIA_INFO_LOG("AudioStream::filledBufferQ_ count %{public}zu", filledBufferQ_.size());

    if (!freeBufferQ_.empty()) {
        bufDesc.buffer = freeBufferQ_.front().buffer;
        bufDesc.bufLength = freeBufferQ_.front().bufLength;
        freeBufferQ_.pop();
    } else {
        bufDesc.buffer = nullptr;
    }

    if (bufDesc.buffer == nullptr) {
        MEDIA_INFO_LOG("AudioStream::GetBufferDesc freeBufferQ_.empty()");
        return ERR_OPERATION_FAILED;
    }

    return SUCCESS;
}

int32_t AudioStream::Enqueue(const BufferDesc &bufDesc)
{
    MEDIA_INFO_LOG("AudioStream::Enqueue");
    if (renderMode_ != RENDER_MODE_CALLBACK) {
        MEDIA_ERR_LOG("AudioStream::Enqueue not supported. Render mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    if (bufDesc.buffer == nullptr) {
        MEDIA_ERR_LOG("AudioStream::Enqueue: failed. bufDesc.buffer == nullptr.");
        return ERR_INVALID_PARAM;
    }
    filledBufferQ_.emplace(bufDesc);

    return SUCCESS;
}

int32_t AudioStream::Clear()
{
    if (renderMode_ != RENDER_MODE_CALLBACK) {
        MEDIA_ERR_LOG("AudioStream::Clear not supported. Render mode is not callback.");
        return ERR_INCORRECT_MODE;
    }

    while (!filledBufferQ_.empty()) {
        freeBufferQ_.emplace(filledBufferQ_.front());
        filledBufferQ_.pop();
    }

    return SUCCESS;
}

void AudioStream::WriteBuffers()
{
    MEDIA_INFO_LOG("AudioStream::WriteBuffers thread start");
    StreamBuffer stream;
    size_t bytesWritten;
    int32_t writeError;

    while (isReadyToWrite_) {
        while (!filledBufferQ_.empty()) {
            if (state_ != RUNNING) {
                MEDIA_ERR_LOG("Write: Illegal  state:%{public}u", state_);
                isReadyToWrite_ = false;
                return;
            }
            MEDIA_DEBUG_LOG("AudioStream::WriteBuffers !filledBufferQ_.empty()");
            stream.buffer = filledBufferQ_.front().buffer;
            stream.bufferLen = filledBufferQ_.front().dataLength;
            MEDIA_DEBUG_LOG("AudioStream::WriteBuffers stream.bufferLen:%{public}d", stream.bufferLen);
            freeBufferQ_.emplace(filledBufferQ_.front());
            filledBufferQ_.pop();
            if (stream.buffer == nullptr) {
                continue;
            }
            bytesWritten = WriteStreamInCb(stream, writeError);
            if (writeError != 0) {
                MEDIA_ERR_LOG("AudioStream::WriteStreamInCb fail, writeError:%{public}d", writeError);
            }
            MEDIA_INFO_LOG("AudioStream::WriteBuffers WriteStream, bytesWritten:%{public}zu", bytesWritten);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(CB_WRITE_BUFFERS_WAIT_IN_US));
    }
    MEDIA_INFO_LOG("AudioStream::WriteBuffers thread end");
}
} // namspace AudioStandard
} // namespace OHOS
