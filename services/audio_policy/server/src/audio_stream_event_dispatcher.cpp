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


#include "audio_stream_event_dispatcher.h"
#include "i_standard_capturer_state_change_listener.h"
#include "i_standard_renderer_state_change_listener.h"

namespace OHOS {
namespace AudioStandard {
AudioStreamEventDispatcher::AudioStreamEventDispatcher()
{
    AUDIO_DEBUG_LOG("AudioStreamEventDispatcher::AudioStreamEventDispatcher()");
}

AudioStreamEventDispatcher::~AudioStreamEventDispatcher()
{
    AUDIO_DEBUG_LOG("AudioStreamEventDispatcher::~AudioStreamEventDispatcher()");
}

void AudioStreamEventDispatcher::addRendererListener(int32_t clientPid,
    const std::shared_ptr<AudioRendererStateChangeCallback> &callback)
{
    std::lock_guard<std::mutex> lock(rendererStateChangeListnerMutex_);
    rendererCBMap_[clientPid] = callback;
    AUDIO_DEBUG_LOG("AudioStreamEventDispatcher::addRendererListener:client %{public}d added", clientPid);
}

void AudioStreamEventDispatcher::removeRendererListener(int32_t clientPid)
{
    std::lock_guard<std::mutex> lock(rendererStateChangeListnerMutex_);
    if (rendererCBMap_.erase(clientPid)) {
        AUDIO_INFO_LOG("AudioStreamEventDispatcher::removeRendererListener:client %{public}d done", clientPid);
        return;
    }
    AUDIO_DEBUG_LOG("AudioStreamEventDispatcher::removeRendererListener:client %{public}d not present", clientPid);
}

void AudioStreamEventDispatcher::addCapturerListener(int32_t clientPid,
    const std::shared_ptr<AudioCapturerStateChangeCallback> &callback)
{
    std::lock_guard<std::mutex> lock(capturerStateChangeListnerMutex_);
    capturerCBMap_[clientPid] = callback;
    AUDIO_DEBUG_LOG("AudioStreamEventDispatcher::addCapturerListener:client %{public}d added", clientPid);
}

void AudioStreamEventDispatcher::removeCapturerListener(int32_t clientPid)
{
    std::lock_guard<std::mutex> lock(capturerStateChangeListnerMutex_);
    if (capturerCBMap_.erase(clientPid)) {
        AUDIO_INFO_LOG("AudioStreamEventDispatcher::removeCapturerListener:client %{public}d done", clientPid);
        return;
    }
    AUDIO_DEBUG_LOG("AudioStreamEventDispatcher::removeCapturerListener:client %{public}d not present", clientPid);
}

void AudioStreamEventDispatcher::SendRendererInfoEventToDispatcher(AudioMode mode,
    std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos)
{
    AUDIO_DEBUG_LOG("AudioStreamEventDispatcher::SendRendererInfoEventToDispatcher:mode %{public}d ", mode);

    std::lock_guard<std::mutex> lock(streamStateChangeQueueMutex_);

    std::vector<std::unique_ptr<AudioRendererChangeInfo>> rendererChangeInfos;
    for (const auto &changeInfo : audioRendererChangeInfos) {
        rendererChangeInfos.push_back(std::make_unique<AudioRendererChangeInfo>(*changeInfo));
    }

    unique_ptr<StreamStateChangeRequest> streamStateChangeRequest = make_unique<StreamStateChangeRequest>();
    if (!streamStateChangeRequest) {
        AUDIO_ERR_LOG("AudioStreamEventDispatcher::SendRendererInfoEventToDispatcher:Memory alloc failed!!");
        return;
    }
    streamStateChangeRequest->mode = mode;
    streamStateChangeRequest->audioRendererChangeInfos = move(rendererChangeInfos);
    streamStateChangeQueue_.push(move(streamStateChangeRequest));
    DispatcherEvent();
}

void AudioStreamEventDispatcher::SendCapturerInfoEventToDispatcher(AudioMode mode,
    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    AUDIO_DEBUG_LOG("AudioStreamEventDispatcher::SendCapturerInfoEventToDispatcher:mode %{public}d ", mode);

    std::lock_guard<std::mutex> lock(streamStateChangeQueueMutex_);

    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> capturerChangeInfos;
    for (const auto &changeInfo : audioCapturerChangeInfos) {
        capturerChangeInfos.push_back(std::make_unique<AudioCapturerChangeInfo>(*changeInfo));
    }

    unique_ptr<StreamStateChangeRequest> streamStateChangeRequest = make_unique<StreamStateChangeRequest>();
    if (!streamStateChangeRequest) {
        AUDIO_ERR_LOG("AudioStreamEventDispatcher::Memory alloc failed!!");
        return;
    }
    streamStateChangeRequest->mode = mode;
    streamStateChangeRequest->audioCapturerChangeInfos = move(capturerChangeInfos);
    streamStateChangeQueue_.push(move(streamStateChangeRequest));
    DispatcherEvent();
}

void AudioStreamEventDispatcher::HandleRendererStreamStateChange(
    const unique_ptr<StreamStateChangeRequest> &streamStateChangeRequest)
{
    std::lock_guard<std::mutex> lock(rendererStateChangeListnerMutex_);
    for (auto it = rendererCBMap_.begin(); it != rendererCBMap_.end(); ++it) {
        std::shared_ptr<AudioRendererStateChangeCallback> rendererStateChangeCb = it->second;
        if (rendererStateChangeCb == nullptr) {
            AUDIO_ERR_LOG("rendererStateChangeCb : nullptr for client : %{public}d", it->first);
            it = rendererCBMap_.erase(it);
            continue;
        }
        rendererStateChangeCb->OnRendererStateChange(streamStateChangeRequest->audioRendererChangeInfos);
    }
}

void AudioStreamEventDispatcher::HandleCapturerStreamStateChange(
    const unique_ptr<StreamStateChangeRequest> &streamStateChangeRequest)
{
    std::lock_guard<std::mutex> lock(capturerStateChangeListnerMutex_);
    for (auto it = capturerCBMap_.begin(); it != capturerCBMap_.end(); ++it) {
        std::shared_ptr<AudioCapturerStateChangeCallback> capturerStateChangeCb = it->second;
        if (capturerStateChangeCb == nullptr) {
            AUDIO_ERR_LOG("capturerStateChangeCb : nullptr for client : %{public}d", it->first);
            it = capturerCBMap_.erase(it);
            continue;
        }
        capturerStateChangeCb->OnCapturerStateChange(streamStateChangeRequest->audioCapturerChangeInfos);
    }
}

void AudioStreamEventDispatcher::DispatcherEvent()
{
    AUDIO_DEBUG_LOG("DispatcherEvent entered");
    while (!streamStateChangeQueue_.empty()) {
        std::unique_ptr<StreamStateChangeRequest> streamStateChangeRequest =
            std::move(streamStateChangeQueue_.front());
        if (streamStateChangeRequest != nullptr) {
            if (streamStateChangeRequest->mode == AUDIO_MODE_PLAYBACK) {
                HandleRendererStreamStateChange(streamStateChangeRequest);
            } else {
                HandleCapturerStreamStateChange(streamStateChangeRequest);
            }
        }
        streamStateChangeQueue_.pop();
    }
}
} // namespace AudioStandard
} // namespace OHOS
