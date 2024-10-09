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
#ifndef AUDIO_SPEED_H
#define AUDIO_SPEED_H

#include <cstdint>
#include <string>

#include "sonic.h"
#include "audio_info.h"
namespace OHOS {
namespace AudioStandard {

class AudioSpeed {
public:
    AudioSpeed();
    AudioSpeed(size_t rate, size_t format, size_t channels);

    ~AudioSpeed();
    int32_t Init();
    int32_t LoadChangeSpeedFunc();
    int32_t SetSpeed(float speed);
    float GetSpeed();

    int32_t ChangeSpeedFor8Bit(uint8_t *buffer, int32_t bufferSize,
        std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize);
    int32_t ChangeSpeedFor16Bit(uint8_t *buffer, int32_t bufferSize,
        std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize);
    int32_t ChangeSpeedFor24Bit(uint8_t *buffer, int32_t bufferSize,
        std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize);
    int32_t ChangeSpeedFor32Bit(uint8_t *buffer, int32_t bufferSize,
        std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize);
    int32_t ChangeSpeedForFloat(float *buffer, int32_t bufferSize, float* outBuffer, int32_t &outBufferSize);

    std::function<int32_t(uint8_t *, int32_t, std::unique_ptr<uint8_t []>&, int32_t&)> ChangeSpeedFunc;

    int32_t ChangeSpeed(uint8_t *buffer, int32_t bufferSize,
        std::unique_ptr<uint8_t []> &outBuffer, int32_t &outBufferSize);
private:
    float speed_ = 0.0f;
    size_t rate_;
    size_t format_;

    std::unique_ptr<uint8_t[]> speedBuffer_ = nullptr;
    int32_t speedBufferSize_ = 0;
    AudioStreamParams streamParam_;
    size_t formatSize_ = 1;
    size_t channels_ = 2;
    sonicStream sonicStream_ = nullptr;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_SPEED_H