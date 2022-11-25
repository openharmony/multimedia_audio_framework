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

#include <sys/time.h>
#include <utility>

#include <climits>
#include <cmath>
#include <cfloat>
#include "securec.h"
#include "audio_log.h"
#include "audio_policy_manager.h"
#include "tone_player_private.h"

namespace OHOS {
namespace AudioStandard {
namespace {
constexpr int32_t C20MS = 20;
constexpr int32_t C1000MS = 1000;
constexpr int32_t CMAXWAIT = 3;
constexpr int32_t CDOUBLE = 2;
constexpr int32_t AMPLITUDE = 16000;
constexpr int32_t BIT8 = 8;
}

#ifdef DUMPFILE
const char *g_tonePlayerTestFilePath = "/data/local/tmp/toneplayer_test.pcm";
#endif // DUMPFILE

std::shared_ptr<TonePlayer> TonePlayer::Create(const AudioRendererInfo &rendererInfo)
{
    return std::make_shared <TonePlayerPrivate>("", rendererInfo);
}

std::shared_ptr<TonePlayer> TonePlayer::Create(const std::string cachePath, const AudioRendererInfo &rendererInfo)
{
    return std::make_shared <TonePlayerPrivate>(cachePath, rendererInfo);
}

TonePlayerPrivate::TonePlayerPrivate(const std::string cachePath, const AudioRendererInfo &rendereInfo)
{
    tonePlayerState_ = TONE_PLAYER_IDLE;
    rendererOptions.streamInfo.encoding = AudioEncodingType::ENCODING_PCM;
    rendererOptions.streamInfo.samplingRate = SAMPLE_RATE_44100;
    rendererOptions.streamInfo.format = SAMPLE_S16LE;
    rendererOptions.streamInfo.channels = MONO;

    // contentType::CONTENT_TYPE_MUSIC;
    rendererOptions.rendererInfo.contentType = rendereInfo.contentType;

    // streamUsage::STREAM_USAGE_MEDIA;
    rendererOptions.rendererInfo.streamUsage = rendereInfo.streamUsage;
    rendererOptions.rendererInfo.rendererFlags = rendereInfo.rendererFlags;
    supportedTones_ = AudioPolicyManager::GetInstance().GetSupportedTones();
    volume_ = TRACK_VOLUME;
    toneInfo_ = NULL;
    initialToneInfo_ = NULL;
    toneDataGenLoop_ = nullptr;
    samplingRate_ = rendererOptions.streamInfo.samplingRate;
    if (!cachePath.empty()) {
        AUDIO_INFO_LOG("copy application cache path");
        cachePath_.assign(cachePath);
    }
    AUDIO_DEBUG_LOG("TonePlayerPrivate constructor: volume=%{public}f, size=%{public}zu",
        volume_, supportedTones_.size());
#ifdef DUMPFILE
    pfd_ = nullptr;
#endif // DUMPFILE
}

TonePlayer::~TonePlayer() = default;

TonePlayerPrivate::~TonePlayerPrivate()
{
    AUDIO_INFO_LOG("TonePlayerPrivate destructor");
    if (audioRenderer_ != nullptr) {
        StopTone();
        mutexLock_.lock();
        if (audioRenderer_ != nullptr) {
            audioRenderer_->Clear();
        } else {
            AUDIO_ERR_LOG("~TonePlayerPrivate audioRenderer_ is null");
        }
        mutexLock_.unlock();
    }
    if (toneDataGenLoop_ && toneDataGenLoop_->joinable()) {
        toneDataGenLoop_->join();
    }
    audioRenderer_ = nullptr;
}

bool TonePlayerPrivate::LoadEventStateHandler()
{
    AUDIO_INFO_LOG("TonePlayerPrivate::LoadEventStateHandler start");
    bool result = true;
    if (audioRenderer_ != nullptr) {
        result = StopTone();
        mutexLock_.lock();
        if (audioRenderer_ != nullptr) {
            if (audioRenderer_->Clear() != 0) {
                result = false;
            }
        } else {
            AUDIO_ERR_LOG("LoadEventStateHandler audioRenderer_ is null");
        }
        mutexLock_.unlock();
        tonePlayerState_ = TONE_PLAYER_IDLE;
        audioRenderer_ = nullptr;
    }
    if (tonePlayerState_ == TONE_PLAYER_IDLE) {
        AUDIO_DEBUG_LOG("Load Init AudioRenderer");
        if ((audioRenderer_ == nullptr) && (!InitAudioRenderer())) {
            AUDIO_ERR_LOG("LoadEventStateHandler InitAudioRenderer fail");
            return false;
        }
        tonePlayerState_ = TONE_PLAYER_INIT;
    }
    if (InitToneWaveInfo() == false) {
        AUDIO_ERR_LOG("Wave Initialization Failed");
        tonePlayerState_ = TONE_PLAYER_IDLE;
        return false;
    }
    tonePlayerState_ = TONE_PLAYER_STARTING;
    toneDataState_ = TONE_DATA_LOADING;
    if (toneDataGenLoop_ == nullptr) {
        toneDataGenLoop_ = std::make_unique<std::thread>(&TonePlayerPrivate::AudioToneDataThreadFunc, this);
    }
    return result;
}

bool TonePlayerPrivate::PlayEventStateHandler()
{
    bool result = true;
    status_t retStatus;
    mutexLock_.unlock();
    mutexLock_.lock();
    if (audioRenderer_ != nullptr) {
        result = audioRenderer_->Start();
    } else {
        AUDIO_ERR_LOG("PlayEventStateHandler audioRenderer_ is null");
    }
    mutexLock_.unlock();
    mutexLock_.lock();
    if (tonePlayerState_ == TONE_PLAYER_STARTING) {
        mutexLock_.unlock();
        std::unique_lock<std::mutex> lock(cbkCondLock_);
        retStatus = waitAudioCbkCond_.wait_for(lock, std::chrono::seconds(CMAXWAIT));
        AUDIO_DEBUG_LOG("Immediate start got notified, status %{public}d", retStatus);
        mutexLock_.lock();
        if (retStatus == std::cv_status::timeout) {
            AUDIO_ERR_LOG("Immediate start timed out, status %{public}d", retStatus);
            return false;
        }
    }
    return result;
}

bool TonePlayerPrivate::TonePlayerStateHandler(int16_t event)
{
    AUDIO_INFO_LOG("TonePlayerPrivate HandlePlayerState");
    switch (event) {
        case PLAYER_EVENT_LOAD:
            if (LoadEventStateHandler() == false) {
                return false;
            }
            break;
        case PLAYER_EVENT_PLAY:
            if (PlayEventStateHandler() == false) {
                tonePlayerState_ = TONE_PLAYER_IDLE;
                return false;
            }
            AUDIO_DEBUG_LOG("PLAYER_EVENT tonePlayerState_ %{public}d", tonePlayerState_);
            break;
        case PLAYER_EVENT_STOP:
            StopEventStateHandler();
            break;
    }
    return true;
}

bool TonePlayerPrivate::LoadTone(ToneType toneType)
{
    AUDIO_INFO_LOG("LoadTone type: %{public}d, tonePlayerState_ %{public}d", toneType, tonePlayerState_);
    bool result = false;
    if (toneType >= NUM_TONES) {
        return result;
    }
    mutexLock_.lock();
    if (tonePlayerState_ != TONE_PLAYER_IDLE && tonePlayerState_ != TONE_PLAYER_INIT) {
        mutexLock_.unlock();
        return result;
    }

    if (std::find(supportedTones_.begin(), supportedTones_.end(), (int32_t)toneType) == supportedTones_.end()) {
        mutexLock_.unlock();
        return result;
    }

    // Get descriptor for requested tone
    initialToneInfo_ = AudioPolicyManager::GetInstance().GetToneConfig(toneType);
    if (initialToneInfo_->segmentCnt == 0) {
        AUDIO_ERR_LOG("LoadTone failed, calling GetToneConfig returned invalid");
        mutexLock_.unlock();
        return result;
    }
    result = TonePlayerStateHandler(PLAYER_EVENT_LOAD);
    mutexLock_.unlock();
#ifdef DUMPFILE
    pfd_ = fopen(g_tonePlayerTestFilePath, "wb+");
    if (pfd_ == nullptr) {
        AUDIO_ERR_LOG("Error opening pcm test file!");
    }
#endif // DUMPFILE
    return result;
}

bool TonePlayerPrivate::StartTone()
{
    AUDIO_INFO_LOG("STARTTONE tonePlayerState_ %{public}d", tonePlayerState_);
    bool retVal = false;
    mutexLock_.lock();
    if (tonePlayerState_ == TONE_PLAYER_IDLE || tonePlayerState_ == TONE_PLAYER_INIT) {
        if (LoadEventStateHandler() == false) {
            mutexLock_.unlock();
            return false;
        }
    }
    retVal = TonePlayerStateHandler(PLAYER_EVENT_PLAY);
    mutexLock_.unlock();
    return retVal;
}

bool TonePlayerPrivate::Release()
{
    bool retVal = true;
    if (audioRenderer_ != nullptr) {
        retVal = StopTone();
        mutexLock_.lock();
        if (audioRenderer_ != nullptr) {
            if (audioRenderer_->Clear() != 0) {
                retVal = false;
            }
        } else {
            AUDIO_ERR_LOG("Release audioRenderer_ is null");
        }
        mutexLock_.unlock();
        tonePlayerState_ = TONE_PLAYER_IDLE;
    }
    audioRenderer_ = nullptr;
    return retVal;
}

bool TonePlayerPrivate::StopTone()
{
    AUDIO_INFO_LOG("StopTone tonePlayerState_ %{public}d", tonePlayerState_);
    bool retVal = true;
    mutexLock_.lock();
    if (tonePlayerState_ == TONE_PLAYER_IDLE || tonePlayerState_ == TONE_PLAYER_INIT) {
        AUDIO_INFO_LOG("-stop tone End");
        mutexLock_.unlock();
        return retVal;
    }

    retVal = TonePlayerStateHandler(PLAYER_EVENT_STOP);
    mutexLock_.unlock();
    mutexLock_.lock();
    if (audioRenderer_ != nullptr) {
        retVal = audioRenderer_->Stop();
    } else {
        AUDIO_ERR_LOG("StopTone audioRenderer_ is null");
    }
    mutexLock_.unlock();
#ifdef DUMPFILE
    if (pfd_) {
        fclose(pfd_);
        pfd_ = nullptr;
    }
#endif // DUMPFILE
    return retVal;
}

void TonePlayerPrivate::StopEventStateHandler()
{
    if (tonePlayerState_ == TONE_PLAYER_PLAYING || tonePlayerState_ == TONE_PLAYER_STARTING) {
        tonePlayerState_ = TONE_PLAYER_STOPPING;
    }
    AUDIO_DEBUG_LOG("WAITING wait_for cond");
    mutexLock_.unlock();
    std::unique_lock<std::mutex> lock(cbkCondLock_);
    status_t retStatus = waitAudioCbkCond_.wait_for(lock, std::chrono::seconds(CMAXWAIT));
    mutexLock_.lock();
    if (retStatus != std::cv_status::timeout) {
        AUDIO_ERR_LOG("StopTone waiting wait_for cond got notified");
        tonePlayerState_ = TONE_PLAYER_INIT;
    } else {
        AUDIO_ERR_LOG("Stop timed out");
        tonePlayerState_ = TONE_PLAYER_IDLE;
    }
}

bool TonePlayerPrivate::InitAudioRenderer()
{
    processSize_ = (rendererOptions.streamInfo.samplingRate * C20MS) / C1000MS;
    if (!cachePath_.empty()) {
        audioRenderer_ = AudioRenderer::Create(cachePath_, rendererOptions);
    } else {
        audioRenderer_ = AudioRenderer::Create(rendererOptions);
    }
    if (audioRenderer_ == nullptr) {
        AUDIO_ERR_LOG("initAudioRenderer: Renderer create failed");
        return false;
    }

    AUDIO_DEBUG_LOG("initAudioRenderer: Playback renderer created");
    if (audioRenderer_->SetRenderMode(RENDER_MODE_CALLBACK)) {
        AUDIO_ERR_LOG("initAudioRenderer: SetRenderMode failed");
        return false;
    }
    AUDIO_DEBUG_LOG("initAudioRenderer: SetRenderMode Sucessful");
    if (audioRenderer_->SetRendererWriteCallback(shared_from_this())) {
        AUDIO_ERR_LOG("initAudioRenderer: SetRendererWriteCallback failed");
        return false;
    }
    AUDIO_DEBUG_LOG("initAudioRenderer: SetRendererWriteCallback Sucessful");
    if (audioRenderer_->SetRendererCallback(shared_from_this())) {
        AUDIO_ERR_LOG("initAudioRenderer: SetRendererCallbackfailed");
        return false;
    }

    AUDIO_DEBUG_LOG("initAudioRenderer: SetRendererCallback Sucessful");
    audioRenderer_->SetVolume(volume_);
    AUDIO_INFO_LOG("initAudioRenderer: SetVolume Sucessful");
    return true;
}

void TonePlayerPrivate::OnInterrupt(const InterruptEvent &interruptEvent)
{
    AUDIO_INFO_LOG("OnInterrupt eventType: %{public}d", interruptEvent.eventType);
}

void TonePlayerPrivate::OnWriteData(size_t length)
{
    AUDIO_INFO_LOG("OnWriteData Cbk: %{public}zu", length);
    AudioToneRendererCallback();
}

void TonePlayerPrivate::OnStateChange(const RendererState state,
    const StateChangeCmdType __attribute__((unused)) cmdType)
{
    AUDIO_INFO_LOG("OnStateChange Cbk: %{public}d not calling", state);
    if (state == RENDERER_RUNNING || state == RENDERER_STOPPED) {
        AUDIO_INFO_LOG("OnStateChange state: %{public}d", state);
    }
}

bool TonePlayerPrivate::CheckToneStopped()
{
    AUDIO_INFO_LOG("CheckToneStopped state: %{public}d tot: %{public}d max: %{public}d not calling",
        tonePlayerState_, totalSample_, maxSample_);
    std::shared_ptr<ToneInfo> toneDesc = toneInfo_;
    if (tonePlayerState_ == TONE_PLAYER_STOPPED) {
        return true;
    }
    if (toneDesc->segments[currSegment_].duration == 0 ||
        totalSample_ > maxSample_ || tonePlayerState_ == TONE_PLAYER_STOPPING) {
        if (tonePlayerState_ == TONE_PLAYER_PLAYING) {
            tonePlayerState_ = TONE_PLAYER_STOPPING;
            AUDIO_DEBUG_LOG("Audicallback move playing to stoping");
        }
        return true;
    }
    return false;
}

bool TonePlayerPrivate::CheckToneStarted(uint32_t reqSample, int8_t *audioBuffer)
{
    std::shared_ptr<ToneInfo> toneDesc = toneInfo_;
    if (tonePlayerState_ == TONE_PLAYER_STARTING) {
        tonePlayerState_ = TONE_PLAYER_PLAYING;
        AUDIO_DEBUG_LOG("Audicallback move to playing");
        if (toneDesc->segments[currSegment_].duration != 0) {
            sampleCount_ = 0;
            GetSamples(toneDesc->segments[currSegment_].waveFreq, audioBuffer, reqSample);
        }
        AUDIO_DEBUG_LOG("CheckToneStarted GenerateStartWave to currseg: %{public}d ", currSegment_);
        return true;
    }
    return false;
}

void TonePlayerPrivate::GetCurrentSegmentUpdated(std::shared_ptr<ToneInfo> toneDesc)
{
    if (toneDesc->segments[currSegment_].loopCnt) {
        if (loopCounter_ < toneDesc->segments[currSegment_].loopCnt) {
            currSegment_ = toneDesc->segments[currSegment_].loopIndx;
            ++loopCounter_;
        } else {
            // completed loop. go to next segment
            loopCounter_ = 0;
            currSegment_++;
        }
    } else {
        // no looping required , go to next segment
        currSegment_++;
    }
    AUDIO_INFO_LOG("GetCurrentSegmentUpdated loopCounter_: %{public}d, currSegment_: %{public}d",
        loopCounter_, currSegment_);
}

bool TonePlayerPrivate::CheckToneContinuity()
{
    AUDIO_INFO_LOG("CheckToneContinuity Entry loopCounter_: %{public}d, currSegment_: %{public}d",
        loopCounter_, currSegment_);
    bool retVal = false;
    std::shared_ptr<ToneInfo> toneDesc = toneInfo_;
    GetCurrentSegmentUpdated(toneDesc);

    // Handle loop if last segment reached
    if (toneDesc->segments[currSegment_].duration == 0) {
        AUDIO_DEBUG_LOG("Last Seg: %{public}d", currSegment_);
        if (currCount_ < toneDesc->repeatCnt) {
            currSegment_ = toneDesc->repeatSegment;
            ++currCount_;
            retVal = true;
        } else {
            retVal = false;
        }
    } else {
        retVal = true;
    }
    AUDIO_INFO_LOG("CheckToneContinuity End loopCounter_: %{public}d, currSegment_: %{public}d currCount_: %{public}d",
        loopCounter_, currSegment_, currCount_);
    return retVal;
}

bool TonePlayerPrivate::ContinueToneplay(uint32_t reqSample, int8_t *audioBuffer)
{
    std::shared_ptr<ToneInfo> toneDesc = toneInfo_;
    if (tonePlayerState_ != TONE_PLAYER_PLAYING) {
        return false;
    }
    if (totalSample_ <= nextSegSample_) {
        if (toneDesc->segments[currSegment_].duration != 0) {
            GetSamples(toneDesc->segments[currSegment_].waveFreq, audioBuffer, reqSample);
        }
        return true;
    }
    AUDIO_DEBUG_LOG(" Current Seg Last minute Playing Tone");

    if (CheckToneContinuity()) {
        if (toneDesc->segments[currSegment_].duration != 0) {
            sampleCount_ = 0;
            GetSamples(toneDesc->segments[currSegment_].waveFreq, audioBuffer, reqSample);
        }
    }
    nextSegSample_
        += (toneDesc->segments[currSegment_].duration * samplingRate_) / C1000MS;
    AUDIO_INFO_LOG("ContinueToneplay nextSegSample_: %{public}d", nextSegSample_);
    return true;
}

void TonePlayerPrivate::AudioToneRendererCallback()
{
    std::unique_lock<std::mutex> lock(dataCondLock_);
    if (toneDataState_ == TONE_DATA_LOADED) {
        AUDIO_DEBUG_LOG("Notifing Data AudioToneRendererCallback");
        waitToneDataCond_.notify_all();
    }
    if (toneDataState_ == TONE_DATA_LOADING) {
        toneDataState_ = TONE_DATA_REQUESTED;
        waitToneDataCond_.wait_for(lock, std::chrono::seconds(CMAXWAIT));
    }
    AUDIO_INFO_LOG("AudioToneRendererCallback Exited");
}

bool TonePlayerPrivate::AudioToneSequenceGen(BufferDesc &bufDesc)
{
    int8_t *audioBuffer = (int8_t *)bufDesc.buffer;
    uint32_t totalBufAvailable = bufDesc.bufLength / sizeof(int16_t);
    bool retVal = true;
    while (totalBufAvailable) {
        uint32_t reqSamples = totalBufAvailable < processSize_ * CDOUBLE ? totalBufAvailable : processSize_;
        bool lSignal = false;
        AUDIO_INFO_LOG("AudioToneDataThreadFunc, lReqSmp: %{public}d totalBufAvailable: %{public}d",
            reqSamples, totalBufAvailable);
        mutexLock_.lock();

        // Update pcm frame count and end time (current time at the end of this process)
        totalSample_ += reqSamples;
        if (CheckToneStopped()) {
            if (tonePlayerState_ == TONE_PLAYER_STOPPING) {
                tonePlayerState_ = TONE_PLAYER_STOPPED;
                mutexLock_.unlock();
                break;
            }
            if (tonePlayerState_ == TONE_PLAYER_STOPPED) {
                tonePlayerState_ = TONE_PLAYER_INIT;
                totalBufAvailable = 0;
                bufDesc.dataLength = 0;
                AUDIO_DEBUG_LOG("Notifing all the STOP");
                mutexLock_.unlock();
                waitAudioCbkCond_.notify_all();
                mutexLock_.lock();
                if (audioRenderer_ != nullptr) {
                    retVal = audioRenderer_->Stop();
                } else {
                    AUDIO_ERR_LOG("AudioToneSequenceGen audioRenderer_ is null");
                }
                mutexLock_.unlock();
                return false;
            }
        } else if (CheckToneStarted(reqSamples, audioBuffer)) {
            bufDesc.dataLength += reqSamples * sizeof(int16_t);
            lSignal = true;
        } else {
            if (ContinueToneplay(reqSamples, audioBuffer)) {
                bufDesc.dataLength += reqSamples * sizeof(int16_t);
            }
        }
        totalBufAvailable -= reqSamples;
        audioBuffer += reqSamples * sizeof(int16_t);
        mutexLock_.unlock();
        if (lSignal) {
            AUDIO_INFO_LOG("Notifing all the start");
            waitAudioCbkCond_.notify_all();
        }
    }
    return retVal;
}

void TonePlayerPrivate::AudioToneDataThreadFunc()
{
    while (true) {
        std::unique_lock<std::mutex> lock(dataCondLock_);
        if (toneDataState_ == TONE_DATA_LOADING) {
            toneDataState_ = TONE_DATA_LOADED;
            waitToneDataCond_.wait_for(lock, std::chrono::seconds(CMAXWAIT));
        }

        BufferDesc bufDesc = {};
        mutexLock_.lock();
        if (audioRenderer_ != nullptr) {
            audioRenderer_->GetBufferDesc(bufDesc);
        } else {
            AUDIO_ERR_LOG("AudioToneDataThreadFunc audioRenderer_ is null");
        }
        mutexLock_.unlock();
        std::shared_ptr<ToneInfo> lpToneDesc = toneInfo_;
        bufDesc.dataLength = 0;
        AUDIO_DEBUG_LOG("AudioToneDataThreadFunc, tonePlayerState_: %{public}d", tonePlayerState_);
        if (bufDesc.bufLength == 0) {
            AUDIO_DEBUG_LOG("notify bufDesc bufLength is 0");
            waitAudioCbkCond_.notify_all();
            return;
        }

        // Clear output buffer: WaveGenerator accumulates into audioBuffer buffer
        memset_s(bufDesc.buffer, bufDesc.bufLength, 0, bufDesc.bufLength);
        if (AudioToneSequenceGen(bufDesc) == false) {
            return;
        }
        AUDIO_INFO_LOG("Exiting the AudioToneDataThreadFunc: %{public}zu,tonePlayerState_: %{public}d",
            bufDesc.dataLength, tonePlayerState_);
        if (bufDesc.dataLength) {
#ifdef DUMPFILE
            size_t writeResult = fwrite((void*)bufDesc.buffer, 1, bufDesc.dataLength, pfd_);
            if (writeResult != bufDesc.dataLength) {
                AUDIO_ERR_LOG("Failed to write the file.");
            }
#endif // DUMPFILE
            mutexLock_.lock();
            if (audioRenderer_ != nullptr) {
                audioRenderer_->Enqueue(bufDesc);
            } else {
                AUDIO_ERR_LOG("AudioToneDataThreadFunc Enqueue audioRenderer_ is null");
            }
            mutexLock_.unlock();
            if (toneDataState_ == TONE_DATA_REQUESTED) {
                AUDIO_INFO_LOG("Notifing Data AudioToneDataThreadFunc");
                waitToneDataCond_.notify_all();
            }
            toneDataState_ = TONE_DATA_LOADING;
            mutexLock_.lock();
            if (tonePlayerState_ == TONE_PLAYER_STOPPED) {
                AUDIO_INFO_LOG("Notifing tone player STOP");
                tonePlayerState_ = TONE_PLAYER_INIT;
                mutexLock_.unlock();
                waitAudioCbkCond_.notify_all();
                return;
            }
            mutexLock_.unlock();
        }
    }
}

bool TonePlayerPrivate::InitToneWaveInfo()
{
    AUDIO_INFO_LOG("InitToneWaveInfo, Entered");
    if (initialToneInfo_ == NULL) {
        return false;
    }
    toneInfo_ = initialToneInfo_;
    maxSample_ = TONEINFO_INF;

    // Initialize tone sequencer
    totalSample_ = 0;
    currSegment_ = 0;
    currCount_ = 0;
    loopCounter_ = 0;
    if (toneInfo_->segments[0].duration == TONEINFO_INF) {
        nextSegSample_ = TONEINFO_INF;
    } else {
        nextSegSample_ = (toneInfo_->segments[0].duration * samplingRate_) / C1000MS;
    }
    AUDIO_INFO_LOG("Prepare wave, nextSegSample_: %{public}d", nextSegSample_);
    return true;
}

int32_t TonePlayerPrivate::GetSamples(uint16_t *freqs, int8_t *buffer, uint32_t reqSamples)
{
    uint32_t index;
    uint8_t *data;
    uint16_t freqVal;
    float pi = 3.1428f;
    for (uint32_t i = 0; i <= TONEINFO_MAX_WAVES; i++) {
        if (freqs[i] == 0) {
            break;
        }
        freqVal = freqs[i];
        AUDIO_INFO_LOG("GetSamples Freq: %{public}d sampleCount_: %{public}d", freqVal, sampleCount_);
        index = sampleCount_;
        data = (uint8_t*)buffer;
        double factor = freqVal * 2 * pi / samplingRate_; // 2 is a parameter in the sine wave formula
        for (uint32_t idx = 0; idx < reqSamples; idx++) {
            int16_t sample = AMPLITUDE * sin(factor * index);
            uint32_t result;
            if (i == 0) {
                result = (sample & 0xFF);
                *data = result & 0xFF;
                data++;
                *data = ((sample & 0xFF00) >> BIT8);
                data++;
            } else {
                result = *data + (sample & 0xFF);
                *data = result & 0xFF;
                data++;
                *data += (result >> BIT8) + ((sample & 0xFF00) >> BIT8);
                data++;
            }
            index++;
        }
    }
    sampleCount_ += reqSamples;
    return 0;
}
} // end namespace AudioStandard
} // end OHOS
