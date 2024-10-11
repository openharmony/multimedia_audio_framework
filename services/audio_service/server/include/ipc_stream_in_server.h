/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef IPC_STREAM_IN_SERVER_H
#define IPC_STREAM_IN_SERVER_H

#include <memory>

#include "ipc_stream_stub.h"
#include "audio_info.h"
#include "audio_process_config.h"
#include "renderer_in_server.h"
#include "capturer_in_server.h"

namespace OHOS {
namespace AudioStandard {
// in plan extends IStatusCallback
class StreamListenerHolder : public IStreamListener {
public:
    StreamListenerHolder();
    ~StreamListenerHolder();
    int32_t RegisterStreamListener(sptr<IpcStreamListener> listener);

    // override IStreamListener
    int32_t OnOperationHandled(Operation operation, int64_t result) override;
private:
    std::mutex listenerMutex_;
    sptr<IpcStreamListener> streamListener_ = nullptr;
};

class IpcStreamInServer : public IpcStreamStub {
public:
    static sptr<IpcStreamInServer> Create(const AudioProcessConfig &config, int32_t &ret);

    IpcStreamInServer(const AudioProcessConfig &config, AudioMode mode);
    ~IpcStreamInServer();

    int32_t Config();

    int32_t RegisterStreamListener(sptr<IRemoteObject> object) override;

    int32_t ResolveBuffer(std::shared_ptr<OHAudioBuffer> &buffer) override;

    int32_t UpdatePosition() override;

    int32_t GetAudioSessionID(uint32_t &sessionId) override;

    int32_t Start() override;

    int32_t Pause() override;

    int32_t Stop() override;

    int32_t Release() override;

    int32_t Flush() override;

    int32_t Drain(bool stopFlag = false) override;

    int32_t UpdatePlaybackCaptureConfig(const AudioPlaybackCaptureConfig &config) override;

    int32_t GetAudioTime(uint64_t &framePos, uint64_t &timestamp) override;

    int32_t GetAudioPosition(uint64_t &framePos, uint64_t &timestamp) override;

    int32_t GetLatency(uint64_t &latency) override;

    int32_t SetRate(int32_t rate) override; // SetRenderRate

    int32_t GetRate(int32_t &rate) override; // SetRenderRate

    int32_t SetLowPowerVolume(float volume) override; // renderer only

    int32_t GetLowPowerVolume(float &volume) override; // renderer only

    int32_t SetAudioEffectMode(int32_t effectMode) override; // renderer only

    int32_t GetAudioEffectMode(int32_t &effectMode) override; // renderer only

    int32_t SetPrivacyType(int32_t privacyType) override; // renderer only

    int32_t GetPrivacyType(int32_t &privacyType) override; // renderer only

    int32_t SetOffloadMode(int32_t state, bool isAppBack) override; // renderer only

    int32_t UnsetOffloadMode() override; // renderer only

    int32_t GetOffloadApproximatelyCacheTime(uint64_t &timestamp, uint64_t &paWriteIndex,
        uint64_t &cacheTimeDsp, uint64_t &cacheTimePa) override; // renderer only

    int32_t OffloadSetVolume(float volume) override; // renderer only

    int32_t UpdateSpatializationState(bool spatializationEnabled, bool headTrackingEnabled) override; // renderer only

    int32_t GetStreamManagerType() override;

    int32_t SetSilentModeAndMixWithOthers(bool on) override;

    int32_t SetClientVolume() override;

    int32_t SetMute(bool isMute) override;

    int32_t RegisterThreadPriority(uint32_t tid, const std::string &bundleName) override;

    // for inner-capturer
    std::shared_ptr<RendererInServer> GetRenderer();
    std::shared_ptr<CapturerInServer> GetCapturer();

private:
    int32_t ConfigRenderer();
    int32_t ConfigCapturer();

private:
    uint32_t clientTid_ = 0;
    std::string clientBundleName_;
    bool clientThreadPriorityRequested_ = false;
    AudioProcessConfig config_;
    std::shared_ptr<StreamListenerHolder> streamListenerHolder_ = nullptr;
    AudioMode mode_ = AUDIO_MODE_PLAYBACK;
    std::shared_ptr<RendererInServer> rendererInServer_ = nullptr;
    std::shared_ptr<CapturerInServer> capturerInServer_ = nullptr;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // IPC_STREAM_IN_SERVER_H
