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

#include "audio_device_descriptor.h"
#include "media_log.h"

namespace OHOS {
namespace AudioStandard {
/**
 * @brief The AudioDeviceDescriptor class provides
 *         different sets of audio devices and their roles
 */
AudioDeviceDescriptor::AudioDeviceDescriptor()
{
    MEDIA_DEBUG_LOG("AudioDeviceDescriptor constructor");
    deviceType_ = DEVICE_TYPE_NONE;
    deviceRole_ = DEVICE_ROLE_NONE;
}

AudioDeviceDescriptor::~AudioDeviceDescriptor()
{
    MEDIA_DEBUG_LOG("AudioDeviceDescriptor::~AudioDeviceDescriptor");
}

bool AudioDeviceDescriptor::Marshalling(Parcel &parcel) const
{
    MEDIA_DEBUG_LOG("AudioDeviceDescriptor::Marshalling called");
    return parcel.WriteInt32(deviceType_) && parcel.WriteInt32(deviceRole_);
}

AudioDeviceDescriptor *AudioDeviceDescriptor::Unmarshalling(Parcel &in)
{
    MEDIA_DEBUG_LOG("AudioDeviceDescriptor::Unmarshalling called");
    AudioDeviceDescriptor *audioDeviceDescriptor = new(std::nothrow) AudioDeviceDescriptor();
    if (audioDeviceDescriptor == nullptr) {
        return nullptr;
    }
    audioDeviceDescriptor->deviceType_ = static_cast<AudioDeviceDescriptor::DeviceType>(in.ReadInt32());
    audioDeviceDescriptor->deviceRole_ = static_cast<AudioDeviceDescriptor::DeviceRole>(in.ReadInt32());
    return audioDeviceDescriptor;
}
} // namespace AudioStandard
} // namespace OHOS
