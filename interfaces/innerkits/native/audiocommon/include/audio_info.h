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
#ifndef AUDIO_INFO_H
#define AUDIO_INFO_H

#ifdef __MUSL__
#include <sys/time.h>
#include <stdint.h>
#endif // __MUSL__

#include <unistd.h>

namespace OHOS {
namespace AudioStandard {
// Audio Device Types
enum DeviceType {
    /**
    * Indicates device type none.
    */
    DEVICE_TYPE_NONE = -1,
    /**
    * Indicates a speaker built in a device.
    */
    SPEAKER = 0,
    /**
    * Indicates a headset, which is the combination of a pair of headphones and a microphone.
    */
    WIRED_HEADSET = 1,
    /**
    * Indicates a Bluetooth device used for telephony.
    */
    BLUETOOTH_SCO = 2,
    /**
    * Indicates a Bluetooth device supporting the Advanced Audio Distribution Profile (A2DP).
    */
    BLUETOOTH_A2DP = 3,
    /**
    * Indicates a microphone built in a device.
    */
    MIC = 4
};

// Audio Role
enum DeviceRole {
    /**
     * Device role none.
     */
    DEVICE_ROLE_NONE = -1,
    /**
     * Input device role.
     */
    INPUT_DEVICE = 0,
    /**
     * Output device role.
     */
    OUTPUT_DEVICE = 1
};

enum AudioStreamType {
    /**
     * Indicates audio streams default.
     */
    STREAM_DEFAULT = -1,
    /**
     * Indicates audio streams media.
     */
    STREAM_MEDIA = 0,
    /**
     * Indicates audio streams of voices in calls.
     */
    STREAM_VOICE_CALL = 1,
    /**
     * Indicates audio streams for system sounds.
     */
    STREAM_SYSTEM = 2,
    /**
     * Indicates audio streams for ringtones.
     */
    STREAM_RING = 3,
    /**
     * Indicates audio streams for music playback.
     */
    STREAM_MUSIC = 4,
    /**
     * Indicates audio streams for alarms.
     */
    STREAM_ALARM = 5,
    /**
     * Indicates audio streams for notifications.
     */
    STREAM_NOTIFICATION = 6,
    /**
     * Indicates audio streams for voice calls routed through a connected Bluetooth device.
     */
    STREAM_BLUETOOTH_SCO = 7,
    /**
     * Indicates audio streams for enforced audible.
     */
    STREAM_ENFORCED_AUDIBLE = 8,
    /**
     * Indicates audio streams for dual-tone multi-frequency (DTMF) tones.
     */
    STREAM_DTMF = 9,
    /**
     * Indicates audio streams exclusively transmitted through the speaker (text-to-speech) of a device.
     */
    STREAM_TTS =  10,
    /**
     * Indicates audio streams used for prompts in terms of accessibility.
     */
    STREAM_ACCESSIBILITY = 11
};

enum AudioEncodingType {
    ENCODING_PCM = 0,
    ENCODING_AAC, // Currently not supported
    ENCODING_INVALID
};

// Ringer Mode
enum AudioRingerMode {
    RINGER_MODE_NORMAL = 0,
    RINGER_MODE_SILENT = 1,
    RINGER_MODE_VIBRATE = 2
};

// format
enum AudioSampleFormat {
    SAMPLE_U8 = 8,
    SAMPLE_S16LE = 16,
    SAMPLE_S24LE = 24,
    SAMPLE_S32LE = 32,
    INVALID_WIDTH = -1
};

// channel
enum AudioChannel {
    MONO = 1,
    STEREO
};

// sampling rate
enum AudioSamplingRate {
    SAMPLE_RATE_8000 = 8000,
    SAMPLE_RATE_11025 = 11025,
    SAMPLE_RATE_16000 = 16000,
    SAMPLE_RATE_22050 = 22050,
    SAMPLE_RATE_32000 = 32000,
    SAMPLE_RATE_44100 = 44100,
    SAMPLE_RATE_48000 = 48000
};

typedef enum {
    /** Invalid audio source */
    AUDIO_SOURCE_INVALID = -1,
    /** Default audio source */
    AUDIO_SOURCE_DEFAULT = 0,
    /** Microphone */
    AUDIO_MIC = 1,
    /** Uplink voice */
    AUDIO_VOICE_UPLINK = 2,
    /** Downlink voice */
    AUDIO_VOICE_DOWNLINK = 3,
    /** Voice call */
    AUDIO_VOICE_CALL = 4,
    /** Camcorder */
    AUDIO_CAMCORDER = 5,
    /** Voice recognition */
    AUDIO_VOICE_RECOGNITION = 6,
    /** Voice communication */
    AUDIO_VOICE_COMMUNICATION = 7,
    /** Remote submix */
    AUDIO_REMOTE_SUBMIX = 8,
    /** Unprocessed audio */
    AUDIO_UNPROCESSED = 9,
    /** Voice performance */
    AUDIO_VOICE_PERFORMANCE = 10,
    /** Echo reference */
    AUDIO_ECHO_REFERENCE = 1997,
    /** Radio tuner */
    AUDIO_RADIO_TUNER = 1998,
    /** Hotword */
    AUDIO_HOTWORD = 1999,
    /** Extended remote submix */
    AUDIO_REMOTE_SUBMIX_EXTEND = 10007,
} AudioSourceType;

struct AudioStreamParams {
    uint32_t samplingRate;
    uint8_t encoding;
    uint8_t format;
    uint8_t channels;
};

/**
 * @brief Represents Timestamp information, including the frame position information and high-resolution time source.
 */
class Timestamp {
public:
    Timestamp() : framePosition(0)
    {
        time.tv_sec = 0;
        time.tv_nsec = 0;
    }
    virtual ~Timestamp() = default;
    uint32_t framePosition;
    struct timespec time;

    /**
     * @brief Enumerates the time base of this <b>Timestamp</b>. Different timing methods are supported.
     *
     */
    enum Timestampbase {
        /** Monotonically increasing time, excluding the system sleep time */
        MONOTONIC = 0
    };
};
typedef void* AudioIOHandle;
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_INFO_H
