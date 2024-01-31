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

#ifndef AUDIO_RENDERER_PRIVATE_H
#define AUDIO_RENDERER_PRIVATE_H

#include "audio_interrupt_callback.h"
#include "audio_renderer.h"
#include "audio_renderer_proxy_obj.h"
#include "audio_utils.h"
#include "i_audio_stream.h"

namespace OHOS {
namespace AudioStandard {
constexpr uint32_t INVALID_SESSION_ID = static_cast<uint32_t>(-1);
class AudioRendererStateChangeCallbackImpl;
class RendererPolicyServiceDiedCallback;
class OutputDeviceChangeWithInfoCallbackImpl;

class AudioRendererPrivate : public AudioRenderer {
public:
    int32_t GetFrameCount(uint32_t &frameCount) const override;
    int32_t GetLatency(uint64_t &latency) const override;
    void SetAudioPrivacyType(AudioPrivacyType privacyType) override;
    int32_t SetParams(const AudioRendererParams params) override;
    int32_t GetParams(AudioRendererParams &params) const override;
    int32_t GetRendererInfo(AudioRendererInfo &rendererInfo) const override;
    int32_t GetStreamInfo(AudioStreamInfo &streamInfo) const override;
    bool Start(StateChangeCmdType cmdType = CMD_FROM_CLIENT) const override;
    int32_t Write(uint8_t *buffer, size_t bufferSize) override;
    int32_t Write(uint8_t *pcmBuffer, size_t pcmSize, uint8_t *metaBuffer, size_t metaSize) override;
    RendererState GetStatus() const override;
    bool GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) const override;
    bool GetAudioPosition(Timestamp &timestamp, Timestamp::Timestampbase base) const override;
    bool Drain() const override;
    bool PauseTransitent(StateChangeCmdType cmdType = CMD_FROM_CLIENT) const override;
    bool Pause(StateChangeCmdType cmdType = CMD_FROM_CLIENT) const override;
    bool Stop() const override;
    bool Flush() const override;
    bool Release() const override;
    int32_t GetBufferSize(size_t &bufferSize) const override;
    int32_t GetAudioStreamId(uint32_t &sessionID) const override;
    int32_t SetAudioRendererDesc(AudioRendererDesc audioRendererDesc) override;
    int32_t SetStreamType(AudioStreamType audioStreamType) override;
    int32_t SetVolume(float volume) const override;
    float GetVolume() const override;
    int32_t SetRenderRate(AudioRendererRate renderRate) const override;
    AudioRendererRate GetRenderRate() const override;
    int32_t SetRendererSamplingRate(uint32_t sampleRate) const override;
    uint32_t GetRendererSamplingRate() const override;
    int32_t SetRendererCallback(const std::shared_ptr<AudioRendererCallback> &callback) override;
    int32_t SetRendererPositionCallback(int64_t markPosition,
        const std::shared_ptr<RendererPositionCallback> &callback) override;
    void UnsetRendererPositionCallback() override;
    int32_t SetRendererPeriodPositionCallback(int64_t frameNumber,
        const std::shared_ptr<RendererPeriodPositionCallback> &callback) override;
    void UnsetRendererPeriodPositionCallback() override;
    int32_t SetBufferDuration(uint64_t bufferDuration) const override;
    int32_t SetRenderMode(AudioRenderMode renderMode) const override;
    AudioRenderMode GetRenderMode() const override;
    int32_t SetRendererWriteCallback(const std::shared_ptr<AudioRendererWriteCallback> &callback) override;
    int32_t SetRendererFirstFrameWritingCallback(
        const std::shared_ptr<AudioRendererFirstFrameWritingCallback> &callback) override;
    void SetPreferredFrameSize(int32_t frameSize) override;
    int32_t GetBufferDesc(BufferDesc &bufDesc) const override;
    int32_t Enqueue(const BufferDesc &bufDesc) const override;
    int32_t Clear() const override;
    int32_t GetBufQueueState(BufferQueueState &bufState) const override;
    void SetApplicationCachePath(const std::string cachePath) override;
    void SetInterruptMode(InterruptMode mode) override;
    int32_t SetParallelPlayFlag(bool parallelPlayFlag) override;
    int32_t SetLowPowerVolume(float volume) const override;
    float GetLowPowerVolume() const override;
    int32_t SetOffloadMode(int32_t state, bool isAppBack) const override;
    int32_t UnsetOffloadMode() const override;
    float GetSingleStreamVolume() const override;
    float GetMinStreamVolume() const override;
    float GetMaxStreamVolume() const override;
    int32_t GetCurrentOutputDevices(DeviceInfo &deviceInfo) const override;
    uint32_t GetUnderflowCount() const override;
    bool IsDeviceChanged(DeviceInfo &newDeviceInfo);
    int32_t RegisterAudioRendererEventListener(const int32_t clientPid,
        const std::shared_ptr<AudioRendererDeviceChangeCallback> &callback) override;
    int32_t UnregisterAudioRendererEventListener(const int32_t clientPid) override;
    int32_t RegisterOutputDeviceChangeWithInfoCallback(
        const std::shared_ptr<AudioRendererOutputDeviceChangeCallback> &callback) override;
    int32_t UnregisterOutputDeviceChangeWithInfoCallback() override;
    void DestroyOutputDeviceChangeWithInfoCallback() override;
    int32_t RegisterAudioPolicyServerDiedCb(const int32_t clientPid,
        const std::shared_ptr<AudioRendererPolicyServiceDiedCallback> &callback) override;
    int32_t UnregisterAudioPolicyServerDiedCb(const int32_t clientPid) override;
    void DestroyAudioRendererStateCallback() override;
    AudioEffectMode GetAudioEffectMode() const override;
    int64_t GetFramesWritten() const override;
    int32_t SetAudioEffectMode(AudioEffectMode effectMode) const override;
    int32_t SetChannelBlendMode(ChannelBlendMode blendMode) override;
    void SetAudioRendererErrorCallback(std::shared_ptr<AudioRendererErrorCallback> errorCallback) override;
    int32_t SetVolumeWithRamp(float volume, int32_t duration) override;

    int32_t RegisterRendererPolicyServiceDiedCallback();
    int32_t RemoveRendererPolicyServiceDiedCallback() const;

    void GetAudioInterrupt(AudioInterrupt &audioInterrupt);

    int32_t SetSpeed(float speed) override;
    float GetSpeed() override;
    bool IsFastRenderer() override;

    static inline AudioStreamParams ConvertToAudioStreamParams(const AudioRendererParams params)
    {
        AudioStreamParams audioStreamParams;

        audioStreamParams.format = params.sampleFormat;
        audioStreamParams.samplingRate = params.sampleRate;
        audioStreamParams.channels = params.channelCount;
        audioStreamParams.encoding = params.encodingType;
        audioStreamParams.channelLayout = params.channelLayout;

        return audioStreamParams;
    }

    AudioPrivacyType privacyType_ = PRIVACY_TYPE_PUBLIC;
    AudioRendererInfo rendererInfo_ = {CONTENT_TYPE_UNKNOWN, STREAM_USAGE_MUSIC, 0};
    std::string cachePath_;
    std::shared_ptr<IAudioStream> audioStream_;
    bool abortRestore_ = false;

    explicit AudioRendererPrivate(AudioStreamType audioStreamType, const AppInfo &appInfo, bool createStream = true);

    ~AudioRendererPrivate();

    friend class AudioRendererStateChangeCallbackImpl;

protected:
    // Method for switching between normal and low latency paths
    void SwitchStream(bool isLowLatencyDevice);

private:
    int32_t InitAudioInterruptCallback();
    int32_t InitOutputDeviceChangeCallback();
    int32_t InitAudioStream(AudioStreamParams audioStreamParams);
    void SetSwitchInfo(IAudioStream::SwitchInfo info, std::shared_ptr<IAudioStream> audioStream);
    bool SwitchToTargetStream(IAudioStream::StreamClass targetClass);
    void SetSelfRendererStateCallback();

    std::shared_ptr<AudioInterruptCallback> audioInterruptCallback_ = nullptr;
    std::shared_ptr<AudioStreamCallback> audioStreamCallback_ = nullptr;
    AppInfo appInfo_ = {};
    AudioInterrupt audioInterrupt_ = {STREAM_USAGE_UNKNOWN, CONTENT_TYPE_UNKNOWN,
        {AudioStreamType::STREAM_DEFAULT, SourceType::SOURCE_TYPE_INVALID, true}, 0};
    uint32_t sessionID_ = INVALID_SESSION_ID;
    std::shared_ptr<AudioRendererProxyObj> rendererProxyObj_;
    FILE *dumpFile_ = nullptr;
    std::shared_ptr<AudioRendererStateChangeCallbackImpl> audioDeviceChangeCallback_ = nullptr;
    std::shared_ptr<AudioRendererErrorCallback> audioRendererErrorCallback_ = nullptr;
    std::shared_ptr<OutputDeviceChangeWithInfoCallbackImpl> outputDeviceChangeCallback_ = nullptr;
    mutable std::shared_ptr<RendererPolicyServiceDiedCallback> audioPolicyServiceDiedCallback_ = nullptr;
    DeviceInfo currentDeviceInfo_ = {};
    bool isFastRenderer_ = false;
    bool isSwitching_ = false;
    mutable std::mutex switchStreamMutex_;

    float speed_ = 1.0;
};

class AudioRendererInterruptCallbackImpl : public AudioInterruptCallback {
public:
    explicit AudioRendererInterruptCallbackImpl(const std::shared_ptr<IAudioStream> &audioStream,
        const AudioInterrupt &audioInterrupt);
    virtual ~AudioRendererInterruptCallbackImpl();

    void OnInterrupt(const InterruptEventInternal &interruptEvent) override;
    void SaveCallback(const std::weak_ptr<AudioRendererCallback> &callback);
private:
    void NotifyEvent(const InterruptEvent &interruptEvent);
    void HandleAndNotifyForcedEvent(const InterruptEventInternal &interruptEvent);
    void NotifyForcePausedToResume(const InterruptEventInternal &interruptEvent);
    bool HandleForceDucking(const InterruptEventInternal &interruptEvent);
    std::shared_ptr<IAudioStream> audioStream_;
    std::weak_ptr<AudioRendererCallback> callback_;
    std::shared_ptr<AudioRendererCallback> cb_;
    AudioInterrupt audioInterrupt_ {};
    bool isForcePaused_ = false;
    bool isForceDucked_ = false;
    float instanceVolBeforeDucking_ = 1.0f;
};

class AudioStreamCallbackRenderer : public AudioStreamCallback {
public:
    virtual ~AudioStreamCallbackRenderer() = default;

    void OnStateChange(const State state, const StateChangeCmdType cmdType = CMD_FROM_CLIENT) override;
    void SaveCallback(const std::weak_ptr<AudioRendererCallback> &callback);
private:
    std::weak_ptr<AudioRendererCallback> callback_;
};

class AudioRendererStateChangeCallbackImpl : public AudioRendererStateChangeCallback {
public:
    AudioRendererStateChangeCallbackImpl();
    virtual ~AudioRendererStateChangeCallbackImpl();

    void OnRendererStateChange(
        const std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos) override;
    void SaveCallback(const std::weak_ptr<AudioRendererDeviceChangeCallback> &callback);
    void setAudioRendererObj(AudioRendererPrivate *rendererObj);
    void UnsetAudioRendererObj();
private:
    std::weak_ptr<AudioRendererDeviceChangeCallback> callback_;
    AudioRendererPrivate *renderer_;
    std::mutex mutex_;
};

class OutputDeviceChangeWithInfoCallbackImpl : public OutputDeviceChangeWithInfoCallback {
public:
    OutputDeviceChangeWithInfoCallbackImpl() = default;
    virtual ~OutputDeviceChangeWithInfoCallbackImpl() = default;
    void OnOutputDeviceChangeWithInfo(
        const uint32_t sessionId, const DeviceInfo &deviceInfo, const AudioStreamDeviceChangeReason reason) override;
    void SaveCallback(const std::weak_ptr<AudioRendererOutputDeviceChangeCallback> &callback)
    {
        callback_ = callback;
    }
    void SetAudioRendererObj(AudioRendererPrivate *rendererObj)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        renderer_ = rendererObj;
    }
    void UnsetAudioRendererObj()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        renderer_ = nullptr;
    }
private:
    std::weak_ptr<AudioRendererOutputDeviceChangeCallback> callback_;
    AudioRendererPrivate *renderer_;
    std::mutex mutex_;
};

class RendererPolicyServiceDiedCallback : public RendererOrCapturerPolicyServiceDiedCallback {
public:
    RendererPolicyServiceDiedCallback();
    virtual ~RendererPolicyServiceDiedCallback();
    void SetAudioRendererObj(AudioRendererPrivate *rendererObj);
    void SetAudioInterrupt(AudioInterrupt &audioInterrupt);
    void OnAudioPolicyServiceDied() override;

private:
    AudioRendererPrivate *renderer_ = nullptr;
    AudioInterrupt audioInterrupt_;
    void RestoreTheadLoop();
    std::unique_ptr<std::thread> restoreThread_ = nullptr;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_RENDERER_PRIVATE_H
