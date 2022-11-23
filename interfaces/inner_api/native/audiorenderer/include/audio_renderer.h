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

#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#include <vector>
#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <cstring>
#include <timestamp.h>
#include "audio_info.h"

namespace OHOS {
namespace AudioStandard {
/**
 * @brief Defines information about audio renderer parameters.
 */

struct AudioRendererParams {
    /** Sample Format */
    AudioSampleFormat sampleFormat = SAMPLE_S16LE;
    /** Sampling rate */
    AudioSamplingRate sampleRate = SAMPLE_RATE_8000;
    /** Number of channels */
    AudioChannel channelCount = MONO;
    /** Encoding Type */
    AudioEncodingType encodingType = ENCODING_PCM;
};

class AudioRendererCallback {
public:
    virtual ~AudioRendererCallback() = default;

    /**
     * Called when an interrupt is received.
     *
     * @param interruptEvent Indicates the InterruptEvent information needed by client.
     * For details, refer InterruptEvent struct in audio_info.h
     */
    virtual void OnInterrupt(const InterruptEvent &interruptEvent) = 0;

    /**
     * Called when renderer state is updated.
     *
     * @param state Indicates updated state of the renderer.
     * For details, refer RendererState enum.
     */
    virtual void OnStateChange(const RendererState state, const StateChangeCmdType cmdType = CMD_FROM_CLIENT) = 0;
};

class RendererPositionCallback {
public:
    virtual ~RendererPositionCallback() = default;

    /**
     * Called when the requested frame number is reached.
     *
     * @param framePosition requested frame position.
     */
    virtual void OnMarkReached(const int64_t &framePosition) = 0;
};

class RendererPeriodPositionCallback {
public:
    virtual ~RendererPeriodPositionCallback() = default;

    /**
     * Called when the requested frame count is written.
     *
     * @param frameCount requested frame frame count for callback.
     */
    virtual void OnPeriodReached(const int64_t &frameNumber) = 0;
};

class AudioRendererWriteCallback {
public:
    virtual ~AudioRendererWriteCallback() = default;

    /**
     * Called when buffer to be enqueued.
     *
     * @param length Indicates requested buffer length.
     */
    virtual void OnWriteData(size_t length) = 0;
};

/**
 * @brief Provides functions for applications to implement audio rendering.
 */
class AudioRenderer {
public:
    /**
     * @brief create renderer instance.
     *
     * @param audioStreamType The audio streamtype to be created.
     * refer AudioStreamType in audio_info.h.
     * @return Returns unique pointer to the AudioRenderer object
    */
    static std::unique_ptr<AudioRenderer> Create(AudioStreamType audioStreamType);

    /**
     * @brief create renderer instance.
     *
     * @param audioStreamType The audio streamtype to be created.
     * refer AudioStreamType in audio_info.h.
     * @param appInfo Originating application's uid and token id can be passed here
     * @return Returns unique pointer to the AudioRenderer object
    */
    static std::unique_ptr<AudioRenderer> Create(AudioStreamType audioStreamType, const AppInfo &appInfo);

    /**
     * @brief create renderer instance.
     *
     * @param rendererOptions The audio renderer configuration to be used while creating renderer instance.
     * refer AudioRendererOptions in audio_info.h.
     * @return Returns unique pointer to the AudioRenderer object
    */
    static std::unique_ptr<AudioRenderer> Create(const AudioRendererOptions &rendererOptions);

    /**
     * @brief create renderer instance.
     *
     * @param rendererOptions The audio renderer configuration to be used while creating renderer instance.
     * refer AudioRendererOptions in audio_info.h.
     * @param appInfo Originating application's uid and token id can be passed here
     * @return Returns unique pointer to the AudioRenderer object
    */
    static std::unique_ptr<AudioRenderer> Create(const AudioRendererOptions &options, const AppInfo &appInfo);

    /**
     * @brief create renderer instance.
     *
     * @param cachePath Application cache path
     * @param rendererOptions The audio renderer configuration to be used while creating renderer instance.
     * refer AudioRendererOptions in audio_info.h.
     * @return Returns unique pointer to the AudioRenderer object
    */
    static std::unique_ptr<AudioRenderer> Create(const std::string cachePath,
        const AudioRendererOptions &rendererOptions);

    /**
     * @brief create renderer instance.
     *
     * @param cachePath Application cache path
     * @param rendererOptions The audio renderer configuration to be used while creating renderer instance.
     * refer AudioRendererOptions in audio_info.h.
     * @param appInfo Originating application's uid and token id can be passed here
     * @return Returns unique pointer to the AudioRenderer object
    */
    static std::unique_ptr<AudioRenderer> Create(const std::string cachePath,
        const AudioRendererOptions &rendererOptions, const AppInfo &appInfo);

    /**
     * @brief Sets audio renderer parameters.
     *
     * @param params Indicates information about audio renderer parameters to set. For details, see
     * {@link AudioRendererParams}.
     * @return Returns {@link SUCCESS} if the setting is successful; returns an error code defined
     * in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetParams(const AudioRendererParams params) = 0;

    /**
     * @brief Registers the renderer callback listener.
     * (1)If using old SetParams(const AudioCapturerParams params) API,
     *    this API must be called immediately after SetParams.
     * (2) Else if using Create(const AudioRendererOptions &rendererOptions),
     *    this API must be called immediately after Create.
     *
     * @return Returns {@link SUCCESS} if callback registration is successful; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetRendererCallback(const std::shared_ptr<AudioRendererCallback> &callback) = 0;

    /**
     * @brief Obtains audio renderer parameters.
     *
     * This function can be called after {@link SetParams} is successful.
     *
     * @param params Indicates information about audio renderer parameters. For details, see
     * {@link AudioRendererParams}.
     * @return Returns {@link SUCCESS} if the parameter information is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetParams(AudioRendererParams &params) const = 0;

    /**
     * @brief Obtains audio renderer information.
     *
     * This function can be called after {@link Create} is successful.
     *
     * @param rendererInfo Indicates information about audio renderer. For details, see
     * {@link AudioRendererInfo}.
     * @return Returns {@link SUCCESS} if the parameter information is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetRendererInfo(AudioRendererInfo &rendererInfo) const = 0;

    /**
     * @brief Obtains renderer stream information.
     *
     * This function can be called after {@link Create} is successful.
     *
     * @param streamInfo Indicates information about audio renderer. For details, see
     * {@link AudioStreamInfo}.
     * @return Returns {@link SUCCESS} if the parameter information is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetStreamInfo(AudioStreamInfo &streamInfo) const = 0;

    /**
     * @brief Starts audio rendering.
     *
     * @return Returns <b>true</b> if the rendering is successfully started; returns <b>false</b> otherwise.
     */
    virtual bool Start(StateChangeCmdType cmdType = CMD_FROM_CLIENT) const = 0;

    /**
     * @brief Writes audio data.
     * * This API cannot be used if render mode is RENDER_MODE_CALLBACK.
     *
     * @param buffer Indicates the pointer to the buffer which contains the audio data to be written.
     * @param bufferSize Indicates the size of the buffer which contains audio data to be written, in bytes.
     * @return Returns the size of the audio data written to the device. The value ranges from <b>0</b> to
     * <b>bufferSize</b>. If the write fails, one of the following error codes is returned.
     * <b>ERR_INVALID_PARAM</b>: The input parameter is incorrect.
     * <b>ERR_ILLEGAL_STATE</b>: The <b>AudioRenderer</b> instance is not initialized.
     * <b>ERR_INVALID_WRITE</b>: The written audio data size is < 0.
     * <b>ERR_WRITE_FAILED</b>: The audio data write failed .
     */
    virtual int32_t Write(uint8_t *buffer, size_t bufferSize) = 0;

    /**
     * @brief Obtains the audio renderer state.
     *
     * @return Returns the audio renderer state defined in {@link RendererState}.
     */
    virtual RendererState GetStatus() const = 0;

    /**
     * @brief Obtains the timestamp.
     *
     * @param timestamp Indicates a {@link Timestamp} instance reference provided by the caller.
     * @param base Indicates the time base, which can be {@link Timestamp.Timestampbase#BOOTTIME} or
     * {@link Timestamp.Timestampbase#MONOTONIC}.
     * @return Returns <b>true</b> if the timestamp is successfully obtained; returns <b>false</b> otherwise.
     */
    virtual bool GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) const = 0;

    /**
     * @brief Obtains the latency in microseconds.
     *
     * @param latency Indicates the reference variable into which latency value will be written.
     * @return Returns {@link SUCCESS} if latency is successfully obtained, returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetLatency(uint64_t &latency) const = 0;

    /**
     * @brief drain renderer buffer.
     *
     * @return Returns <b>true</b> if the buffer is successfully drained; returns <b>false</b> otherwise.
     */
    virtual bool Drain() const = 0;

    /**
     * @brief flush renderer stream.
     *
     * @return Returns <b>true</b> if the object is successfully flushed; returns <b>false</b> otherwise.
     */
    virtual bool Flush() const = 0;

    /**
     * @brief Pauses audio rendering.
     *
     * @return Returns <b>true</b> if the rendering is successfully Paused; returns <b>false</b> otherwise.
     */
    virtual bool Pause(StateChangeCmdType cmdType = CMD_FROM_CLIENT) const = 0;

    /**
     * @brief Stops audio rendering.
     *
     * @return Returns <b>true</b> if the rendering is successfully stopped; returns <b>false</b> otherwise.
     */
    virtual bool Stop() const = 0;

    /**
     * @brief Releases a local <b>AudioRenderer</b> object.
     *
     * @return Returns <b>true</b> if the object is successfully released; returns <b>false</b> otherwise.
     */
    virtual bool Release() const = 0;

    /**
     * @brief Obtains a reasonable minimum buffer size for rendering, however, the renderer can
     *        accept other write sizes as well.
     *
     * @param bufferSize Indicates the reference variable into which buffer size value will be written.
     * @return Returns {@link SUCCESS} if bufferSize is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetBufferSize(size_t &bufferSize) const = 0;

    /**
     * @brief Obtains the renderer stream id.
     *
     * @param sessionId Indicates the reference variable into which stream id value will be written.
     * @return Returns {@link SUCCESS} if stream id is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetAudioStreamId(uint32_t &sessionID) const = 0;

    /**
     * @brief Obtains the number of frames required in the current condition, in bytes per sample.
     *
     * @param frameCount Indicates the reference variable in which framecount will be written
     * @return Returns {@link SUCCESS} if frameCount is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetFrameCount(uint32_t &frameCount) const = 0;

    /**
     * @brief Set audio renderer descriptors
     *
     * @param audioRendererDesc Audio renderer descriptor
     * @return Returns {@link SUCCESS} if attribute is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetAudioRendererDesc(AudioRendererDesc audioRendererDesc) const = 0;

    /**
     * @brief Update the stream type
     *
     * @param audioStreamType Audio stream type
     * @return Returns {@link SUCCESS} if volume is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetStreamType(AudioStreamType audioStreamType) const = 0;

    /**
     * @brief Set the track volume
     *
     * @param volume The volume to be set for the current track.
     * @return Returns {@link SUCCESS} if volume is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetVolume(float volume) const = 0;

    /**
     * @brief Obtains the current track volume
     *
     * @return Returns current track volume
     */
    virtual float GetVolume() const = 0;

    /**
     * @brief Set the render rate
     *
     * @param renderRate The rate at which the stream needs to be rendered.
     * @return Returns {@link SUCCESS} if render rate is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetRenderRate(AudioRendererRate renderRate) const = 0;

    /**
     * @brief Obtains the current render rate
     *
     * @return Returns current render rate
     */
    virtual AudioRendererRate GetRenderRate() const = 0;

    /**
     * @brief Registers the renderer position callback listener
     *
     * @return Returns {@link SUCCESS} if callback registration is successful; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetRendererPositionCallback(int64_t markPosition,
        const std::shared_ptr<RendererPositionCallback> &callback) = 0;

    /**
     * @brief Unregisters the renderer position callback listener
     *
     */
    virtual void UnsetRendererPositionCallback() = 0;

    /**
     * @brief Registers the renderer period position callback listener
     *
     * @return Returns {@link SUCCESS} if callback registration is successful; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetRendererPeriodPositionCallback(int64_t frameNumber,
        const std::shared_ptr<RendererPeriodPositionCallback> &callback) = 0;

    /**
     * @brief Unregisters the renderer period position callback listener
     *
     */
    virtual void UnsetRendererPeriodPositionCallback() = 0;

    /**
     * @brief set the buffer duration for renderer, minimum buffer duration is 5msec
     *         maximum is 20msec
     *
     * @param bufferDuration  Indicates a buffer duration to be set for renderer
     * @return Returns {@link SUCCESS} if bufferDuration is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetBufferDuration(uint64_t bufferDuration) const = 0;

    /**
     * @brief Obtains the formats supported by renderer.
     *
     * @return Returns vector with supported formats.
     */
    static std::vector<AudioSampleFormat> GetSupportedFormats();

    /**
     * @brief Obtains the SupportedSamplingRates supported by renderer.
     *
     * @return Returns vector with supported SupportedSamplingRates.
     */
    static std::vector<AudioSamplingRate> GetSupportedSamplingRates();

    /**
     * @brief Obtains the channels supported by renderer.
     *
     * @return Returns vector with supported channels.
     */
    static std::vector<AudioChannel> GetSupportedChannels();

    /**
     * @brief Obtains the encoding types supported by renderer.
     *
     * @return Returns vector with supported encoding types.
     */
    static std::vector<AudioEncodingType> GetSupportedEncodingTypes();

    /**
     * @brief Sets the render mode. By default the mode is RENDER_MODE_NORMAL.
     * This API is needs to be used only if RENDER_MODE_CALLBACK is required.
     *
     * * @param renderMode The mode of render.
     * @return  Returns {@link SUCCESS} if render mode is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetRenderMode(AudioRenderMode renderMode) const = 0;

    /**
     * @brief Obtains the render mode.
     *
     * @return  Returns current render mode.
     */
    virtual AudioRenderMode GetRenderMode() const = 0;

    /**
     * @brief Registers the renderer write callback listener.
     * This API should only be used if RENDER_MODE_CALLBACK is needed.
     *
     * @return Returns {@link SUCCESS} if callback registration is successful; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback) = 0;

    /**
     * @brief Gets the BufferDesc to fill the data.
     * This API should only be used if RENDER_MODE_CALLBACK is needed.
     *
     * @param bufDesc Indicates the buffer descriptor in which data will filled.
     * refer BufferQueueState in audio_info.h.
     * @return Returns {@link SUCCESS} if bufDesc is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetBufferDesc(BufferDesc &bufDesc) const = 0;

    /**
     * @brief Enqueues the buffer to the bufferQueue.
     * This API should only be used if RENDER_MODE_CALLBACK is needed.
     *
     * @param bufDesc Indicates the buffer descriptor in which buffer data will filled.
     * refer BufferQueueState in audio_info.h.
     * @return Returns {@link SUCCESS} if bufDesc is successfully enqued; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t Enqueue(const BufferDesc &bufDesc) const = 0;

    /**
     * @brief Clears the bufferQueue.
     * This API should only be used if RENDER_MODE_CALLBACK is needed.
     *
     * @return Returns {@link SUCCESS} if successful; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t Clear() const = 0;

    /**
     * @brief Obtains the current state of bufferQueue.
     * This API should only be used if RENDER_MODE_CALLBACK is needed.
     *
     * @param bufDesc Indicates the bufState reference in which state will be obtained.
     * refer BufferQueueState in audio_info.h.
     * @return Returns {@link SUCCESS} if bufState is successfully obtained; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t GetBufQueueState(BufferQueueState &bufState) const = 0;

    /**
     * @brief Set the application cache path to access the application resources
     *
     * @param cachePath Indicates application cache path.
     * @return none
     */
    virtual void SetApplicationCachePath(const std::string cachePath) = 0;

    /**
     * @brief Set interrupt mode.
     *
     * @param mode The interrupt mode.
     * @return none
     */
    virtual void SetInterruptMode(InterruptMode mode) = 0;

    /**
     * @brief Set volume discount factor.
     *
     * @param volume Adjustment percentage.
     * @return Whether the operation is effective
     */
    virtual int32_t SetLowPowerVolume(float volume) const = 0;

    /**
     * @brief Get volume discount factor.
     *
     * @param none.
     * @return volume adjustment percentage.
     */
    virtual float GetLowPowerVolume() const = 0;

    /**
     * @brief Get single stream volume.
     *
     * @param none.
     * @return single stream volume.
     */
    virtual float GetSingleStreamVolume() const = 0;
    
    virtual ~AudioRenderer();
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif  // AUDIO_RENDERER_H
