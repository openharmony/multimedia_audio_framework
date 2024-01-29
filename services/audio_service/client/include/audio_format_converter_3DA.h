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
#ifndef AUDIO_CONVERTER_3DA
#define AUDIO_CONVERTER_3DA

#include <cstdint>
#include <cstring>
#include <string>
#include <dlfcn.h>
#include <unistd.h>
#include "audio_info.h"
#include "audio_effect.h"
#include "audio_converter_parser.h"

namespace OHOS {
namespace AudioStandard {

class LibLoader {
public:
    LibLoader() = default;
    ~LibLoader();
    bool Init();
    bool AddAlgoHandle(Library lib);
    void SetIOBufferConfig(bool isInput, uint8_t format, uint32_t channels, uint64_t channelLayout);
    int32_t ApplyAlgo(AudioBuffer &inputBuffer, AudioBuffer &outputBuffer);
    bool FlushAlgo();

private:
    bool LoadLibrary(const std::string &relativePath) noexcept;
    std::unique_ptr<AudioEffectLibEntry> libEntry_;
    AudioEffectHandle handle_;
    AudioEffectConfig ioBufferConfig_;
    void* libHandle_;
};

class AudioFormatConverter3DA {
public:
    AudioFormatConverter3DA() = default;
    int32_t Init(const AudioStreamParams info);
    bool GetInputBufferSize(size_t &bufferSize);
    bool CheckInputValid(const BufferDesc pcmBuf, const BufferDesc metaBuf);
    bool AllocateMem();
    int32_t Process(const BufferDesc pcmBuf, const BufferDesc metaBuf);
    void ConverterChannels(uint8_t &channel, uint64_t &channelLayout);
    void GetOutputBufferStream(uint8_t *&buffer, uint32_t &bufferLen);
    bool Flush();

private:
    int32_t GetPcmLength(int32_t channels, int8_t bps);
    int32_t GetMetaLength();

    std::unique_ptr<uint8_t[]> outPcmBuf_;

    int32_t inChannel_;
    int32_t outChannel_;

    uint8_t bps_;
    uint8_t encoding_;

    AudioChannelLayout outChannelLayout_;

    bool loadSuccess_;

    LibLoader externalLoader_;
};

} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_CONVERTER_3DA