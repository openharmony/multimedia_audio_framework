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

#ifndef RENDERER_SINK_ADAPTER_H
#define RENDERER_SINK_ADAPTER_H

#include <stdio.h>

#include <audio_renderer_sink_intf.h>
#include <bluetooth_renderer_sink_intf.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    enum AudioFormat format;
    uint32_t sampleFmt;
    uint32_t sampleRate;
    uint32_t channel;
    float volume;
} SinkAttr;

struct RendererSinkAdapter {
    int32_t (*RendererSinkInit)(const SinkAttr *attr);
    void (*RendererSinkDeInit)(void);
    int32_t (*RendererSinkStart)(void);
    int32_t (*RendererSinkStop)(void);
    int32_t (*RendererRenderFrame)(char *data, uint64_t len, uint64_t *writeLen);
    int32_t (*RendererSinkSetVolume)(float left, float right);
    int32_t (*RendererSinkGetLatency)(uint32_t *latency);
};

int32_t LoadSinkAdapter(const char *device, struct RendererSinkAdapter **sinkAdapter);
int32_t UnLoadSinkAdapter(struct RendererSinkAdapter *sinkAdapter);
#ifdef __cplusplus
}
#endif
#endif // RENDERER_SINK_ADAPTER_H