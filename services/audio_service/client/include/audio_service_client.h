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
#include <chrono>
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
#include "audio_spatialization_manager.h"

namespace OHOS {
namespace AudioStandard {
class AudioSpatializationStateChangeCallbackImpl;

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

constexpr size_t DEFAULT_BUFFER_TIME_MS = 20;

struct StreamBuffer {
    uint8_t *buffer; // the virtual address of stream
    uint32_t bufferLen; // stream length in bytes
};

struct AudioCache {
    std::unique_ptr<uint8_t[]> buffer;
    uint32_t readIndex;
    uint32_t writeIndex;
    uint32_t totalCacheSize;
    uint32_t totalCacheSizeTgt;
    bool isFull;
};

static std::map<AudioChannelSet, pa_channel_position> chSetToPaPositionMap = {
    {FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_LEFT}, {FRONT_RIGHT, PA_CHANNEL_POSITION_FRONT_RIGHT},
    {FRONT_CENTER, PA_CHANNEL_POSITION_FRONT_CENTER}, {LOW_FREQUENCY, PA_CHANNEL_POSITION_LFE},
    {SIDE_LEFT, PA_CHANNEL_POSITION_SIDE_LEFT}, {SIDE_RIGHT, PA_CHANNEL_POSITION_SIDE_RIGHT},
    {BACK_LEFT, PA_CHANNEL_POSITION_REAR_LEFT}, {BACK_RIGHT, PA_CHANNEL_POSITION_REAR_RIGHT},
    {FRONT_LEFT_OF_CENTER, PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER},
    {FRONT_RIGHT_OF_CENTER, PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER},
    {BACK_CENTER, PA_CHANNEL_POSITION_REAR_CENTER}, {TOP_CENTER, PA_CHANNEL_POSITION_TOP_CENTER},
    {TOP_FRONT_LEFT, PA_CHANNEL_POSITION_TOP_FRONT_LEFT}, {TOP_FRONT_CENTER, PA_CHANNEL_POSITION_TOP_FRONT_CENTER},
    {TOP_FRONT_RIGHT, PA_CHANNEL_POSITION_TOP_FRONT_RIGHT}, {TOP_BACK_LEFT, PA_CHANNEL_POSITION_TOP_REAR_LEFT},
    {TOP_BACK_CENTER, PA_CHANNEL_POSITION_TOP_REAR_CENTER}, {TOP_BACK_RIGHT, PA_CHANNEL_POSITION_TOP_REAR_RIGHT},
    /** Channel layout positions below do not have precise mapped pulseaudio positions */
    {STEREO_LEFT, PA_CHANNEL_POSITION_FRONT_LEFT}, {STEREO_RIGHT, PA_CHANNEL_POSITION_FRONT_RIGHT},
    {WIDE_LEFT, PA_CHANNEL_POSITION_FRONT_LEFT}, {WIDE_RIGHT, PA_CHANNEL_POSITION_FRONT_RIGHT},
    {SURROUND_DIRECT_LEFT, PA_CHANNEL_POSITION_SIDE_LEFT}, {SURROUND_DIRECT_RIGHT, PA_CHANNEL_POSITION_SIDE_LEFT},
    {BOTTOM_FRONT_CENTER, PA_CHANNEL_POSITION_FRONT_CENTER},
    {BOTTOM_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_LEFT}, {BOTTOM_FRONT_RIGHT, PA_CHANNEL_POSITION_FRONT_RIGHT},
    {TOP_SIDE_LEFT, PA_CHANNEL_POSITION_TOP_REAR_LEFT}, {TOP_SIDE_RIGHT, PA_CHANNEL_POSITION_TOP_REAR_RIGHT},
    {LOW_FREQUENCY_2, PA_CHANNEL_POSITION_LFE},
};

static std::map<uint8_t, AudioChannelLayout> defaultChCountToLayoutMap = {
    {1, CH_LAYOUT_MONO}, {2, CH_LAYOUT_STEREO}, {3, CH_LAYOUT_SURROUND},
    {4, CH_LAYOUT_2POINT0POINT2}, {5, CH_LAYOUT_5POINT0_BACK}, {6, CH_LAYOUT_5POINT1},
    {7, CH_LAYOUT_6POINT1_BACK}, {8, CH_LAYOUT_5POINT1POINT2}, {10, CH_LAYOUT_7POINT1POINT2},
    {12, CH_LAYOUT_7POINT1POINT4}, {14, CH_LAYOUT_9POINT1POINT4}, {16, CH_LAYOUT_9POINT1POINT6}
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

enum UpdatePositionTimeNode {
    START_NODE = 1,
    CORKED_NODE = 2,
    RUNNING_NODE = 3,
    USER_NODE = 4,
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

    int32_t StopStreamPlayback();

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
    * Provides the current timestamp for playback/record stream created using CreateStream
    *
    * @param framePosition will be filled up with current frame position
    * @param timeStamp will be filled up with current timestamp
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t GetCurrentPosition(uint64_t &framePosition, uint64_t &timeStamp);

    /**
    * Update the stream positon and timestamp
    *
    * @param node The time node for updating the stream position and its timestamp
    * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    int32_t UpdateStreamPosition(UpdatePositionTimeNode node);

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

    int32_t SetStreamOffloadMode(int32_t state, bool isAppBack);
    int32_t UnsetStreamOffloadMode();
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
    bool CheckRecordingCreate(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid,
        SourceType sourceType = SOURCE_TYPE_MIC) override;

    bool CheckRecordingStateChange(uint32_t appTokenId, uint64_t appFullTokenId, int32_t appUid,
        AudioPermissionState state) override;
    int32_t SetStreamLowPowerVolume(float powerVolumeFactor);
    float GetStreamLowPowerVolume();
    float GetSingleStreamVol();

    // Audio timer callback
    virtual void OnTimeOut() override;

    void SetClientID(int32_t clientPid, int32_t clientUid, uint32_t appTokenId) override;

    IAudioStream::StreamClass GetStreamClass() override;
    void GetStreamSwitchInfo(SwitchInfo& info);

    void SetWakeupCapturerState(bool isWakeupCapturer) override;
    int32_t HandleMainLoopStart();

    void SetCapturerSource(int capturerSource) override;

    int32_t SetRendererFirstFrameWritingCallback(
        const std::shared_ptr<AudioRendererFirstFrameWritingCallback> &callback) override;

    void OnFirstFrameWriting() override;

    int32_t GetBufferSizeForCapturer(size_t &bufferSize);

    int32_t GetFrameCountForCapturer(uint32_t &frameCount);

    int32_t GetClientPid();

    int32_t RegisterSpatializationStateEventListener();

    int32_t UnregisterSpatializationStateEventListener(uint32_t sessionID);

    void OnSpatializationStateChange(const AudioSpatializationState &spatializationState);

    int32_t SetStreamSpeed(float speed);

    float GetStreamSpeed();

    uint32_t GetAppTokenId() const;

protected:
    virtual void ProcessEvent(const AppExecFwk::InnerEvent::Pointer &event) override;
    void SendWriteBufferRequestEvent();
    void SendReadBufferRequestEvent();
    void HandleWriteRequestEvent();
    void HandleReadRequestEvent();
    int32_t SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback) override;
    int32_t SetCapturerReadCallback(const std::shared_ptr<AudioCapturerReadCallback> &callback) override;
    bool offloadEnable_ = false;
    void CheckOffloadBreakWaitWrite();

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
    std::mutex streamPositionMutex_;
    bool runnerReleased_ = false;
    AudioCache acache_;
    const void *internalReadBuffer_;
    size_t internalRdBufLen_;
    size_t internalRdBufIndex_;
    size_t setBufferSize_;
    int32_t streamCmdStatus_;
    int32_t streamDrainStatus_;
    int32_t streamFlushStatus_;
    bool isMainLoopStarted_ = false;
    uint32_t streamId_ = 0;
    bool isContextConnected_ = false;
    bool isStreamConnected_ = false;
    bool isInnerCapturerStream_ = false;
    bool isWakeupCapturerStream_ = false;
    int capturerSource_ = -1;
    AudioPrivacyType mPrivacyType;
    StreamUsage mStreamUsage;

    std::unique_ptr<uint8_t[]> preBuf_ {nullptr};
    uint32_t sinkLatencyInMsec_ {0};

    int32_t clientPid_ = 0;
    int32_t clientUid_ = 0;
    uint32_t appTokenId_ = 0;

    std::string appCookiePath = "";
    std::string cachePath_ = "";

    float volumeFactor_ = 1.0f;
    float powerVolumeFactor_ = 1.0f;
    float duckVolumeFactor_ = 1.0f;
    AudioStreamType streamType_ = STREAM_MUSIC;
    AudioSystemManager *audioSystemManager_ = nullptr;
    uint64_t channelLayout_;

    pa_cvolume cvolume;
    uint32_t streamIndex_ = 0;
    uint32_t sessionID_ = 0;
    uint32_t volumeChannels_ = STEREO;
    bool streamInfoUpdated_ = false;
    bool firstFrame_ = true;
    bool hasFirstFrameWrited_ = false;

    AudioRendererRate renderRate;
    uint32_t rendererSampleRate;
    AudioRenderMode renderMode_;
    AudioCaptureMode captureMode_;
    std::string effectSceneName = "";
    AudioEffectMode effectMode;
    std::shared_ptr<AudioCapturerReadCallback> readCallback_;
    std::shared_ptr<AudioRendererWriteCallback> writeCallback_;
    int64_t writeCbStamp_ = 0; // used to measure callback duration
    uint32_t mFrameSize = 0;
    bool mMarkReached = false;
    uint64_t mFrameMarkPosition = 0;
    uint64_t mFramePeriodNumber = 0;

    uint64_t mTotalBytesWritten = 0;
    uint64_t mFramePeriodWritten = 0;
    std::shared_ptr<RendererPositionCallback> mRenderPositionCb;
    std::shared_ptr<RendererPeriodPositionCallback> mRenderPeriodPositionCb;
    std::shared_ptr<AudioRendererFirstFrameWritingCallback> firstFrameWritingCb_ = nullptr;

    uint64_t mTotalBytesRead = 0;
    uint64_t mFramePeriodRead = 0;
    std::shared_ptr<CapturerPositionCallback> mCapturePositionCb;
    std::shared_ptr<CapturerPeriodPositionCallback> mCapturePeriodPositionCb;

    std::vector<std::unique_ptr<std::thread>> mPositionCBThreads;
    std::vector<std::unique_ptr<std::thread>> mPeriodPositionCBThreads;

    std::weak_ptr<AudioStreamCallback> streamCallback_;
    State state_;
    bool breakingWritePa_ = false;
    StateChangeCmdType stateChangeCmdType_ = CMD_FROM_CLIENT;
    pa_stream_success_cb_t PAStreamCorkSuccessCb;

    std::string spatializationEnabled_ = "Invalid";
    std::string headTrackingEnabled_ = "Invalid";
    uint32_t spatializationRegisteredSessionID_ = 0;
    bool firstSpatializationRegistered_ = true;
    std::shared_ptr<AudioSpatializationStateChangeCallbackImpl> spatializationStateChangeCallback_ = nullptr;
    pa_usec_t paLatency_ = 0;
    bool isGetLatencySuccess_ = true;

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
    int64_t offloadTsOffset_ = 0;
    uint64_t offloadTsLast_ = 0;
    uint64_t offloadWriteIndex_ = 0;
    uint64_t offloadReadIndex_ = 0;
    uint64_t offloadTimeStamp_ = 0;
    uint64_t offloadLastHdiPosFrames_ = 0;
    uint64_t offloadLastHdiPosTs_ = 0;
    uint64_t offloadLastUpdatePaInfoTs_ = 0;
    std::timed_mutex offloadWaitWriteableMutex_;
    AudioOffloadType offloadStatePolicy_ = OFFLOAD_DEFAULT;
    AudioOffloadType offloadNextStateTargetPolicy_ = OFFLOAD_DEFAULT;
    time_t lastOffloadUpdateFinishTime_ = 0;
    float speed_ = 1.0;

    bool firstUpdatePosition_ = true;
    uint64_t lastStreamPosition_ = 0;
    uint64_t lastPositionTimestamp_ = 0;
    uint64_t lastHdiPosition_ = 0;
    uint64_t lastOffloadStreamCorkedPosition_ = 0;
    uint64_t preFrameNum_ = 0;

    int32_t ConnectStreamToPA();
    int32_t HandlePAStreamConnect(const std::string &deviceNameS, int32_t latencyInMSec);
    int32_t WaitStreamReady();
    std::pair<const int32_t, const std::string> GetDeviceNameForConnect();
    int32_t UpdatePAProbListOffload(AudioOffloadType statePolicy);
    int32_t UpdatePolicyOffload(AudioOffloadType statePolicy);
    int32_t InitializePAProbListOffload();
    int32_t CheckOffloadPolicyChanged();
    void GetAudioLatencyOffload(uint64_t &latency);
    void ResetOffload();
    int32_t OffloadStopStream();
    void GetOffloadCurrentTimeStamp(uint64_t paTimeStamp, uint64_t paWriteIndex, uint64_t &outTimeStamp);
    void GetOffloadApproximatelyCacheTime(uint64_t timeStamp, uint64_t paWriteIndex, uint64_t &cacheTimePaDsp);
    int32_t CreateStreamWithPa(AudioStreamParams audioParams, AudioStreamType audioType);

    // Audio cache related functions. These APIs are applicable only for playback scenarios
    int32_t InitializeAudioCache();
    size_t WriteToAudioCache(const StreamBuffer &stream);
    int32_t DrainAudioCache();

    int32_t UpdateReadBuffer(uint8_t *buffer, size_t &length, size_t &readSize);
    int32_t PaWriteStream(const uint8_t *buffer, size_t &length);
    int32_t WaitWriteable(size_t length, size_t &writableSize);
    int32_t AdjustAcache(const StreamBuffer &stream, size_t &cachedLen);
    int32_t SetStreamVolumeInML(float volume);
    void HandleRenderPositionCallbacks(size_t bytesWritten);
    void HandleCapturePositionCallbacks(size_t bytesRead);

    void WriteStateChangedSysEvents();
    int32_t UpdateOffloadStreamPosition(UpdatePositionTimeNode node, uint64_t& frames,
        int64_t& timeSec, int64_t& timeNanoSec);

    int32_t SetPaProplist(pa_proplist *propList, pa_channel_map &map,
        AudioStreamParams &audioParams, const std::string &streamName, const std::string &streamStartTime);

    void UpdatePropListForFlush();
    
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
    uint32_t ConvertChLayoutToPaChMap(const uint64_t &channelLayout, pa_channel_map &paMap);

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
    static void PAStreamEventCb(pa_stream *stream, const char *event, pa_proplist *pl, void *userdata);
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
    static void PAStreamUpdateTimingInfoSuccessCb(pa_stream *stream, int32_t success, void *userdata);

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

    virtual int32_t NotifyCapturerAdded(uint32_t sessionID) = 0;

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

class AudioSpatializationStateChangeCallbackImpl : public AudioSpatializationStateChangeCallback {
public:
    AudioSpatializationStateChangeCallbackImpl();
    virtual ~AudioSpatializationStateChangeCallbackImpl();

    void OnSpatializationStateChange(const AudioSpatializationState &spatializationState) override;
    void setAudioServiceClientObj(AudioServiceClient *serviceClientObj);
private:
    AudioServiceClient *serviceClient_ = nullptr;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_SERVICE_CLIENT_H
