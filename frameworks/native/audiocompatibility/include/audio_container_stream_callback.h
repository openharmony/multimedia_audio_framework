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
#include <audio_error.h>
#include <audio_info.h>
#include <audio_timer.h>
#include "audio_capturer.h"
#include "audio_renderer.h"
#include "audio_system_manager.h"

#include "ipc_types.h"
#include "iremote_broker.h"
#include "iremote_proxy.h"
#include "iremote_stub.h"
#include "iservice_registry.h"

namespace OHOS {
namespace AudioStandard {
enum ASClientType {
    AUDIO_SERVICE_CLIENT_PLAYBACK,
    AUDIO_SERVICE_CLIENT_RECORD,
    AUDIO_SERVICE_CLIENT_CONTROLLER
};

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

/**
 * @brief Enumerates the stream states of the current device.
 *
 * @since 1.0
 * @version 1.0
 */
enum State {
    /** INVALID */
    INVALID = -1,
    /** New */
    NEW,
    /** Prepared */
    PREPARED,
    /** Running */
    RUNNING,
    /** Stopped */
    STOPPED,
    /** Released */
    RELEASED,
    /** Paused */
    PAUSED
};

/**
 * @brief Enumerates the stream states of the current device.
 *
 * @since 1.0
 * @version 1.0
 */
enum CMD_SEND {
    CMD_CREATE_AUDIOSTREAM = 1,
    CMD_START_AUDIOSTREAM = 2,
    CMD_PAUSE_AUDIOSTREAM = 3,
    CMD_STOP_AUDIOSTREAM = 4,
    CMD_FLUSH_AUDIOSTREAM = 5,
    CMD_DRAIN_AUDIOSTREAM = 6,
    CMD_GET_AUDIO_SESSIONID = 7,
    CMD_SET_AUDIORENDERER_MODE = 8,
    CMD_WRITE_AUDIOSTREAM = 10,
    CMD_RELEASE_AUDIOSTREAM = 11,
    CMD_GET_MINIMUM_BUFFERSIZE = 12,
    CMD_GET_MINIMUM_FRAMECOUNT = 13,
    CMD_SET_STREAM_VOLUME = 15,
    CMD_GET_AUDIO_LATENCY = 16,
    CMD_SET_STREAM_TYPE = 17,
    CMD_SET_STREAM_RENDER_RATE = 19,
    CMD_GET_CURRENT_TIMESTAMP = 21,
    CMD_GET_AUDIOSTREAM_PARAMS = 29,
    CMD_WRITE_RENDERER_CALLBACK = 30,
    CMD_READ_STREAM = 39,
};

enum AUDIO_STREAM_ERR {
    // Error code used
    AUDIO_CLIENT_SUCCESS = 0,
    AUDIO_CLIENT_ERR = -1,
    AUDIO_CLIENT_INVALID_PARAMS_ERR = -2,
    AUDIO_CLIENT_INIT_ERR = -3,
    AUDIO_CLIENT_CREATE_STREAM_ERR = -4,
    AUDIO_CLIENT_START_STREAM_ERR = -5,
    AUDIO_CLIENT_READ_STREAM_ERR = -6,
    AUDIO_CLIENT_WRITE_STREAM_ERR = -7,
    AUDIO_CLIENT_PA_ERR = -8,
};


class AudioStreamCallback {
public:
    virtual ~AudioStreamCallback() = default;
    /**
     * Called when stream state is updated.
     *
     * @param state Indicates the InterruptEvent information needed by client.
     * For details, refer InterruptEvent struct in audio_info.h
    */
    virtual void OnStateChange(const State state, StateChangeCmdType cmdType = CMD_FROM_CLIENT) = 0;
};

class AudioContainerStreamCallback {
    // Need to check required state changes to update applications
    virtual void OnStreamStateChangeCb() const = 0;
    virtual void OnStreamBufferUnderFlowCb() const = 0;
    virtual void OnStreamBufferOverFlowCb() const = 0;
    virtual void OnErrorCb(AudioServiceErrorCodes error) const = 0;
    virtual void OnEventCb(AudioServiceEventTypes error) const = 0;
};

class AudioRendererCallbacks : public AudioContainerStreamCallback {
public:
    virtual ~AudioRendererCallbacks();
    virtual void OnSinkDeviceUpdatedCb() const = 0;
};

class AudioCapturerCallbacks : public AudioContainerStreamCallback {
public:
    virtual ~AudioCapturerCallbacks();
    virtual void OnSourceDeviceUpdatedCb() const = 0;
};

class IAudioContainerService : public IRemoteBroker {
public:
    /**
     * Initializes audio service client for the required client type
     *
     * @param eClientType indicates the client type like playback, record or controller.
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
     */
    virtual int32_t InitializeGa(ASClientType eClientType) = 0;

    // Stream handling APIs

    /**
     * Creates & initializes resources based on the audioParams and audioType
     *
     * @param audioParams indicate format, sampling rate and number of channels
     * @param audioType indicate the stream type like music, system, ringtone etc
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
     */
    virtual int32_t CreateStreamGa(AudioStreamParams audioParams, AudioStreamType audioType) = 0;

    /**
     * @brief Obtains session ID
     *
     * @return Returns unique session ID for the created session.
    */
    virtual int32_t GetSessionIDGa(uint32_t &sessionID, const int32_t &trackId) = 0;

    /**
     * Starts the stream created using CreateStreamGa
     *
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
     */
    virtual int32_t StartStreamGa(const int32_t &trackId) = 0;

    /**
     * Stops the stream created using CreateStreamGa
     *
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
     */
    virtual int32_t StopStreamGa(const int32_t &trackId) = 0;

    /**
     * Flushes the stream created using CreateStreamGa. This is applicable for
     * playback only
     *
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
     */
    virtual int32_t FlushStreamGa(const int32_t &trackId) = 0;

    /**
     * Drains the stream created using CreateStreamGa. This is applicable for
     * playback only
     *
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
     */
    virtual int32_t DrainStreamGa(const int32_t &trackId) = 0;

    /**
     * Pauses the stream
     *
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
     */
    virtual int32_t PauseStreamGa(const int32_t &trackId) = 0;

    /**
     * Update the stream type
     *
     * @param audioStreamType Audio stream type
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
     */
    virtual int32_t SetStreamTypeGa(AudioStreamType audioStreamType, const int32_t &trackId) = 0;

    /**
     * Sets the volume of the stream associated with session ID
     *
     * @param sessionID indicates the ID for the active stream to be controlled
     * @param volume indicates volume level between 0 to 65536
     * @return returns {@code 0} if success; returns {@code -1} otherwise.
     */
    virtual int32_t SetStreamVolumeGa(uint32_t sessionID, uint32_t volume) = 0;

    /**
     * Get the volume of the stream associated with session ID
     *
     * @param sessionID indicates the ID for the active stream to be controlled
     * @return Returns volume level between 0 to 65536
     */
    virtual uint32_t GetStreamVolumeGa(uint32_t sessionID) = 0;
    
    /**
     * Writes audio data of the stream created using CreateStreamGa to active sink device
     *
     * @param buffer contains audio data to write
     * @param bufferSize indicates the size of audio data in bytes to write from the buffer
     * @param pError indicates pointer to error which will be filled in case of internal errors
     * @return Returns size of audio data written in bytes.
     */
    virtual size_t WriteStreamGa(const StreamBuffer &stream, int32_t &pError, const int32_t &trackId) = 0;

    /**
     * Writes audio data of the stream created using CreateStreamGa
     *
     * @param buffer contains audio data to write
     * @param bufferSize indicates the size of audio data in bytes to write from the buffer
     * @param pError indicates pointer to error which will be filled in case of internal errors
     * @return Returns size of audio data written in bytes.
     */
    virtual size_t WriteStreamInCbGa(const StreamBuffer &stream, int32_t &pError, const int32_t &trackId) = 0;

    /**
     * Reads audio data of the stream created using CreateStreamGa from active source device
     *
     * @param StreamBuffer including buffer to be filled with audio data
     * andbufferSize indicating the size of audio data to read into buffer
     * @param isBlocking indicates if the read is blocking or not
     * @return Returns size read if success; returns {@code -1} failure.
     */
    virtual int32_t ReadStreamGa(StreamBuffer &stream, bool isBlocking, const int32_t &trackId) = 0;

    /**
     * Release the resources allocated using CreateStreamGa
     *
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    virtual int32_t ReleaseStreamGa(const int32_t &trackId) = 0;

    /**
     * Provides the current timestamp for playback/record stream created using CreateStreamGa
     *
     * @param timeStamp will be filled up with the current timestamp
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    virtual int32_t GetCurrentTimeStampGa(uint64_t &timeStamp, const int32_t &trackId) = 0;

    /**
     * Provides the current latency for playback/record stream created using CreateStreamGa
     *
     * @param latency will be filled up with current latency in microseconds
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    virtual int32_t GetAudioLatencyGa(uint64_t &latency, const int32_t &trackId) = 0;

    /**
     * Provides the playback/record stream parameters created using CreateStreamGa
     *
     * @param audioParams will be filled up with stream audio parameters
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    virtual int32_t GetAudioStreamParamsGa(AudioStreamParams &audioParams, const int32_t &trackId) = 0;

    /**
     * Provides the minimum buffer size required for this audio stream
     * created using CreateStreamGa
     * @param minBufferSize will be set to minimum buffer size in bytes
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    virtual int32_t GetMinimumBufferSizeGa(size_t &minBufferSize, const int32_t &trackId) = 0;

    /**
     * Provides the minimum frame count required for this audio stream
     * created using CreateStreamGa
     * @param frameCount will be set to minimum number of frames
     * @return Returns {@code 0} if success; returns {@code -1} otherwise.
    */
    virtual int32_t GetMinimumFrameCountGa(uint32_t &frameCount, const int32_t &trackId) = 0;

    /**
     * @brief Set the buffer duration in msec
     *
     * @param bufferSizeInMsec buffer size in duration.
     * @return Returns {@link SUCCESS} defined in {@link audio_errors.h} otherwise.
    */
    virtual int32_t SetBufferSizeInMsecGa(int32_t bufferSizeInMsec) = 0;

    /**
     * Provides the sampling rate for the active audio stream
     * created using CreateStreamGa
     *
     * @return Returns sampling rate in Hz
    */
    virtual uint32_t GetSamplingRateGa() const = 0;

    /**
     * Provides the channel count for the active audio stream
     * created using CreateStreamGa
     *
     * @return Returns number of channels
    */
    virtual uint8_t GetChannelCountGa() const = 0;

    /**
     * Provides the sample size for the active audio stream
     * created using CreateStreamGa
     *
     * @return Returns sample size in number of bits
    */
    virtual uint8_t GetSampleSizeGa() const = 0;

    // Device volume & route handling APIS

    // Audio stream callbacks

    /**
     * Register for callbacks associated with the playback stream created using CreateStreamGa
     *
     * @param cb indicates pointer for registered callbacks
     * @return none
    */
    virtual void RegisterAudioRendererCallbackGa(const AudioRendererCallback &cb) = 0;

    /**
     * Register for callbacks associated with the record stream created using CreateStream
     *
     * @param cb indicates pointer for registered callbacks
     * @return none
    */
    virtual void RegisterAudioCapturerCallbackGa(const AudioCapturerCallback &cb) = 0;

    /**
     * Set the renderer frame position callback
     *
     * @param callback indicates pointer for registered callbacks
     * @return none
    */
    virtual void SetRendererPositionCallbackGa(int64_t markPosition,
        const std::shared_ptr<RendererPositionCallback> &callback) = 0;

    /**
     * Unset the renderer frame position callback
     *
     * @return none
    */
    virtual void UnsetRendererPositionCallbackGa() = 0;

    /**
     * Set the renderer frame period position callback
     *
     * @param callback indicates pointer for registered callbacks
     * @return none
    */
    virtual void SetRendererPeriodPositionCallbackGa(int64_t markPosition,
        const std::shared_ptr<RendererPeriodPositionCallback> &callback) = 0;

    /**
     * Unset the renderer frame period position callback
     *
     * @return none
    */
    virtual void UnsetRendererPeriodPositionCallbackGa() = 0;

    /**
     * Set the capturer frame position callback
     *
     * @param callback indicates pointer for registered callbacks
     * @return none
    */
    virtual void SetCapturerPositionCallbackGa(int64_t markPosition,
        const std::shared_ptr<CapturerPositionCallback> &callback) = 0;

    /**
     * Unset the capturer frame position callback
     *
     * @return none
    */
    virtual void UnsetCapturerPositionCallbackGa() = 0;

    /**
     * Set the capturer frame period position callback
     *
     * @param callback indicates pointer for registered callbacks
     * @return none
    */
    virtual void SetCapturerPeriodPositionCallbackGa(int64_t markPosition,
        const std::shared_ptr<CapturerPeriodPositionCallback> &callback) = 0;

    /**
     * Unset the capturer frame period position callback
     *
     * @return none
    */
    virtual void UnsetCapturerPeriodPositionCallbackGa() = 0;

    /**
     * @brief Set the track volume
     *
     * @param volume The volume to be set for the current track.
     * @return Returns {@link SUCCESS} if volume is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
    */
    virtual int32_t SetStreamVolumeGa(float volume, const int32_t &trackId) = 0;

    /**
     * @brief Obtains the current track volume
     *
     * @return Returns current track volume
    */
    virtual float GetStreamVolumeGa() = 0;

    /**
     * @brief Set the render rate
     *
     * @param renderRate The rate at which the stream needs to be rendered.
     * @return Returns {@link SUCCESS} if render rate is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
    */
    virtual int32_t SetStreamRenderRateGa(AudioRendererRate renderRate, const int32_t &trackId) = 0;

    /**
     * @brief Obtains the current render rate
     *
     * @return Returns current render rate
    */
    virtual AudioRendererRate GetStreamRenderRateGa() = 0;

    /**
     * @brief Saves StreamCallback
     *
     * @param callback callback reference to be saved.
     * @return none.
    */
    virtual void SaveStreamCallbackGa(const std::weak_ptr<AudioStreamCallback> &callback) = 0;

    /**
     * @brief Sets the render mode. By default the mode is RENDER_MODE_NORMAL.
     * This API is needs to be used only if RENDER_MODE_CALLBACK is required.
     *
     * @param renderMode The mode of render.
     * @return Returns {@link SUCCESS} if render mode is successfully set; returns an error code
     * defined in {@link audio_errors.h} otherwise.
    */
    virtual int32_t SetAudioRenderModeGa(AudioRenderMode renderMode, const int32_t &trackId) = 0;

    /**
     * @brief Obtains the render mode.
     *
     * @return Returns current render mode.
    */
    virtual AudioRenderMode GetAudioRenderModeGa() = 0;

    /**
     * @brief Registers the renderer write callback listener.
     * This API should only be used if RENDER_MODE_CALLBACK is needed.
     *
     * @return Returns {@link SUCCESS} if callback registreation is successful; returns an error code
     * defined in {@link audio_errors.h} otherwise.
    */
    virtual int32_t SaveWriteCallbackGa(const std::weak_ptr<AudioRendererWriteCallback> &callback,
        const int32_t &trackId = 0) = 0;

    virtual int32_t SetAudioCaptureMode(AudioCaptureMode captureMode) = 0;

    virtual int32_t SaveReadCallback(const std::weak_ptr<AudioCapturerReadCallback> &callback) = 0;
    /**
     * @brief Set the applicationcache path to access the application resources
     *
     * @return none
    */
    virtual void SetAppCachePath(const std::string cachePath) = 0;

    virtual AudioCaptureMode GetAudioCaptureMode() = 0;
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"ohos.sysability.samgr.IAudioStreamServiceGateway");
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_SERVICE_CLIENT_H
