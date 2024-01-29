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

#include "cockpit_phone_router.h"
#include "audio_device_manager.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {

vector<unique_ptr<AudioDeviceDescriptor>> GetBTCarDevices(const vector<unique_ptr<AudioDeviceDescriptor>> &descs)
{
    vector<unique_ptr<AudioDeviceDescriptor>> carDescs;
    for (const auto &desc : descs) {
        if (desc == nullptr || desc->deviceCategory_ != BT_CAR) {
            continue;
        }
        carDescs.push_back(make_unique<AudioDeviceDescriptor>(*desc));
    }
    return carDescs;
}

unique_ptr<AudioDeviceDescriptor> CockpitPhoneRouter::GetMediaRenderDevice(StreamUsage streamUsage, int32_t clientUID)
{
    return make_unique<AudioDeviceDescriptor>();
}

unique_ptr<AudioDeviceDescriptor> CockpitPhoneRouter::GetCallRenderDevice(StreamUsage streamUsage, int32_t clientUID)
{
    vector<unique_ptr<AudioDeviceDescriptor>> descs =
        AudioDeviceManager::GetAudioDeviceManager().GetCommRenderPublicDevices();
    vector<unique_ptr<AudioDeviceDescriptor>> carDescs = GetBTCarDevices(descs);
    unique_ptr<AudioDeviceDescriptor> desc = GetLatestConnectDeivce(carDescs);
    AUDIO_DEBUG_LOG("streamUsage %{public}d clientUID %{public}d fetch device %{public}d", streamUsage,
        clientUID, desc->deviceType_);
    return desc;
}

unique_ptr<AudioDeviceDescriptor> CockpitPhoneRouter::GetCallCaptureDevice(SourceType sourceType, int32_t clientUID)
{
    vector<unique_ptr<AudioDeviceDescriptor>> descs =
        AudioDeviceManager::GetAudioDeviceManager().GetCommCapturePublicDevices();
    vector<unique_ptr<AudioDeviceDescriptor>> carDescs = GetBTCarDevices(descs);
    unique_ptr<AudioDeviceDescriptor> desc = GetLatestConnectDeivce(carDescs);
    AUDIO_DEBUG_LOG("sourceType %{public}d clientUID %{public}d fetch device %{public}d", sourceType,
        clientUID, desc->deviceType_);
    return desc;
}

unique_ptr<AudioDeviceDescriptor> CockpitPhoneRouter::GetRingRenderDevice(StreamUsage streamUsage, int32_t clientUID)
{
    return make_unique<AudioDeviceDescriptor>();
}

unique_ptr<AudioDeviceDescriptor> CockpitPhoneRouter::GetRecordCaptureDevice(SourceType sourceType, int32_t clientUID)
{
    return make_unique<AudioDeviceDescriptor>();
}

unique_ptr<AudioDeviceDescriptor> CockpitPhoneRouter::GetToneRenderDevice(StreamUsage streamUsage, int32_t clientUID)
{
    return make_unique<AudioDeviceDescriptor>();
}

} // namespace AudioStandard
} // namespace OHOS