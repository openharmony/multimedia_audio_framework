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

#include "media_log.h"
#include "audio_system_manager.h"

namespace OHOS {
namespace AudioStandard {
/**
 * @brief The AudioDeviceDescriptor provides
 *         different sets of audio devices and their roles
 */
AudioDeviceDescriptor::AudioDeviceDescriptor(DeviceType type, DeviceRole role) : deviceType_(type), deviceRole_(role)
{}

AudioDeviceDescriptor::AudioDeviceDescriptor()
    : AudioDeviceDescriptor(DeviceType::DEVICE_TYPE_NONE, DeviceRole::DEVICE_ROLE_NONE)
{}

AudioDeviceDescriptor::~AudioDeviceDescriptor()
{}

bool AudioDeviceDescriptor::Marshalling(Parcel &parcel) const
{
    return parcel.WriteInt32(deviceType_) && parcel.WriteInt32(deviceRole_);
}

sptr<AudioDeviceDescriptor> AudioDeviceDescriptor::Unmarshalling(Parcel &in)
{
    sptr<AudioDeviceDescriptor> audioDeviceDescriptor = new(std::nothrow) AudioDeviceDescriptor();
    if (audioDeviceDescriptor == nullptr) {
        return nullptr;
    }

    audioDeviceDescriptor->deviceType_ = static_cast<DeviceType>(in.ReadInt32());
    audioDeviceDescriptor->deviceRole_ = static_cast<DeviceRole>(in.ReadInt32());
    return audioDeviceDescriptor;
}
} // namespace AudioStandard
} // namespace OHOS
