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

#ifndef AUDIO_RENDERER_SINK_H
#define AUDIO_RENDERER_SINK_H

#include "audio_info.h"
#include "audio_manager.h"
#include "audio_sink_callback.h"
#include "running_lock.h"

#include <cstdio>
#include <list>

namespace OHOS {
namespace AudioStandard {
typedef struct {
    const char *adapterName;
    uint32_t openMicSpeaker;
    AudioFormat format;
    uint32_t sampleFmt;
    uint32_t sampleRate;
    uint32_t channel;
    float volume;
    const char *filePath;
} AudioSinkAttr;

class AudioRendererSink {
public:
    int32_t Init(AudioSinkAttr &attr);
    void DeInit(void);
    int32_t Flush(void);
    int32_t Pause(void);
    int32_t Reset(void);
    int32_t Resume(void);
    int32_t Start(void);
    int32_t Stop(void);
    int32_t RenderFrame(char &data, uint64_t len, uint64_t &writeLen);
    int32_t SetVolume(float left, float right);
    int32_t GetVolume(float &left, float &right);
    int32_t SetVoiceVolume(float volume);
    int32_t GetLatency(uint32_t *latency);
    int32_t GetTransactionId(uint64_t *transactionId);
    int32_t SetAudioScene(AudioScene audioScene, DeviceType activeDevice);
    int32_t SetOutputRoute(DeviceType outputDevice, AudioPortPin &outputPortPin);
    int32_t SetOutputRoute(DeviceType outputDevice);
    static AudioRendererSink *GetInstance(void);
    void SetAudioParameter(const AudioParamKey key, const std::string& condition, const std::string& value);
    std::string GetAudioParameter(const AudioParamKey key, const std::string& condition);
    void SetAudioMonoState(bool audioMono);
    void SetAudioBalanceValue(float audioBalance);
    bool rendererInited_;

private:
    AudioRendererSink();
    ~AudioRendererSink();
    AudioSinkAttr attr_;
    bool started_;
    bool paused_;
    float leftVolume_;
    float rightVolume_;
    int32_t routeHandle_ = -1;
    uint32_t openSpeaker_;
    std::string adapterNameCase_;
    struct AudioManager *audioManager_;
    struct AudioAdapter *audioAdapter_;
    struct AudioRender *audioRender_;
    struct AudioPort audioPort_ = {};
    bool audioMonoState_ = false;
    bool audioBalanceState_ = false;
    float leftBalanceCoef_ = 1.0f;
    float rightBalanceCoef_ = 1.0f;

    std::shared_ptr<PowerMgr::RunningLock> mKeepRunningLock;

    int32_t CreateRender(const struct AudioPort &renderPort);
    int32_t InitAudioManager();
    void AdjustStereoToMono(char *data, uint64_t len);
    void AdjustAudioBalance(char *data, uint64_t len);
#ifdef DUMPFILE
    FILE *pfd;
#endif // DUMPFILE
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_RENDERER_SINK_H
