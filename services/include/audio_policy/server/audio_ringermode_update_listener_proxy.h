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

#ifndef AUDIO_RINGERMODE_UPDATE_LISTENER_PROXY_H
#define AUDIO_RINGERMODE_UPDATE_LISTENER_PROXY_H

#include "audio_system_manager.h"
#include "i_standard_ringermode_update_listener.h"

namespace OHOS {
namespace AudioStandard {
class AudioRingerModeUpdateListenerProxy : public IRemoteProxy<IStandardRingerModeUpdateListener> {
public:
    explicit AudioRingerModeUpdateListenerProxy(const sptr<IRemoteObject> &impl);
    virtual ~AudioRingerModeUpdateListenerProxy();
    DISALLOW_COPY_AND_MOVE(AudioRingerModeUpdateListenerProxy);
    void OnRingerModeUpdated(const AudioRingerMode &ringerMode) override;

private:
    static inline BrokerDelegator<AudioRingerModeUpdateListenerProxy> delegator_;
};

class AudioRingerModeListenerCallback : public AudioRingerModeCallback {
public:
    AudioRingerModeListenerCallback(const sptr<IStandardRingerModeUpdateListener> &listener);
    virtual ~AudioRingerModeListenerCallback();
    DISALLOW_COPY_AND_MOVE(AudioRingerModeListenerCallback);
    void OnRingerModeUpdated(const AudioRingerMode &ringerMode) override;
private:
    sptr<IStandardRingerModeUpdateListener> listener_ = nullptr;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_RINGERMODE_UPDATE_LISTENER_PROXY_H
