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

#ifndef AUDIO_RENDERER_FILE_SINK_H
#define AUDIO_RENDERER_FILE_SINK_H

#include "audio_info.h"

#include <cstdio>
#include <list>
#include <iostream>
#include <string>

namespace OHOS {
namespace AudioStandard {
class AudioRendererFileSink {
public:
    int32_t Init(const char *filePath);
    void DeInit(void);
    int32_t Start(void);
    int32_t Stop(void);
    int32_t Flush(void);
    int32_t Reset(void);
    int32_t Pause(void);
    int32_t Resume(void);
    int32_t RenderFrame(char &data, uint64_t len, uint64_t &writeLen);
    int32_t SetVolume(float left, float right);
    int32_t GetLatency(uint32_t *latency);
    int32_t GetTransactionId(uint64_t *transactionId);
    static AudioRendererFileSink *GetInstance(void);
private:
    AudioRendererFileSink();
    ~AudioRendererFileSink();
    FILE *filePtr_ = nullptr;
    std::string filePath_;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_RENDERER_FILE_SINK_H
