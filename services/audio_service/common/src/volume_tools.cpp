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

#include "volume_tools.h"
#include "audio_errors.h"
#include "audio_log.h"

namespace {
static const int32_t UINT8_SHIFT = 0x80;
static const int32_t INT24_SHIFT = 8;
static const uint32_t SHIFT_EIGHT = 8;
static const uint32_t SHIFT_SIXTEEN = 16;
static const uint32_t ARRAY_INDEX_TWO = 2;
static const size_t MIN_FRAME_SIZE = 1;
}
namespace OHOS {
namespace AudioStandard {
bool VolumeTools::IsVolumeValid(float volFloat)
{
    return volFloat >= 0.0 && volFloat <= 1.0;
}

bool VolumeTools::IsVolumeValid(int32_t volInt)
{
    return volInt >= INT32_VOLUME_MIN && volInt <= INT32_VOLUME_MAX;
}
bool VolumeTools::IsVolumeValid(ChannelVolumes vols)
{
    if (vols.channel > CHANNEL_16 || vols.channel < MONO) {
        return false;
    }
    for (size_t i = 0; i < vols.channel; i++) {
        if (!IsVolumeValid(vols.volStart[i]) || !IsVolumeValid(vols.volEnd[i])) {
            return false;
        }
    }

    return true;
}

int32_t VolumeTools::GetInt32Vol(float volFloat)
{
    if (IsVolumeValid(volFloat)) {
        return volFloat * INT32_VOLUME_MAX;
    }
    if (volFloat < 0.0) {
        return INT32_VOLUME_MIN;
    }
    return INT32_VOLUME_MAX;
}

ChannelVolumes VolumeTools::GetChannelVolumes(AudioChannel channel, int32_t volStart, int32_t volEnd)
{
    ChannelVolumes vols = {};
    if (!IsVolumeValid(volStart) || !IsVolumeValid(volEnd) || channel > CHANNEL_16 || channel < MONO) {
        AUDIO_ERR_LOG("GetChannelVolumes failed with invalid vol:%{public}d %{public}d channel: %{public}d", volStart,
            volEnd, channel);
        return vols;
    }
    for (size_t i = 0; i < channel; i++) {
        vols.volStart[i] = volStart;
        vols.volEnd[i] = volEnd;
    }
    vols.channel = channel;
    return vols;
}

ChannelVolumes VolumeTools::GetChannelVolumes(AudioChannel channel, float volStart, float volEnd)
{
    ChannelVolumes vols = {};
    if (!IsVolumeValid(volStart) || !IsVolumeValid(volEnd) || channel > CHANNEL_16 || channel < MONO) {
        AUDIO_ERR_LOG("GetChannelVolumes failed with invalid vol:%{public}f %{public}f channel: %{public}d", volStart,
            volEnd, channel);
        return vols;
    }
    for (size_t i = 0; i < channel; i++) {
        vols.volStart[i] = GetInt32Vol(volStart);
        vols.volEnd[i] = GetInt32Vol(volEnd);
    }
    vols.channel = channel;
    return vols;
}

size_t GetByteSize(AudioSampleFormat format)
{
    size_t bitWidthSize = 0;
    switch (format) {
        case SAMPLE_U8:
            bitWidthSize = 1; // size is 1
            break;
        case SAMPLE_S16LE:
            bitWidthSize = 2; // size is 2
            break;
        case SAMPLE_S24LE:
            bitWidthSize = 3; // size is 3
            break;
        case SAMPLE_S32LE:
            bitWidthSize = 4; // size is 4
            break;
        case SAMPLE_F32LE:
            bitWidthSize = 4; // size is 4
            break;
        default:
            bitWidthSize = 2; // default size is 2
            break;
    }
    return bitWidthSize;
}

static inline uint32_t ReadInt24LE(const uint8_t *p)
{
    return ((uint32_t) p[ARRAY_INDEX_TWO] << SHIFT_SIXTEEN) | ((uint32_t) p[1] << SHIFT_EIGHT) | ((uint32_t) p[0]);
}

static inline void WriteInt24LE(uint8_t *p, uint32_t u)
{
    p[ARRAY_INDEX_TWO] = (uint8_t) (u >> SHIFT_SIXTEEN);
    p[1] = (uint8_t) (u >> SHIFT_EIGHT);
    p[0] = (uint8_t) u;
}

inline int32_t VolumeFlatten(int32_t vol)
{
    return vol < INT32_VOLUME_MIN ? 0 : (vol > INT32_VOLUME_MAX ? INT32_VOLUME_MAX : vol);
}

void ProcessOneFrame(uint8_t *ptr, AudioSampleFormat format, int32_t vol)
{
    int64_t temp = 0;
    int16_t *raw16 = nullptr;
    int32_t *raw32 = nullptr;
    float *rawFloat = nullptr;
    switch (format) {
        case SAMPLE_U8:
            temp = *ptr - UINT8_SHIFT;
            temp = (temp * vol) >> VOLUME_SHIFT;
            temp = temp < INT8_MIN ? INT8_MIN : (temp > INT8_MAX ? INT8_MAX : temp);
            *ptr = static_cast<uint8_t>(temp + UINT8_SHIFT);
            break;
        case SAMPLE_S16LE:
            raw16 = reinterpret_cast<int16_t *>(ptr);
            temp = (*raw16 * static_cast<int64_t>(vol)) >> VOLUME_SHIFT;
            *raw16 = temp > INT16_MAX ? INT16_MAX : (temp < INT16_MIN ? INT16_MIN : temp);
            break;
        case SAMPLE_S24LE:
            temp = static_cast<int32_t>(ReadInt24LE(ptr) << INT24_SHIFT) * static_cast<int64_t>(vol) >> VOLUME_SHIFT;
            WriteInt24LE(ptr, (static_cast<uint32_t>(temp) >> INT24_SHIFT));
            break;
        case SAMPLE_S32LE:
            raw32 = reinterpret_cast<int32_t *>(ptr);
            // int32_t * int16_t, max result is int48_t
            temp = (*raw32 * static_cast<int64_t>(vol)) >> VOLUME_SHIFT;
            *raw32 = temp > INT32_MAX ? INT32_MAX : (temp < INT32_MIN ? INT32_MIN : temp);
            break;
        case SAMPLE_F32LE:
            rawFloat = reinterpret_cast<float *>(ptr);
            *rawFloat = *rawFloat * (static_cast<float>(vol) / INT32_VOLUME_MAX);
            break;
        default:
            AUDIO_ERR_LOG("ProcessOneFrame with invalid format");
            break;
    }
}

// |---------frame1--------|---------frame2--------|---------frame3--------|
// |ch1-ch2-ch3-ch4-ch5-ch6|ch1-ch2-ch3-ch4-ch5-ch6|ch1-ch2-ch3-ch4-ch5-ch6|
int32_t VolumeTools::Process(const BufferDesc &buffer, AudioSampleFormat format, ChannelVolumes vols)
{
    // parms check
    if (format > SAMPLE_F32LE || !IsVolumeValid(vols)) {
        AUDIO_ERR_LOG("Process failed with invalid params");
        return ERR_INVALID_PARAM;
    }
    size_t byteSizePerData = GetByteSize(format);
    size_t byteSizePerFrame = byteSizePerData * vols.channel;
    if (buffer.buffer == nullptr || buffer.bufLength % byteSizePerFrame != 0) {
        AUDIO_ERR_LOG("Process failed with invalid buffer, size is %{public}zu", buffer.bufLength);
        return ERR_INVALID_PARAM;
    }

    size_t frameSize = buffer.bufLength / byteSizePerFrame;
    if (frameSize <= MIN_FRAME_SIZE) {
        AUDIO_ERR_LOG("Process failed with invalid frameSize, size is %{public}zu", frameSize);
        return ERR_INVALID_PARAM;
    }

    float volStep[CHANNEL_MAX] = {};
    for (size_t channelIdx = 0; channelIdx < vols.channel; channelIdx++) {
        if (vols.volEnd[channelIdx] == vols.volStart[channelIdx]) {
            volStep[channelIdx] = 0.0;
        } else {
            volStep[channelIdx] = (static_cast<float>(vols.volEnd[channelIdx] - vols.volStart[channelIdx])) /
                (frameSize - MIN_FRAME_SIZE);
        }
    }
    for (size_t frameIndex = 0; frameIndex < frameSize; frameIndex++) {
        for (size_t channelIdx = 0; channelIdx < vols.channel; channelIdx++) {
            int32_t vol = volStep[channelIdx] * frameIndex + vols.volStart[channelIdx];
            vol = VolumeFlatten(vol);
            uint8_t *samplePtr = buffer.buffer + frameIndex * byteSizePerFrame + channelIdx * byteSizePerData;
            ProcessOneFrame(samplePtr, format, vol);
        }
    }

    return SUCCESS;
}
} // namespace AudioStandard
} // namespace OHOS