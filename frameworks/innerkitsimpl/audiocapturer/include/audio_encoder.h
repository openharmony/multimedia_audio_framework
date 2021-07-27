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

#ifndef AUDIO_ENCODER_H
#define AUDIO_ENCODER_H

#include <cstddef>
#include <cstdint>
#include <time.h>
#include <memory>
#include <vector>
#include "audio_errors.h"
#include "media_info.h"
#include "format.h"
#include "codec_interface.h"
namespace OHOS {
namespace Audio {
constexpr int32_t AUDIO_ENC_PARAM_NUM = 8;
/* count of audio frame in Buffer */
constexpr uint32_t AUDIO_FRAME_NUM_IN_BUF = 30;

/* sample per frame for all encoder(aacplus:1024) */
constexpr uint32_t AUDIO_POINT_NUM = 1024;

struct AudioEncodeConfig {
    AudioCodecFormat audioFormat;
    uint32_t bitRate = 0;
    uint32_t sampleRate = 0;
    uint32_t channelCount = 0;
    AudioBitWidth bitWidth = BIT_WIDTH_16;
};

struct AudioStream {
    uint8_t *buffer = nullptr;    /* the virtual address of stream */
    uint32_t bufferLen = 0;   /* stream length, by bytes */
    int64_t timeStamp = 0;
};

class AudioEncoder {
public:
    AudioEncoder();
    virtual ~AudioEncoder();

    /**
     * Init audio encoder with AudioEncodeConfig.
     */
    int32_t Initialize(const AudioEncodeConfig &input);

    /**
     * Binds the source deviceId.
     */
    int32_t BindSource(uint32_t deviceId);

    /**
     * Obtains whether the current encoding is muted.
     */
    int32_t GetMute(bool &muted);

    /**
     * Sets whether the current encoding is muted.
     */
    int32_t SetMute(bool muted);

    /**
     * Start the audio encoder.
     */
    int32_t Start();

    /**
     * Reads the source data and returns the actual read size.
     */
    int32_t ReadStream(AudioStream &stream, bool isBlockingRead);

    /**
     * Stop the audio encoder.
     */
    int32_t Stop();

private:
    int32_t InitAudioEncoderAttr(const AudioEncodeConfig &input);

private:
    CODEC_HANDLETYPE encHandle_;
    CodecType domainKind_ = AUDIO_ENCODER;
    AvCodecMime codecMime_ = MEDIA_MIMETYPE_AUDIO_AAC;
    Profile profile_ = INVALID_PROFILE;
    AudioSampleRate sampleRate_ = AUD_SAMPLE_RATE_INVALID;
    uint32_t bitRate_ = 0;
    AudioSoundMode soundMode_ = AUD_SOUND_MODE_INVALID;
    uint32_t ptNumPerFrm_ = AUDIO_POINT_NUM;
    uint32_t bufSize_ = AUDIO_FRAME_NUM_IN_BUF;
    Param encAttr_[AUDIO_ENC_PARAM_NUM];
    bool started_;
};
}  // namespace Audio
}  // namespace OHOS
#endif  // AUDIO_ENCODER_H
