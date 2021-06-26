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

#include "audio_errors.h"
#include "audio_policy_base.h"
#include "audio_policy_server.h"
#include "audio_policy_types.h"
#include "media_log.h"

namespace OHOS {
namespace AudioStandard {
int AudioPolicyManagerStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    switch (code) {
        case SET_STREAM_VOLUME: {
            AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
            float volume = data.ReadFloat();
            int result = SetStreamVolume(streamType, volume);
            if (result == SUCCESS)
                reply.WriteInt32(MEDIA_OK);
            else
                reply.WriteInt32(MEDIA_ERR);
            break;
        }

        case SET_RINGER_MODE: {
            AudioRingerMode rMode = static_cast<AudioRingerMode>(data.ReadInt32());
            int32_t result = SetRingerMode(rMode);
            reply.WriteInt32(result);
            break;
        }

        case GET_RINGER_MODE: {
            AudioRingerMode rMode = GetRingerMode();
            reply.WriteInt32(static_cast<int>(rMode));
            break;
        }

        case GET_STREAM_VOLUME: {
            AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
            float volume = GetStreamVolume(streamType);
            reply.WriteFloat(volume);
            break;
        }

        case SET_STREAM_MUTE: {
            AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
            bool mute = data.ReadBool();
            int result = SetStreamMute(streamType, mute);
            if (result == SUCCESS)
                reply.WriteInt32(MEDIA_OK);
            else
                reply.WriteInt32(MEDIA_ERR);
            break;
        }

        case GET_STREAM_MUTE: {
            AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
            bool mute = GetStreamMute(streamType);
            reply.WriteBool(mute);
            break;
        }

        case IS_STREAM_ACTIVE: {
            AudioStreamType streamType = static_cast<AudioStreamType>(data.ReadInt32());
            bool isActive = IsStreamActive(streamType);
            reply.WriteBool(isActive);
            break;
        }

        case SET_DEVICE_ACTIVE: {
            DeviceType deviceType = static_cast<DeviceType>(data.ReadInt32());
            bool active = data.ReadBool();
            int32_t result = SetDeviceActive(deviceType, active);
            if (result == SUCCESS)
                reply.WriteInt32(MEDIA_OK);
            else
                reply.WriteInt32(MEDIA_ERR);
            break;
        }

        case IS_DEVICE_ACTIVE: {
            DeviceType deviceType = static_cast<DeviceType>(data.ReadInt32());
            bool result = IsDeviceActive(deviceType);
            reply.WriteBool(result);
            break;
        }

        default: {
            MEDIA_ERR_LOG("default case, need check AudioPolicyManagerStub");
            return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
        }
    }
    return MEDIA_OK;
}

bool AudioPolicyManagerStub::IsPermissionValid()
{
    return true;
}
} // namespace audio_policy
} // namespace OHOS
