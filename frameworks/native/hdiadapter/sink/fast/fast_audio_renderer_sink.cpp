/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include <cinttypes>
#include <climits>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <list>
#include <string>
#include <unistd.h>

#include <sys/mman.h>

#include "power_mgr_client.h"
#include "securec.h"
#include "running_lock.h"
#include "v1_0/iaudio_manager.h"

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_utils.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
namespace {
const int32_t HALF_FACTOR = 2;
const uint32_t MAX_AUDIO_ADAPTER_NUM = 5;
const float DEFAULT_VOLUME_LEVEL = 1.0f;
const uint32_t AUDIO_CHANNELCOUNT = 2;
const uint32_t AUDIO_SAMPLE_RATE_48K = 48000;
const uint32_t DEEP_BUFFER_RENDER_PERIOD_SIZE = 3840;
const uint32_t INT_32_MAX = 0x7fffffff;
const uint32_t PCM_8_BIT = 8;
const uint32_t PCM_16_BIT = 16;
const uint32_t PCM_24_BIT = 24;
const uint32_t PCM_32_BIT = 32;
const uint32_t FAST_OUTPUT_STREAM_ID = 21; // 13 + 1 * 8
const int64_t SECOND_TO_NANOSECOND = 1000000000;
const int INVALID_FD = -1;
}

class FastAudioRendererSinkInner : public FastAudioRendererSink {
public:
    int32_t Init(IAudioSinkAttr attr) override;
    bool IsInited(void) override;
    void DeInit(void) override;

    int32_t Start(void) override;
    int32_t Stop(void) override;
    int32_t Flush(void) override;
    int32_t Reset(void) override;
    int32_t Pause(void) override;
    int32_t Resume(void) override;

    int32_t RenderFrame(char &data, uint64_t len, uint64_t &writeLen) override;
    int32_t SetVolume(float left, float right) override;
    int32_t GetVolume(float &left, float &right) override;
    int32_t SetVoiceVolume(float volume) override;
    int32_t GetLatency(uint32_t *latency) override;
    int32_t GetTransactionId(uint64_t *transactionId) override;
    int32_t SetAudioScene(AudioScene audioScene, DeviceType activeDevice) override;
    int32_t SetOutputRoute(DeviceType deviceType) override;

    void SetAudioParameter(const AudioParamKey key, const std::string& condition, const std::string& value) override;
    std::string GetAudioParameter(const AudioParamKey key, const std::string& condition) override;
    void RegisterParameterCallback(IAudioSinkCallback* callback) override;

    void SetAudioMonoState(bool audioMono) override;
    void SetAudioBalanceValue(float audioBalance) override;

    int32_t GetMmapBufferInfo(int &fd, uint32_t &totalSizeInframe, uint32_t &spanSizeInframe,
        uint32_t &byteSizePerFrame) override;
    int32_t GetMmapHandlePosition(uint64_t &frames, int64_t &timeSec, int64_t &timeNanoSec) override;

    FastAudioRendererSinkInner();
    ~FastAudioRendererSinkInner();

private:
    void KeepRunningLock();
    void KeepRunningUnlock();

    int32_t PrepareMmapBuffer();
    void ReleaseMmapBuffer();

    int32_t CheckPositionTime();
    void PreparePosition();

    AudioFormat ConverToHdiFormat(AudioSampleFormat format);
    int32_t CreateRender(const struct AudioPort &renderPort);
    int32_t InitAudioManager();

private:
    IAudioSinkAttr attr_;
    bool rendererInited_;
    bool started_;
    bool paused_;
    float leftVolume_;
    float rightVolume_;
    int32_t routeHandle_ = -1;
    std::string adapterNameCase_;
    struct IAudioManager *audioManager_;
    struct IAudioAdapter *audioAdapter_;
    struct IAudioRender *audioRender_;
    struct AudioAdapterDescriptor adapterDesc_;
    struct AudioPort audioPort_ = {};
    uint32_t renderId_ = 0;

    size_t bufferSize_ = 0;
    uint32_t bufferTotalFrameSize_ = 0;

    int bufferFd_ = INVALID_FD;
    uint32_t frameSizeInByte_ = 1;
    uint32_t eachReadFrameSize_ = 0;

    std::shared_ptr<PowerMgr::RunningLock> keepRunningLock_;

#ifdef DEBUG_DIRECT_USE_HDI
    char *bufferAddresss_ = nullptr;
    bool isFirstWrite_ = true;
    uint64_t alreadyReadFrames_ = 0;
    uint32_t curReadPos_ = 0;
    uint32_t curWritePos_ = 0;
    uint32_t writeAheadPeriod_ = 1;

    int privFd_ = INVALID_FD; // invalid fd
#endif
};

FastAudioRendererSinkInner::FastAudioRendererSinkInner()
    : rendererInited_(false), started_(false), paused_(false), leftVolume_(DEFAULT_VOLUME_LEVEL),
      rightVolume_(DEFAULT_VOLUME_LEVEL), audioManager_(nullptr), audioAdapter_(nullptr),
      audioRender_(nullptr)
{
    attr_ = {};
}

FastAudioRendererSinkInner::~FastAudioRendererSinkInner()
{
    FastAudioRendererSinkInner::DeInit();
}

IMmapAudioRendererSink *FastAudioRendererSink::GetInstance()
{
    static FastAudioRendererSinkInner audioRenderer;

    return &audioRenderer;
}

bool FastAudioRendererSinkInner::IsInited()
{
    return rendererInited_;
}

void FastAudioRendererSinkInner::DeInit()
{
    KeepRunningUnlock();

    keepRunningLock_ = nullptr;

    started_ = false;
    rendererInited_ = false;
    if ((audioRender_ != nullptr) && (audioAdapter_ != nullptr)) {
        audioAdapter_->DestroyRender(audioAdapter_, renderId_);
    }
    audioRender_ = nullptr;

    if ((audioManager_ != nullptr) && (audioAdapter_ != nullptr)) {
        if (routeHandle_ != -1) {
            audioAdapter_->ReleaseAudioRoute(audioAdapter_, routeHandle_);
        }
        audioManager_->UnloadAdapter(audioManager_, adapterDesc_.adapterName);
    }
    audioAdapter_ = nullptr;
    audioManager_ = nullptr;

    ReleaseMmapBuffer();
}

void InitAttrs(struct AudioSampleAttributes &attrs)
{
    /* Initialization of audio parameters for playback */
    attrs.channelCount = AUDIO_CHANNELCOUNT;
    attrs.sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs.interleaved = true;
    attrs.streamId = FAST_OUTPUT_STREAM_ID;
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
            for (uint32_t port = 0; port < desc->portsLen; port++) {
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

int32_t FastAudioRendererSinkInner::InitAudioManager()
{
    AUDIO_INFO_LOG("FastAudioRendererSink: Initialize audio proxy manager");

    audioManager_ = IAudioManagerGet(false);
    if (audioManager_ == nullptr) {
        return ERR_INVALID_HANDLE;
    }

    return 0;
}

uint32_t PcmFormatToBits(AudioSampleFormat format)
{
    switch (format) {
        case SAMPLE_U8:
            return PCM_8_BIT;
        case SAMPLE_S16LE:
            return PCM_16_BIT;
        case SAMPLE_S24LE:
            return PCM_24_BIT;
        case SAMPLE_S32LE:
            return PCM_32_BIT;
        case SAMPLE_F32LE:
            return PCM_32_BIT;
        default:
            return PCM_24_BIT;
    }
}

int32_t FastAudioRendererSinkInner::GetMmapBufferInfo(int &fd, uint32_t &totalSizeInframe, uint32_t &spanSizeInframe,
    uint32_t &byteSizePerFrame)
{
    if (bufferFd_ == INVALID_FD) {
        AUDIO_ERR_LOG("buffer fd has been released!");
        return ERR_INVALID_HANDLE;
    }
    fd = bufferFd_;
    totalSizeInframe = bufferTotalFrameSize_;
    spanSizeInframe = eachReadFrameSize_;
    byteSizePerFrame = PcmFormatToBits(attr_.format) * attr_.channel / PCM_8_BIT;
    return SUCCESS;
}

int32_t FastAudioRendererSinkInner::GetMmapHandlePosition(uint64_t &frames, int64_t &timeSec, int64_t &timeNanoSec)
{
    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("Audio render is null!");
        return ERR_INVALID_HANDLE;
    }

    struct AudioTimeStamp timestamp = {};
    int32_t ret = audioRender_->GetMmapPosition(audioRender_, &frames, &timestamp);
    if (ret != 0) {
        AUDIO_ERR_LOG("Hdi GetMmapPosition filed, ret:%{public}d!", ret);
        return ERR_OPERATION_FAILED;
    }
#ifdef DEBUG_DIRECT_USE_HDI
    alreadyReadFrames_ = frames; // frames already read.
    curReadPos_ = frameSizeInByte_ * (frames - bufferTotalFrameSize_ * (frames / bufferTotalFrameSize_));
    CHECK_AND_RETURN_RET_LOG((curReadPos_ >= 0 && curReadPos_ < bufferSize_), ERR_INVALID_PARAM, "curReadPos invalid");
    AUDIO_DEBUG_LOG("GetMmapHandlePosition frames[:%{public}" PRIu64 "] tvsec:%{public}" PRId64 " tvNSec:"
        "%{public}" PRId64 " alreadyReadFrames:%{public}" PRId64 " curReadPos[%{public}d]",
        frames, timestamp.tvSec, timestamp.tvNSec, alreadyReadFrames_, curReadPos_);
#endif

    int64_t maxSec = 9223372036; // (9223372036 + 1) * 10^9 > INT64_MAX, seconds should not bigger than it.
    if (timestamp.tvSec < 0 || timestamp.tvSec > maxSec || timestamp.tvNSec < 0 ||
        timestamp.tvNSec > SECOND_TO_NANOSECOND) {
        AUDIO_ERR_LOG("Hdi GetMmapPosition get invaild second:%{public}" PRId64 " or nanosecond:%{public}" PRId64 " !",
            timestamp.tvSec, timestamp.tvNSec);
        return ERR_OPERATION_FAILED;
    }
    timeSec = timestamp.tvSec;
    timeNanoSec = timestamp.tvNSec;

    return SUCCESS;
}

void FastAudioRendererSinkInner::ReleaseMmapBuffer()
{
#ifdef DEBUG_DIRECT_USE_HDI
    if (bufferAddresss_ != nullptr) {
        munmap(bufferAddresss_, bufferSize_);
        bufferAddresss_ = nullptr;
        bufferSize_ = 0;
        AUDIO_INFO_LOG("ReleaseMmapBuffer end.");
    } else {
        AUDIO_WARNING_LOG("ReleaseMmapBuffer buffer already null.");
    }
    if (privFd_ != INVALID_FD) {
        close(privFd_);
        privFd_ = INVALID_FD;
    }
#endif
    if (bufferFd_ != INVALID_FD) {
        close(bufferFd_);
        bufferFd_ = INVALID_FD;
    }
}

int32_t FastAudioRendererSinkInner::PrepareMmapBuffer()
{
    int32_t totalBifferInMs = 40; // 5 * (6 + 2 * (1)) = 40ms, the buffer size, not latency.
    frameSizeInByte_ = PcmFormatToBits(attr_.format) * attr_.channel / PCM_8_BIT;
    int32_t reqBufferFrameSize = totalBifferInMs * (attr_.sampleRate / 1000);

    struct AudioMmapBufferDescriptor desc = {0};
    int32_t ret = audioRender_->ReqMmapBuffer(audioRender_, reqBufferFrameSize, &desc);
    if (ret != 0) {
        AUDIO_ERR_LOG("ReqMmapBuffer failed, ret:%{public}d", ret);
        return ERR_OPERATION_FAILED;
    }
    AUDIO_INFO_LOG("AudioMmapBufferDescriptor memoryAddress[%{private}p] memoryFd[%{public}d] totalBufferFrames"
        "[%{public}d] transferFrameSize[%{public}d] isShareable[%{public}d] offset[%{public}d]", desc.memoryAddress,
        desc.memoryFd, desc.totalBufferFrames, desc.transferFrameSize, desc.isShareable, desc.offset);

    bufferFd_ = desc.memoryFd; // fcntl(fd, 1030,3) after dup?
    int32_t periodFrameMaxSize = 1920000; // 192khz * 10s
    if (desc.totalBufferFrames < 0 || desc.transferFrameSize < 0 || desc.transferFrameSize > periodFrameMaxSize) {
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
#ifdef DEBUG_DIRECT_USE_HDI
    privFd_ = dup(bufferFd_);
    bufferAddresss_ = (char *)mmap(nullptr, bufferSize_, PROT_READ | PROT_WRITE, MAP_SHARED, privFd_, 0);
    if (bufferAddresss_ == nullptr || bufferAddresss_ == MAP_FAILED) {
        AUDIO_ERR_LOG("mmap buffer failed!");
        return ERR_OPERATION_FAILED;
    }
#endif
    return SUCCESS;
}

AudioFormat FastAudioRendererSinkInner::ConverToHdiFormat(AudioSampleFormat format)
{
    AudioFormat hdiFormat;
    switch (format) {
        case SAMPLE_U8:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_8_BIT;
            break;
        case SAMPLE_S16LE:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_16_BIT;
            break;
        case SAMPLE_S24LE:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_24_BIT;
            break;
        case SAMPLE_S32LE:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_32_BIT;
            break;
        default:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_16_BIT;
            break;
    }

    return hdiFormat;
}

int32_t FastAudioRendererSinkInner::CreateRender(const struct AudioPort &renderPort)
{
    int32_t ret;
    struct AudioSampleAttributes param;
    InitAttrs(param);
    param.sampleRate = attr_.sampleRate;
    param.channelCount = attr_.channel;
    param.format = ConverToHdiFormat(attr_.format);
    param.frameSize = PcmFormatToBits(attr_.format) * param.channelCount / PCM_8_BIT;
    param.startThreshold = DEEP_BUFFER_RENDER_PERIOD_SIZE / (param.frameSize); // not passed in hdi
    AUDIO_INFO_LOG("FastAudioRendererSink Create render format: %{public}d and device:%{public}d", param.format,
        attr_.deviceType);
    struct AudioDeviceDescriptor deviceDesc;
    deviceDesc.portId = renderPort.portId;
    switch (static_cast<DeviceType>(attr_.deviceType)) {
        case DEVICE_TYPE_EARPIECE:
            deviceDesc.pins = PIN_OUT_EARPIECE;
            break;
        case DEVICE_TYPE_SPEAKER:
            deviceDesc.pins = PIN_OUT_SPEAKER;
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
            deviceDesc.pins = PIN_OUT_HEADSET;
            break;
        case DEVICE_TYPE_USB_HEADSET:
            deviceDesc.pins = PIN_OUT_USB_EXT;
            break;
        case DEVICE_TYPE_BLUETOOTH_SCO:
            deviceDesc.pins = PIN_OUT_BLUETOOTH_SCO;
            break;
        default:
            deviceDesc.pins = PIN_OUT_SPEAKER;
            break;
    }
    char desc[] = "";
    deviceDesc.desc = desc;
    ret = audioAdapter_->CreateRender(audioAdapter_, &deviceDesc, &param, &audioRender_, &renderId_);
    if (ret != 0 || audioRender_ == nullptr) {
        AUDIO_ERR_LOG("AudioDeviceCreateRender failed");
        audioManager_->UnloadAdapter(audioManager_, adapterDesc_.adapterName);
        return ERR_NOT_STARTED;
    }

    return SUCCESS;
}

int32_t FastAudioRendererSinkInner::Init(IAudioSinkAttr attr)
{
    AUDIO_INFO_LOG("Init.");
    attr_ = attr;
    adapterNameCase_ = attr_.adapterName;  // Set sound card information
    enum AudioPortDirection port = PORT_OUT; // Set port information

    if (InitAudioManager() != 0) {
        AUDIO_ERR_LOG("Init audio manager Fail");
        return ERR_NOT_STARTED;
    }

    uint32_t size = MAX_AUDIO_ADAPTER_NUM;
    AudioAdapterDescriptor descs[MAX_AUDIO_ADAPTER_NUM];
    int32_t ret = audioManager_->GetAllAdapters(audioManager_,
        (struct AudioAdapterDescriptor *)&descs, &size);
    if (size > MAX_AUDIO_ADAPTER_NUM || size == 0 || ret != 0) {
        AUDIO_ERR_LOG("Get adapters Fail");
        return ERR_NOT_STARTED;
    }

    int32_t index = SwitchAdapterRender((struct AudioAdapterDescriptor *)&descs, adapterNameCase_, port, audioPort_,
        size);
    if (index < 0) {
        AUDIO_ERR_LOG("Switch Adapter Fail");
        return ERR_NOT_STARTED;
    }

    adapterDesc_ = descs[index];
    if (audioManager_->LoadAdapter(audioManager_, &adapterDesc_, &audioAdapter_) != 0) {
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

    return SUCCESS;
}

void FastAudioRendererSinkInner::PreparePosition()
{
#ifdef DEBUG_DIRECT_USE_HDI
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
#endif
}

int32_t FastAudioRendererSinkInner::RenderFrame(char &data, uint64_t len, uint64_t &writeLen)
{
#ifdef DEBUG_DIRECT_USE_HDI
    int64_t stamp = ClockTime::GetCurNano();
    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("Audio Render Handle is nullptr!");
        return ERR_INVALID_HANDLE;
    }

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

    stamp = (ClockTime::GetCurNano() - stamp) / AUDIO_US_PER_SECOND;
    AUDIO_DEBUG_LOG("Render len[%{public}" PRIu64 "] cost[%{public}" PRId64 "]ms curWritePos[%{public}d] dataBefore"
        "<%{public}" PRIu64 "> dataAfter<%{public}" PRIu64 ">", len, stamp, curWritePos_, dataBefore, dataAfter);
    return SUCCESS;
#else
    AUDIO_WARNING_LOG("RenderFrame is not supported.");
    return ERR_NOT_SUPPORTED;
#endif
}

int32_t FastAudioRendererSinkInner::CheckPositionTime()
{
    int32_t tryCount = 10;
    uint64_t frames = 0;
    int64_t timeSec = 0;
    int64_t timeNanoSec = 0;
    int64_t maxHandleCost = 10000000; // ns
    int64_t waitTime = 2000000; // 2ms
    while (tryCount-- > 0) {
        ClockTime::RelativeSleep(waitTime); // us
        int32_t ret = GetMmapHandlePosition(frames, timeSec, timeNanoSec);
        int64_t curTime = ClockTime::GetCurNano();
        int64_t curSec = curTime / AUDIO_NS_PER_SECOND;
        int64_t curNanoSec = curTime - curSec * AUDIO_NS_PER_SECOND;
        if (ret != SUCCESS || curSec != timeSec || curNanoSec - timeNanoSec > maxHandleCost) {
            AUDIO_WARNING_LOG("CheckPositionTime[%{public}d]:ret %{public}d", tryCount, ret);
            continue;
        } else {
            AUDIO_INFO_LOG("CheckPositionTime end, position and time is ok.");
            return SUCCESS;
        }
    }
    return ERROR;
}

int32_t FastAudioRendererSinkInner::Start(void)
{
    AUDIO_INFO_LOG("Start.");
    int64_t stamp = ClockTime::GetCurNano();
    int32_t ret;

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("FastAudioRendererSink::Start audioRender_ null!");
        return ERR_INVALID_HANDLE;
    }

    if (!started_) {
        ret = audioRender_->Start(audioRender_);
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Start failed!");
            return ERR_NOT_STARTED;
        }
        if (CheckPositionTime() != SUCCESS) {
            AUDIO_ERR_LOG("FastAudioRendererSink::CheckPositionTime failed!");
            return ERR_NOT_STARTED;
        }
    }
    KeepRunningLock();
    started_ = true;
    AUDIO_DEBUG_LOG("Start cost[%{public}" PRId64 "]ms", (ClockTime::GetCurNano() - stamp) / AUDIO_US_PER_SECOND);
    return SUCCESS;
}

void FastAudioRendererSinkInner::KeepRunningLock()
{
    if (keepRunningLock_ == nullptr) {
        keepRunningLock_ = PowerMgr::PowerMgrClient::GetInstance().CreateRunningLock("AudioFastBackgroundPlay",
            PowerMgr::RunningLockType::RUNNINGLOCK_BACKGROUND_AUDIO);
    }

    if (keepRunningLock_ != nullptr) {
        AUDIO_INFO_LOG("FastAudioRendererSink call KeepRunningLock lock");
        int32_t timeOut = -1; // -1 for lasting.
        keepRunningLock_->Lock(timeOut);
    } else {
        AUDIO_ERR_LOG("Fast: keepRunningLock_ is null, playback can not work well!");
    }
}

void FastAudioRendererSinkInner::KeepRunningUnlock()
{
    if (keepRunningLock_ != nullptr) {
        AUDIO_INFO_LOG("FastAudioRendererSink call KeepRunningLock UnLock");
        keepRunningLock_->UnLock();
    } else {
        AUDIO_ERR_LOG("Fast: keepRunningLock_ is null, playback can not work well!");
    }
}


int32_t FastAudioRendererSinkInner::SetVolume(float left, float right)
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

    ret = audioRender_->SetVolume(audioRender_, volume);
    if (ret) {
        AUDIO_ERR_LOG("FastAudioRendererSink::Set volume failed!");
    }

    return ret;
}

int32_t FastAudioRendererSinkInner::GetVolume(float &left, float &right)
{
    left = leftVolume_;
    right = rightVolume_;
    return SUCCESS;
}

int32_t FastAudioRendererSinkInner::SetVoiceVolume(float volume)
{
    AUDIO_ERR_LOG("FastAudioRendererSink SetVoiceVolume not supported.");
    return ERR_NOT_SUPPORTED;
}

int32_t FastAudioRendererSinkInner::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    AUDIO_ERR_LOG("FastAudioRendererSink SetAudioScene not supported.");
    return ERR_NOT_SUPPORTED;
}

int32_t FastAudioRendererSinkInner::SetOutputRoute(DeviceType deviceType)
{
    AUDIO_ERR_LOG("FastAudioRendererSink SetOutputRoute not supported.");
    return ERR_NOT_SUPPORTED;
}

void FastAudioRendererSinkInner::SetAudioParameter(const AudioParamKey key, const std::string& condition,
    const std::string& value)
{
    AUDIO_ERR_LOG("FastAudioRendererSink SetAudioParameter not supported.");
    return;
}

std::string FastAudioRendererSinkInner::GetAudioParameter(const AudioParamKey key, const std::string& condition)
{
    AUDIO_ERR_LOG("FastAudioRendererSink GetAudioParameter not supported.");
    return "";
}

void FastAudioRendererSinkInner::RegisterParameterCallback(IAudioSinkCallback* callback)
{
    AUDIO_ERR_LOG("FastAudioRendererSink RegisterParameterCallback not supported.");
}

void FastAudioRendererSinkInner::SetAudioMonoState(bool audioMono)
{
    AUDIO_ERR_LOG("FastAudioRendererSink SetAudioMonoState not supported.");
    return;
}

void FastAudioRendererSinkInner::SetAudioBalanceValue(float audioBalance)
{
    AUDIO_ERR_LOG("FastAudioRendererSink SetAudioBalanceValue not supported.");
    return;
}

int32_t FastAudioRendererSinkInner::GetTransactionId(uint64_t *transactionId)
{
    AUDIO_ERR_LOG("FastAudioRendererSink %{public}s", __func__);
    *transactionId = 6; // 6 is the mmap device.
    return ERR_NOT_SUPPORTED;
}

int32_t FastAudioRendererSinkInner::GetLatency(uint32_t *latency)
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

int32_t FastAudioRendererSinkInner::Stop(void)
{
    AUDIO_INFO_LOG("Stop.");

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("FastAudioRendererSink::Stop failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }
    KeepRunningUnlock();

    if (started_) {
        int32_t ret = audioRender_->Stop(audioRender_);
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Stop failed! ret: %{public}d.", ret);
            return ERR_OPERATION_FAILED;
        }
    }
    started_ = false;

    return SUCCESS;
}

int32_t FastAudioRendererSinkInner::Pause(void)
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
        ret = audioRender_->Pause(audioRender_);
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Pause failed!");
            return ERR_OPERATION_FAILED;
        }
    }
    paused_ = true;

    return SUCCESS;
}

int32_t FastAudioRendererSinkInner::Resume(void)
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
        ret = audioRender_->Resume(audioRender_);
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Resume failed!");
            return ERR_OPERATION_FAILED;
        }
    }
    paused_ = false;

    return SUCCESS;
}

int32_t FastAudioRendererSinkInner::Reset(void)
{
    int32_t ret;

    if (started_ && audioRender_ != nullptr) {
        ret = audioRender_->Flush(audioRender_);
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Reset failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return SUCCESS;
}

int32_t FastAudioRendererSinkInner::Flush(void)
{
    int32_t ret;

    if (started_ && audioRender_ != nullptr) {
        ret = audioRender_->Flush(audioRender_);
        if (ret != 0) {
            AUDIO_ERR_LOG("FastAudioRendererSink::Flush failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return SUCCESS;
}
} // namespace AudioStandard
} // namespace OHOS
