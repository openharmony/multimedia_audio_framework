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

#ifndef AUDIO_SERVICE_CLIENT_H
#define AUDIO_SERVICE_CLIENT_H

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <audio_error.h>
#include <audio_info.h>
#include <audio_timer.h>

#include "event_handler.h"
#include "event_runner.h"

#include "audio_capturer.h"
#include "audio_policy_manager.h"
#include "audio_renderer.h"
#include "audio_system_manager.h"
#include "i_audio_stream.h"

namespace OHOS {
namespace AudioStandard {
enum ASClientType {
    AUDIO_SERVICE_CLIENT_PLAYBACK,
    AUDIO_SERVICE_CLIENT_RECORD,
    AUDIO_SERVICE_CLIENT_CONTROLLER
};

typedef pa_sink_input_info    SinkInputInfo;
typedef pa_source_output_info SourceOutputInfo;
typedef pa_sink_info          SinkDeviceInfo;
typedef pa_source_info        SourceDeviceInfo;
typedef pa_client_info        ClientInfo;

struct StreamBuffer {
    uint8_t *buffer; // the virtual address of stream
    uint32_t bufferLen; // stream length in bytes
};

struct AudioCache {
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t readIndex;
    uint32_t writeIndex;
    uint32_t totalCacheSize;
    bool isFull;
};

class AudioRendererCallbacks {
public:
    virtual ~AudioRendererCallbacks();
    virtual void OnSinkDeviceUpdatedCb() const = 0;
    // Need to check required state changes to update applications
    virtual void OnStreamStateChangeCb() const = 0;
    virtual void OnStreamBufferUnderFlowCb() const = 0;
    virtual void OnStreamBufferOverFlowCb() const = 0;
    virtual void OnErrorCb(AudioServiceErrorCodes error) const = 0;
    virtual void OnEventCb(AudioServiceEventTypes error) const = 0;
};

class AudioCapturerCallbacks {
public:
    virtual ~AudioCapturerCallbacks();
    virtual void OnSourceDeviceUpdatedCb() const = 0;
    // Need to check required state changes to update applications
    virtual void OnStreamStateChangeCb() const = 0;
    virtual void OnStreamBufferUnderFlowCb() const = 0;
    virtual void OnStreamBufferOverFlowCb() const = 0;
    virtual void OnErrorCb(AudioServiceErrorCodes error) const = 0;
    virtual void OnEventCb(AudioServiceEventTypes error) const = 0;
};

class AudioServiceClient : public IAudioStream, public AudioTimer, public AppExecFwk::EventHandler {
public:
    static constexpr char PA_RUNTIME_DIR[] = "/data/data/.pulse_dir/runtime";
    static constexpr char PA_STATE_DIR[] = "/data/data/.pulse_dir/state";
    static constexpr char PA_HOME_DIR[] = "/data/data/.pulse_dir/state";

    AudioServiceClient();
    virtual ~AudioServiceClient();

    /**
    * Initializes audio service client for the required client type
    *
    * @param eClientType indicates the client type like playback, record or controller.
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t Initialize(ASClientType eClientType);

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
    int32_t GetSessionID(uint32_t &sessionID) const;

    /**
    * Starts the stream created using CreateStream
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t StartStream(StateChangeCmdType cmdType = CMD_FROM_CLIENT);

    /**
    * Stops the stream created using CreateStream
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t StopStream();

    /**
    * Flushes the stream created using CreateStream. This is applicable for
    * playback only
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t FlushStream();

    /**
    * Drains the stream created using CreateStream. This is applicable for
    * playback only
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t DrainStream();

    /**
    * Pauses the stream
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t PauseStream(StateChangeCmdType cmdType = CMD_FROM_CLIENT);

    /**
    * Update the stream type
    *
    * @param audioStreamType Audio stream type
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t SetStreamType(AudioStreamType audioStreamType);

    /**
    * Writes audio data of the stream created using CreateStream to active sink device
    *
    * @param buffer contains audio data to write
    * @param bufferSize indicates the size of audio data in bytes to write from the buffer
    * @param pError indicates pointer to error which will be filled in case of internal errors
    * @return returns size of audio data written in bytes.
    */
    size_t WriteStream(const StreamBuffer &stream, int32_t &pError);
    int32_t RenderPrebuf(uint32_t writeLen);

    /**
    * Writes audio data of the stream created using CreateStream to active sink device
    Used when render mode is RENDER_MODE_CALLBACK
    *
    * @param buffer contains audio data to write
    * @param bufferSize indicates the size of audio data in bytes to write from the buffer
    * @param pError indicates pointer to error which will be filled in case of internal errors
    * @return returns size of audio data written in bytes.
    */
    size_t WriteStreamInCb(const StreamBuffer &stream, int32_t &pError);

    /**
    * Reads audio data of the stream created using CreateStream from active source device
    *
    * @param StreamBuffer including buffer to be filled with audio data
    * and bufferSize indicating the size of audio data to read into buffer
    * @param isBlocking indicates if the read is blocking or not
    * @return Returns size read if success; returns {@code -1} failure.
    */
    int32_t ReadStream(StreamBuffer &stream, bool isBlocking);

    /**
    * Release the resources allocated using CreateStream
    *
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t ReleaseStream(bool releaseRunner = true);

    /**
    * Provides the current timestamp for playback/record stream created using CreateStream
    *
    * @param timeStamp will be filled up with current timestamp
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetCurrentTimeStamp(uint64_t &timeStamp);

    /**
    * Provides the current latency for playback/record stream created using CreateStream
    *
    * @param latency will be filled up with the current latency in microseconds
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetAudioLatency(uint64_t &latency);

    /**
    * Provides the playback/record stream parameters created using CreateStream
    *
    * @param audioParams will be filled up with stream audio parameters
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetAudioStreamParams(AudioStreamParams &audioParams) const;

    /**
    * Provides the minimum buffer size required for this audio stream
    * created using CreateStream
    * @param minBufferSize will be set to minimum buffer size in bytes
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetMinimumBufferSize(size_t &minBufferSize) const;

    /**
    * Provides the minimum frame count required for this audio stream
    * created using CreateStream
    * @param frameCount will be set to minimum number of frames
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetMinimumFrameCount(uint32_t &frameCount) const;

    /**
    * Provides the sampling rate for the active audio stream
    * created using CreateStream
    *
    * @return Returns sampling rate in Hz
    */
    uint32_t GetSamplingRate() const;

    /**
    * Provides the channel count for the active audio stream
    * created using CreateStream
    *
    * @return Returns number of channels
    */
    uint8_t GetChannelCount() const;

    /**
    * Provides the sample size for the active audio stream
    * created using CreateStream
    *
    * @return Returns sample size in number of bits
    */
    uint8_t GetSampleSize() const;

    void SetStreamInnerCapturerState(bool isInnerCapturer);

    void SetStreamPrivacyType(AudioPrivacyType privacyType);

    /**
    * Provides the underflow count required for this audio stream
    * created using CreateStream
    * @param underFlowCount will be get to number of frames
    * @return Returns number of underflow
    */
    uint32_t GetUnderflowCount() override;

    // Device volume & route handling APIs

    // Audio stream callbacks

    /**
    * Register for callbacks associated with the playback stream created using CreateStream
    *
    * @param cb indicates pointer for registered callbacks
    * @return none
    */
    void RegisterAudioRendererCallbacks(const AudioRendererCallbacks &cb);

    /**
    * Register for callbacks associated with the record stream created using CreateStream
    *
    * @param cb indicates pointer for registered callbacks
    * @return none
    */
    void RegisterAudioCapturerCallbacks(const AudioCapturerCallbacks &cb);

    /**
    * Set the renderer frame position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetRendererPositionCallback(int64_t markPosition, const std::shared_ptr<RendererPositionCallback> &callback)
        override;

    /**
    * Unset the renderer frame position callback
    *
    * @return none
    */
    void UnsetRendererPositionCallback() override;

    /**
    * Set the renderer frame period position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetRendererPeriodPositionCallback(int64_t markPosition,
        const std::shared_ptr<RendererPeriodPositionCallback> &callback) override;

    /**
    * Unset the renderer frame period position callback
    *
    * @return none
    */
    void UnsetRendererPeriodPositionCallback() override;

    /**
    * Set the capturer frame position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetCapturerPositionCallback(int64_t markPosition, const std::shared_ptr<CapturerPositionCallback> &callback)
        override;

    /**
    * Unset the capturer frame position callback
    *
    * @return none
    */
    void UnsetCapturerPositionCallback() override;

    /**
    * Set the capturer frame period position callback
    *
    * @param callback indicates pointer for registered callbacks
    * @return none
    */
    void SetCapturerPeriodPositionCallback(int64_t markPosition,
        const std::shared_ptr<CapturerPeriodPositionCallback> &callback) override;

    /**
    * Unset the capturer frame period position callback
    *
    * @return none
    */
    void UnsetCapturerPeriodPositionCallback() override;

    /**
     * @brief Set the track volume
     *
     * @param volume The volume to be set for the current track.
     * @return Returns {@link SUCCESS} if volume is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetStreamVolume(float volume);

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
    int32_t SetStreamRenderRate(AudioRendererRate renderRate);

    /**
     * @brief Obtains the current render rate
     *
     * @return Returns current render rate
     */
    AudioRendererRate GetStreamRenderRate();

    /**
    * @brief Set stream render sampling rate
    *
    * @param sampleRate The sample rate at which the stream needs to be rendered.
    * @return Returns {@link SUCCESS} if render rate is successfully set; returns an error code
    * defined in {@link audio_errors.h} otherwise.
    */
    int32_t SetRendererSamplingRate(uint32_t sampleRate) override;

    /**
    * @brief Obtains render sampling rate
    *
    * @return Returns current render sampling rate
    */
    uint32_t GetRendererSamplingRate() override;

    /**
     * @brief Set the buffer duration in msec
     *
     * @param bufferSizeInMsec buffer size in duration.
     * @return Returns {@link SUCCESS} defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetBufferSizeInMsec(int32_t bufferSizeInMsec) override;

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
    int32_t SetAudioRenderMode(AudioRenderMode renderMode);

    /**
     * @brief Obtains the render mode.
     *
     * @return  Returns current render mode.
     */
    AudioRenderMode GetAudioRenderMode();

    /**
     * @brief Gets the audio effect mode.
     *
     * @return  Returns current audio effect mode.
     */
    AudioEffectMode GetStreamAudioEffectMode();

    /**
     * @brief Gets the audio frame size that has been written.
     *
     * @return Returns the audio frame size that has been written.
     */
    int64_t GetStreamFramesWritten();

    /**
     * @brief Gets the audio frame size that has been read.
     *
     * @return Returns the audio frame size that has been read.
     */
    int64_t GetStreamFramesRead();

    /**
     * @brief Sets the audio effect mode.
     *
     * @param effectMode The audio effect mode at which the stream needs to be rendered.
     * @return  Returns {@link SUCCESS} if audio effect mode is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
     */
    int32_t SetStreamAudioEffectMode(AudioEffectMode effectMode);

    void SetStreamUsage(StreamUsage usage);
	
    int32_t SetAudioCaptureMode(AudioCaptureMode captureMode);
    AudioCaptureMode GetAudioCaptureMode();
    /**
     * @brief Set the applicationcache path to access the application resources
     *
     * @return none
     */
    void SetApplicationCachePath(const std::string cachePath) override;

    /**
     * @brief Verifies the clients permsiion based on appTokenId
     *
     * @return Returns whether the authentication was success or not
     */
    bool CheckRecordingCreate(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid) override;

    bool CheckRecordingStateChange(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid,
        AudioPermissionState state) override;
    int32_t SetStreamLowPowerVolume(float powerVolumeFactor);
    float GetStreamLowPowerVolume();
    float GetSingleStreamVol();

    // Audio timer callback
    virtual void OnTimeOut() override;

    void SetClientID(int32_t clientPid, int32_t clientUid) override;

    IAudioStream::StreamClass GetStreamClass() override;
    void GetStreamSwitchInfo(SwitchInfo& info);

    void SetWakeupCapturerState(bool isWakeupCapturer) override;

protected:
    virtual void ProcessEvent(const AppExecFwk::InnerEvent::Pointer &event) override;
    void SendWriteBufferRequestEvent();
    void SendReadBufferRequestEvent();
    void HandleWriteRequestEvent();
    void HandleReadRequestEvent();
    int32_t SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback) override;
    int32_t SetCapturerReadCallback(const std::shared_ptr<AudioCapturerReadCallback> &callback) override;

private:
    pa_threaded_mainloop *mainLoop;
    pa_mainloop_api *api;
    pa_context *context;
    pa_stream *paStream;
    pa_sample_spec sampleSpec;

    std::mutex dataMutex_;
    std::condition_variable dataCv_;
    std::mutex ctrlMutex_;
    std::mutex capturerMarkReachedMutex_;
    std::mutex capturerPeriodReachedMutex_;
    std::mutex rendererMarkReachedMutex_;
    std::mutex rendererPeriodReachedMutex_;
    std::mutex runnerMutex_;
    std::mutex writeCallbackMutex_;
    std::mutex stoppingMutex_;
    bool runnerReleased_ = false;
    AudioCache acache_;
    const void *internalReadBuffer_;
    size_t internalRdBufLen_;
    size_t internalRdBufIndex_;
    size_t setBufferSize_;
    int32_t streamCmdStatus_;
    int32_t streamDrainStatus_;
    int32_t streamFlushStatus_;
    bool isMainLoopStarted_;
    bool isContextConnected_;
    bool isStreamConnected_;
    bool isInnerCapturerStream_;
    bool isWakeupCapturerStream_ = false;
    AudioPrivacyType mPrivacyType;
    StreamUsage mStreamUsage;

    std::unique_ptr<uint8_t[]> preBuf_ {nullptr};
    uint32_t sinkLatencyInMsec_ {0};

    int32_t clientPid_ = 0;
    int32_t clientUid_ = 0;

    std::string appCookiePath = "";
    std::string cachePath_ = "";

    float mVolumeFactor;
    float mPowerVolumeFactor;
    AudioStreamType mStreamType;
    AudioSystemManager *mAudioSystemMgr;

    pa_cvolume cvolume;
    uint32_t streamIndex;
    uint32_t sessionID_;
    uint32_t volumeChannels;
    bool streamInfoUpdated;
    bool firstFrame_;

    AudioRendererRate renderRate;
    uint32_t rendererSampleRate;
    AudioRenderMode renderMode_;
    AudioCaptureMode captureMode_;
    std::string effectSceneName = "";
    AudioEffectMode effectMode;
    std::shared_ptr<AudioCapturerReadCallback> readCallback_;
    std::shared_ptr<AudioRendererWriteCallback> writeCallback_;
    int64_t mWriteCbStamp = 0; // used to measure callback duration
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
    StateChangeCmdType stateChangeCmdType_ = CMD_FROM_CLIENT;
    pa_stream_success_cb_t PAStreamCorkSuccessCb;

    // To be set while using audio stream
    // functionality for callbacks
    AudioRendererCallbacks *mAudioRendererCallbacks;
    AudioCapturerCallbacks *mAudioCapturerCallbacks;

    std::map<uint32_t, SinkDeviceInfo*> sinkDevices;
    std::map<uint32_t, SourceDeviceInfo*> sourceDevices;
    std::map<uint32_t, SinkInputInfo*> sinkInputs;
    std::map<uint32_t, SourceOutputInfo*> sourceOutputs;
    std::map<uint32_t, ClientInfo*> clientInfo;

    IAudioStream::StreamClass streamClass_;

    ASClientType eAudioClientType;

    uint32_t underFlowCount;
    int32_t ConnectStreamToPA();
    const std::string GetDeviceNameForConnect();

    // Audio cache related functions. These APIs are applicable only for playback scenarios
    int32_t InitializeAudioCache();
    size_t WriteToAudioCache(const StreamBuffer &stream);
    int32_t DrainAudioCache();

    int32_t UpdateReadBuffer(uint8_t *buffer, size_t &length, size_t &readSize);
    int32_t PaWriteStream(const uint8_t *buffer, size_t &length);
    void HandleRenderPositionCallbacks(size_t bytesWritten);
    void HandleCapturePositionCallbacks(size_t bytesRead);

    void WriteStateChangedSysEvents();

    // Error code used
    static const int32_t AUDIO_CLIENT_SUCCESS = 0;
    static const int32_t AUDIO_CLIENT_ERR = -1;
    static const int32_t AUDIO_CLIENT_INVALID_PARAMS_ERR = -2;
    static const int32_t AUDIO_CLIENT_INIT_ERR = -3;
    static const int32_t AUDIO_CLIENT_CREATE_STREAM_ERR = -4;
    static const int32_t AUDIO_CLIENT_START_STREAM_ERR = -5;
    static const int32_t AUDIO_CLIENT_READ_STREAM_ERR = -6;
    static const int32_t AUDIO_CLIENT_WRITE_STREAM_ERR = -7;
    static const int32_t AUDIO_CLIENT_PA_ERR = -8;
    static const int32_t AUDIO_CLIENT_PERMISSION_ERR = -9;

    // Default values
    static const uint32_t MINIMUM_BUFFER_SIZE = 1024;
    static const uint32_t DEFAULT_SAMPLING_RATE = 44100;
    static const uint8_t DEFAULT_CHANNEL_COUNT = 2;
    static const uint8_t DEFAULT_SAMPLE_SIZE = 2;
    static const uint32_t DEFAULT_STREAM_VOLUME = 65536;
    static const std::string GetStreamName(AudioStreamType audioType);
    static pa_sample_spec ConvertToPAAudioParams(AudioStreamParams audioParams);
    static AudioStreamParams ConvertFromPAAudioParams(pa_sample_spec paSampleSpec);
    static const std::string GetEffectModeName(AudioEffectMode effectMode);

    static constexpr float MAX_STREAM_VOLUME_LEVEL = 1.0f;
    static constexpr float MIN_STREAM_VOLUME_LEVEL = 0.0f;

    // audio channel index
    static const uint8_t CHANNEL1_IDX = 0;
    static const uint8_t CHANNEL2_IDX = 1;
    static const uint8_t CHANNEL3_IDX = 2;
    static const uint8_t CHANNEL4_IDX = 3;
    static const uint8_t CHANNEL5_IDX = 4;
    static const uint8_t CHANNEL6_IDX = 5;
    static const uint8_t CHANNEL7_IDX = 6;
    static const uint8_t CHANNEL8_IDX = 7;

    // Resets PA audio client and free up resources if any with this API
    void ResetPAAudioClient();
    // For setting some environment variables required while running from hap
    void SetEnv();
    int32_t CorkStream();

    // Callbacks to be implemented
    static void PAStreamStateCb(pa_stream *stream, void *userdata);
    static void PAStreamMovedCb(pa_stream *stream, void *userdata);
    static void PAStreamUnderFlowCb(pa_stream *stream, void *userdata);
    static void PAContextStateCb(pa_context *context, void *userdata);
    static void PAStreamReadCb(pa_stream *stream, size_t length, void *userdata);
    static void PAStreamStartSuccessCb(pa_stream *stream, int32_t success, void *userdata);
    static void PAStreamStopSuccessCb(pa_stream *stream, int32_t success, void *userdata);
    static void PAStreamAsyncStopSuccessCb(pa_stream *stream, int32_t success, void *userdata);
    static void PAStreamPauseSuccessCb(pa_stream *stream, int32_t success, void *userdata);
    static void PAStreamWriteCb(pa_stream *stream, size_t length, void *userdata);
    static void PAStreamDrainSuccessCb(pa_stream *stream, int32_t success, void *userdata);
    static void PAStreamDrainInStopCb(pa_stream *stream, int32_t success, void *userdata);
    static void PAStreamFlushSuccessCb(pa_stream *stream, int32_t success, void *userdata);
    static void PAStreamLatencyUpdateCb(pa_stream *stream, void *userdata);
    static void PAStreamSetBufAttrSuccessCb(pa_stream *stream, int32_t success, void *userdata);

    static void GetSinkInputInfoCb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata);
    static void SetPaVolume(const AudioServiceClient &client);
    static AudioVolumeType GetVolumeTypeFromStreamType(AudioStreamType streamType);

    // OnRenderMarkReach SetRenderMarkReached UnsetRenderMarkReach  by eventHandler
    void SendRenderMarkReachedRequestEvent(uint64_t mFrameMarkPosition);
    void HandleRenderMarkReachedEvent(uint64_t mFrameMarkPosition);
    void SendSetRenderMarkReachedRequestEvent(const std::shared_ptr<RendererPositionCallback> &callback);
    void HandleSetRenderMarkReachedEvent(const std::shared_ptr<RendererPositionCallback> &callback);
    void SendUnsetRenderMarkReachedRequestEvent();
    void HandleUnsetRenderMarkReachedEvent();

    // OnRenderPeriodReach SetRenderPeriodReach UnsetRenderPeriodReach by eventHandler
    void SendRenderPeriodReachedRequestEvent(uint64_t mFramePeriodNumber);
    void HandleRenderPeriodReachedEvent(uint64_t mFramePeriodNumber);
    void SendSetRenderPeriodReachedRequestEvent(const std::shared_ptr<RendererPeriodPositionCallback> &callback);
    void HandleSetRenderPeriodReachedEvent(const std::shared_ptr<RendererPeriodPositionCallback> &callback);
    void SendUnsetRenderPeriodReachedRequestEvent();
    void HandleUnsetRenderPeriodReachedEvent();

    // OnCapturerMarkReach SetCapturerMarkReach UnsetCapturerMarkReach by eventHandler
    void SendCapturerMarkReachedRequestEvent(uint64_t mFrameMarkPosition);
    void HandleCapturerMarkReachedEvent(uint64_t mFrameMarkPosition);
    void SendSetCapturerMarkReachedRequestEvent(const std::shared_ptr<CapturerPositionCallback> &callback);
    void HandleSetCapturerMarkReachedEvent(const std::shared_ptr<CapturerPositionCallback> &callback);
    void SendUnsetCapturerMarkReachedRequestEvent();
    void HandleUnsetCapturerMarkReachedEvent();

    // OnCapturerPeriodReach SetCapturerPeriodReach UnsetCapturerPeriodReach by eventHandler
    void SendCapturerPeriodReachedRequestEvent(uint64_t mFramePeriodNumber);
    void HandleCapturerPeriodReachedEvent(uint64_t mFramePeriodNumber);
    void SendSetCapturerPeriodReachedRequestEvent(
        const std::shared_ptr<CapturerPeriodPositionCallback> &callback);
    void HandleSetCapturerPeriodReachedEvent(
        const std::shared_ptr<CapturerPeriodPositionCallback> &callback);
    void SendUnsetCapturerPeriodReachedRequestEvent();
    void HandleUnsetCapturerPeriodReachedEvent();

    enum {
        WRITE_BUFFER_REQUEST = 0,
        READ_BUFFER_REQUEST,

        RENDERER_MARK_REACHED_REQUEST,
        SET_RENDERER_MARK_REACHED_REQUEST,
        UNSET_RENDERER_MARK_REACHED_REQUEST,

        RENDERER_PERIOD_REACHED_REQUEST,
        SET_RENDERER_PERIOD_REACHED_REQUEST,
        UNSET_RENDERER_PERIOD_REACHED_REQUEST,

        CAPTURER_PERIOD_REACHED_REQUEST,
        SET_CAPTURER_PERIOD_REACHED_REQUEST,
        UNSET_CAPTURER_PERIOD_REACHED_REQUEST,

        CAPTURER_MARK_REACHED_REQUEST,
        SET_CAPTURER_MARK_REACHED_REQUEST,
        UNSET_CAPTURER_MARK_REACHED_REQUEST,
    };
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_SERVICE_CLIENT_H
