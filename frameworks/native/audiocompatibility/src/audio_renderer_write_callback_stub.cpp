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

#include "audio_renderer_write_callback_stub.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
AudioRendererWriteCallbackStub::AudioRendererWriteCallbackStub()
{
}

AudioRendererWriteCallbackStub::~AudioRendererWriteCallbackStub()
{
}

int AudioRendererWriteCallbackStub::OnRemoteRequest(
    uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    AUDIO_DEBUG_LOG("AudioRendererWriteCallbackStub::OnRemoteRequest");
    if (data.ReadInterfaceToken() != GetDescriptor()) {
        AUDIO_ERR_LOG("AudioRendererWriteCallbackStub: ReadInterfaceToken failed");
        return -1;
    }
    switch (code) {
        case ON_WRITE_DATA: {
            size_t length = static_cast<size_t>(data.ReadInt64());
            OnWriteData(length);
            reply.WriteInt32(AUDIO_OK);
            break;
        }
        default: {
            reply.WriteInt32(AUDIO_INVALID_PARAM);
            break;
        }
    }
    return AUDIO_OK;
}

void AudioRendererWriteCallbackStub::OnWriteData(size_t length)
{
    AUDIO_DEBUG_LOG("AudioRendererWriteCallbackStub::OnWriteData");
    std::shared_ptr<AudioRendererWriteCallback> cb = callback_.lock();
    if (cb != nullptr) {
        AUDIO_DEBUG_LOG("AudioRendererWriteCallbackStub::OnWriteData CALLBACK NOT NULL");
        cb->OnWriteData(length);
    } else {
        AUDIO_DEBUG_LOG("AudioRendererWriteCallbackStub::OnWriteData CALLBACK NULL");
    }
}

void AudioRendererWriteCallbackStub::SetOnRendererWriteCallback(
    const std::weak_ptr<AudioRendererWriteCallback> &callback)
{
    AUDIO_DEBUG_LOG("AudioRendererWriteCallbackStub::SetOnRendererWriteCallback");
    callback_ = callback;
}
} // namespace AudioStandard
} // namespace OHOS