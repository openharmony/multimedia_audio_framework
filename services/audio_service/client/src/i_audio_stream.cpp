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

#include "i_audio_stream.h"
#include <map>

#include "audio_log.h"
#include "audio_stream.h"
#include "fast_audio_stream.h"

namespace OHOS {
namespace AudioStandard {
const std::map<std::pair<ContentType, StreamUsage>, AudioStreamType> streamTypeMap_ = IAudioStream::CreateStreamMap();
std::map<std::pair<ContentType, StreamUsage>, AudioStreamType> IAudioStream::CreateStreamMap()
{
    std::map<std::pair<ContentType, StreamUsage>, AudioStreamType> streamMap;
    // Mapping relationships from content and usage to stream type in design
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_UNKNOWN)] = STREAM_MUSIC;
    streamMap[std::make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_VOICE_CALL;
    streamMap[std::make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_VOICE_MODEM_COMMUNICATION)] = STREAM_VOICE_CALL;
    streamMap[std::make_pair(CONTENT_TYPE_PROMPT, STREAM_USAGE_SYSTEM)] = STREAM_SYSTEM;
    streamMap[std::make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_RING;
    streamMap[std::make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_MEDIA)] = STREAM_MUSIC;
    streamMap[std::make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_MEDIA)] = STREAM_MOVIE;
    streamMap[std::make_pair(CONTENT_TYPE_GAME, STREAM_USAGE_MEDIA)] = STREAM_GAME;
    streamMap[std::make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_MEDIA)] = STREAM_SPEECH;
    streamMap[std::make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_ALARM)] = STREAM_ALARM;
    streamMap[std::make_pair(CONTENT_TYPE_PROMPT, STREAM_USAGE_NOTIFICATION)] = STREAM_NOTIFICATION;
    streamMap[std::make_pair(CONTENT_TYPE_PROMPT, STREAM_USAGE_ENFORCED_TONE)] = STREAM_SYSTEM_ENFORCED;
    streamMap[std::make_pair(CONTENT_TYPE_DTMF, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_DTMF;
    streamMap[std::make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_VOICE_ASSISTANT;
    streamMap[std::make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_ACCESSIBILITY)] = STREAM_ACCESSIBILITY;
    streamMap[std::make_pair(CONTENT_TYPE_ULTRASONIC, STREAM_USAGE_SYSTEM)] = STREAM_ULTRASONIC;

    // Old mapping relationships from content and usage to stream type
    streamMap[std::make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_VOICE_ASSISTANT;
    streamMap[std::make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_UNKNOWN)] = STREAM_NOTIFICATION;
    streamMap[std::make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_MEDIA)] = STREAM_NOTIFICATION;
    streamMap[std::make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_RING;
    streamMap[std::make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_UNKNOWN)] = STREAM_RING;
    streamMap[std::make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_MEDIA)] = STREAM_RING;
    streamMap[std::make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_RING;

    // Only use stream usage to choose stream type
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_MEDIA)] = STREAM_MUSIC;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_MUSIC)] = STREAM_MUSIC;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_VOICE_CALL;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_VOICE_MODEM_COMMUNICATION)] = STREAM_VOICE_CALL;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_VOICE_ASSISTANT;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_ALARM)] = STREAM_ALARM;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_VOICE_MESSAGE)] = STREAM_VOICE_MESSAGE;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_RING;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_RINGTONE)] = STREAM_RING;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_NOTIFICATION)] = STREAM_NOTIFICATION;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_ACCESSIBILITY)] = STREAM_ACCESSIBILITY;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_SYSTEM)] = STREAM_SYSTEM;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_MOVIE)] = STREAM_MOVIE;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_GAME)] = STREAM_GAME;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_AUDIOBOOK)] = STREAM_SPEECH;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_NAVIGATION)] = STREAM_NAVIGATION;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_DTMF)] = STREAM_DTMF;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_ENFORCED_TONE)] = STREAM_SYSTEM_ENFORCED;
    streamMap[std::make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_ULTRASONIC)] = STREAM_ULTRASONIC;

    return streamMap;
}

AudioStreamType IAudioStream::GetStreamType(ContentType contentType, StreamUsage streamUsage)
{
    AudioStreamType streamType = STREAM_MUSIC;
    auto pos = streamTypeMap_.find(std::make_pair(contentType, streamUsage));
    if (pos != streamTypeMap_.end()) {
        streamType = pos->second;
    }

    if (streamType == STREAM_MEDIA) {
        streamType = STREAM_MUSIC;
    }

    return streamType;
}

const std::string IAudioStream::GetEffectSceneName(AudioStreamType audioType)
{
    std::string name;
    switch (audioType) {
        case STREAM_MUSIC:
            name = "SCENE_MUSIC";
            break;
        case STREAM_GAME:
            name = "SCENE_GAME";
            break;
        case STREAM_MOVIE:
            name = "SCENE_MOVIE";
            break;
        case STREAM_SPEECH:
        case STREAM_VOICE_CALL:
        case STREAM_VOICE_ASSISTANT:
            name = "SCENE_SPEECH";
            break;
        case STREAM_RING:
        case STREAM_ALARM:
        case STREAM_NOTIFICATION:
        case STREAM_SYSTEM:
        case STREAM_DTMF:
        case STREAM_SYSTEM_ENFORCED:
            name = "SCENE_RING";
            break;
        default:
            name = "SCENE_OTHERS";
    }

    const std::string sceneName = name;
    return sceneName;
}

bool IAudioStream::IsStreamSupported(int32_t streamFlags, const AudioStreamParams &params)
{
    // 0 for normal stream
    if (streamFlags == 0) {
        return true;
    }
    // 1 for fast stream
    if (streamFlags == STREAM_FLAG_FAST) {
        // check audio sample rate
        AudioSamplingRate samplingRate = static_cast<AudioSamplingRate>(params.samplingRate);
        auto rateItem = std::find(AUDIO_FAST_STREAM_SUPPORTED_SAMPLING_RATES.begin(),
            AUDIO_FAST_STREAM_SUPPORTED_SAMPLING_RATES.end(), samplingRate);
        if (rateItem == AUDIO_FAST_STREAM_SUPPORTED_SAMPLING_RATES.end()) {
            return false;
        }

        // check audio channel
        AudioChannel channels = static_cast<AudioChannel>(params.channels);
        auto channelItem = std::find(AUDIO_FAST_STREAM_SUPPORTED_CHANNELS.begin(),
            AUDIO_FAST_STREAM_SUPPORTED_CHANNELS.end(), channels);
        if (channelItem == AUDIO_FAST_STREAM_SUPPORTED_CHANNELS.end()) {
            return false;
        }

        // check audio sample format
        AudioSampleFormat format = static_cast<AudioSampleFormat>(params.format);
        auto formatItem = std::find(AUDIO_FAST_STREAM_SUPPORTED_FORMATS.begin(),
            AUDIO_FAST_STREAM_SUPPORTED_FORMATS.end(), format);
        if (formatItem == AUDIO_FAST_STREAM_SUPPORTED_FORMATS.end()) {
            return false;
        }
    }
    return true;
}

std::shared_ptr<IAudioStream> IAudioStream::GetPlaybackStream(StreamClass streamClass, AudioStreamParams params,
    AudioStreamType eStreamType, int32_t appUid)
{
    if (streamClass == FAST_STREAM) {
        (void)params;
        AUDIO_INFO_LOG("Create fast playback stream");
        return std::make_shared<FastAudioStream>(eStreamType, AUDIO_MODE_PLAYBACK, appUid);
    }
    if (streamClass == PA_STREAM) {
        return std::make_shared<AudioStream>(eStreamType, AUDIO_MODE_PLAYBACK, appUid);
    }
    return nullptr;
}

std::shared_ptr<IAudioStream> IAudioStream::GetRecordStream(StreamClass streamClass, AudioStreamParams params,
    AudioStreamType eStreamType, int32_t appUid)
{
    if (streamClass == FAST_STREAM) {
        (void)params;
        AUDIO_INFO_LOG("Create fast record stream");
        return std::make_shared<FastAudioStream>(eStreamType, AUDIO_MODE_RECORD, appUid);
    }
    if (streamClass == PA_STREAM) {
        return std::make_shared<AudioStream>(eStreamType, AUDIO_MODE_RECORD, appUid);
    }
    return nullptr;
}
} // namespace AudioStandard
} // namespace OHOS
