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

#ifndef AUDIO_STREAM_EVENT_DISPATCHER_INFO_H
#define AUDIO_STREAM_EVENT_DISPATCHER_INFO_H

#include <cstdlib>
#include <map>
#include <queue>
#include <stdio.h>
#include <thread>
#include <unordered_map>
#include <vector>
#include "audio_log.h"
#include "audio_stream_manager.h"
#include "iremote_proxy.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;
struct StreamStateChangeRequest {
    AudioMode mode;
    std::vector<std::unique_ptr<AudioRendererChangeInfo>> audioRendererChangeInfos;
    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
};

class AudioStreamEventDispatcher {
public:
    static AudioStreamEventDispatcher& GetAudioStreamEventDispatcher()
    {
        static AudioStreamEventDispatcher audioStreamEventDispatcher;
        return audioStreamEventDispatcher;
    }

    AudioStreamEventDispatcher();
    ~AudioStreamEventDispatcher();

    void addRendererListener(int32_t clientPid,
                                const std::shared_ptr<AudioRendererStateChangeCallback> &callback);
    void removeRendererListener(int32_t clientPid);
    void addCapturerListener(int32_t clientPid,
                                const std::shared_ptr<AudioCapturerStateChangeCallback> &callback);
    void removeCapturerListener(int32_t clientPid);
    void SendRendererInfoEventToDispatcher(AudioMode mode,
        std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos);
    void SendCapturerInfoEventToDispatcher(AudioMode mode,
        std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos);
    void HandleCapturerStreamStateChange(const unique_ptr<StreamStateChangeRequest> &streamStateChangeRequest);
    void HandleRendererStreamStateChange(const unique_ptr<StreamStateChangeRequest> &streamStateChangeRequest);
    void DispatcherEvent();

private:
    std::mutex rendererStateChangeListnerMutex_;
    std::mutex capturerStateChangeListnerMutex_;
    std::mutex streamStateChangeQueueMutex_;
    std::unordered_map<int32_t, std::shared_ptr<AudioRendererStateChangeCallback>> rendererCBMap_;
    std::unordered_map<int32_t, std::shared_ptr<AudioCapturerStateChangeCallback>> capturerCBMap_;
    std::queue<unique_ptr<StreamStateChangeRequest>> streamStateChangeQueue_;
    std::unique_ptr<StreamStateChangeRequest> request;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_STREAM_EVENT_DISPATCHER_INFO_H