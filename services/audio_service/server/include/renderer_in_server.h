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

#ifndef RENDERER_IN_SERVER_H
#define RENDERER_IN_SERVER_H

#include <mutex>
#include "i_renderer_stream.h"
#include "i_stream_listener.h"
#include "oh_audio_buffer.h"

namespace OHOS {
namespace AudioStandard {
class RendererInServer : public IStatusCallback, public IWriteCallback,
    public std::enable_shared_from_this<RendererInServer> {
public:
    RendererInServer(AudioProcessConfig processConfig, std::weak_ptr<IStreamListener> streamListener);
    virtual ~RendererInServer();
    void OnStatusUpdate(IOperation operation) override;
    void HandleOperationFlushed();
    int32_t OnWriteData(size_t length) override;
    
    int32_t ResolveBuffer(std::shared_ptr<OHAudioBuffer> &buffer);
    int32_t GetSessionId(uint32_t &sessionId);
    int32_t Start();
    int32_t Pause();
    int32_t Flush();
    int32_t Drain();
    int32_t Stop();
    int32_t Release();

    int32_t GetAudioTime(uint64_t &framePos, uint64_t &timeStamp);
    int32_t GetLatency(uint64_t &latency);
    int32_t SetRate(int32_t rate);
    int32_t SetLowPowerVolume(float volume);
    int32_t GetLowPowerVolume(float &volume);
    int32_t SetAudioEffectMode(int32_t effectMode);
    int32_t GetAudioEffectMode(int32_t &effectMode);
    int32_t SetPrivacyType(int32_t privacyType);
    int32_t GetPrivacyType(int32_t &privacyType);

    int32_t SetOffloadMode(int32_t state, bool isAppBack);
    int32_t UnsetOffloadMode();
    int32_t GetOffloadApproximatelyCacheTime(uint64_t &timeStamp, uint64_t &paWriteIndex,
        uint64_t &cacheTimeDsp, uint64_t &cacheTimePa);
    int32_t OffloadSetVolume(float volume);

    int32_t Init();
    int32_t ConfigServerBuffer();
    int32_t InitBufferStatus();
    int32_t UpdateWriteIndex();
    BufferDesc DequeueBuffer(size_t length);
    int32_t WriteData();
    void WriteEmptyData();
    int32_t DrainAudioBuffer();
    int32_t GetInfo();

private:
    void OnStatusUpdateSub(IOperation operation);
    std::mutex statusLock_;
    std::condition_variable statusCv_;
    std::shared_ptr<IRendererStream> stream_ = nullptr;
    uint32_t streamIndex_ = -1;
    IOperation operation_ = OPERATION_INVALID;
    IStatus status_ = I_STATUS_IDLE;

    std::weak_ptr<IStreamListener> streamListener_;
    AudioProcessConfig processConfig_;
    size_t totalSizeInFrame_ = 0;
    size_t spanSizeInFrame_ = 0;
    size_t spanSizeInByte_ = 0;
    size_t byteSizePerFrame_ = 0;
    bool isBufferConfiged_  = false;
    std::atomic<bool> isInited_ = false;
    std::shared_ptr<OHAudioBuffer> audioServerBuffer_ = nullptr;
    int32_t needForceWrite_ = 0;
    bool afterDrain = false;
    std::mutex updateIndexLock_;
    bool resetTime_ = false;
    uint64_t resetTimestamp_ = 0;
    std::mutex writeLock_;
    FILE *dumpC2S_ = nullptr; // client to server dump file
    bool underRunLogFlag_ = false;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // RENDERER_IN_SERVER_H
