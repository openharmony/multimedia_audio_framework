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

#ifndef REMOTE_AUDIO_RENDERER_SINK_H
#define REMOTE_AUDIO_RENDERER_SINK_H

#include "audio_info.h"
#include "audio_manager.h"
#include "audio_sink_callback.h"

#include <cstdio>
#include <list>
#include <string>
#include <map>

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
    const char *deviceNetworkId;
    int32_t device_type;
} RemoteAudioSinkAttr;

class RemoteAudioRendererSink {
public:
    int32_t Init(RemoteAudioSinkAttr &attr);
    void DeInit(void);
    int32_t Start(void);
    int32_t Stop(void);
    int32_t Flush(void);
    int32_t Reset(void);
    int32_t Pause(void);
    int32_t Resume(void);
    int32_t RenderFrame(char &data, uint64_t len, uint64_t &writeLen);
    int32_t SetVolume(float left, float right);
    int32_t GetVolume(float &left, float &right);
    int32_t GetLatency(uint32_t *latency);
    int32_t SetAudioScene(AudioScene audioScene);
    int32_t OpenOutput(DeviceType outputDevice);
    static RemoteAudioRendererSink *GetInstance(const char *deviceNetworkId);
    bool rendererInited_;
    void RegisterParameterCallback(AudioSinkCallback* callback);
    void SetAudioParameter(const AudioParamKey key, const std::string& condition, const std::string& value);
    std::string GetAudioParameter(const AudioParamKey key, const std::string& condition);
    static int32_t ParamEventCallback(AudioExtParamKey key, const char *condition, const char *value, void *reserved,
        void *cookie);
    std::string GetNetworkId();
    AudioSinkCallback* GetParamCallback();
private:
    static std::map<std::string, RemoteAudioRendererSink *> allsinks;
    explicit RemoteAudioRendererSink(const std::string& deviceNetworkId);
    ~RemoteAudioRendererSink();
    RemoteAudioSinkAttr attr_;
    std::string deviceNetworkId_;
    bool isRenderCreated = false;
    bool started_;
    bool paused_;
    float leftVolume_;
    float rightVolume_;
    int32_t routeHandle_ = -1;
    int32_t openSpeaker_;
    std::string adapterNameCase_;
    struct AudioManager *audioManager_;
    struct AudioAdapter *audioAdapter_;
    struct AudioRender *audioRender_;
    struct AudioPort audioPort_;
    AudioSinkCallback* callback_;
    bool paramCallbackRegistered_ = false;

    int32_t GetTargetAdapterPort(struct AudioAdapterDescriptor *descs, int32_t size, const char *networkId);
    int32_t CreateRender(const struct AudioPort &renderPort);
    struct AudioManager *GetAudioManager();
#ifdef DEBUG_DUMP_FILE
    FILE *pfd;
#endif // DEBUG_DUMP_FILE
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_RENDERER_SINK_H
