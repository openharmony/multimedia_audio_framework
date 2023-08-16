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

#ifndef AUDIO_RENDERER_STATE_CHANGE_LISTENER_STUB_H
#define AUDIO_RENDERER_STATE_CHANGE_LISTENER_STUB_H

#include "audio_stream_manager.h"
#include "i_standard_renderer_state_change_listener.h"

namespace OHOS {
namespace AudioStandard {
class AudioRendererStateChangeListenerStub : public IRemoteStub<IStandardRendererStateChangeListener> {
public:
    AudioRendererStateChangeListenerStub();
    virtual ~AudioRendererStateChangeListenerStub();

    int OnRemoteRequest(uint32_t code, MessageParcel &data,
                                MessageParcel &reply, MessageOption &option) override;
    void OnRendererStateChange(const std::vector<std::unique_ptr<AudioRendererChangeInfo>>
        &audioRendererChangeInfos) override;
    void SetCallback(const std::weak_ptr<AudioRendererStateChangeCallback> &callback);
private:
    void ReadAudioRendererChangeInfo(MessageParcel &data,
        std::unique_ptr<AudioRendererChangeInfo> &rendererChangeInfo);
    std::weak_ptr<AudioRendererStateChangeCallback> callback_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_RENDERER_STATE_CHANGE_LISTENER_STUB_H
