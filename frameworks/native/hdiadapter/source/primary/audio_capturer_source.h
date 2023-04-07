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

#ifndef AUDIO_CAPTURER_SOURCE_H
#define AUDIO_CAPTURER_SOURCE_H

#include "audio_info.h"
#include "audio_manager.h"
#include "i_audio_capturer_source.h"
#include "running_lock.h"

#include <cstdio>
#include <list>

namespace OHOS {
namespace AudioStandard {
#define AUDIO_CHANNELCOUNT 2
#define AUDIO_SAMPLE_RATE_48K 48000
#define DEEP_BUFFER_CAPTURE_PERIOD_SIZE 4096
#define INT_32_MAX 0x7fffffff
#define PERIOD_SIZE 1024
#define PATH_LEN 256
#define AUDIO_BUFF_SIZE (16 * 1024)
#define PCM_8_BIT 8
#define PCM_16_BIT 16
#define INTERNAL_INPUT_STREAM_ID 1

class AudioCapturerSource : public IAudioCapturerSource {
public:
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

    int32_t SetInputRoute(DeviceType deviceType, AudioPortPin &inputPortPin);

    int32_t SetInputRoute(DeviceType deviceType) override;

    uint64_t GetTransactionId() override;

    static AudioCapturerSource *GetInstance(void);
    bool capturerInited_;
    static bool micMuteState_;

private:
    const int32_t HALF_FACTOR = 2;
    const int32_t MAX_AUDIO_ADAPTER_NUM = 5;
    const float MAX_VOLUME_LEVEL = 15.0f;

    IAudioSourceAttr attr_;
    bool started_;
    bool paused_;
    float leftVolume_;
    float rightVolume_;

    int32_t routeHandle_ = -1;
    uint32_t openMic_;
    std::string adapterNameCase_;
    struct AudioManager *audioManager_;
    struct AudioAdapter *audioAdapter_;
    struct AudioCapture *audioCapture_;
    struct AudioPort audioPort;
    std::shared_ptr<PowerMgr::RunningLock> mKeepRunningLock;

    int32_t CreateCapture(struct AudioPort &capturePort);
    int32_t InitAudioManager();

#ifdef CAPTURE_DUMP
    FILE *pfd;
#endif

    AudioCapturerSource();
    ~AudioCapturerSource();
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif  // AUDIO_CAPTURER_SOURCE_H
