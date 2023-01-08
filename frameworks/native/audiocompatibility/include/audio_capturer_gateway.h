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

#ifndef AUDIO_CAPTURER_GATEWAY_H
#define AUDIO_CAPTURER_GATEWAY_H

#include "audio_stream.h"
#include "audio_container_stream_base.h"

namespace OHOS {
namespace AudioStandard {
class AudioCapturerGateway : public AudioCapturer {
public:

    int32_t GetFrameCount(uint32_t &frameCount) const override;
    int32_t SetParams(const AudioCapturerParams params) override;
    int32_t SetCapturerCallback(const std::shared_ptr<AudioCapturerCallback> &callback) override;
    int32_t GetParams(AudioCapturerParams &params) const override;
    int32_t GetCapturerInfo(AudioCapturerInfo &capturerInfo) const override;
    int32_t GetStreamInfo(AudioStreamInfo &streamInfo) const override;
    bool Start() const override;
    int32_t Read(uint8_t &buffer, size_t userSize, bool isBlockingRead) const override;
    CapturerState GetStatus() const override;
    bool GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base) const override;
    bool Pause() const override;
    bool Stop() const override;
    bool Flush() const override;
    bool Release() const override;
    int32_t GetBufferSize(size_t &bufferSize) const override;
    int32_t GetAudioStreamId(uint32_t &sessionID) const override;
    int32_t SetCapturerPositionCallback(int64_t markPosition,
        const std::shared_ptr<CapturerPositionCallback> &callback) override;
    void UnsetCapturerPositionCallback() override;
    int32_t SetCapturerPeriodPositionCallback(int64_t frameNumber,
        const std::shared_ptr<CapturerPeriodPositionCallback> &callback) override;
    void UnsetCapturerPeriodPositionCallback() override;
    int32_t SetBufferDuration(uint64_t bufferDuration) const override;
    int32_t SetCaptureMode(AudioCaptureMode capturerMode)const override;
    AudioCaptureMode GetCaptureMode()const override;
    int32_t SetCapturerReadCallback(const std::shared_ptr<AudioCapturerReadCallback> &callback) override;
    int32_t GetBufferDesc(BufferDesc &bufDesc)const override;
    int32_t Enqueue(const BufferDesc &bufDesc)const override;
    int32_t Clear()const override;
    int32_t GetBufQueueState(BufferQueueState &bufState)const override;
    void SetApplicationCachePath(const std::string cachePath) override;

    AudioCapturerInfo capturerInfo_ = {};

    explicit AudioCapturerGateway(AudioStreamType audioStreamType, const AppInfo &appInfo);
    virtual ~AudioCapturerGateway();
    bool isChannelChange_ = false;

private:
    std::shared_ptr<AudioContainerCaptureStream> audioStream_;
    std::shared_ptr<AudioStreamCallback> audioStreamCallback_ = nullptr;
    AppInfo appInfo_ = {};
};

class AudioStreamCapturerCallback : public AudioStreamCallback {
public:
    virtual ~AudioStreamCapturerCallback() = default;

    void OnStateChange(const State state, StateChangeCmdType cmdType = CMD_FROM_CLIENT) override;
    void SaveCallback(const std::weak_ptr<AudioCapturerCallback> &callback);

private:
    std::weak_ptr<AudioCapturerCallback> callback_;
};
}  // namespace AudioStandard
}  // namespace OHOS

#endif // AUDIO_CAPTURER_GATEWAY_H
