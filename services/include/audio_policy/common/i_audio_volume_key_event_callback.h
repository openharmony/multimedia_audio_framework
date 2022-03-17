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

#ifndef ST_AUDIO_VOLUME_KEY_EVENT_CALLBACK_H
#define ST_AUDIO_VOLUME_KEY_EVENT_CALLBACK_H

#include "ipc_types.h"
#include "iremote_broker.h"
#include "iremote_proxy.h"
#include "iremote_stub.h"
#include "audio_info.h"

namespace OHOS {
namespace AudioStandard {
class IAudioVolumeKeyEventCallback : public IRemoteBroker {
public:
    virtual void OnVolumeKeyEvent(AudioStreamType streamType, int32_t volumeLevel, bool isUpdateUi) = 0;
    enum {
        ON_VOLUME_KEY_EVENT = 0,
    };
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"IAudioVolumeKeyEventCallback");
};
} // namespace AudioStandard
} // namespace OHOS
#endif // ST_AUDIO_VOLUME_KEY_EVENT_CALLBACK_H
