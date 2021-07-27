/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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

#ifndef AUDIO_SOURCE_H
#define AUDIO_SOURCE_H

#include <cstddef>
#include <cstdint>
#include <time.h>
#include <memory>
#include <vector>

#include "audio_errors.h"
#include "media_info.h"
#include "format.h"
#include "audio_manager.h"

namespace OHOS {
namespace Audio {
struct AudioSourceConfig {
    /**
     * Select the device to be used for setting the current audio source based on the device enumerated by
     * EnumDeviceBySourceType.
     */
    uint32_t deviceId;
    AudioCodecFormat audioFormat;
    int32_t sampleRate = 0;
    int32_t channelCount = 0;
    bool interleaved;
    AudioBitWidth bitWidth = BIT_WIDTH_16;
    AudioStreamType streamUsage;
};

class AudioSource {
public:
    AudioSource();
    virtual ~AudioSource();

    /**
     * Enumerates supported devices based on the input source type, including device names and device IDs.
     */
    int32_t EnumDeviceBySourceType(AudioSourceType inputSource, std::vector<AudioDeviceDesc> &devices);

     /**
     * Obtains the minimum frame count (in BytesPerSample) required in the specified conditions.
     *
     * @param sampleRate Indicates the sampling rate (Hz).
     * @param channelCount Indicates the audio channel count.
     * @param audioFormat Indicates the audio data format.
     * @param frameCount the minimum frame count (in BytesPerSample).
     * @return Returns {@code true} if success; returns {@code false} otherwise.
     */
    static bool GetMinFrameCount(int32_t sampleRate, int32_t channelCount,
                                 AudioCodecFormat audioFormat, size_t &frameCount);

    /**
     * Obtains the frame count (in BytesPerSample) required in the current conditions.
     *
     * @return Returns the frame count (in BytesPerSample); returns {@code -1} if an exception occurs.
     */
    uint64_t GetFrameCount();

    /**
     * Initializes the current source based on AudioSourceConfig.
     */
    int32_t Initialize(const AudioSourceConfig &input);

    /**
     * Set the input device ID, which is used when a device needs to be switched.
     */
    int32_t SetInputDevice(uint32_t deviceId);

    /**
     * Obtains the current device ID.
     */
    int32_t GetCurrentDeviceId(uint32_t &deviceId);

    /**
     * Start the source.
     */
    int32_t Start();

    /**
     * Reads the source data and returns the actual read size.
     */
    int32_t ReadFrame(uint8_t &buffer, size_t bufferBytes, bool isBlockingRead);

    /**
     * Stop the source.
     */
    int32_t Stop();

private:
    int32_t InitCheck();

private:
    bool initialized_;
    bool started_;
    AudioAdapter *audioAdapter_;
    AudioCapture *audioCapture_;
    AudioPort capturePort_ = {};
};
}  // namespace Audio
}  // namespace OHOS
#endif  // AUDIO_SOURCE_H
