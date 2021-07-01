/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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

#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H

#include <memory>

#include "audio_info.h"

namespace OHOS {
namespace AudioStandard {
/**
 * @brief Defines information about audio record parameters
 */
struct AudioRecorderParams {
    /** Audio source type */
    AudioSourceType inputSource = AUDIO_MIC;
    /** Audio codec format */
    AudioEncodingType audioEncoding = ENCODING_PCM;
    /** Sampling rate */
    AudioSamplingRate samplingRate = SAMPLE_RATE_44100;
    /** Number of audio channels */
    AudioChannel audioChannel = MONO;
    /** Audio stream type */
    AudioStreamType streamType = STREAM_MEDIA;
    /** audioSampleFormat */
    AudioSampleFormat audioSampleFormat = SAMPLE_S16LE;
};

/**
 * @brief Enumerates the recording states of the current device.
 */
enum RecorderState {
    /** Create new recorder instance */
    RECORDER_NEW,
    /** Recorder Prepared state */
    RECORDER_PREPARED,
    /** Recorder Running state */
    RECORDER_RUNNING,
    /** Recorder Stopped state */
    RECORDER_STOPPED,
    /** Recorder Released state */
    RECORDER_RELEASED,
    /** Recorder INVALID state */
    RECORDER_INVALID
};

/**
 * @brief Provides functions for applications to implement audio recording.
 */
class AudioRecorder {
public:
    /**
     * @brief creater recorder instance.
    */
    static std::unique_ptr<AudioRecorder> Create(AudioStreamType audioStreamType);

    /**
     * @brief Obtains the number of frames required in the current condition, in bytes per sample.
     *
     * @param frameCount Indicates the pointer in which framecount will be written
     * @return Returns {@link SUCCESS} if frameCount is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetFrameCount(uint32_t &frameCount) = 0;

    /**
     * @brief Sets audio record parameters.
     *
     * @param params Indicates information about audio record parameters to set. For details, see
     * {@link AudioRecorderParams}.
     * @return Returns {@link SUCCESS} if the setting is successful; returns an error code defined
     * in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetParams(const AudioRecorderParams params) = 0;

    /**
     * @brief Obtains audio recorder parameters.
     *
     * This function can be called after {@link SetParams} is successful.
     *
     * @param params Indicates information about audio recorder parameters. For details, see {@link AudioRecorderParams}.
     * @return Returns {@link SUCCESS} if the parameter information is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetParams(AudioRecorderParams &params) = 0;

    /**
     * @brief Starts audio recording.
     *
     * @return Returns <b>true</b> if the recording is successfully started; returns <b>false</b> otherwise.
     */
    virtual bool Start() = 0;

    /**
     * @brief record audio data.
     *
     * @param buffer Indicates the pointer to the buffer into which the audio data is to be written.
     * @param userSize Indicates the size of the buffer into which the audio data is to be written, in bytes.
     * <b>userSize >= frameCount * channelCount * BytesPerSample</b> must evaluate to <b>true</b>. You can call
     * {@link GetFrameCount} to obtain the <b>frameCount</b> value.
     * @param isBlockingRead Specifies whether data reading will be blocked.
     * @return Returns the size of the audio data read from the device. The value ranges from <b>0</b> to
     * <b>userSize</b>. If the reading fails, one of the following error codes is returned.
     * <b>ERR_INVALID_PARAM</b>: The input parameter is incorrect.
     * <b>ERR_ILLEGAL_STATE</b>: The <b>AudioRecorder</b> instance is not initialized.
     * <b>ERR_INVALID_READ</b>: The read size < 0.
     */
    virtual int32_t Read(uint8_t &buffer, size_t userSize, bool isBlockingRead) = 0;

    /**
     * @brief Obtains the audio record state.
     *
     * @return Returns the audio record state defined in {@link RecorderState}.
     */
    virtual RecorderState GetStatus() = 0;

    /**
     * @brief Obtains the Timestamp.
     *
     * @param timestamp Indicates a {@link Timestamp} instance reference provided by the caller.
     * @param base Indicates the time base, which can be {@link Timestamp.Timestampbase#BOOTTIME} or
     * {@link Timestamp.Timestampbase#MONOTONIC}.
     * @return Returns <b>true</b> if the timestamp is successfully obtained; returns <b>false</b> otherwise.
     */
    virtual bool GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) = 0;

    /**
     * @brief Stops audio recording.
     *
     * @return Returns <b>true</b> if the recording is successfully stopped; returns <b>false</b> otherwise.
     */
    virtual bool Stop() = 0;
    /**
     * @brief flush record stream.
     *
     * @return Returns <b>true</b> if the object is successfully flushed; returns <b>false</b> otherwise.
     */
    virtual bool Flush() = 0;

    /**
     * @brief Releases a local <b>AudioRecorder</b> object.
     *
     * @return Returns <b>true</b> if the object is successfully released; returns <b>false</b> otherwise.
     */
    virtual bool Release() = 0;

    /**
     * @brief Obtains a reasonable minimum buffer size for recorder, however, the recorder can
     *        accept other read sizes as well.
     *
     * @param bufferSize Indicates a buffersize pointer value that wil be written.
     * @return Returns {@link SUCCESS} if bufferSize is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetBufferSize(size_t &bufferSize) = 0;

    /**
     * @brief Obtains the recorder supported formats.
     *
     * @return vector with recorder supported formats.
     */
    static std::vector<AudioSampleFormat> GetSupportedFormats();

    /**
     * @brief Obtains the recorder supported channels.
     *
     * @return vector with recorder supported channels.
     */
    static std::vector<AudioChannel> GetSupportedChannels();

    /**
     * @brief Obtains the recorder supported encoding types.
     *
     * @return vector with recorder supported encoding types.
     */
    static std::vector<AudioEncodingType> GetSupportedEncodingTypes();

    /**
     * @brief Obtains the recorder supported SupportedSamplingRates.
     *
     * @return vector with recorder supported SupportedSamplingRates.
     */
    static std::vector<AudioSamplingRate> GetSupportedSamplingRates();

    virtual ~AudioRecorder();
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif  // AUDIO_RECORDER_H
