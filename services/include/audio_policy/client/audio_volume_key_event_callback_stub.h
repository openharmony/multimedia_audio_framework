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

#ifndef ST_AUDIO_VOLUME_KEY_EVENT_CALLBACK_STUB_H
#define ST_AUDIO_VOLUME_KEY_EVENT_CALLBACK_STUB_H

#include "audio_system_manager.h"
#include "i_audio_volume_key_event_callback.h"

namespace OHOS {
namespace AudioStandard {
class AudioVolumeKeyEventCallbackStub : public IRemoteStub<IAudioVolumeKeyEventCallback> {
public:
    AudioVolumeKeyEventCallbackStub();
    virtual ~AudioVolumeKeyEventCallbackStub();
    virtual int OnRemoteRequest(uint32_t code, MessageParcel &data,
            MessageParcel &reply, MessageOption &option) override;
    void OnVolumeKeyEvent(AudioStreamType streamType, int32_t volumeLevel, bool isUpdateUi) override;
    void SetOnVolumeKeyEventCallback(const std::weak_ptr<VolumeKeyEventCallback> &callback);

private:
    std::weak_ptr<VolumeKeyEventCallback> callback_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // ST_AUDIO_VOLUME_KEY_EVENT_CALLBACK_H
