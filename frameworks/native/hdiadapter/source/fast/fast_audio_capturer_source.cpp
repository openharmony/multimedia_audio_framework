/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <string>
#include <cinttypes>
#ifdef FEATURE_POWER_MANAGER
#include "power_mgr_client.h"
#include "running_lock.h"
#endif

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_utils.h"

#include "v2_0/iaudio_manager.h"
#include "fast_audio_capturer_source.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
class FastAudioCapturerSourceInner : public FastAudioCapturerSource {
public:
    int32_t Init(const IAudioSourceAttr &attr) override;
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

    int32_t SetInputRoute(DeviceType inputDevice, AudioPortPin &inputPortPin);

    int32_t SetInputRoute(DeviceType inputDevice) override;

    uint64_t GetTransactionId() override;
    int32_t GetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec) override;
    void RegisterWakeupCloseCallback(IAudioSourceCallback *callback) override;
    void RegisterAudioCapturerSourceCallback(IAudioSourceCallback *callback) override;
    void RegisterParameterCallback(IAudioSourceCallback *callback) override;

    int32_t GetMmapBufferInfo(int &fd, uint32_t &totalSizeInframe, uint32_t &spanSizeInframe,
        uint32_t &byteSizePerFrame) override;
    int32_t GetMmapHandlePosition(uint64_t &frames, int64_t &timeSec, int64_t &timeNanoSec) override;

    FastAudioCapturerSourceInner();
    ~FastAudioCapturerSourceInner() override;
private:
    static constexpr int32_t INVALID_FD = -1;
    static constexpr int32_t HALF_FACTOR = 2;
    static constexpr uint32_t MAX_AUDIO_ADAPTER_NUM = 5;
    static constexpr float MAX_VOLUME_LEVEL = 15.0f;
    static constexpr int64_t SECOND_TO_NANOSECOND = 1000000000;
    static constexpr  uint32_t PCM_8_BIT = 8;
    static constexpr  uint32_t PCM_16_BIT = 16;
    static constexpr  uint32_t PCM_24_BIT = 24;
    static constexpr  uint32_t PCM_32_BIT = 32;
    static constexpr uint32_t AUDIO_CHANNELCOUNT = 2;
    static constexpr uint32_t AUDIO_SAMPLE_RATE_48K = 48000;
    static constexpr uint32_t INT_32_MAX = 0x7fffffff;
    static constexpr uint32_t FAST_INPUT_STREAM_ID = 22; // 14 + 1 * 8
    int32_t routeHandle_ = -1;

    IAudioSourceAttr attr_;
    bool capturerInited_;
    bool started_;
    bool paused_;

    uint32_t captureId_ = 0;
    uint32_t openMic_;
    std::string adapterNameCase_;
    struct IAudioManager *audioManager_ = nullptr;
    struct IAudioAdapter *audioAdapter_ = nullptr;
    struct IAudioCapture *audioCapture_ = nullptr;
    struct AudioAdapterDescriptor adapterDesc_;
    struct AudioPort audioPort;

    size_t bufferSize_ = 0;
    uint32_t bufferTotalFrameSize_ = 0;

    int bufferFd_ = INVALID_FD;
    uint32_t eachReadFrameSize_ = 0;
#ifdef FEATURE_POWER_MANAGER
    std::shared_ptr<PowerMgr::RunningLock> keepRunningLock_;
#endif
private:
    void InitAttrsCapture(struct AudioSampleAttributes &attrs);
    int32_t SwitchAdapterCapture(struct AudioAdapterDescriptor *descs, uint32_t size,
    const std::string &adapterNameCase, enum AudioPortDirection portFlag, struct AudioPort &capturePort);
    int32_t CreateCapture(const struct AudioPort &capturePort);
    int32_t PrepareMmapBuffer();
    int32_t InitAudioManager();
    uint32_t PcmFormatToBits(HdiAdapterFormat format);
    AudioFormat ConvertToHdiFormat(HdiAdapterFormat format);
    int32_t CheckPositionTime();
};
#ifdef FEATURE_POWER_MANAGER
constexpr int32_t RUNNINGLOCK_LOCK_TIMEOUTMS_LASTING = -1;
#endif
FastAudioCapturerSourceInner::FastAudioCapturerSourceInner() : attr_({}), capturerInited_(false), started_(false),
    paused_(false), openMic_(0), audioManager_(nullptr), audioAdapter_(nullptr), audioCapture_(nullptr)
{}

FastAudioCapturerSourceInner::~FastAudioCapturerSourceInner()
{
    AUDIO_DEBUG_LOG("~FastAudioCapturerSourceInner");
}
FastAudioCapturerSource *FastAudioCapturerSource::GetInstance()
{
    static FastAudioCapturerSourceInner audioCapturer;
    return &audioCapturer;
}

bool FastAudioCapturerSourceInner::IsInited(void)
{
    return capturerInited_;
}

void FastAudioCapturerSourceInner::DeInit()
{
    started_ = false;
    capturerInited_ = false;

    if (audioAdapter_ != nullptr) {
        audioAdapter_->DestroyCapture(audioAdapter_, captureId_);
    }
    audioCapture_ = nullptr;

    if (audioManager_ != nullptr) {
        audioManager_->UnloadAdapter(audioManager_, adapterDesc_.adapterName);
    }
    audioAdapter_ = nullptr;
    audioManager_ = nullptr;
}

void FastAudioCapturerSourceInner::InitAttrsCapture(struct AudioSampleAttributes &attrs)
{
    /* Initialization of audio parameters for playback */
    attrs.format = AUDIO_FORMAT_TYPE_PCM_16_BIT;
    attrs.channelCount = AUDIO_CHANNELCOUNT;
    attrs.sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs.interleaved = true;
    attrs.streamId = FAST_INPUT_STREAM_ID;
    attrs.type = AUDIO_MMAP_NOIRQ; // enable mmap!
    attrs.period = 0;
    attrs.frameSize = PCM_16_BIT * attrs.channelCount / PCM_8_BIT;
    attrs.isBigEndian = false;
    attrs.isSignedData = true;
    attrs.startThreshold = 0;
    attrs.stopThreshold = INT_32_MAX;
    /* 16 * 1024 */
    attrs.silenceThreshold = 0;
}

int32_t FastAudioCapturerSourceInner::SwitchAdapterCapture(struct AudioAdapterDescriptor *descs, uint32_t size,
    const std::string &adapterNameCase, enum AudioPortDirection portFlag, struct AudioPort &capturePort)
{
    CHECK_AND_RETURN_RET(descs != nullptr, ERROR);

    for (uint32_t index = 0; index < size; index++) {
        struct AudioAdapterDescriptor *desc = &descs[index];
        if (desc == nullptr || desc->adapterName == nullptr) {
            continue;
        }
        CHECK_AND_CONTINUE_LOG(adapterNameCase.compare(desc->adapterName) == 0,
            "not equal: %{public}s <-> %{public}s", adapterNameCase.c_str(), desc->adapterName);
        AUDIO_DEBUG_LOG("adapter name: %{public}s <-> %{public}s", adapterNameCase.c_str(), desc->adapterName);
        for (uint32_t port = 0; port < desc->portsLen; port++) {
            // Only find out the port of out in the sound card
            if (desc->ports[port].dir == portFlag) {
                capturePort = desc->ports[port];
                return index;
            }
        }
    }
    AUDIO_ERR_LOG("SwitchAdapterCapture Fail");

    return ERR_INVALID_INDEX;
}

int32_t FastAudioCapturerSourceInner::InitAudioManager()
{
    AUDIO_INFO_LOG("FastAudioCapturerSourceInner: Initialize audio proxy manager");

    audioManager_ = IAudioManagerGet(false);
    if (audioManager_ == nullptr) {
        return ERR_INVALID_HANDLE;
    }

    return 0;
}


AudioFormat FastAudioCapturerSourceInner::ConvertToHdiFormat(HdiAdapterFormat format)
{
    AudioFormat hdiFormat;
    switch (format) {
        case SAMPLE_U8:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_8_BIT;
            break;
        case SAMPLE_S16:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_16_BIT;
            break;
        case SAMPLE_S24:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_24_BIT;
            break;
        case SAMPLE_S32:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_32_BIT;
            break;
        default:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_16_BIT;
            break;
    }

    return hdiFormat;
}

int32_t FastAudioCapturerSourceInner::CreateCapture(const struct AudioPort &capturePort)
{
    int32_t ret;
    struct AudioSampleAttributes param;
    // User needs to set
    InitAttrsCapture(param);
    param.sampleRate = attr_.sampleRate;
    param.format = ConvertToHdiFormat(attr_.format);
    param.isBigEndian = attr_.isBigEndian;
    param.channelCount = attr_.channel;
    if (param.channelCount == MONO) {
        param.channelLayout = CH_LAYOUT_MONO;
    } else if (param.channelCount == STEREO) {
        param.channelLayout = CH_LAYOUT_STEREO;
    }
    param.silenceThreshold = attr_.bufferSize;
    param.frameSize = param.format * param.channelCount;
    param.startThreshold = 0;

    struct AudioDeviceDescriptor deviceDesc;
    deviceDesc.portId = capturePort.portId;
    char desc[] = "";
    deviceDesc.desc = desc;

    switch (static_cast<DeviceType>(attr_.deviceType)) {
        case DEVICE_TYPE_MIC:
            deviceDesc.pins = PIN_IN_MIC;
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
            deviceDesc.pins = PIN_IN_HS_MIC;
            break;
        case DEVICE_TYPE_USB_HEADSET:
            deviceDesc.pins = PIN_IN_USB_EXT;
            break;
        default:
            AUDIO_WARNING_LOG("Unsupported device type:%{public}d, use default mic instead", attr_.deviceType);
            deviceDesc.pins = PIN_IN_MIC;
            break;
    }

    ret = audioAdapter_->CreateCapture(audioAdapter_, &deviceDesc, &param, &audioCapture_, &captureId_);
    CHECK_AND_RETURN_RET_LOG(audioCapture_ != nullptr && ret >= 0,
        ERR_NOT_STARTED, "Create capture failed");

    return 0;
}

uint32_t FastAudioCapturerSourceInner::PcmFormatToBits(HdiAdapterFormat format)
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

int32_t FastAudioCapturerSourceInner::GetMmapBufferInfo(int &fd, uint32_t &totalSizeInframe, uint32_t &spanSizeInframe,
    uint32_t &byteSizePerFrame)
{
    CHECK_AND_RETURN_RET_LOG(bufferFd_ != INVALID_FD, ERR_INVALID_HANDLE, "buffer fd has been released!");
    fd = bufferFd_;
    totalSizeInframe = bufferTotalFrameSize_;
    spanSizeInframe = eachReadFrameSize_;
    byteSizePerFrame = PcmFormatToBits(attr_.format) * attr_.channel / PCM_8_BIT;
    return SUCCESS;
}

int32_t FastAudioCapturerSourceInner::GetMmapHandlePosition(uint64_t &frames, int64_t &timeSec, int64_t &timeNanoSec)
{
    CHECK_AND_RETURN_RET_LOG(audioCapture_ != nullptr, ERR_INVALID_HANDLE, "Audio render is null!");

    struct AudioTimeStamp timestamp = {};
    int32_t ret = audioCapture_->GetMmapPosition(audioCapture_, &frames, &timestamp);
    CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_OPERATION_FAILED, "Hdi GetMmapPosition filed, ret:%{public}d!", ret);

    int64_t maxSec = 9223372036; // (9223372036 + 1) * 10^9 > INT64_MAX, seconds should not bigger than it.
    CHECK_AND_RETURN_RET_LOG(timestamp.tvSec >= 0 && timestamp.tvSec <= maxSec && timestamp.tvNSec >= 0 &&
        timestamp.tvNSec <= SECOND_TO_NANOSECOND, ERR_OPERATION_FAILED,
        "Hdi GetMmapPosition get invaild second:%{public}" PRId64 " or nanosecond:%{public}" PRId64 " !",
        timestamp.tvSec, timestamp.tvNSec);
    timeSec = timestamp.tvSec;
    timeNanoSec = timestamp.tvNSec;

    return SUCCESS;
}

int32_t FastAudioCapturerSourceInner::PrepareMmapBuffer()
{
    int32_t totalBifferInMs = 40; // 5 * (6 + 2 * (1)) = 40ms, the buffer size, not latency.
    uint32_t frameSizeInByte = PcmFormatToBits(attr_.format) * attr_.channel / PCM_8_BIT;
    int32_t reqBufferFrameSize = totalBifferInMs * (attr_.sampleRate / 1000);

    struct AudioMmapBufferDescriptor desc = {0};
    int32_t ret = audioCapture_->ReqMmapBuffer(audioCapture_, reqBufferFrameSize, &desc);
    CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_OPERATION_FAILED, "ReqMmapBuffer failed, ret:%{public}d", ret);
    AUDIO_INFO_LOG("AudioMmapBufferDescriptor memoryAddress[%{private}p] memoryFd[%{public}d] totalBufferFrames"
        "[%{public}d] transferFrameSize[%{public}d] isShareable[%{public}d] offset[%{public}d]", desc.memoryAddress,
        desc.memoryFd, desc.totalBufferFrames, desc.transferFrameSize, desc.isShareable, desc.offset);

    bufferFd_ = desc.memoryFd; // fcntl(fd, 1030,3) after dup?
    int32_t periodFrameMaxSize = 1920000; // 192khz * 10s
    CHECK_AND_RETURN_RET_LOG(desc.totalBufferFrames >= 0 && desc.transferFrameSize >= 0 &&
        desc.transferFrameSize <= periodFrameMaxSize, ERR_OPERATION_FAILED,
        "ReqMmapBuffer invalid values: totalBufferFrames[%{public}d] transferFrameSize[%{public}d]",
        desc.totalBufferFrames, desc.transferFrameSize);
    bufferTotalFrameSize_ = desc.totalBufferFrames; // 1440 ~ 3840
    eachReadFrameSize_ = desc.transferFrameSize; // 240

    CHECK_AND_RETURN_RET_LOG(frameSizeInByte <= ULLONG_MAX / bufferTotalFrameSize_,
        ERR_OPERATION_FAILED, "BufferSize will overflow!");
    bufferSize_ = bufferTotalFrameSize_ * frameSizeInByte;

    return SUCCESS;
}

int32_t FastAudioCapturerSourceInner::Init(const IAudioSourceAttr &attr)
{
    CHECK_AND_RETURN_RET_LOG(InitAudioManager() == 0, ERR_INVALID_HANDLE, "Init audio manager Fail");
    attr_ = attr;
    int32_t ret;
    int32_t index;
    uint32_t size = MAX_AUDIO_ADAPTER_NUM;
    AudioAdapterDescriptor descs[MAX_AUDIO_ADAPTER_NUM];
    ret = audioManager_->GetAllAdapters(audioManager_, (struct AudioAdapterDescriptor *)&descs, &size);
    CHECK_AND_RETURN_RET_LOG(size <= MAX_AUDIO_ADAPTER_NUM && size != 0 && ret == 0, ERR_NOT_STARTED,
        "Get adapters Fail");
    // Get qualified sound card and port
    adapterNameCase_ = attr_.adapterName;
    openMic_ = attr_.openMicSpeaker;
    index = SwitchAdapterCapture((struct AudioAdapterDescriptor *)&descs, size, adapterNameCase_, PORT_IN, audioPort);
    CHECK_AND_RETURN_RET_LOG(index >= 0, ERR_NOT_STARTED, "Switch Adapter Capture Fail");
    adapterDesc_ = descs[index];
    int32_t loadAdapter = audioManager_->LoadAdapter(audioManager_, &adapterDesc_, &audioAdapter_);
    CHECK_AND_RETURN_RET_LOG(loadAdapter == 0, ERR_NOT_STARTED, "Load Adapter Fail");

    CHECK_AND_RETURN_RET_LOG(audioAdapter_ != nullptr, ERR_NOT_STARTED, "Load audio device failed");

    // Inittialization port information, can fill through mode and other parameters
    int32_t initAllPorts = audioAdapter_->InitAllPorts(audioAdapter_);
    CHECK_AND_RETURN_RET_LOG(initAllPorts == 0, ERR_DEVICE_INIT, "InitAllPorts failed");
    bool tmp = CreateCapture(audioPort) == SUCCESS && PrepareMmapBuffer() == SUCCESS;
    CHECK_AND_RETURN_RET_LOG(tmp, ERR_NOT_STARTED, "Create capture failed");
    ret = SetInputRoute(static_cast<DeviceType>(attr_.deviceType));
    if (ret < 0) {
        AUDIO_WARNING_LOG("update route FAILED: %{public}d", ret);
    }
    capturerInited_ = true;

    return SUCCESS;
}

int32_t FastAudioCapturerSourceInner::CaptureFrame(char *frame, uint64_t requestBytes, uint64_t &replyBytes)
{
    AUDIO_ERR_LOG("CaptureFrame in fast mode is not supported!");
    return ERR_DEVICE_NOT_SUPPORTED;
}

int32_t FastAudioCapturerSourceInner::CheckPositionTime()
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

int32_t FastAudioCapturerSourceInner::Start(void)
{
    AUDIO_INFO_LOG("Start.");
#ifdef FEATURE_POWER_MANAGER
    if (keepRunningLock_ == nullptr) {
        keepRunningLock_ = PowerMgr::PowerMgrClient::GetInstance().CreateRunningLock("AudioFastCapturer",
            PowerMgr::RunningLockType::RUNNINGLOCK_BACKGROUND_AUDIO);
    }
    if (keepRunningLock_ != nullptr) {
        AUDIO_INFO_LOG("keepRunningLock lock");
        keepRunningLock_->Lock(RUNNINGLOCK_LOCK_TIMEOUTMS_LASTING); // -1 for lasting.
    } else {
        AUDIO_WARNING_LOG("keepRunningLock is null, capture can not work well!");
    }
#endif

    if (!started_) {
        int32_t ret = audioCapture_->Start(audioCapture_);
        if (ret < 0) {
            return ERR_NOT_STARTED;
        }
        int32_t err = CheckPositionTime();
        CHECK_AND_RETURN_RET_LOG(err == SUCCESS, ERR_NOT_STARTED,
            "CheckPositionTime failed!");
        started_ = true;
    }

    return SUCCESS;
}

int32_t FastAudioCapturerSourceInner::SetVolume(float left, float right)
{
    return ERR_DEVICE_NOT_SUPPORTED;
}

int32_t FastAudioCapturerSourceInner::GetVolume(float &left, float &right)
{
    return ERR_DEVICE_NOT_SUPPORTED;
}

int32_t FastAudioCapturerSourceInner::SetMute(bool isMute)
{
    AUDIO_ERR_LOG("SetMute in fast mode is not supported!");
    return ERR_DEVICE_NOT_SUPPORTED;
}

int32_t FastAudioCapturerSourceInner::GetMute(bool &isMute)
{
    AUDIO_ERR_LOG("SetMute in fast mode is not supported!");
    return ERR_DEVICE_NOT_SUPPORTED;
}

static int32_t SetInputPortPin(DeviceType inputDevice, AudioRouteNode &source)
{
    int32_t ret = SUCCESS;

    switch (inputDevice) {
        case DEVICE_TYPE_MIC:
        case DEVICE_TYPE_EARPIECE:
        case DEVICE_TYPE_SPEAKER:
            source.ext.device.type = PIN_IN_MIC;
            source.ext.device.desc = (char *)"pin_in_mic";
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
            source.ext.device.type = PIN_IN_HS_MIC;
            source.ext.device.desc = (char *)"pin_in_hs_mic";
            break;
        default:
            ret = ERR_NOT_SUPPORTED;
            break;
    }

    return ret;
}

int32_t FastAudioCapturerSourceInner::SetInputRoute(DeviceType inputDevice)
{
    AudioPortPin inputPortPin = PIN_IN_MIC;
    return SetInputRoute(inputDevice, inputPortPin);
}

int32_t FastAudioCapturerSourceInner::SetInputRoute(DeviceType inputDevice, AudioPortPin &inputPortPin)
{
    AudioRouteNode source = {};
    AudioRouteNode sink = {};
    int32_t ret = SetInputPortPin(inputDevice, source);
    if (ret != SUCCESS) {
        return ret;
    }
    inputPortPin = source.ext.device.type;
    AUDIO_INFO_LOG("Input PIN is: 0x%{public}X", inputPortPin);
    source.portId = static_cast<int32_t>(audioPort.portId);
    source.role = AUDIO_PORT_SOURCE_ROLE;
    source.type = AUDIO_PORT_DEVICE_TYPE;
    source.ext.device.moduleId = 0;
    source.ext.device.desc = (char *)"";

    sink.portId = 0;
    sink.role = AUDIO_PORT_SINK_ROLE;
    sink.type = AUDIO_PORT_MIX_TYPE;
    sink.ext.mix.moduleId = 0;
    sink.ext.mix.streamId = FAST_INPUT_STREAM_ID;
    sink.ext.device.desc = (char *)"";

    AudioRoute route = {
        .sources = &source,
        .sourcesLen = 1,
        .sinks = &sink,
        .sinksLen = 1,
    };

    CHECK_AND_RETURN_RET_LOG(audioAdapter_ != nullptr, ERR_OPERATION_FAILED,
        "AudioAdapter object is null.");

    ret = audioAdapter_->UpdateAudioRoute(audioAdapter_, &route, &routeHandle_);
    return (ret == SUCCESS) ? SUCCESS : ERR_OPERATION_FAILED;
}

int32_t FastAudioCapturerSourceInner::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    return ERR_DEVICE_NOT_SUPPORTED;
}

uint64_t FastAudioCapturerSourceInner::GetTransactionId()
{
    AUDIO_INFO_LOG("FastAudioCapturerSourceInner::GetTransactionId in");
    return reinterpret_cast<uint64_t>(audioCapture_);
}

int32_t FastAudioCapturerSourceInner::GetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec)
{
    return ERR_DEVICE_NOT_SUPPORTED;
}

void FastAudioCapturerSourceInner::RegisterWakeupCloseCallback(IAudioSourceCallback *callback)
{
    AUDIO_ERR_LOG("RegisterWakeupCloseCallback FAILED");
}

void FastAudioCapturerSourceInner::RegisterAudioCapturerSourceCallback(IAudioSourceCallback *callback)
{
    AUDIO_ERR_LOG("RegisterAudioCapturerSourceCallback FAILED");
}

void FastAudioCapturerSourceInner::RegisterParameterCallback(IAudioSourceCallback *callback)
{
    AUDIO_ERR_LOG("RegisterParameterCallback in fast mode is not supported!");
}

int32_t FastAudioCapturerSourceInner::Stop(void)
{
    AUDIO_INFO_LOG("Enter");
#ifdef FEATURE_POWER_MANAGER
    if (keepRunningLock_ != nullptr) {
        AUDIO_INFO_LOG("keepRunningLock unLock");
        keepRunningLock_->UnLock();
    } else {
        AUDIO_WARNING_LOG("keepRunningLock is null, capture can not work well!");
    }
#endif

    if (started_ && audioCapture_ != nullptr) {
        int32_t ret = audioCapture_->Stop(audioCapture_);
        CHECK_AND_RETURN_RET_LOG(ret >= 0, ERR_OPERATION_FAILED, "Stop capture Failed");
    }
    started_ = false;

    return SUCCESS;
}

int32_t FastAudioCapturerSourceInner::Pause(void)
{
    if (started_ && audioCapture_ != nullptr) {
        int32_t ret = audioCapture_->Pause(audioCapture_);
        CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_OPERATION_FAILED, "pause capture Failed");
    }
    paused_ = true;

    return SUCCESS;
}

int32_t FastAudioCapturerSourceInner::Resume(void)
{
    if (paused_ && audioCapture_ != nullptr) {
        int32_t ret = audioCapture_->Resume(audioCapture_);
        CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_OPERATION_FAILED, "resume capture Failed");
    }
    paused_ = false;

    return SUCCESS;
}

int32_t FastAudioCapturerSourceInner::Reset(void)
{
    if (started_ && audioCapture_ != nullptr) {
        audioCapture_->Flush(audioCapture_);
    }

    return SUCCESS;
}

int32_t FastAudioCapturerSourceInner::Flush(void)
{
    if (started_ && audioCapture_ != nullptr) {
        audioCapture_->Flush(audioCapture_);
    }

    return SUCCESS;
}
} // namespace AudioStandard
} // namesapce OHOS
