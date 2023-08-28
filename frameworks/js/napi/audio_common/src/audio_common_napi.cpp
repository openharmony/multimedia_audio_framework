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
#include "audio_info.h"
#include "audio_manager_napi.h"

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
        case ERR_PERMISSION_DENIED:
        case NAPI_ERR_PERMISSION_DENIED:
            err_message = NAPI_ERROR_PERMISSION_DENIED_INFO;
            code = NAPI_ERR_PERMISSION_DENIED;
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
        case AudioManagerNapi::ALARM:
        case AudioManagerNapi::ACCESSIBILITY:
        case AudioManagerNapi::ULTRASONIC:
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

bool AudioCommonNapi::IsSameCallback(napi_env env, napi_value callback, napi_ref refCallback)
{
    bool isEquals = false;
    napi_value copyValue = nullptr;

    napi_get_reference_value(env, refCallback, &copyValue);
    if (napi_strict_equals(env, copyValue, callback, &isEquals) != napi_ok) {
        AUDIO_ERR_LOG("IsSameCallback: get napi_strict_equals failed");
        return false;
    }

    return isEquals;
}

bool AudioCommonNapi::IsLegalInputArgumentStreamUsage(int32_t streamUsage)
{
    bool result = false;
    switch (streamUsage) {
        case STREAM_USAGE_UNKNOWN:
        case STREAM_USAGE_MEDIA:
        case STREAM_USAGE_VOICE_COMMUNICATION:
        case STREAM_USAGE_VOICE_ASSISTANT:
        case STREAM_USAGE_ALARM:
        case STREAM_USAGE_VOICE_MESSAGE:
        case STREAM_USAGE_NOTIFICATION_RINGTONE:
        case STREAM_USAGE_NOTIFICATION:
        case STREAM_USAGE_ACCESSIBILITY:
        case STREAM_USAGE_SYSTEM:
        case STREAM_USAGE_MOVIE:
        case STREAM_USAGE_GAME:
        case STREAM_USAGE_AUDIOBOOK:
        case STREAM_USAGE_NAVIGATION:
        case STREAM_USAGE_DTMF:
        case STREAM_USAGE_ENFORCED_TONE:
        case STREAM_USAGE_ULTRASONIC:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

bool AudioCommonNapi::IsLegalInputArgumentAudioEffectMode(int32_t audioEffectMode)
{
    bool result = false;
    switch (audioEffectMode) {
        case AudioEffectMode::EFFECT_NONE:
        case AudioEffectMode::EFFECT_DEFAULT:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

bool AudioCommonNapi::IsLegalInputArgumentVolumeAdjustType(int32_t adjustType)
{
    bool result = false;
    switch (adjustType) {
        case VolumeAdjustType::VOLUME_UP:
        case VolumeAdjustType::VOLUME_DOWN:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

bool AudioCommonNapi::IsLegalInputArgumentDeviceType(int32_t deviceType)
{
    bool result = false;
    switch (deviceType) {
        case DeviceType::DEVICE_TYPE_EARPIECE:
        case DeviceType::DEVICE_TYPE_SPEAKER:
        case DeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case DeviceType::DEVICE_TYPE_WIRED_HEADPHONES:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
        case DeviceType::DEVICE_TYPE_MIC:
        case DeviceType::DEVICE_TYPE_USB_HEADSET:
        case DeviceType::DEVICE_TYPE_FILE_SINK:
        case DeviceType::DEVICE_TYPE_FILE_SOURCE:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

bool AudioCommonNapi::IsLegalInputArgumentInterruptMode(int32_t interruptMode)
{
    bool result = false;
    switch (interruptMode) {
        case InterruptMode::SHARE_MODE:
        case InterruptMode::INDEPENDENT_MODE:
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
