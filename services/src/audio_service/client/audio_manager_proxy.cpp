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

#include "audio_manager_proxy.h"
#include "audio_system_manager.h"
#include "media_log.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
AudioManagerProxy::AudioManagerProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IStandardAudioService>(impl)
{
}

int32_t AudioManagerProxy::GetMaxVolume(AudioSystemManager::AudioVolumeType volumeType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioManagerProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int>(volumeType));
    int32_t error = Remote()->SendRequest(GET_MAX_VOLUME, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("Get max volume failed, error: %d", error);
        return error;
    }

    int32_t volume = reply.ReadInt32();
    return volume;
}

int32_t AudioManagerProxy::GetMinVolume(AudioSystemManager::AudioVolumeType volumeType)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioManagerProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(static_cast<int32_t>(volumeType));

    int32_t error = Remote()->SendRequest(GET_MIN_VOLUME, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("Get min volume failed, error: %d", error);
        return error;
    }

    int32_t volume = reply.ReadInt32();
    return volume;
}

int32_t AudioManagerProxy::SetMicrophoneMute(bool isMute)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioManagerProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteBool(isMute);
    int32_t error = Remote()->SendRequest(SET_MICROPHONE_MUTE, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("SetMicrophoneMute failed, error: %d", error);
        return error;
    }

    int32_t result = reply.ReadInt32();
    return result;
}

bool AudioManagerProxy::IsMicrophoneMute()
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioManagerProxy: WriteInterfaceToken failed");
        return false;
    }
    int32_t error = Remote()->SendRequest(IS_MICROPHONE_MUTE, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("IsMicrophoneMute failed, error: %d", error);
        return false;
    }

    bool isMute = reply.ReadBool();
    return isMute;
}

int32_t AudioManagerProxy::SetAudioScene(AudioScene audioScene)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioManagerProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(static_cast<int32_t>(audioScene));

    int32_t error = Remote()->SendRequest(SET_AUDIO_SCENE, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("SetAudioScene failed, error: %d", error);
        return false;
    }

    int32_t result = reply.ReadInt32();
    return result;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioManagerProxy::GetDevices(DeviceFlag deviceFlag)
{
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo = {};
    return deviceInfo;
}

const std::string AudioManagerProxy::GetAudioParameter(const std::string &key)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioManagerProxy: WriteInterfaceToken failed");
        return "";
    }
    data.WriteString(static_cast<std::string>(key));
    int32_t error = Remote()->SendRequest(GET_AUDIO_PARAMETER, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("Get audio parameter failed, error: %d", error);
        const std::string value = "";
        return value;
    }

    const std::string value = reply.ReadString();
    return value;
}

void AudioManagerProxy::SetAudioParameter(const std::string &key, const std::string &value)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioManagerProxy: WriteInterfaceToken failed");
        return;
    }
    data.WriteString(static_cast<std::string>(key));
    data.WriteString(static_cast<std::string>(value));
    int32_t error = Remote()->SendRequest(SET_AUDIO_PARAMETER, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("Get audio parameter failed, error: %d", error);
        return;
    }
}

const char *AudioManagerProxy::RetrieveCookie(int32_t &size)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    const char *cookieInfo = nullptr;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioManagerProxy: WriteInterfaceToken failed");
        return nullptr;
    }

    int32_t error = Remote()->SendRequest(RETRIEVE_COOKIE, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("retrieve cookie failed, error: %d", error);
        return nullptr;
    }

    size = reply.ReadInt32();
    if (size > 0) {
        cookieInfo = reinterpret_cast<const char *>(reply.ReadRawData(size));
    }

    return cookieInfo;
}

int32_t AudioManagerProxy::UpdateActiveDeviceRoute(DeviceType type, DeviceFlag flag)
{
    return ERR_NONE;
}
} // namespace AudioStandard
} // namespace OHOS
