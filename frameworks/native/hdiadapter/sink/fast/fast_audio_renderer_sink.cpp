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

#include "fast_audio_renderer_sink.h"

#include <cstring>
#include <dlfcn.h>
#include <string>
#include <cinttypes>
#include <sys/mman.h>
#include <unistd.h>

#include "securec.h"

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_utils.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
namespace {
const int32_t HALF_FACTOR = 2;
const int32_t MAX_AUDIO_ADAPTER_NUM = 5;
const float DEFAULT_VOLUME_LEVEL = 1.0f;
const uint32_t AUDIO_CHANNELCOUNT = 2;
const uint32_t AUDIO_SAMPLE_RATE_48K = 48000;
const uint32_t DEEP_BUFFER_RENDER_PERIOD_SIZE = 3840;
const uint32_t INT_32_MAX = 0x7fffffff;
const uint32_t PCM_8_BIT = 8;
const uint32_t PCM_16_BIT = 16;
const uint32_t PCM_24_BIT = 24;
const uint32_t PCM_32_BIT = 32;
const uint32_t INTERNAL_OUTPUT_STREAM_ID = 0;
const int64_t SECOND_TO_NANOSECOND = 1000000000;
}
#ifdef DUMPFILE
const char *g_audioOutTestFilePath = "/data/local/tmp/fast_audio_dump.pcm";
#endif // DUMPFILE

FastAudioRendererSink::FastAudioRendererSink()
    : rendererInited_(false), started_(false), paused_(false), leftVolume_(DEFAULT_VOLUME_LEVEL),
      rightVolume_(DEFAULT_VOLUME_LEVEL), audioManager_(nullptr), audioAdapter_(nullptr),
      audioRender_(nullptr)
{
    attr_ = {};
#ifdef DUMPFILE
    pfd = nullptr;
#endif // DUMPFILE
}

FastAudioRendererSink::~FastAudioRendererSink()
{
    FastAudioRendererSink::DeInit();
}

FastAudioRendererSink *FastAudioRendererSink::GetInstance()
{
    static FastAudioRendererSink audioRenderer_;

    return &audioRenderer_;
}

void FastAudioRendererSink::DeInit()
{
    started_ = false;
    rendererInited_ = false;
    if ((audioRender_ != nullptr) && (audioAdapter_ != nullptr)) {
        audioAdapter_->DestroyRender(audioAdapter_, audioRender_);
    }
    audioRender_ = nullptr;

    if ((audioManager_ != nullptr) && (audioAdapter_ != nullptr)) {
        if (routeHandle_ != -1) {
            audioAdapter_->ReleaseAudioRoute(audioAdapter_, routeHandle_);
        }
        audioManager_->UnloadAdapter(audioManager_, audioAdapter_);
    }
    audioAdapter_ = nullptr;
    audioManager_ = nullptr;

    ReleaseMmapBuffer();
#ifdef DUMPFILE
    if (pfd) {
        fclose(pfd);
        pfd = nullptr;
    }
#endif // DUMPFILE
}

void InitAttrs(struct AudioSampleAttributes &attrs)
{
    /* Initialization of audio parameters for playback */
    attrs.channelCount = AUDIO_CHANNELCOUNT;
    attrs.sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs.interleaved = true;
    attrs.streamId = INTERNAL_OUTPUT_STREAM_ID;
    attrs.type = AUDIO_MMAP_NOIRQ; // enable mmap!
    attrs.period = DEEP_BUFFER_RENDER_PERIOD_SIZE;
    attrs.isBigEndian = false;
    attrs.isSignedData = true;
    attrs.stopThreshold = INT_32_MAX;
    attrs.silenceThreshold = 0;
}

static int32_t SwitchAdapterRender(struct AudioAdapterDescriptor *descs, string adapterNameCase,
    enum AudioPortDirection portFlag, struct AudioPort &renderPort, int32_t size)
{
    if (descs == nullptr) {
        return ERROR;
    }

    for (int32_t index = 0; index < size; index++) {
        struct AudioAdapterDescriptor *desc = &descs[index];
        if (desc == nullptr || desc->adapterName == nullptr) {
            continue;
        }
        if (!strcmp(desc->adapterName, adapterNameCase.c_str())) {
            for (uint32_t port = 0; port < desc->portNum; port++) {
                // Only find out the port of out in the sound card
                if (desc->ports[port].dir == portFlag) {
                    renderPort = desc->ports[port];
                    return index;
                }
            }
        }
    }
    AUDIO_ERR_LOG("SwitchAdapterRender Fail");

    return ERR_INVALID_INDEX;
}

int32_t FastAudioRendererSink::InitAudioManager()
{
    AUDIO_INFO_LOG("FastAudioRendererSink: Initialize audio proxy manager");

    audioManager_ = GetAudioManagerFuncs();
    if (audioManager_ == nullptr) {
        return ERR_INVALID_HANDLE;
    }

    return 0;
}

uint32_t PcmFormatToBits(enum AudioFormat format)
{
    switch (format) {
        case AUDIO_FORMAT_PCM_8_BIT:
            return PCM_8_BIT;
        case AUDIO_FORMAT_PCM_16_BIT:
            return PCM_16_BIT;
        case AUDIO_FORMAT_PCM_24_BIT:
            return PCM_24_BIT;
        case AUDIO_FORMAT_PCM_32_BIT:
            return PCM_32_BIT;
        default:
            return PCM_24_BIT;
    }
}

int32_t FastAudioRendererSink::GetMmapHandlePosition(uint64_t &frames, int64_t &timeSec, int64_t &timeNanoSec)
{
    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("Audio render is null!");
        return ERR_INVALID_HANDLE;
    }

    struct AudioTimeStamp timestamp = {};
    int32_t ret = audioRender_->attr.GetMmapPosition((AudioHandle)audioRender_, &frames, &timestamp);
    if (ret != 0) {
        AUDIO_ERR_LOG("Hdi GetMmapPosition filed, ret:%{public}d!", ret);
        return ERR_OPERATION_FAILED;
    }
    alreadyReadFrames_ = frames; // frames already read.
    curReadPos_ = frameSizeInByte_ * (frames - bufferTotalFrameSize_ * (frames / bufferTotalFrameSize_));
    CHECK_AND_RETURN_RET_LOG((curReadPos_ >= 0 && curReadPos_ < bufferSize_), ERR_INVALID_PARAM, "curReadPos invalid");

    int64_t maxSec = 9223372036; // (9223372036 + 1) * 10^9 > INT64_MAX, seconds should not bigger than it.
    if (timestamp.tvSec < 0 || timestamp.tvSec > maxSec || timestamp.tvNSec < 0 ||
        timestamp.tvNSec > SECOND_TO_NANOSECOND) {
        AUDIO_ERR_LOG("Hdi GetMmapPosition get invaild second:%{public}" PRId64 " or nanosecond:%{public}" PRId64 " !",
            timestamp.tvSec, timestamp.tvNSec);
        return ERR_OPERATION_FAILED;
    }
    timeSec = timestamp.tvSec;
    timeNanoSec = timestamp.tvNSec;

    AUDIO_DEBUG_LOG("GetMmapHandlePosition frames[:%{public}" PRIu64 "] tvsec:%{public}" PRId64 " tvNSec:"
        "%{public}" PRId64 " alreadyReadFrames:%{public}" PRId64 " curReadPos[%{public}d]",
        frames, timeSec, timeNanoSec, alreadyReadFrames_, curReadPos_);

    return SUCCESS;
}

void FastAudioRendererSink::ReleaseMmapBuffer()
{
    if (bufferAddresss_ != nullptr) {
        munmap(bufferAddresss_, bufferSize_);
        bufferAddresss_ = nullptr;
        bufferSize_ = 0;
        AUDIO_INFO_LOG("ReleaseMmapBuffer end.");
    } else {
        AUDIO_WARNING_LOG("ReleaseMmapBuffer buffer already null.");
    }
}

int32_t FastAudioRendererSink::PrepareMmapBuffer()
{
    int32_t totalBifferInMs = 50; // 5 * (6 + 2 * (2)) = 50ms, the buffer size, not latency.
    frameSizeInByte_ = PcmFormatToBits(attr_.format) * attr_.channel / PCM_8_BIT;
    int32_t reqBufferFrameSize = totalBifferInMs * (attr_.sampleRate / 1000);

    struct AudioMmapBufferDescripter desc = {0};
    int32_t ret = audioRender_->attr.ReqMmapBuffer((AudioHandle)audioRender_, reqBufferFrameSize, &desc);
    if (ret != 0) {
        AUDIO_ERR_LOG("ReqMmapBuffer failed, ret:%{public}d", ret);
        return ERR_OPERATION_FAILED;
    }
    AUDIO_INFO_LOG("AudioMmapBufferDescripter memoryAddress[%{private}p] memoryFd[%{public}d] totalBufferFrames"
        "[%{public}d] transferFrameSize[%{public}d] isShareable[%{public}d] offset[%{public}d]",desc.memoryAddress,
        desc.memoryFd, desc.totalBufferFrames, desc.transferFrameSize, desc.isShareable , desc.offset);

    bufferFd_ = dup(desc.memoryFd); // fcntl(fd, 1030,3) after dup?

    if (desc.totalBufferFrames < 0 || desc.totalBufferFrames > UINT32_MAX || desc.transferFrameSize < 0 ||
        desc.transferFrameSize > UINT32_MAX) {
        AUDIO_ERR_LOG("ReqMmapBuffer invalid values: totalBufferFrames[%{public}d] transferFrameSize[%{public}d]",
            desc.totalBufferFrames, desc.transferFrameSize);
        return ERR_OPERATION_FAILED;
    }
    bufferTotalFrameSize_ = desc.totalBufferFrames; // 1440 ~ 3840
    eachReadFrameSize_ = desc.transferFrameSize; // 240

    if (frameSizeInByte_ > ULLONG_MAX / bufferTotalFrameSize_) {
        AUDIO_ERR_LOG("BufferSize will overflow!");
        return ERR_OPERATION_FAILED;
    }
    bufferSize_ = bufferTotalFrameSize_ * frameSizeInByte_;
    bufferAddresss_ = (char *)mmap(nullptr, bufferSize_, PROT_READ | PROT_WRITE, MAP_SHARED, bufferFd_, 0);
    if (bufferAddresss_ == nullptr) {
        AUDIO_ERR_LOG("mmap buffer failed!");
        return ERR_OPERATION_FAILED;
    }
    return SUCCESS;
}



int32_t FastAudioRendererSink::CreateRender(const struct AudioPort &renderPort)
{
    int32_t ret;
    struct AudioSampleAttributes param;
    InitAttrs(param);
    param.sampleRate = attr_.sampleRate;
    param.channelCount = attr_.channel;
    param.format = attr_.format;
    param.frameSize = PcmFormatToBits(param.format) * param.channelCount / PCM_8_BIT;
    param.startThreshold = DEEP_BUFFER_RENDER_PERIOD_SIZE / (param.frameSize); // not passed in hdi
    AUDIO_INFO_LOG("FastAudioRendererSink Create render format: %{public}d", param.format);
    struct AudioDeviceDescriptor deviceDesc;
    deviceDesc.portId = renderPort.portId;
    deviceDesc.pins = PIN_OUT_SPEAKER;
    deviceDesc.desc = nullptr;
    ret = audioAdapter_->CreateRender(audioAdapter_, &deviceDesc, &param, &audioRender_);
    if (ret != 0 || audioRender_ == nullptr) {
        AUDIO_ERR_LOG("AudioDeviceCreateRender failed");
        audioManager_->UnloadAdapter(audioManager_, audioAdapter_);
        return ERR_NOT_STARTED;
    }

    return SUCCESS;
}

int32_t FastAudioRendererSink::Init(AudioSinkAttr &attr)
{
    AUDIO_INFO_LOG("Init.");
    attr_ = attr;
    adapterNameCase_ = attr_.adapterName;  // Set sound card information
    enum AudioPortDirection port = PORT_OUT; // Set port information

    if (InitAudioManager() != 0) {
        AUDIO_ERR_LOG("Init audio manager Fail");
        return ERR_NOT_STARTED;
    }

    int32_t size = 0;
    int32_t ret;
    struct AudioAdapterDescriptor *descs = nullptr;
    ret = audioManager_->GetAllAdapters(audioManager_, &descs, &size);
    if (size > MAX_AUDIO_ADAPTER_NUM || size == 0 || descs == nullptr || ret != 0) {
        AUDIO_ERR_LOG("Get adapters Fail");
        return ERR_NOT_STARTED;
    }

    int32_t index = SwitchAdapterRender(descs, adapterNameCase_, port, audioPort_, size);
    if (index < 0) {
        AUDIO_ERR_LOG("Switch Adapter Fail");
        return ERR_NOT_STARTED;
    }

    struct AudioAdapterDescriptor *desc = &descs[index];
    if (audioManager_->LoadAdapter(audioManager_, desc, &audioAdapter_) != 0) {
        AUDIO_ERR_LOG("Load Adapter Fail");
        return ERR_NOT_STARTED;
    }
    if (audioAdapter_ == nullptr) {
        AUDIO_ERR_LOG("Load audio device failed");
        return ERR_NOT_STARTED;
    }

    // Initialization port information, can fill through mode and other parameters
    ret = audioAdapter_->InitAllPorts(audioAdapter_);
    if (ret != 0) {
        AUDIO_ERR_LOG("InitAllPorts failed");
        return ERR_NOT_STARTED;
    }

    if (CreateRender(audioPort_) != SUCCESS || PrepareMmapBuffer() != SUCCESS) {
        AUDIO_ERR_LOG("Create render failed, Audio Port: %{public}d", audioPort_.portId);
        return ERR_NOT_STARTED;
    }

    rendererInited_ = true;

#ifdef DUMPFILE
    pfd = fopen(g_audioOutTestFilePath, "wb+");
    if (pfd == nullptr) {
        AUDIO_ERR_LOG("Error opening pcm test file!");
    }
#endif // DUMPFILE

    return SUCCESS;
}

void FastAudioRendererSink::PreparePosition()
{
    isFirstWrite_ = false;
    uint64_t frames = 0;
    int64_t timeSec = 0;
    int64_t timeNanoSec = 0;
    GetMmapHandlePosition(frames, timeSec, timeNanoSec); // get first start position
    int32_t periodByteSize = eachReadFrameSize_ * frameSizeInByte_;
    if (periodByteSize * writeAheadPeriod_ > ULLONG_MAX - curReadPos_) {
        AUDIO_ERR_LOG("TempPos will overflow!");
        return;
    }
    size_t tempPos = curReadPos_ + periodByteSize * writeAheadPeriod_; // 1 period ahead
    curWritePos_ = (tempPos < bufferSize_ ? tempPos : tempPos - bufferSize_);
    AUDIO_INFO_LOG("First render frame start with curReadPos_[%{public}d] curWritePos_[%{public}d]", curReadPos_,
        curWritePos_);
}

int32_t FastAudioRendererSink::RenderFrame(char &data, uint64_t len, uint64_t &writeLen)
{
    int64_t stamp = GetNowTimeMs();
    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("Audio Render Handle is nullptr!");
        return ERR_INVALID_HANDLE;
    }

#ifdef DUMPFILE
    size_t writeResult = fwrite((void*)&data, 1, len, pfd);
    if (writeResult != len) {
        AUDIO_ERR_LOG("Failed to write the file.");
    }
#endif // DUMPFILE

    if (len > (bufferSize_ - eachReadFrameSize_ * frameSizeInByte_ * writeAheadPeriod_)) {
        writeLen = 0;
        AUDIO_ERR_LOG("RenderFrame failed,too large len[%{public}" PRIu64 "]!", len);
        return ERR_WRITE_FAILED;
    }

    if (isFirstWrite_) {
        PreparePosition();
    }

    CHECK_AND_RETURN_RET_LOG((curWritePos_ >= 0 && curWritePos_ < bufferSize_), ERR_INVALID_PARAM,
        "curWritePos_ invalid");
    char *writePtr = bufferAddresss_ + curWritePos_;
    uint64_t dataBefore = *(uint64_t *)writePtr;
    uint64_t dataAfter = 0;
    uint64_t tempPos = curWritePos_ + len;
    if (tempPos <= bufferSize_) {
        if (memcpy_s(writePtr, (bufferSize_ - curWritePos_), static_cast<void *>(&data), len)) {
            AUDIO_ERR_LOG("copy failed");
            return ERR_WRITE_FAILED;
        }
        dataAfter = *(uint64_t *)writePtr;
        curWritePos_ = (tempPos == bufferSize_ ? 0 : tempPos);
    } else {
        AUDIO_DEBUG_LOG("(tempPos%{public}" PRIu64 ")curWritePos_ + len > bufferSize_", tempPos);
        size_t writeableSize = bufferSize_ - curWritePos_;
        if (memcpy_s(writePtr, writeableSize, static_cast<void *>(&data), writeableSize) ||
            memcpy_s(bufferAddresss_, bufferSize_, static_cast<void *>((char *)&data + writeableSize),
            (len - writeableSize))) {
            AUDIO_ERR_LOG("copy failed");
            return ERR_WRITE_FAILED;
        }
        curWritePos_ = len - writeableSize;
    }
    writeLen = len;

    stamp = GetNowTimeMs() - stamp;
    AUDIO_DEBUG_LOG("Render len[%{public}" PRIu64 "] cost[%{public}" PRId64 "]ms curWritePos[%{public}d] dataBefore"
        "<%{public}" PRIu64 "> dataAfter<%{public}" PRIu64 ">", len, stamp, curWritePos_, dataBefore, dataAfter);
    return SUCCESS;
}

int32_t FastAudioRendererSink::Start(void)
{
    AUDIO_INFO_LOG("Start.");
    int64_t stamp = GetNowTimeMs();
    int32_t ret;

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("FastAudioRendererSink::Start audioRender_ null!");
        return ERR_INVALID_HANDLE;
    }

    if (!started_) {
        ret = audioRender_->control.Start(reinterpret_cast<AudioHandle>(audioRender_));
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Start failed!");
            return ERR_NOT_STARTED;
        }
    }
    started_ = true;
    AUDIO_DEBUG_LOG("Start cost[%{public}" PRId64 "]ms", GetNowTimeMs() - stamp);
    return SUCCESS;
}

int32_t FastAudioRendererSink::SetVolume(float left, float right)
{
    int32_t ret;
    float volume;

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("FastAudioRendererSink::SetVolume failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    leftVolume_ = left;
    rightVolume_ = right;
    if ((leftVolume_ == 0) && (rightVolume_ != 0)) {
        volume = rightVolume_;
    } else if ((leftVolume_ != 0) && (rightVolume_ == 0)) {
        volume = leftVolume_;
    } else {
        volume = (leftVolume_ + rightVolume_) / HALF_FACTOR;
    }

    ret = audioRender_->volume.SetVolume(reinterpret_cast<AudioHandle>(audioRender_), volume);
    if (ret) {
        AUDIO_ERR_LOG("FastAudioRendererSink::Set volume failed!");
    }

    return ret;
}

int32_t FastAudioRendererSink::GetVolume(float &left, float &right)
{
    left = leftVolume_;
    right = rightVolume_;
    return SUCCESS;
}

int32_t FastAudioRendererSink::SetVoiceVolume(float volume)
{
    if (audioAdapter_ == nullptr) {
        AUDIO_ERR_LOG("FastAudioRendererSink: SetVoiceVolume failed audio adapter null");
        return ERR_INVALID_HANDLE;
    }
    AUDIO_DEBUG_LOG("FastAudioRendererSink: SetVoiceVolume %{public}f", volume);
    return audioAdapter_->SetVoiceVolume(audioAdapter_, volume);
}

int32_t FastAudioRendererSink::GetLatency(uint32_t *latency)
{
    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("FastAudioRendererSink: GetLatency failed audio render null");
        return ERR_INVALID_HANDLE;
    }

    if (!latency) {
        AUDIO_ERR_LOG("FastAudioRendererSink: GetLatency failed latency null");
        return ERR_INVALID_PARAM;
    }

    uint32_t hdiLatency;
    if (audioRender_->GetLatency(audioRender_, &hdiLatency) == 0) {
        *latency = hdiLatency;
        return SUCCESS;
    } else {
        return ERR_OPERATION_FAILED;
    }
}

int32_t FastAudioRendererSink::Stop(void)
{
    AUDIO_INFO_LOG("Stop.");

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("FastAudioRendererSink::Stop failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    if (started_) {
        int32_t ret = audioRender_->control.Stop(reinterpret_cast<AudioHandle>(audioRender_));
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Stop failed!");
            return ERR_OPERATION_FAILED;
        }
    }
    started_ = false;

    return SUCCESS;
}

int32_t FastAudioRendererSink::Pause(void)
{
    int32_t ret;

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("FastAudioRendererSink::Pause failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    if (!started_) {
        AUDIO_ERR_LOG("FastAudioRendererSink::Pause invalid state!");
        return ERR_OPERATION_FAILED;
    }

    if (!paused_) {
        ret = audioRender_->control.Pause(reinterpret_cast<AudioHandle>(audioRender_));
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Pause failed!");
            return ERR_OPERATION_FAILED;
        }
    }
    paused_ = true;

    return SUCCESS;
}

int32_t FastAudioRendererSink::Resume(void)
{
    int32_t ret;

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("FastAudioRendererSink::Resume failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    if (!started_) {
        AUDIO_ERR_LOG("FastAudioRendererSink::Resume invalid state!");
        return ERR_OPERATION_FAILED;
    }

    if (paused_) {
        ret = audioRender_->control.Resume(reinterpret_cast<AudioHandle>(audioRender_));
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Resume failed!");
            return ERR_OPERATION_FAILED;
        }
    }
    paused_ = false;

    return SUCCESS;
}

int32_t FastAudioRendererSink::Reset(void)
{
    int32_t ret;

    if (started_ && audioRender_ != nullptr) {
        ret = audioRender_->control.Flush(reinterpret_cast<AudioHandle>(audioRender_));
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Reset failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return SUCCESS;
}

int32_t FastAudioRendererSink::Flush(void)
{
    int32_t ret;

    if (started_ && audioRender_ != nullptr) {
        ret = audioRender_->control.Flush(reinterpret_cast<AudioHandle>(audioRender_));
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Flush failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return SUCCESS;
}
} // namespace AudioStandard
} // namespace OHOS

#ifdef __cplusplus
extern "C" {
#endif

using namespace OHOS::AudioStandard;

FastAudioRendererSink *g_audioRendrSinkInstance = FastAudioRendererSink::GetInstance();

int32_t FillinFastAudioRenderSinkWapper(const char *deviceNetworkId, void **wapper)
{
    FastAudioRendererSink *instance = FastAudioRendererSink::GetInstance();
    if (instance != nullptr) {
        *wapper = static_cast<void *>(instance);
    } else {
        *wapper = nullptr;
    }

    return SUCCESS;
}

int32_t FastAudioRendererSinkInit(void *wapper, AudioSinkAttr *attr)
{
    (void)wapper;
    int32_t ret;
    if (g_audioRendrSinkInstance->rendererInited_) {
        AUDIO_INFO_LOG("FastAudioRendererSinkInit already inited.");
        return SUCCESS;
    }

    ret = g_audioRendrSinkInstance->Init(*attr);
    return ret;
}

void FastAudioRendererSinkDeInit(void *wapper)
{
    (void)wapper;
    if (g_audioRendrSinkInstance->rendererInited_) {
        g_audioRendrSinkInstance->DeInit();
    }
}

int32_t FastAudioRendererSinkStop(void *wapper)
{
    (void)wapper;
    int32_t ret;

    if (!g_audioRendrSinkInstance->rendererInited_) {
        AUDIO_INFO_LOG("FastAudioRendererSinkStop already deinited.");
        return SUCCESS;
    }

    ret = g_audioRendrSinkInstance->Stop();
    return ret;
}

int32_t FastAudioRendererSinkStart(void *wapper)
{
    (void)wapper;
    int32_t ret;

    if (!g_audioRendrSinkInstance->rendererInited_) {
        AUDIO_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    ret = g_audioRendrSinkInstance->Start();
    return ret;
}

int32_t FastAudioRendererSinkPause(void *wapper)
{
    (void)wapper;
    if (!g_audioRendrSinkInstance->rendererInited_) {
        AUDIO_ERR_LOG("Renderer pause failed");
        return ERR_NOT_STARTED;
    }

    return g_audioRendrSinkInstance->Pause();
}

int32_t FastAudioRendererSinkResume(void *wapper)
{
    (void)wapper;
    if (!g_audioRendrSinkInstance->rendererInited_) {
        AUDIO_ERR_LOG("Renderer resume failed");
        return ERR_NOT_STARTED;
    }

    return g_audioRendrSinkInstance->Resume();
}

int32_t FastAudioRendererRenderFrame(void *wapper, char &data, uint64_t len, uint64_t &writeLen)
{
    (void)wapper;
    int32_t ret;

    if (!g_audioRendrSinkInstance->rendererInited_) {
        AUDIO_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    ret = g_audioRendrSinkInstance->RenderFrame(data, len, writeLen);
    return ret;
}

int32_t FastAudioRendererSinkSetVolume(void *wapper, float left, float right)
{
    (void)wapper;
    int32_t ret;

    if (!g_audioRendrSinkInstance->rendererInited_) {
        AUDIO_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    ret = g_audioRendrSinkInstance->SetVolume(left, right);
    return ret;
}

int32_t FastAudioRendererSinkGetLatency(void *wapper, uint32_t *latency)
{
    (void)wapper;
    int32_t ret;

    if (!g_audioRendrSinkInstance->rendererInited_) {
        AUDIO_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    if (!latency) {
        AUDIO_ERR_LOG("FastAudioRendererSinkGetLatency failed latency null");
        return ERR_INVALID_PARAM;
    }

    ret = g_audioRendrSinkInstance->GetLatency(latency);
    return ret;
}

int32_t FastAudioRendererSinkGetTransactionId(uint64_t *transactionId)
{
    AUDIO_ERR_LOG("FastAudioRendererSinkGetTransactionId failed transaction id null");
    return ERR_INVALID_PARAM;
}
#ifdef __cplusplus
}
#endif
