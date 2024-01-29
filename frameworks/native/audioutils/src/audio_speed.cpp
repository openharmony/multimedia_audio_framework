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
#include "audio_speed.h"
#include "audio_log.h"
#include "audio_utils.h"
#include "audio_errors.h"

namespace OHOS {
namespace AudioStandard {

static const int32_t MAX_BUFFER_SIZE = 100000;

AudioSpeed::AudioSpeed(size_t rate, size_t format, size_t channels):rate_(rate), format_(format)
{
    AUDIO_INFO_LOG("AudioSpeed construct");
    Init();
    streamParam_ = {};
}

AudioSpeed::~AudioSpeed()
{
    AUDIO_INFO_LOG("~AudioSpeed destroy");
    if (sonicStream_ != nullptr) {
        sonicDestroyStream(sonicStream_);
        sonicStream_ = nullptr;
        AUDIO_INFO_LOG("Sonic stream destroy");
    }
}

int32_t AudioSpeed::Init()
{
    sonicStream_ = sonicCreateStream(rate_, channels_);
    LoadChangeSpeedFunc();

    return SUCCESS;
}

int32_t AudioSpeed::LoadChangeSpeedFunc()
{
    switch (format_) {
        case SAMPLE_U8:
            formatSize_ = 1; // size is 1
            ChangeSpeedFunc = std::bind(&AudioSpeed::ChangeSpeedFor8Bit, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
            break;
        case SAMPLE_S16LE:
            formatSize_ = 2; // size is 2
            ChangeSpeedFunc = std::bind(&AudioSpeed::ChangeSpeedFor16Bit, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
            break;
        case SAMPLE_S24LE:
            formatSize_ = 3; // size is 3
            ChangeSpeedFunc = std::bind(&AudioSpeed::ChangeSpeedFor24Bit, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
            break;
        case SAMPLE_S32LE:
            formatSize_ = 4; // size is 4
            ChangeSpeedFunc = std::bind(&AudioSpeed::ChangeSpeedFor32Bit, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
            break;
        default:
            formatSize_ = 2; // size is 2
            ChangeSpeedFunc = std::bind(&AudioSpeed::ChangeSpeedFor16Bit, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    }
    AUDIO_INFO_LOG("load change speed func for format %{public}zu", format_);
    return SUCCESS;
}

int32_t AudioSpeed::SetSpeed(float speed)
{
    AUDIO_INFO_LOG("SetSpeed %{public}f", speed);
    speed_ = speed;
    sonicSetSpeed(sonicStream_, speed_);
    return SUCCESS;
}

float AudioSpeed::GetSpeed()
{
    return speed_;
}

int32_t AudioSpeed::ChangeSpeedFor8Bit(uint8_t *buffer, int32_t bufferSize,
    std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize)
{
    int32_t numSamples = bufferSize / (formatSize_ * channels_);
    int32_t res = sonicWriteUnsignedCharToStream(sonicStream_, static_cast<unsigned char*>(buffer), numSamples);
    CHECK_AND_RETURN_RET_LOG(res == 1, 0, "sonic write unsigned char to stream failed.");

    int32_t outSamples = sonicReadUnsignedCharFromStream(sonicStream_,
        static_cast<unsigned char*>(outBuffer.get()), MAX_BUFFER_SIZE);
    CHECK_AND_RETURN_RET_LOG(outSamples != 0, bufferSize, "sonic stream is not full continue to write.");

    outBufferSize = outSamples * (formatSize_ * channels_);
    return bufferSize;
}

int32_t AudioSpeed::ChangeSpeedFor16Bit(uint8_t *buffer, int32_t bufferSize,
    std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize)
{
    int32_t numSamples = bufferSize / (formatSize_ * channels_);
    int32_t res = sonicWriteShortToStream(sonicStream_, reinterpret_cast<short*>(buffer), numSamples);
    CHECK_AND_RETURN_RET_LOG(res == 1, 0, "sonic write short to stream failed.");

    int32_t outSamples = sonicReadShortFromStream(sonicStream_, reinterpret_cast<short*>(outBuffer.get()),
        MAX_BUFFER_SIZE);
    CHECK_AND_RETURN_RET_LOG(outSamples != 0, bufferSize, "sonic stream is not full continue to write.");

    outBufferSize = outSamples * (formatSize_ * channels_);
    return bufferSize;
}

int32_t AudioSpeed::ChangeSpeedFor24Bit(uint8_t *buffer, int32_t bufferSize,
    std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize)
{
    if (bufferSize <= 0 || bufferSize > MAX_BUFFER_SIZE) {
        AUDIO_ERR_LOG("BufferSize is illegal");
        return ERR_MEMORY_ALLOC_FAILED;
    }
    float* bitTofloat = new(std::nothrow) float[bufferSize];
    ConvertFrom24BitToFloat(bufferSize / formatSize_, buffer, bitTofloat);

    float* speedBuf = new(std::nothrow) float[MAX_BUFFER_SIZE];
    int32_t ret = ChangeSpeedForFloat(bitTofloat, bufferSize, speedBuf, outBufferSize);

    ConvertFromFloatTo24Bit(outBufferSize / formatSize_, speedBuf, outBuffer.get());

    delete [] bitTofloat;
    delete [] speedBuf;
    return ret;
}

int32_t AudioSpeed::ChangeSpeedFor32Bit(uint8_t *buffer, int32_t bufferSize,
    std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize)
{
    if (bufferSize <= 0 || bufferSize > MAX_BUFFER_SIZE) {
        AUDIO_ERR_LOG("BufferSize is illegal");
        return ERR_MEMORY_ALLOC_FAILED;
    }
    float* bitTofloat = new(std::nothrow) float[bufferSize];
    ConvertFrom32BitToFloat(bufferSize / formatSize_, reinterpret_cast<int32_t *>(buffer), bitTofloat);

    float* speedBuf = new(std::nothrow) float[MAX_BUFFER_SIZE];
    int32_t ret = ChangeSpeedForFloat(bitTofloat, bufferSize, speedBuf, outBufferSize);

    ConvertFromFloatTo32Bit(outBufferSize / formatSize_, speedBuf, reinterpret_cast<int32_t *>(outBuffer.get()));

    delete [] bitTofloat;
    delete [] speedBuf;
    return ret;
}

int32_t AudioSpeed::ChangeSpeedForFloat(float *buffer, int32_t bufferSize,
    float* outBuffer, int32_t &outBufferSize)
{
    int32_t numSamples = bufferSize / (formatSize_ * channels_);
    int32_t res = sonicWriteFloatToStream(sonicStream_, buffer, numSamples);
    CHECK_AND_RETURN_RET_LOG(res == 1, 0, "sonic write float to stream failed.");
    int32_t outSamples = sonicReadFloatFromStream(sonicStream_, outBuffer, MAX_BUFFER_SIZE);
    outBufferSize = outSamples * (formatSize_ * channels_);
    return bufferSize;
}
} // namespace AudioStandard
} // namespace OHOS
