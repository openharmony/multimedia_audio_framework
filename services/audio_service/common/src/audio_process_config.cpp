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

#include "audio_process_config.h"

#include <sstream>

#include "audio_errors.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
int32_t ProcessConfig::WriteConfigToParcel(const AudioProcessConfig &config, MessageParcel &parcel)
{
    // AppInfo
    parcel.WriteInt32(config.appInfo.appUid);
    parcel.WriteUint32(config.appInfo.appTokenId);
    parcel.WriteInt32(config.appInfo.appPid);

    // AudioStreamInfo
    parcel.WriteInt32(config.streamInfo.samplingRate);
    parcel.WriteInt32(config.streamInfo.encoding);
    parcel.WriteInt32(config.streamInfo.format);
    parcel.WriteInt32(config.streamInfo.channels);
    parcel.WriteUint64(config.streamInfo.channelLayout);

    // AudioMode
    parcel.WriteInt32(config.audioMode);

    // AudioRendererInfo
    parcel.WriteInt32(config.rendererInfo.contentType);
    parcel.WriteInt32(config.rendererInfo.streamUsage);
    parcel.WriteInt32(config.rendererInfo.rendererFlags);

    //AudioPrivacyType
    parcel.WriteInt32(config.privacyType);

    // AudioCapturerInfo
    parcel.WriteInt32(config.capturerInfo.sourceType);
    parcel.WriteInt32(config.capturerInfo.capturerFlags);

    // streamType
    parcel.WriteInt32(config.streamType);

    // Recorder only
    parcel.WriteBool(config.isInnerCapturer);
    parcel.WriteBool(config.isWakeupCapturer);

    return SUCCESS;
}

int32_t ProcessConfig::ReadConfigFromParcel(AudioProcessConfig &config, MessageParcel &parcel)
{
    // AppInfo
    config.appInfo.appUid = parcel.ReadInt32();
    config.appInfo.appTokenId = parcel.ReadUint32();
    config.appInfo.appPid = parcel.ReadInt32();

    // AudioStreamInfo
    config.streamInfo.samplingRate = static_cast<AudioSamplingRate>(parcel.ReadInt32());
    config.streamInfo.encoding = static_cast<AudioEncodingType>(parcel.ReadInt32());
    config.streamInfo.format = static_cast<AudioSampleFormat>(parcel.ReadInt32());
    config.streamInfo.channels = static_cast<AudioChannel>(parcel.ReadInt32());
    config.streamInfo.channelLayout = static_cast<AudioChannelLayout>(parcel.ReadUint64());

    // AudioMode
    config.audioMode = static_cast<AudioMode>(parcel.ReadInt32());

    // AudioRendererInfo
    config.rendererInfo.contentType = static_cast<ContentType>(parcel.ReadInt32());
    config.rendererInfo.streamUsage = static_cast<StreamUsage>(parcel.ReadInt32());
    config.rendererInfo.rendererFlags = parcel.ReadInt32();

    //AudioPrivacyType
    config.privacyType = static_cast<AudioPrivacyType>(parcel.ReadInt32());

    // AudioCapturerInfo
    config.capturerInfo.sourceType = static_cast<SourceType>(parcel.ReadInt32());
    config.capturerInfo.capturerFlags = parcel.ReadInt32();

    // streamType
    config.streamType = static_cast<AudioStreamType>(parcel.ReadInt32());

    // Recorder only
    config.isInnerCapturer = parcel.ReadBool();
    config.isWakeupCapturer = parcel.ReadBool();
    return SUCCESS;
}

std::string ProcessConfig::DumpProcessConfig(const AudioProcessConfig &config)
{
    std::stringstream temp;

    // AppInfo
    temp << "appInfo:pid<" << config.appInfo.appPid << "> uid<" << config.appInfo.appUid << "> tokenId<" <<
        config.appInfo.appTokenId << "> ";

    // streamInfo
    temp << "streamInfo:format(" << config.streamInfo.format << ") encoding(" << config.streamInfo.encoding <<
        ") channels(" << config.streamInfo.channels << ") samplingRate(" << config.streamInfo.samplingRate << ") ";

    // audioMode
    if (config.audioMode == AudioMode::AUDIO_MODE_PLAYBACK) {
        temp << "[rendererInfo]:streamUsage(" << config.rendererInfo.streamUsage << ") contentType(" <<
            config.rendererInfo.contentType << ") flag(" << config.rendererInfo.rendererFlags << ") ";
    } else {
        temp << "[capturerInfo]:sourceType(" << config.capturerInfo.sourceType << ") flag(" <<
            config.capturerInfo.capturerFlags << ") ";
    }

    temp << "streamType<" << config.streamType << ">";

    return temp.str();
}
} // namespace AudioStandard
} // namespace OHOS

