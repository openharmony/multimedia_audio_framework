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

#ifndef ST_AUDIO_DEVICE_DESCRIPTOR_H
#define ST_AUDIO_DEVICE_DESCRIPTOR_H

#include "parcel.h"

namespace OHOS {
namespace AudioStandard {
/**
 * @brief The AudioDeviceDescriptor class provides different sets of audio devices and their roles
 */

class AudioDeviceDescriptor : public Parcelable {
public:
enum DeviceFlag {
        /**
         * Indicates all output audio devices.
         */
        OUTPUT_DEVICES_FLAG = 0,
        /**
         * Indicates all input audio devices.
         */
        INPUT_DEVICES_FLAG = 1,
        /**
         * Indicates all audio devices.
         */
        ALL_DEVICES_FLAG = 2
};

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

    DeviceType getType();
    DeviceRole getRole();
    DeviceType deviceType_;
    DeviceRole deviceRole_;
    AudioDeviceDescriptor();
    virtual ~AudioDeviceDescriptor();
    bool Marshalling(Parcel &parcel) const override;
    static AudioDeviceDescriptor* Unmarshalling(Parcel &parcel);
};
} // namespace AudioStandard
} // namespace OHOS
#endif // ST_AUDIO_DEVICE_DESCRIPTOR_H
