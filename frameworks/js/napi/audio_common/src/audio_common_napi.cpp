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

#include "audio_common_napi.h"
#include "audio_log.h"
#include "audio_manager_napi.h"
#include "audio_info.h"

namespace OHOS {
namespace AudioStandard {
std::string AudioCommonNapi::GetStringArgument(napi_env env, napi_value value)
{
    std::string strValue = "";
    size_t bufLength = 0;
    napi_status status = napi_get_value_string_utf8(env, value, nullptr, 0, &bufLength);
    if (status == napi_ok && bufLength > 0 && bufLength < PATH_MAX) {
        char *buffer = (char *)malloc((bufLength + 1) * sizeof(char));
        CHECK_AND_RETURN_RET_LOG(buffer != nullptr, strValue, "no memory");
        status = napi_get_value_string_utf8(env, value, buffer, bufLength + 1, &bufLength);
        if (status == napi_ok) {
            AUDIO_DEBUG_LOG("argument = %{public}s", buffer);
            strValue = buffer;
        }
        free(buffer);
        buffer = nullptr;
    }
    return strValue;
}

std::string AudioCommonNapi::getMessageByCode(int32_t &code)
{
    std::string err_message;
    switch (code) {
        case NAPI_ERR_INVALID_PARAM:
            err_message = NAPI_ERR_INVALID_PARAM_INFO;
            break;
        case NAPI_ERR_NO_MEMORY:
            err_message = NAPI_ERR_NO_MEMORY_INFO;
            break;
        case NAPI_ERR_ILLEGAL_STATE:
            err_message = NAPI_ERR_ILLEGAL_STATE_INFO;
            break;
        case NAPI_ERR_UNSUPPORTED:
            err_message = NAPI_ERR_UNSUPPORTED_INFO;
            break;
        case NAPI_ERR_TIMEOUT:
            err_message = NAPI_ERR_TIMEOUT_INFO;
            break;
        case NAPI_ERR_STREAM_LIMIT:
            err_message = NAPI_ERR_STREAM_LIMIT_INFO;
            break;
        case NAPI_ERR_SYSTEM:
            err_message = NAPI_ERR_SYSTEM_INFO;
            break;
        case NAPI_ERR_INPUT_INVALID:
            err_message = NAPI_ERR_INPUT_INVALID_INFO;
            break;
        default:
            err_message = NAPI_ERR_SYSTEM_INFO;
            code = NAPI_ERR_SYSTEM;
            break;
    }
    return err_message;
}

void AudioCommonNapi::throwError(napi_env env, int32_t code)
{
    std::string messageValue = AudioCommonNapi::getMessageByCode(code);
    napi_throw_error(env, (std::to_string(code)).c_str(), messageValue.c_str());
}

bool AudioCommonNapi::IsLegalInputArgumentVolLevel(int32_t volLevel)
{
    return (volLevel < MIN_VOLUME_LEVEL || volLevel > MAX_VOLUME_LEVEL) ? false : true;
}

bool AudioCommonNapi::IsLegalInputArgumentVolType(int32_t inputType)
{
    bool result = false;
    switch (inputType) {
        case AudioManagerNapi::RINGTONE:
        case AudioManagerNapi::MEDIA:
        case AudioManagerNapi::VOICE_CALL:
        case AudioManagerNapi::VOICE_ASSISTANT:
        case AudioManagerNapi::ALL:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

bool AudioCommonNapi::IsLegalInputArgumentDeviceFlag(int32_t deviceFlag)
{
    bool result = false;
    switch (deviceFlag) {
        case DeviceFlag::NONE_DEVICES_FLAG:
        case DeviceFlag::OUTPUT_DEVICES_FLAG:
        case DeviceFlag::INPUT_DEVICES_FLAG:
        case DeviceFlag::ALL_DEVICES_FLAG:
        case DeviceFlag::DISTRIBUTED_OUTPUT_DEVICES_FLAG:
        case DeviceFlag::DISTRIBUTED_INPUT_DEVICES_FLAG:
        case DeviceFlag::ALL_DISTRIBUTED_DEVICES_FLAG:
        case DeviceFlag::ALL_L_D_DEVICES_FLAG:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

bool AudioCommonNapi::IsLegalInputArgumentActiveDeviceType(int32_t activeDeviceFlag)
{
    bool result = false;
    switch (activeDeviceFlag) {
        case ActiveDeviceType::SPEAKER:
        case ActiveDeviceType::BLUETOOTH_SCO:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

bool AudioCommonNapi::IsLegalInputArgumentCommunicationDeviceType(int32_t communicationDeviceType)
{
    bool result = false;
    switch (communicationDeviceType) {
        case CommunicationDeviceType::COMMUNICATION_SPEAKER:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

bool AudioCommonNapi::IsLegalInputArgumentRingMode(int32_t ringerMode)
{
    bool result = false;
    switch (ringerMode) {
        case AudioRingerMode::RINGER_MODE_SILENT:
        case AudioRingerMode::RINGER_MODE_VIBRATE:
        case AudioRingerMode::RINGER_MODE_NORMAL:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

}  // namespace AudioStandard
}  // namespace OHOS