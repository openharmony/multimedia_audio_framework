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

#ifndef AUDIO_TONEPLAYER_PRIVATE_H_
#define AUDIO_TONEPLAYER_PRIVATE_H_

#include <map>
#include <thread>
#include "tone_player.h"
#include "audio_renderer.h"

namespace OHOS {
namespace AudioStandard {
class TonePlayerPrivate : public AudioRendererWriteCallback, public AudioRendererCallback, public TonePlayer,
    public std::enable_shared_from_this<TonePlayerPrivate> {
public:
    TonePlayerPrivate(const std::string cachePath, const AudioRendererInfo &rendererInfo);
    ~TonePlayerPrivate();
    bool LoadTone(ToneType toneType) override;
    bool StartTone() override;
    bool StopTone() override;
    bool Release() override;
    void OnInterrupt(const InterruptEvent &interruptEvent) override;
    void OnStateChange(const RendererState state, const StateChangeCmdType __attribute__((unused)) cmdType) override;
    void OnWriteData(size_t length) override;

private:
    static constexpr float TRACK_VOLUME = 0.5f;

    enum tone_player_state {
        TONE_PLAYER_IDLE,  // TonePlayer is being initialized or initialization failed
        TONE_PLAYER_INIT,  // TonePlayer has been successfully initialized and is not playing
        TONE_PLAYER_STARTING,  // TonePlayer is starting playing
        TONE_PLAYER_PLAYING,  // TonePlayer is playing tone
        TONE_PLAYER_STOPPING,  // TonePlayer is the last End phase
        TONE_PLAYER_STOPPED,  // The Audiorenerer will be stopped
    };

    enum tone_player_event {
        PLAYER_EVENT_LOAD,  // Event used to load the tone
        PLAYER_EVENT_PLAY,  // TonePlayer is starting playing
        PLAYER_EVENT_STOP,  // Event used to playing tone
    };

    enum tone_data_state {
        TONE_DATA_LOADING,  // STATE when tone data is preparing
        TONE_DATA_LOADED,  // STATE when thread filled the data gone for waiting
        TONE_DATA_REQUESTED,  // STATE when AudioRender waiting for the data
    };
    typedef std::cv_status status_t;
    AudioRendererOptions rendererOptions = {};
    uint32_t currSegment_;  // Current segment index in ToneDescriptor segments[]
    uint32_t currCount_;  // Current sequence repeat count
    std::shared_ptr<ToneInfo> toneInfo_;  // pointer to active tone Info
    std::shared_ptr<ToneInfo> initialToneInfo_;  // pointer to new active tone Info
    std::vector<int32_t> supportedTones_;
    volatile uint16_t tonePlayerState_;  // TonePlayer state (tone_state)
    std::string cachePath_; // NAPI interface to create AudioRenderer
    uint32_t loopCounter_; // Current tone loopback count
    uint32_t totalSample_;  // Total no. of tone samples played
    uint32_t nextSegSample_;  // Position of next segment transition expressed in samples
    uint32_t maxSample_;  // Maximum number of audio samples played (maximun tone duration)
    uint32_t samplingRate_;  // AudioFlinger Sampling rate
    uint32_t sampleCount_; // Initial value should be zero before any new Tone renderering
    std::unique_ptr<AudioRenderer> audioRenderer_;  // Pointer to AudioRenderer used for playback
    std::mutex mutexLock_; // Mutex to control concurent access
    std::mutex cbkCondLock_; // Mutex associated to waitAudioCbkCond_
    std::condition_variable waitAudioCbkCond_; // condition enabling interface
    std::mutex dataCondLock_; // Mutex associated to waitToneDataCond_
    std::condition_variable waitToneDataCond_; // condition enabling interface
    tone_data_state toneDataState_;
    // to wait for audio rendere callback completion after a change is requested
    float volume_;  // Volume applied to audio Renderer
#ifdef DUMPFILE
    FILE *pfd_;
#endif // DUMPFILE
    uint32_t processSize_;  // In audioRenderer, Size of audio blocks generated at a time
    bool InitAudioRenderer();
    void AudioToneRendererCallback();
    void AudioToneDataThreadFunc();
    bool InitToneWaveInfo();
    bool TonePlayerStateHandler(int16_t event);
    int32_t GetSamples(uint16_t *freqs, int8_t *buffer, uint32_t samples);
    bool LoadEventStateHandler();
    bool PlayEventStateHandler();
    void StopEventStateHandler();
    bool ContinueToneplay(uint32_t sampleCnt, int8_t *audioBuf);
    bool CheckToneStarted(uint32_t sampleCnt, int8_t *audioBuf);
    bool CheckToneStopped();
    void GetCurrentSegmentUpdated(std::shared_ptr<ToneInfo> toneDesc);
    bool CheckToneContinuity ();
    bool AudioToneSequenceGen(BufferDesc &bufDesc);
    std::unique_ptr<std::thread> toneDataGenLoop_ = nullptr;
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* AUDIO_TONEPLAYER_H_ */
