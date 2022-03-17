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

#ifndef ST_AUDIO_VOLUME_KEY_EVENT_CALLBACK_PROXY_H
#define ST_AUDIO_VOLUME_KEY_EVENT_CALLBACK_PROXY_H

#include "iremote_proxy.h"
#include "audio_system_manager.h"
#include "i_audio_volume_key_event_callback.h"

namespace OHOS {
namespace AudioStandard {
class VolumeKeyEventCallbackListner : public VolumeKeyEventCallback {
public:
    explicit VolumeKeyEventCallbackListner(const sptr<IAudioVolumeKeyEventCallback> &listener);
    virtual ~VolumeKeyEventCallbackListner();
    DISALLOW_COPY_AND_MOVE(VolumeKeyEventCallbackListner);
    void OnVolumeKeyEvent(AudioStreamType streamType, int32_t volumeLevel, bool isUpdateUi) override;

private:
    sptr<IAudioVolumeKeyEventCallback> listener_ = nullptr;
};
class AudioVolumeKeyEventCallbackProxy : public IRemoteProxy<IAudioVolumeKeyEventCallback> {
public:
    explicit AudioVolumeKeyEventCallbackProxy(const sptr<IRemoteObject> &impl);
    virtual ~AudioVolumeKeyEventCallbackProxy() = default;
    void OnVolumeKeyEvent(AudioStreamType streamType, int32_t volumeLevel, bool isUpdateUi) override;
private:
    static inline BrokerDelegator<AudioVolumeKeyEventCallbackProxy> delegator_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // ST_AUDIO_VOLUME_KEY_EVENT_CALLBACK_PROXY_H
