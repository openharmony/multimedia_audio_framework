/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

/**
 * @file audio_renderer_death_recipient.h
 *
 * @brief audio renderer death recipient.
 */

#ifndef AUDIO_STREAM_DEATH_RECIPIENT_H
#define AUDIO_STREAM_DEATH_RECIPIENT_H

#include "iremote_object.h"
#include "nocopyable.h"

namespace OHOS {
namespace AudioStandard {
class AudioStreamDeathRecipient : public IRemoteObject::DeathRecipient {
public:
    explicit AudioStreamDeathRecipient(pid_t pid) : m_pid(pid) {};

    ~AudioStreamDeathRecipient() override = default;
    DISALLOW_COPY_AND_MOVE(AudioStreamDeathRecipient);

    void OnRemoteDied(const wptr <IRemoteObject> &remote) override
    {
        (void) remote;
        if (m_callback != nullptr) {
            m_callback(m_pid);
        }
    }

    using ServerDiedCallback = std::function<void(pid_t)>;

    void SetServerDiedCallback(ServerDiedCallback callback)
    {
        m_callback = callback;
    }

private:
    pid_t m_pid = 0;
    ServerDiedCallback m_callback = nullptr;
};
}
}

#endif // AUDIO_STREAM_DEATH_RECIPIENT_H
