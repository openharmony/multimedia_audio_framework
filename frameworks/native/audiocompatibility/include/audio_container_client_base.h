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

#ifndef AUDIO_STREAM_SERVICE_CLIENT_H
#define AUDIO_STREAM_SERVICE_CLIENT_H

#include "audio_container_stream_callback.h"

namespace OHOS {
namespace AudioStandard {
class AudioContainerClientBase : public IRemoteProxy<IAudioContainerService> {
public:
    explicit AudioContainerClientBase(const sptr<IRemoteObject> &impl);
    virtual ~AudioContainerClientBase();

    /**
    * Initializes audio service client for the required client type
    *
    * @param eClientType indicates the client type like playback, record or controller.
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t InitializeGa(ASClientType eClientType) override;

    // Stream handling APIs

    /**
    * Creates & initializes resources based on the audioParams and audioType
    *
    * @param audioParams indicate format, sampling rate and number of channels
    * @param audioType indicate the stream type like music, system, ringtone etc
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t CreateStreamGa(AudioStreamParams audioParams, AudioStreamType audioType) override;

    /**
     * @brief Obtains session ID .
     *
     * @return Returns unique session ID for the created session
    */
    int32_t GetSessionIDGa(uint32_t &sessionID, const int32_t &trackId) override;

    /**
    * Starts the stream created using CreateStreamGa
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t StartStreamGa(const int32_t &trackId) override;

    /**
    * Stops the stream created using CreateStreamGa
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t StopStreamGa(const int32_t &trackId) override;

    /**
    * Flushes the stream created using CreateStreamGa. This is applicable for
    * playback only
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t FlushStreamGa(const int32_t &trackId) override;

    /**
    * Drains the stream created using CreateStreamGa. This is applicable for
    * playback only
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t DrainStreamGa(const int32_t &trackId) override;

    /**
    * Pauses the stream
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t PauseStreamGa(const int32_t &trackId) override;

    /**
    * Update the stream type
    *
    * @param audioStreamType Audio stream type
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t SetStreamTypeGa(AudioStreamType audioStreamType, const int32_t &trackId) override;

    /**
    * Sets the volume of the stream associated with session ID
    *
    * @param sessionID indicates the ID for the active stream to be controlled
    * @param volume indicates volume level between 0 to 65536
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t SetStreamVolumeGa(uint32_t sessionID, uint32_t volume) override;

    /**
    * Get the volume of the stream associated with session ID
    *
    * @param sessionID indicates the ID for the active stream to be controlled
    * @return returns volume level between 0 to 65536
    */
    uint32_t GetStreamVolumeGa(uint32_t sessionID) override;

    /**
    * Writes audio data of the stream created using CreateStreamGa to active sink device
    *
    * @param buffer contains audio data to write
    * @param bufferSize indicates the size of audio data in bytes to write from the buffer
    * @param pError indicates pointer to error which will be filled in case of internal errors
    * @return returns size of audio data written in bytes.
    */
    size_t WriteStreamGa(const StreamBuffer &stream, int32_t &pError, const int32_t &trackId) override;

    /**
    * Writes audio data of the stream created using CreateStreamGa
    *
    * @param buffer contains audio data to write
    * @param bufferSize indicates the size of audio data in bytes to write from the buffer
    * @param pError indicates pointer to error which will be filled in case of internal errors
    * @return returns size of audio data written in bytes.
    */
    size_t WriteStreamInCbGa(const StreamBuffer &stream, int32_t &pError, const int32_t &trackId) override;

    /**
    * Reads audio data of the stream created using CreateStreamGa from active source device
    *
    * @param StreamBuffer including buffer to be filled with audio data
    * and bufferSize indicating the size of audio data to read into buffer
    * @param isBlocking indicates if the read is blocking or not
    * @return Returns size read if success; returns {@code -1} failure.
    */
    int32_t ReadStreamGa(StreamBuffer &stream, bool isBlocking, const int32_t &trackId) override;

    /**
    * Release the resources allocated using CreateStreamGa
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t ReleaseStreamGa(const int32_t &trackId) override;

    /**
    * Provides the current timestamp for playback/record stream created using CreateStreamGa
    *
    * @param timeStamp will be filled up with current timestamp
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetCurrentTimeStampGa(uint64_t &timeStamp, const int32_t &trackId) override;

    /**
    * Provides the current latency for playback/record stream created using CreateStreamGa
    *
    * @param latency will be filled up with the current latency in microseconds
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetAudioLatencyGa(uint64_t &latency, const int32_t &trackId) override;

    /**
    * Provides the playback/record stream parameters created using CreateStreamGa
    *
    * @param audioParams will be filled up with stream audio parameters
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetAudioStreamParamsGa(AudioStreamParams &audioParams, const int32_t &trackId) override;

    /**
    * Provides the minimum buffer size required for this audio stream
    * created using CreateStreamGa
    * @param minBufferSize will be set to minimum buffer size in bytes
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetMinimumBufferSizeGa(size_t &minBufferSize, const int32_t &trackId) override;

    /**
    * Provides the minimum frame count required for this audio stream
    * created using CreateStreamGa
    * @param frameCount will be set to minimum number of frames
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetMinimumFrameCountGa(uint32_t &frameCount, const int32_t &trackId) override;

    /**
     * @brief Set the buffer duration in msec
     *
     * @param bufferSizeInMsec buffer size in duration.
     * @return Returns {@link SUCCESS} defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetBufferSizeInMsecGa(int32_t bufferSizeInMsec) override;

    /**
    * Provides the sampling rate for the active audio stream
    * created using CreateStreamGa
    *
    * @return Returns sampling rate in Hz
    */
    uint32_t GetSamplingRateGa() const override;

    /**
    * Provides the channel count for the active audio stream
    * created using CreateStreamGa
    *
    * @return Returns number of channels
    */
    uint8_t GetChannelCountGa() const override;

    /**
    * Provides the sample size for the active audio stream
    * created using CreateStreamGa
    *
    * @return Returns sample size in number of bits
    */
    uint8_t GetSampleSizeGa() const override;

    // Device volume & route handling APIs

    // Audio stream callbacks

    /**
    * Register for callbacks associated with the playback stream created using CreateStreamGa
    *
    * @param cb indicates pointer for registered callbacks
    * @return none
    */
    void RegisterAudioRendererCallbackGa(const AudioRendererCallback &cb) override;

    /**
    * Register for callbacks associated with the record stream created using CreateStreamGa
    *
    * @param cb indicates pointer for registered callbacks
    * @return none
    */
    void RegisterAudioCapturerCallbackGa(const AudioCapturerCallback &cb) override;

    /**
    * Set the renderer frame position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetRendererPositionCallbackGa(int64_t markPosition,
        const std::shared_ptr<RendererPositionCallback> &callback) override;

    /**
    * Unset the renderer frame position callback
    *
    * @return none
    */
    void UnsetRendererPositionCallbackGa() override;

    /**
    * Set the renderer frame period position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetRendererPeriodPositionCallbackGa(int64_t markPosition,
        const std::shared_ptr<RendererPeriodPositionCallback> &callback) override;

    /**
    * Unset the renderer frame period position callback
    *
    * @return none
    */
    void UnsetRendererPeriodPositionCallbackGa() override;

    /**
    * Set the capturer frame position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetCapturerPositionCallbackGa(int64_t markPosition,
        const std::shared_ptr<CapturerPositionCallback> &callback) override;

    /**
    * Unset the capturer frame position callback
    *
    * @return none
    */
    void UnsetCapturerPositionCallbackGa() override;

    /**
    * Set the capturer frame period position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetCapturerPeriodPositionCallbackGa(int64_t markPosition,
        const std::shared_ptr<CapturerPeriodPositionCallback> &callback) override;

    /**
    * Unset the capturer frame period position callback
    *
    * @return none
    */
    void UnsetCapturerPeriodPositionCallbackGa() override;

    /**
     * @brief Set the track volume
     *
     * @param volume The volume to be set for the current track.
     * @return Returns {@link SUCCESS} if volume is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetStreamVolumeGa(float volume, const int32_t &trackId) override;

    /**
     * @brief Obtains the current track volume
     *
     * @return Returns current track volume
     */
    float GetStreamVolumeGa() override;

    /**
     * @brief Set the render rate
     *
     * @param renderRate The rate at which the stream needs to be rendered.
     * @return Returns {@link SUCCESS} if render rate is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetStreamRenderRateGa(AudioRendererRate renderRate, const int32_t &trackId) override;

    /**
     * @brief Obtains the current render rate
     *
     * @return Returns current render rate
     */
    AudioRendererRate GetStreamRenderRateGa() override;

    /**
     * @brief Saves StreamCallback
     *
     * @param callback callback reference to be saved.
     * @return none.
     */

    void SaveStreamCallbackGa(const std::weak_ptr<AudioStreamCallback> &callback) override;

    /**
     * @brief Sets the render mode. By default the mode is RENDER_MODE_NORMAL.
     * This API is needs to be used only if RENDER_MODE_CALLBACK is required.
     *
     * * @param renderMode The mode of render.
     * @return  Returns {@link SUCCESS} if render mode is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetAudioRenderModeGa(AudioRenderMode renderMode, const int32_t &trackId) override;

    /**
     * @brief Obtains the render mode.
     *
     * @return  Returns current render mode.
     */
    AudioRenderMode GetAudioRenderModeGa() override;

    /**
     * @brief Registers the renderer write callback listener.
     * This API should only be used if RENDER_MODE_CALLBACK is needed.
     *
     * @return Returns {@link SUCCESS} if callback registration is successful; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    int32_t SaveWriteCallbackGa(const std::weak_ptr<AudioRendererWriteCallback> &callback,
        const int32_t &trackId = 0) override;

    int32_t SetAudioCaptureMode(AudioCaptureMode captureMode) override;

    int32_t SaveReadCallback(const std::weak_ptr<AudioCapturerReadCallback> &callback) override;
    /**
     * @brief Set the applicationcache path to access the application resources
     *
     * @return none
     */
    void SetAppCachePath(const std::string cachePath) override;

    AudioCaptureMode GetAudioCaptureMode() override;

    std::mutex dataMutex;
    std::mutex ctrlMutex;

    int32_t audioTrackId = 0;

    const void *internalReadBuffer;
    size_t internalRdBufLen;
    size_t internalRdBufIndex;

    float mVolumeFactor;
    AudioStreamType mStreamType;
    AudioSystemManager *mAudioSystemMgr;

    AudioRendererRate renderRate;
    AudioRenderMode renderMode_;
    std::weak_ptr<AudioRendererWriteCallback> writeCallback_;

    uint32_t mFrameSize = 0;
    bool mMarkReached = false;
    uint64_t mFrameMarkPosition = 0;
    uint64_t mFramePeriodNumber = 0;

    uint64_t mTotalBytesWritten = 0;
    uint64_t mFramePeriodWritten = 0;
    std::shared_ptr<RendererPositionCallback> mRenderPositionCb;
    std::shared_ptr<RendererPeriodPositionCallback> mRenderPeriodPositionCb;

    uint64_t mTotalBytesRead = 0;
    uint64_t mFramePeriodRead = 0;
    std::shared_ptr<CapturerPositionCallback> mCapturePositionCb;
    std::shared_ptr<CapturerPeriodPositionCallback> mCapturePeriodPositionCb;

    std::vector<std::unique_ptr<std::thread>> mPositionCBThreads;
    std::vector<std::unique_ptr<std::thread>> mPeriodPositionCBThreads;

    std::weak_ptr<AudioStreamCallback> streamCallback_;
    State state_;

    // To be set while using audio stream
    // functionality for callbacks
    AudioRendererCallback *mAudioRendererCallbacks;
    AudioCapturerCallback *mAudioCapturerCallbacks;

    ASClientType eAudioClientType;

    // These APIs are applicable only for playback scenarios
    void InitializeClientGa();
    int32_t InitializeAudioCacheGa();

    int32_t UpdateReadBufferGa(uint8_t *buffer, size_t &length, size_t &readSize);
    int32_t WriteStreamInnerGa(const uint8_t *buffer, size_t &length, const int32_t &trackId);
    void HandleRenderPositionCallbacksGa(size_t bytesWritten);
    void HandleCapturePositionCallbacksGa(size_t bytesRead);

    // Default values
    static const uint32_t MINIMUM_BUFFER_SIZE = 1024;
    static const uint32_t DEFAULT_SAMPLING_RATE = 44100;
    static const uint8_t DEFAULT_CHANNEL_COUNT = 2;
    static const uint8_t DEFAULT_SAMPLE_SIZE = 2;
    static const uint32_t DEFAULT_STREAM_VOLUME = 65536;
    static const std::string GetStreamNameGa(AudioStreamType audioType);

    static constexpr float MAX_STREAM_VOLUME_LEVEL = 1.0f;
    static constexpr float MIN_STREAM_VOLUME_LEVEL = 0.0f;

    // Resets PA audio client and free up resources if any with this API
    void ResetPAAudioClientGa();

    static inline BrokerDelegator<AudioContainerClientBase> delegator_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_STREAM_SERVICE_CLIENT_H