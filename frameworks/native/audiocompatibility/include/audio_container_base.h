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

#ifndef AUDIO_CONTAINER_BASE_H
#define AUDIO_CONTAINER_BASE_H

#include "audio_container_stream_callback.h"
#include "audio_stream_death_recipient.h"

#include "audio_log.h"
#include "unistd.h"
#include "securec.h"

namespace OHOS {
namespace AudioStandard {
class AudioContainerBase : public AudioTimer {
public:
    /**
    * Initializes audio service client for the required client type
    *
    * @param eClientType indicates the client type like playback, record or controller.
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t Initialize(ASClientType eClientType, int serviceId);

    // Stream handling APIs

    /**
    * Creates & initializes resources based on the audioParams and audioType
    *
    * @param audioParams indicate format, sampling rate and number of channels
    * @param audioType indicate the stream type like music, system, ringtone etc
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t CreateStream(AudioStreamParams audioParams, AudioStreamType audioType);

    /**
     * @brief Obtains session ID .
     *
     * @return Returns unique session ID for the created session
    */
    int32_t GetSessionID(uint32_t &sessionID, const int32_t &trackId) const;

    /**
    * Starts the stream created using CreateStreamGa
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t StartStream(const int32_t &trackId);

    /**
    * Stops the stream created using CreateStreamGa
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t StopStream(const int32_t &trackId);

    /**
    * Flushes the stream created using CreateStreamGa. This is applicable for
    * playback only
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t FlushStream(const int32_t &trackId);

    /**
    * Drains the stream created using CreateStreamGa. This is applicable for
    * playback only
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t DrainStream(const int32_t &trackId);

    /**
    * Pauses the stream
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t PauseStream(const int32_t &trackId);

    /**
    * Update the stream type
    *
    * @param audioStreamType Audio stream type
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t SetStreamType(AudioStreamType audioStreamType, const int32_t &trackId);

    /**
    * Sets the volume of the stream associated with session ID
    *
    * @param sessionID indicates the ID for the active stream to be controlled
    * @param volume indicates volume level between 0 to 65536
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t SetStreamVolume(uint32_t sessionID, uint32_t volume);

    /**
    * Get the volume of the stream associated with session ID
    *
    * @param sessionID indicates the ID for the active stream to be controlled
    * @return returns volume level between 0 to 65536
    */
    uint32_t GetStreamVolume(uint32_t sessionID);

    /**
    * Writes audio data of the stream created using CreateStreamGa to active sink device
    *
    * @param buffer contains audio data to write
    * @param bufferSize indicates the size of audio data in bytes to write from the buffer
    * @param pError indicates pointer to error which will be filled in case of internal errors
    * @return returns size of audio data written in bytes.
    */
    size_t WriteStream(const StreamBuffer &stream, int32_t &pError, const int32_t &trackId);

    /**
    * Writes audio data of the stream created using CreateStreamGa to active sink device
    Used when render mode is RENDER_MODE_CALLBACK
    *
    * @param buffer contains audio data to write
    * @param bufferSize indicates the size of audio data in bytes to write from the buffer
    * @param pError indicates pointer to error which will be filled in case of internal errors
    * @return returns size of audio data written in bytes.
    */
    size_t WriteStreamInCb(const StreamBuffer &stream, int32_t &pError, const int32_t &trackId);

    /**
    * Reads audio data of the stream created using CreateStreamGa from active source device
    *
    * @param StreamBuffer including buffer to be filled with audio data
    * and bufferSize indicating the size of audio data to read into buffer
    * @param isBlocking indicates if the read is blocking or not
    * @return Returns size read if success; returns {@code -1} failure.
    */
    virtual int32_t ReadStream(StreamBuffer &stream, bool isBlocking, const int32_t &trackId);

    /**
    * Release the resources allocated using CreateStreamGa
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t ReleaseStream(const int32_t &trackId);

    /**
    * Provides the current timestamp for playback/record stream created using CreateStreamGa
    *
    * @param timeStamp will be filled up with current timestamp
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetCurrentTimeStamp(uint64_t &timeStamp, const int32_t &trackId);

    /**
    * Provides the current latency for playback/record stream created using CreateStreamGa
    *
    * @param latency will be filled up with the current latency in microseconds
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetAudioLatency(uint64_t &latency, const int32_t &trackId) const;

    /**
    * Provides the playback/record stream parameters created using CreateStreamGa
    *
    * @param audioParams will be filled up with stream audio parameters
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetAudioStreamParams(AudioStreamParams &audioParams, const int32_t &trackId);

    /**
    * Provides the minimum buffer size required for this audio stream
    * created using CreateStreamGa
    * @param minBufferSize will be set to minimum buffer size in bytes
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetMinimumBufferSize(size_t &minBufferSize, const int32_t &trackId) const;

    /**
    * Provides the minimum frame count required for this audio stream
    * created using CreateStreamGa
    * @param frameCount will be set to minimum number of frames
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetMinimumFrameCount(uint32_t &frameCount, const int32_t &trackId) const;

    /**
     * @brief Set the track volume
     *
     * @param volume The volume to be set for the current track.
     * @return Returns {@link SUCCESS} if volume is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetStreamVolume(float volume, const int32_t &trackId);

    /**
     * @brief Obtains the current track volume
     *
     * @return Returns current track volume
     */
    float GetStreamVolume();

    /**
     * @brief Set the render rate
     *
     * @param renderRate The rate at which the stream needs to be rendered.
     * @return Returns {@link SUCCESS} if render rate is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetStreamRenderRate(AudioRendererRate renderRate, const int32_t &trackId);

    /**
     * @brief Obtains the current render rate
     *
     * @return Returns current render rate
     */
    AudioRendererRate GetStreamRenderRate();

    /**
     * @brief Saves StreamCallback
     *
     * @param callback callback reference to be saved.
     * @return none.
     */

    void SaveStreamCallback(const std::weak_ptr<AudioStreamCallback> &callback);

    /**
     * @brief Sets the render mode. By default the mode is RENDER_MODE_NORMAL.
     * This API is needs to be used only if RENDER_MODE_CALLBACK is required.
     *
     * * @param renderMode The mode of render.
     * @return  Returns {@link SUCCESS} if render mode is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetAudioRenderMode(AudioRenderMode renderMode, const int32_t &trackId);

    /**
     * @brief Obtains the render mode.
     *
     * @return  Returns current render mode.
     */
    AudioRenderMode GetAudioRenderMode();

    /**
     * @brief Set the buffer duration in msec
     *
     * @param bufferSizeInMsec buffer size in duration.
     * @return Returns {@link SUCCESS} defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetBufferSizeInMsec(int32_t bufferSizeInMsec);

    /**
    * Set the renderer frame position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetRendererPositionCallback(int64_t markPosition, const std::shared_ptr<RendererPositionCallback> &callback);

    /**
    * Unset the renderer frame position callback
    *
    * @return none
    */
    void UnsetRendererPositionCallback();

    /**
    * Set the renderer frame period position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetRendererPeriodPositionCallback(int64_t markPosition,
        const std::shared_ptr<RendererPeriodPositionCallback> &callback);

    /**
    * Unset the renderer frame period position callback
    *
    * @return none
    */
    void UnsetRendererPeriodPositionCallback();

    /**
    * Set the capturer frame position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetCapturerPositionCallback(int64_t markPosition, const std::shared_ptr<CapturerPositionCallback> &callback);

    /**
    * Unset the capturer frame position callback
    *
    * @return none
    */
    void UnsetCapturerPositionCallback();

    /**
    * Set the capturer frame period position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetCapturerPeriodPositionCallback(int64_t markPosition,
        const std::shared_ptr<CapturerPeriodPositionCallback> &callback);

    /**
    * Unset the capturer frame period position callback
    *
    * @return none
    */
    void UnsetCapturerPeriodPositionCallback();

    // Audio timer callback
    virtual void OnTimeOut();

    int32_t SetAudioCaptureMode(AudioCaptureMode captureMode);

    int32_t SaveReadCallback(const std::weak_ptr<AudioCapturerReadCallback> &callback);
    /**
     * @brief Set the applicationcache path to access the application resources
     *
     * @return none
     */
    void SetAppCachePath(const std::string cachePath);

    AudioCaptureMode GetAudioCaptureMode();

    void InitAudioStreamManagerGa(int serviceId);

    /**
     * @brief Registers the renderer write callback listener.
     * This API should only be used if RENDER_MODE_CALLBACK is needed.
     *
     * @return Returns {@link SUCCESS} if callback registration is successful; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    virtual int32_t SaveWriteCallback(const std::weak_ptr<AudioRendererWriteCallback> &callback,
        const int32_t &trackId = 0);

    static void RegisterAudioStreamDeathRecipient(sptr<IRemoteObject> &object);

private:
    static void AudioStreamDied(pid_t pid);
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_CONTAINER_BASE_H