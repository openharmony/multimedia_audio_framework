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

#include "audio_policy_service_proxy.h"
#include "media_log.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
AudioPolicyServiceProxy::AudioPolicyServiceProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IStandardAudioService>(impl)
{
}

int32_t AudioPolicyServiceProxy::GetMaxVolume(AudioSystemManager::AudioVolumeType volumeType)
{
    return 0;
}

int32_t AudioPolicyServiceProxy::GetMinVolume(AudioSystemManager::AudioVolumeType volumeType)
{
    return 0;
}

int32_t AudioPolicyServiceProxy::SetMicrophoneMute(bool isMute)
{
    return 0;
}

bool AudioPolicyServiceProxy::IsMicrophoneMute()
{
    return false;
}

int32_t AudioPolicyServiceProxy::SetAudioScene(AudioScene audioScene)
{
    MEDIA_DEBUG_LOG("[AudioPolicyServiceProxy] SetAudioScene %{public}d", audioScene);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioPolicyServiceProxy: WriteInterfaceToken failed");
        return -1;
    }

    data.WriteInt32(static_cast<int32_t>(audioScene));

    int32_t error = Remote()->SendRequest(SET_AUDIO_SCENE, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("SetAudioScene failed, error: %d", error);
        return false;
    }

    int32_t result = reply.ReadInt32();
    MEDIA_DEBUG_LOG("[AudioPolicyServiceProxy] SetAudioScene result %{public}d", result);
    return result;
}

int32_t AudioPolicyServiceProxy::UpdateActiveDeviceRoute(DeviceType type, DeviceFlag flag)
{
    MEDIA_DEBUG_LOG("[%{public}s]", __func__);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioPolicyServiceProxy: WriteInterfaceToken failed");
        return -1;
    }
    data.WriteInt32(type);
    data.WriteInt32(flag);

    auto error = Remote()->SendRequest(UPDATE_ROUTE_REQ, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("UpdateActiveDeviceRoute failed, error: %{public}d", error);
        return false;
    }

    auto result = reply.ReadInt32();
    MEDIA_DEBUG_LOG("[UPDATE_ROUTE_REQ] result %{public}d", result);
    return result;
}

const char *AudioPolicyServiceProxy::RetrieveCookie(int32_t &size)
{
    return nullptr;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyServiceProxy::GetDevices(DeviceFlag deviceFlag)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;

    if (!data.WriteInterfaceToken(GetDescriptor())) {
        MEDIA_ERR_LOG("AudioPolicyServiceProxy: WriteInterfaceToken failed");
        return deviceInfo;
    }
    data.WriteInt32(static_cast<int32_t>(deviceFlag));

    int32_t error = Remote()->SendRequest(GET_DEVICES, data, reply, option);
    if (error != ERR_NONE) {
        MEDIA_ERR_LOG("Get devices failed, error: %d", error);
        return deviceInfo;
    }

    int32_t size = reply.ReadInt32();
    for (int32_t i = 0; i < size; i++) {
        deviceInfo.push_back(AudioDeviceDescriptor::Unmarshalling(reply));
    }

    return deviceInfo;
}

const std::string AudioPolicyServiceProxy::GetAudioParameter(const std::string &key)
{
    return "";
}

void AudioPolicyServiceProxy::SetAudioParameter(const std::string &key, const std::string &value)
{
    return;
}
} // namespace AudioStandard
} // namespace OHOS
