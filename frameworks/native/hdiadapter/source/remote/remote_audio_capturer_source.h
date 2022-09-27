/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License") override;
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

#ifndef REMOTE_AUDIO_CAPTURER_SOURCE_H
#define REMOTE_AUDIO_CAPTURER_SOURCE_H

#include "audio_info.h"
#include "audio_manager.h"
#include "i_audio_capturer_source.h"

#include <cstdio>
#include <list>
#include <map>

namespace OHOS {
namespace AudioStandard {
class RemoteAudioCapturerSource : public IAudioCapturerSource {
public:
    static RemoteAudioCapturerSource *GetInstance(std::string deviceNetworkId);

    int32_t Init(IAudioSourceAttr &atrr) override;
    bool IsInited(void) override;
    void DeInit(void) override;

    int32_t Start(void) override;
    int32_t Stop(void) override;
    int32_t Flush(void) override;
    int32_t Reset(void) override;
    int32_t Pause(void) override;
    int32_t Resume(void) override;
    int32_t CaptureFrame(char *frame, uint64_t requestBytes, uint64_t &replyBytes) override;
    int32_t SetVolume(float left, float right) override;
    int32_t GetVolume(float &left, float &right) override;
    int32_t SetMute(bool isMute) override;
    int32_t GetMute(bool &isMute) override;
    int32_t SetAudioScene(AudioScene audioScene, DeviceType activeDevice) override;
    int32_t SetInputRoute(DeviceType deviceType) override;
    uint64_t GetTransactionId() override;
private:
    static std::map<std::string, RemoteAudioCapturerSource *> allRemoteSources;

    const uint32_t maxInt32 = 0x7fffffff;
    const uint32_t audioBufferSize = 16 * 1024;
    const uint32_t internalInputStreamId = 1;
    const uint32_t deepBufferCapturePeriodSize = 4096;

    explicit RemoteAudioCapturerSource(std::string deviceNetworkId);
    ~RemoteAudioCapturerSource();

    IAudioSourceAttr attr_;
    std::string deviceNetworkId_;
    bool capturerInited_;
    bool started_;
    bool paused_;
    bool micMuteState_ = false;

    int32_t routeHandle_ = -1;
    struct AudioManager *audioManager_;
    struct AudioAdapter *audioAdapter_;
    struct AudioCapture *audioCapture_;
    struct AudioPort audioPort;

    int32_t CreateCapture(struct AudioPort &capturePort);
    int32_t InitAudioManager();
#ifdef DEBUG_CAPTURE_DUMP
    FILE *pfd;
#endif // DEBUG_CAPTURE_DUMP
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif  // AUDIO_CAPTURER_SOURCE_H
