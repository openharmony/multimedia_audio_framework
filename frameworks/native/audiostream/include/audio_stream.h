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
#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include <mutex>
#include <condition_variable>
#include "timestamp.h"
#include "event_handler.h"
#include "event_runner.h"
#include "audio_info.h"
#include "audio_channel_blend.h"
#include "audio_service_client.h"
#include "audio_stream_tracker.h"
#include "volume_ramp.h"
#include "audio_format_converter_3DA.h"
#ifdef SONIC_ENABLE
#include "sonic.h"
#include "audio_speed.h"
#endif

namespace OHOS {
namespace AudioStandard {
static constexpr int32_t MAX_WRITECB_NUM_BUFFERS = 1;
static constexpr int32_t MAX_READCB_NUM_BUFFERS = 3;
static constexpr int32_t ONE_MINUTE = 60;
class AudioStreamPolicyServiceDiedCallbackImpl;

class AudioStream : public AudioServiceClient {
public:
    AudioStream(AudioStreamType eStreamType, AudioMode eMode, int32_t appUid);
    virtual ~AudioStream();

    void SetRendererInfo(const AudioRendererInfo &rendererInfo) override;
    void SetCapturerInfo(const AudioCapturerInfo &capturerInfo) override;
    int32_t SetAudioStreamInfo(const AudioStreamParams info,
        const std::shared_ptr<AudioClientTracker> &proxyObj) override;
    int32_t GetAudioStreamInfo(AudioStreamParams &info) override;
    int32_t GetAudioSessionID(uint32_t &sessionID) override;
    State GetState() override;
    bool GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) override;
    bool GetAudioPosition(Timestamp &timestamp, Timestamp::Timestampbase base) override;
    int32_t GetBufferSize(size_t &bufferSize) override;
    int32_t GetFrameCount(uint32_t &frameCount) override;
    int32_t GetLatency(uint64_t &latency) override;
    int32_t SetAudioStreamType(AudioStreamType audioStreamType) override;
    int32_t SetVolume(float volume) override;
    float GetVolume() override;
    int32_t SetRenderRate(AudioRendererRate renderRate) override;
    AudioRendererRate GetRenderRate() override;
    int32_t SetStreamCallback(const std::shared_ptr<AudioStreamCallback> &callback) override;
    void SetPreferredFrameSize(int32_t frameSize) override;
    // callback mode api
    int32_t SetRenderMode(AudioRenderMode renderMode) override;
    AudioRenderMode GetRenderMode() override;
    int32_t SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback) override;
    int32_t SetCaptureMode(AudioCaptureMode captureMode) override;
    AudioCaptureMode GetCaptureMode() override;
    int32_t SetCapturerReadCallback(const std::shared_ptr<AudioCapturerReadCallback> &callback) override;
    int32_t GetBufferDesc(BufferDesc &bufDesc) override;
    int32_t GetBufQueueState(BufferQueueState &bufState) override;
    int32_t Enqueue(const BufferDesc &bufDesc) override;
    int32_t Clear() override;
    void SubmitAllFreeBuffers();

    void SetInnerCapturerState(bool isInnerCapturer) override;
    void SetPrivacyType(AudioPrivacyType privacyType) override;

    int32_t SetLowPowerVolume(float volume) override;
    float GetLowPowerVolume() override;
    int32_t SetOffloadMode(int32_t state, bool isAppBack) override;
    int32_t UnsetOffloadMode() override;
    float GetSingleStreamVolume() override;
    AudioEffectMode GetAudioEffectMode() override;
    int32_t SetAudioEffectMode(AudioEffectMode effectMode) override;
    int64_t GetFramesWritten() override;
    int64_t GetFramesRead() override;

    std::vector<AudioSampleFormat> GetSupportedFormats() const;
    std::vector<AudioEncodingType> GetSupportedEncodingTypes() const;
    std::vector<AudioSamplingRate> GetSupportedSamplingRates() const;

    // Common APIs
    bool StartAudioStream(StateChangeCmdType cmdType = CMD_FROM_CLIENT) override;
    bool PauseAudioStream(StateChangeCmdType cmdType = CMD_FROM_CLIENT) override;
    bool StopAudioStream() override;
    bool ReleaseAudioStream(bool releaseRunner = true) override;
    bool FlushAudioStream() override;

    // Playback related APIs
    bool DrainAudioStream() override;
    int32_t Write(uint8_t *buffer, size_t buffer_size) override;
    int32_t Write(uint8_t *pcmBuffer, size_t pcmSize, uint8_t *metaBuffer, size_t metaSize) override;
    int32_t SetSpeed(float speed) override;
    float GetSpeed() override;
    int32_t ChangeSpeed(uint8_t *buffer, int32_t bufferSize,
        std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize) override;
    int32_t WriteSpeedBuffer(int32_t bufferSize, uint8_t *speedBuffer, size_t speedBufferSize) override;
    int32_t GetStreamBufferCB(StreamBuffer &stream, std::unique_ptr<uint8_t[]> &speedBuffer, int32_t &speedBufferSize);

    // Recording related APIs
    int32_t Read(uint8_t &buffer, size_t userSize, bool isBlockingRead) override;
    void SetStreamTrackerState(bool trackerRegisteredState) override;
    void GetSwitchInfo(SwitchInfo& info) override;
    int32_t SetChannelBlendMode(ChannelBlendMode blendMode) override;
    int32_t SetVolumeWithRamp(float volume, int32_t duration) override;

    int32_t RegisterRendererOrCapturerPolicyServiceDiedCB(
        const std::shared_ptr<RendererOrCapturerPolicyServiceDiedCallback> &callback) override;
    int32_t RemoveRendererOrCapturerPolicyServiceDiedCB() override;

    bool RestoreAudioStream() override;

private:
    enum {
        BIN_TEST_MODE = 1,   //for bin file test
        JS_TEST_MODE,        //for js app test
    };

    void OpenDumpFile();
    void ProcessDataByAudioBlend(uint8_t *buffer, size_t bufferSize);
    void ProcessDataByVolumeRamp(uint8_t *buffer, size_t bufferSize);
    void RegisterTracker(const std::shared_ptr<AudioClientTracker> &proxyObj);
    void WriteMuteDataSysEvent(uint8_t *buffer, size_t bufferSize);
    int32_t InitFromParams(AudioStreamParams &param);

    int32_t RegisterAudioStreamPolicyServerDiedCb();
    int32_t UnregisterAudioStreamPolicyServerDiedCb();

    int32_t NotifyCapturerAdded(uint32_t sessionID) override;

    AudioStreamType eStreamType_;
    AudioMode eMode_;
    State state_;
    bool resetTime_;
    uint64_t resetTimestamp_;
    struct timespec baseTimestamp_ = {0};
    AudioRenderMode renderMode_;
    AudioCaptureMode captureMode_;
    std::queue<BufferDesc> freeBufferQ_;
    std::queue<BufferDesc> filledBufferQ_;
    std::array<std::unique_ptr<uint8_t[]>, MAX_WRITECB_NUM_BUFFERS> writeBufferPool_ = {};
    std::array<std::unique_ptr<uint8_t[]>, MAX_READCB_NUM_BUFFERS> readBufferPool_ = {};
    std::unique_ptr<std::thread> writeThread_ = nullptr;
    std::unique_ptr<std::thread> readThread_ = nullptr;
    bool isReadyToWrite_;
    bool isReadyToRead_;
    void WriteCbTheadLoop();
    void ReadCbThreadLoop();
    std::unique_ptr<AudioStreamTracker> audioStreamTracker_;
    AudioRendererInfo rendererInfo_;
    AudioCapturerInfo capturerInfo_;
    uint32_t sessionId_;

    bool isFirstRead_;
    bool isFirstWrite_;
    bool isPausing_;

    std::mutex bufferQueueLock_;
    std::condition_variable bufferQueueCV_;
    AudioStreamParams streamParams_;
    AudioStreamParams streamOriginParams_;
    AudioBlend audioBlend_;
    VolumeRamp volumeRamp_;
    FILE *pfd_;
    bool streamTrackerRegistered_ = false;
    std::time_t startMuteTime_ = 0;
    bool isUpEvent_ = false;

    float speed_ = 1.0;
#ifdef SONIC_ENABLE
    size_t bufferSize_;
    std::unique_ptr<AudioSpeed> audioSpeed_ = nullptr;
#endif
    std::unique_ptr<AudioFormatConverter3DA> converter_;

    std::shared_ptr<AudioStreamPolicyServiceDiedCallbackImpl> audioStreamPolicyServiceDiedCB_ = nullptr;
    std::shared_ptr<AudioClientTracker> proxyObj_ = nullptr;
};

class AudioStreamPolicyServiceDiedCallbackImpl : public AudioStreamPolicyServiceDiedCallback {
public:
    AudioStreamPolicyServiceDiedCallbackImpl();
    virtual ~AudioStreamPolicyServiceDiedCallbackImpl();
    void OnAudioPolicyServiceDied() override;
    void SaveRendererOrCapturerPolicyServiceDiedCB(
        const std::shared_ptr<RendererOrCapturerPolicyServiceDiedCallback> &callback);
    void RemoveRendererOrCapturerPolicyServiceDiedCB();

private:
    std::mutex mutex_;
    std::shared_ptr<RendererOrCapturerPolicyServiceDiedCallback> policyServiceDiedCallback_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_STREAM_H
