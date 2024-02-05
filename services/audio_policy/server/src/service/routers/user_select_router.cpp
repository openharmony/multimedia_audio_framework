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

#include "user_select_router.h"
#include "audio_log.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
unique_ptr<AudioDeviceDescriptor> UserSelectRouter::GetMediaRenderDevice(StreamUsage streamUsage, int32_t clientUID)
{
    unique_ptr<AudioDeviceDescriptor> perDev_ =
        AudioStateManager::GetAudioStateManager().GetPerferredMediaRenderDevice();
    vector<unique_ptr<AudioDeviceDescriptor>> mediaDevices =
        AudioDeviceManager::GetAudioDeviceManager().GetAvailableDevicesByUsage(MEDIA_OUTPUT_DEVICES);
    if (perDev_->deviceId_ == 0) {
        AUDIO_DEBUG_LOG(" PerferredMediaRenderDevice is null");
        return make_unique<AudioDeviceDescriptor>();
    } else {
        AUDIO_INFO_LOG(" PerferredMediaRenderDevice deviceId is %{public}d", perDev_->deviceId_);
        return RouterBase::GetPairCaptureDevice(perDev_, mediaDevices);
    }
}

unique_ptr<AudioDeviceDescriptor> UserSelectRouter::GetCallRenderDevice(StreamUsage streamUsage, int32_t clientUID)
{
    unique_ptr<AudioDeviceDescriptor> perDev_ =
        AudioStateManager::GetAudioStateManager().GetPerferredCallRenderDevice();
    vector<unique_ptr<AudioDeviceDescriptor>> callDevices =
        AudioDeviceManager::GetAudioDeviceManager().GetAvailableDevicesByUsage(CALL_OUTPUT_DEVICES);
    if (perDev_->deviceId_ == 0) {
        AUDIO_DEBUG_LOG(" PerferredCallRenderDevice is null");
        return make_unique<AudioDeviceDescriptor>();
    } else {
        AUDIO_INFO_LOG(" PerferredCallRenderDevice deviceId is %{public}d", perDev_->deviceId_);
        return RouterBase::GetPairCaptureDevice(perDev_, callDevices);
    }
}

unique_ptr<AudioDeviceDescriptor> UserSelectRouter::GetCallCaptureDevice(SourceType sourceType, int32_t clientUID)
{
    unique_ptr<AudioDeviceDescriptor> perDev_ =
        AudioStateManager::GetAudioStateManager().GetPerferredCallCaptureDevice();
    vector<unique_ptr<AudioDeviceDescriptor>> callDevices =
        AudioDeviceManager::GetAudioDeviceManager().GetAvailableDevicesByUsage(CALL_INPUT_DEVICES);
    if (perDev_->deviceId_ == 0) {
        AUDIO_DEBUG_LOG(" PerferredCallCaptureDevice is null");
        return make_unique<AudioDeviceDescriptor>();
    } else {
        AUDIO_INFO_LOG(" PerferredCallCaptureDevice deviceId is %{public}d", perDev_->deviceId_);
        return RouterBase::GetPairCaptureDevice(perDev_, callDevices);
    }
}

unique_ptr<AudioDeviceDescriptor> UserSelectRouter::GetRingRenderDevice(StreamUsage streamUsage, int32_t clientUID)
{
    return make_unique<AudioDeviceDescriptor>();
}

unique_ptr<AudioDeviceDescriptor> UserSelectRouter::GetRecordCaptureDevice(SourceType sourceType, int32_t clientUID)
{
    unique_ptr<AudioDeviceDescriptor> perDev_ =
        AudioStateManager::GetAudioStateManager().GetPerferredRecordCaptureDevice();
    vector<unique_ptr<AudioDeviceDescriptor>> recordDevices =
        AudioDeviceManager::GetAudioDeviceManager().GetAvailableDevicesByUsage(MEDIA_INPUT_DEVICES);
    if (perDev_->deviceId_ == 0) {
        AUDIO_DEBUG_LOG(" PerferredRecordCaptureDevice is null");
        return make_unique<AudioDeviceDescriptor>();
    } else {
        AUDIO_INFO_LOG(" PerferredRecordCaptureDevice deviceId is %{public}d", perDev_->deviceId_);
        return RouterBase::GetPairCaptureDevice(perDev_, recordDevices);
    }
}

unique_ptr<AudioDeviceDescriptor> UserSelectRouter::GetToneRenderDevice(StreamUsage streamUsage, int32_t clientUID)
{
    return make_unique<AudioDeviceDescriptor>();
}

} // namespace AudioStandard
} // namespace OHOS