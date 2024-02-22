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

#include "audio_endpoint.h"

#include <atomic>
#include <cinttypes>
#include <condition_variable>
#include <thread>
#include <vector>
#include <mutex>

#include "securec.h"

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_schedule.h"
#include "audio_utils.h"
#include "fast_audio_renderer_sink.h"
#include "fast_audio_capturer_source.h"
#include "i_audio_capturer_source.h"
#include "linear_pos_time_model.h"
#include "policy_handler.h"
#include "remote_fast_audio_renderer_sink.h"
#include "remote_fast_audio_capturer_source.h"

namespace OHOS {
namespace AudioStandard {
namespace {
    static constexpr int32_t VOLUME_SHIFT_NUMBER = 16; // 1 >> 16 = 65536, max volume
    static constexpr int64_t MAX_SPAN_DURATION_IN_NANO = 100000000; // 100ms
    static constexpr int32_t SLEEP_TIME_IN_DEFAULT = 400; // 400ms
    static constexpr int64_t DELTA_TO_REAL_READ_START_TIME = 0; // 0ms
}

static enum HdiAdapterFormat ConvertToHdiAdapterFormat(AudioSampleFormat format)
{
    enum HdiAdapterFormat adapterFormat;
    switch (format) {
        case AudioSampleFormat::SAMPLE_U8:
            adapterFormat = HdiAdapterFormat::SAMPLE_U8;
            break;
        case AudioSampleFormat::SAMPLE_S16LE:
            adapterFormat = HdiAdapterFormat::SAMPLE_S16;
            break;
        case AudioSampleFormat::SAMPLE_S24LE:
            adapterFormat = HdiAdapterFormat::SAMPLE_S24;
            break;
        case AudioSampleFormat::SAMPLE_S32LE:
            adapterFormat = HdiAdapterFormat::SAMPLE_S32;
            break;
        default:
            adapterFormat = HdiAdapterFormat::INVALID_WIDTH;
            break;
    }

    return adapterFormat;
}

class AudioEndpointInner : public AudioEndpoint {
public:
    explicit AudioEndpointInner(EndpointType type, uint64_t id);
    ~AudioEndpointInner();

    bool Config(const DeviceInfo &deviceInfo) override;
    bool StartDevice();
    bool StopDevice();

    // when audio process start.
    int32_t OnStart(IAudioProcessStream *processStream) override;
    // when audio process pause.
    int32_t OnPause(IAudioProcessStream *processStream) override;
    // when audio process request update handle info.
    int32_t OnUpdateHandleInfo(IAudioProcessStream *processStream) override;

    /**
     * Call LinkProcessStream when first create process or link other process with this endpoint.
     * Here are cases:
     *   case1: endpointStatus_ = UNLINKED, link not running process; UNLINKED-->IDEL & godown
     *   case2: endpointStatus_ = UNLINKED, link running process; UNLINKED-->IDEL & godown
     *   case3: endpointStatus_ = IDEL, link not running process; IDEL-->IDEL
     *   case4: endpointStatus_ = IDEL, link running process; IDEL-->STARTING-->RUNNING
     *   case5: endpointStatus_ = RUNNING; RUNNING-->RUNNING
    */
    int32_t LinkProcessStream(IAudioProcessStream *processStream) override;
    int32_t UnlinkProcessStream(IAudioProcessStream *processStream) override;

    int32_t GetPreferBufferInfo(uint32_t &totalSizeInframe, uint32_t &spanSizeInframe) override;

    void Dump(std::stringstream &dumpStringStream) override;

    std::string GetEndpointName() override;
    EndpointType GetEndpointType() override
    {
        return endpointType_;
    }
    int32_t SetVolume(AudioStreamType streamType, float volume) override;

    int32_t ResolveBuffer(std::shared_ptr<OHAudioBuffer> &buffer) override;

    std::shared_ptr<OHAudioBuffer> GetBuffer() override
    {
        return dstAudioBuffer_;
    }

    EndpointStatus GetStatus() override;

    void Release() override;

private:
    bool ConfigInputPoint(const DeviceInfo &deviceInfo);
    int32_t PrepareDeviceBuffer(const DeviceInfo &deviceInfo);
    int32_t GetAdapterBufferInfo(const DeviceInfo &deviceInfo);
    void ReSyncPosition();
    void RecordReSyncPosition();
    void InitAudiobuffer(bool resetReadWritePos);
    void ProcessData(const std::vector<AudioStreamData> &srcDataList, const AudioStreamData &dstData);
    int64_t GetPredictNextReadTime(uint64_t posInFrame);
    int64_t GetPredictNextWriteTime(uint64_t posInFrame);
    bool PrepareNextLoop(uint64_t curWritePos, int64_t &wakeUpTime);
    bool RecordPrepareNextLoop(uint64_t curReadPos, int64_t &wakeUpTime);

    /**
     * @brief Get the current read position in frame and the read-time with it.
     *
     * @param frames the read position in frame
     * @param nanoTime the time in nanosecond when device-sink start read the buffer
    */
    bool GetDeviceHandleInfo(uint64_t &frames, int64_t &nanoTime);
    int32_t GetProcLastWriteDoneInfo(const std::shared_ptr<OHAudioBuffer> processBuffer, uint64_t curWriteFrame,
        uint64_t &proHandleFrame, int64_t &proHandleTime);

    bool IsAnyProcessRunning();
    bool CheckAllBufferReady(int64_t checkTime, uint64_t curWritePos);
    bool ProcessToEndpointDataHandle(uint64_t curWritePos);
    void GetAllReadyProcessData(std::vector<AudioStreamData> &audioDataList);

    std::string GetStatusStr(EndpointStatus status);

    int32_t WriteToSpecialProcBuf(const std::shared_ptr<OHAudioBuffer> &procBuf, const BufferDesc &readBuf);
    void WriteToProcessBuffers(const BufferDesc &readBuf);
    int32_t ReadFromEndpoint(uint64_t curReadPos);
    bool KeepWorkloopRunning();

    void EndpointWorkLoopFuc();
    void RecordEndpointWorkLoopFuc();

    // Call GetMmapHandlePosition in ipc may block more than a cycle, call it in another thread.
    void AsyncGetPosTime();

private:
    static constexpr int64_t ONE_MILLISECOND_DURATION = 1000000; // 1ms
    static constexpr int64_t THREE_MILLISECOND_DURATION = 3000000; // 3ms
    static constexpr int64_t WRITE_TO_HDI_AHEAD_TIME = -1000000; // ahead 1ms
    static constexpr int32_t UPDATE_THREAD_TIMEOUT = 1000; // 1000ms
    enum ThreadStatus : uint32_t {
        WAITTING = 0,
        SLEEPING,
        INRUNNING
    };
    // SamplingRate EncodingType SampleFormat Channel
    DeviceInfo deviceInfo_;
    AudioStreamInfo dstStreamInfo_;
    EndpointType endpointType_;
    int32_t id_ = 0;
    std::mutex listLock_;
    std::vector<IAudioProcessStream *> processList_;
    std::vector<std::shared_ptr<OHAudioBuffer>> processBufferList_;

    std::atomic<bool> isInited_ = false;

    IMmapAudioRendererSink *fastSink_ = nullptr;
    IMmapAudioCapturerSource *fastSource_ = nullptr;

    LinearPosTimeModel readTimeModel_;
    LinearPosTimeModel writeTimeModel_;

    int64_t spanDuration_ = 0; // nano second
    int64_t serverAheadReadTime_ = 0;
    int dstBufferFd_ = -1; // -1: invalid fd.
    uint32_t dstTotalSizeInframe_ = 0;
    uint32_t dstSpanSizeInframe_ = 0;
    uint32_t dstByteSizePerFrame_ = 0;
    std::shared_ptr<OHAudioBuffer> dstAudioBuffer_ = nullptr;

    std::atomic<EndpointStatus> endpointStatus_ = INVALID;

    std::atomic<ThreadStatus> threadStatus_ = WAITTING;
    std::thread endpointWorkThread_;
    std::mutex loopThreadLock_;
    std::condition_variable workThreadCV_;
    int64_t lastHandleProcessTime_ = 0;

    std::thread updatePosTimeThread_;
    std::mutex updateThreadLock_;
    std::condition_variable updateThreadCV_;
    std::atomic<bool> stopUpdateThread_ = false;

    std::atomic<uint64_t> posInFrame_ = 0;
    std::atomic<int64_t> timeInNano_ = 0;

    bool isDeviceRunningInIdel_ = true; // will call start sink when linked.
    bool needReSyncPosition_ = true;
    FILE *dumpDcp_ = nullptr;
    FILE *dumpHdi_ = nullptr;
};

std::shared_ptr<AudioEndpoint> AudioEndpoint::CreateEndpoint(EndpointType type, uint64_t id,
    AudioStreamType streamType, const DeviceInfo &deviceInfo)
{
    std::shared_ptr<AudioEndpoint> audioEndpoint = nullptr;
    if (type == EndpointType::TYPE_INDEPENDENT && deviceInfo.deviceRole != INPUT_DEVICE &&
         deviceInfo.networkId == LOCAL_NETWORK_ID) {
        audioEndpoint = std::make_shared<AudioEndpointSeparate>(type, id, streamType);
    } else {
        audioEndpoint = std::make_shared<AudioEndpointInner>(type, id);
    }
    CHECK_AND_RETURN_RET_LOG(audioEndpoint != nullptr, nullptr, "Create AudioEndpoint failed.");

    if (!audioEndpoint->Config(deviceInfo)) {
        AUDIO_ERR_LOG("Config AudioEndpoint failed.");
        audioEndpoint = nullptr;
    }
    return audioEndpoint;
}

AudioEndpointInner::AudioEndpointInner(EndpointType type, uint64_t id) : endpointType_(type), id_(id)
{
    AUDIO_INFO_LOG("AudioEndpoint type:%{public}d", endpointType_);
}

std::string AudioEndpointInner::GetEndpointName()
{
    // temp method to get device key, should be same with AudioService::GetAudioEndpointForDevice.
    return deviceInfo_.networkId + std::to_string(deviceInfo_.deviceId) + "_" + std::to_string(id_);
}

int32_t AudioEndpointInner::SetVolume(AudioStreamType streamType, float volume)
{
    // No need set hdi volume in shared stream mode.
    return SUCCESS;
}

int32_t AudioEndpointInner::ResolveBuffer(std::shared_ptr<OHAudioBuffer> &buffer)
{
    return SUCCESS;
}

AudioEndpoint::EndpointStatus AudioEndpointInner::GetStatus()
{
    AUDIO_INFO_LOG("AudioEndpoint get status:%{public}s", GetStatusStr(endpointStatus_).c_str());
    return endpointStatus_.load();
}

void AudioEndpointInner::Release()
{
    // Wait for thread end and then clear other data to avoid using any cleared data in thread.
    AUDIO_INFO_LOG("Release enter.");
    if (!isInited_.load()) {
        AUDIO_WARNING_LOG("already released");
        return;
    }

    isInited_.store(false);
    workThreadCV_.notify_all();
    if (endpointWorkThread_.joinable()) {
        AUDIO_DEBUG_LOG("AudioEndpoint join work thread start");
        endpointWorkThread_.join();
        AUDIO_DEBUG_LOG("AudioEndpoint join work thread end");
    }

    stopUpdateThread_.store(true);
    updateThreadCV_.notify_all();
    if (updatePosTimeThread_.joinable()) {
        AUDIO_DEBUG_LOG("AudioEndpoint join update thread start");
        updatePosTimeThread_.join();
        AUDIO_DEBUG_LOG("AudioEndpoint join update thread end");
    }

    if (fastSink_ != nullptr) {
        fastSink_->DeInit();
        fastSink_ = nullptr;
    }

    if (fastSource_ != nullptr) {
        fastSource_->DeInit();
        fastSource_ = nullptr;
    }

    endpointStatus_.store(INVALID);

    if (dstAudioBuffer_ != nullptr) {
        AUDIO_INFO_LOG("Set device buffer null");
        dstAudioBuffer_ = nullptr;
    }
    DumpFileUtil::CloseDumpFile(&dumpDcp_);
    DumpFileUtil::CloseDumpFile(&dumpHdi_);
}

AudioEndpointInner::~AudioEndpointInner()
{
    if (isInited_.load()) {
        AudioEndpointInner::Release();
    }
    AUDIO_INFO_LOG("~AudioEndpoint()");
}

void AudioEndpointInner::Dump(std::stringstream &dumpStringStream)
{
    // dump endpoint stream info
    dumpStringStream << std::endl << "Endpoint stream info:" << std::endl;
    dumpStringStream << " samplingRate:" << dstStreamInfo_.samplingRate << std::endl;
    dumpStringStream << " channels:" << dstStreamInfo_.channels << std::endl;
    dumpStringStream << " format:" << dstStreamInfo_.format << std::endl;

    // dump status info
    dumpStringStream << " Current endpoint status:" << GetStatusStr(endpointStatus_) << std::endl;
    if (dstAudioBuffer_ != nullptr) {
        dumpStringStream << " Currend hdi read position:" << dstAudioBuffer_->GetCurReadFrame() << std::endl;
        dumpStringStream << " Currend hdi write position:" << dstAudioBuffer_->GetCurWriteFrame() << std::endl;
    }

    // dump linked process info
    std::lock_guard<std::mutex> lock(listLock_);
    dumpStringStream << processBufferList_.size() << " linked process:" << std::endl;
    for (auto item : processBufferList_) {
        dumpStringStream << " process read position:" << item->GetCurReadFrame() << std::endl;
        dumpStringStream << " process write position:" << item->GetCurWriteFrame() << std::endl << std::endl;
    }
    dumpStringStream << std::endl;
}

bool AudioEndpointInner::ConfigInputPoint(const DeviceInfo &deviceInfo)
{
    AUDIO_INFO_LOG("ConfigInputPoint enter.");
    IAudioSourceAttr attr = {};
    attr.sampleRate = dstStreamInfo_.samplingRate;
    attr.channel = dstStreamInfo_.channels;
    attr.format = ConvertToHdiAdapterFormat(dstStreamInfo_.format);
    attr.deviceNetworkId = deviceInfo.networkId.c_str();
    attr.deviceType = deviceInfo.deviceType;

    if (deviceInfo.networkId == LOCAL_NETWORK_ID) {
        attr.adapterName = "primary";
        fastSource_ = FastAudioCapturerSource::GetInstance();
    } else {
        attr.adapterName = "remote";
        fastSource_ = RemoteFastAudioCapturerSource::GetInstance(deviceInfo.networkId);
    }
    CHECK_AND_RETURN_RET_LOG(fastSource_ != nullptr, false, "ConfigInputPoint GetInstance failed.");

    int32_t err = fastSource_->Init(attr);
    if (err != SUCCESS || !fastSource_->IsInited()) {
        AUDIO_ERR_LOG("init remote fast fail, err %{public}d.", err);
        fastSource_ = nullptr;
        return false;
    }
    if (PrepareDeviceBuffer(deviceInfo) != SUCCESS) {
        fastSource_->DeInit();
        fastSource_ = nullptr;
        return false;
    }

    bool ret = writeTimeModel_.ConfigSampleRate(dstStreamInfo_.samplingRate);
    CHECK_AND_RETURN_RET_LOG(ret != false, false, "Config LinearPosTimeModel failed.");

    endpointStatus_ = UNLINKED;
    isInited_.store(true);
    endpointWorkThread_ = std::thread(&AudioEndpointInner::RecordEndpointWorkLoopFuc, this);
    pthread_setname_np(endpointWorkThread_.native_handle(), "OS_AudioEpLoop");

    updatePosTimeThread_ = std::thread(&AudioEndpointInner::AsyncGetPosTime, this);
    pthread_setname_np(updatePosTimeThread_.native_handle(), "OS_AudioEpUpdate");

    DumpFileUtil::OpenDumpFile(DUMP_SERVER_PARA, DUMP_ENDPOINT_HDI_FILENAME, &dumpHdi_);
    return true;
}

bool AudioEndpointInner::Config(const DeviceInfo &deviceInfo)
{
    AUDIO_INFO_LOG("Config enter, deviceRole %{public}d.", deviceInfo.deviceRole);
    deviceInfo_ = deviceInfo;
    bool res = deviceInfo_.audioStreamInfo.CheckParams();
    CHECK_AND_RETURN_RET_LOG(res, false, "samplingRate or channels size is 0");

    dstStreamInfo_ = {
        *deviceInfo.audioStreamInfo.samplingRate.rbegin(),
        deviceInfo.audioStreamInfo.encoding,
        deviceInfo.audioStreamInfo.format,
        *deviceInfo.audioStreamInfo.channels.rbegin()
    };
    dstStreamInfo_.channelLayout = deviceInfo.audioStreamInfo.channelLayout;

    if (deviceInfo.deviceRole == INPUT_DEVICE) {
        return ConfigInputPoint(deviceInfo);
    }

    fastSink_ = deviceInfo.networkId != LOCAL_NETWORK_ID ?
        RemoteFastAudioRendererSink::GetInstance(deviceInfo.networkId) : FastAudioRendererSink::GetInstance();
    IAudioSinkAttr attr = {};
    attr.adapterName = "primary";
    attr.sampleRate = dstStreamInfo_.samplingRate; // 48000hz
    attr.channel = dstStreamInfo_.channels; // STEREO = 2
    attr.format = ConvertToHdiAdapterFormat(dstStreamInfo_.format); // SAMPLE_S16LE = 1
    attr.deviceNetworkId = deviceInfo.networkId.c_str();
    attr.deviceType = static_cast<int32_t>(deviceInfo.deviceType);

    fastSink_->Init(attr);
    if (!fastSink_->IsInited()) {
        fastSink_ = nullptr;
        return false;
    }
    if (PrepareDeviceBuffer(deviceInfo) != SUCCESS) {
        fastSink_->DeInit();
        fastSink_ = nullptr;
        return false;
    }

    float initVolume = 1.0; // init volume to 1.0
    fastSink_->SetVolume(initVolume, initVolume);

    bool ret = readTimeModel_.ConfigSampleRate(dstStreamInfo_.samplingRate);
    CHECK_AND_RETURN_RET_LOG(ret != false, false, "Config LinearPosTimeModel failed.");

    endpointStatus_ = UNLINKED;
    isInited_.store(true);
    endpointWorkThread_ = std::thread(&AudioEndpointInner::EndpointWorkLoopFuc, this);
    pthread_setname_np(endpointWorkThread_.native_handle(), "OS_AudioEpLoop");

    updatePosTimeThread_ = std::thread(&AudioEndpointInner::AsyncGetPosTime, this);
    pthread_setname_np(updatePosTimeThread_.native_handle(), "OS_AudioEpUpdate");

    DumpFileUtil::OpenDumpFile(DUMP_SERVER_PARA, DUMP_ENDPOINT_HDI_FILENAME, &dumpHdi_);
    DumpFileUtil::OpenDumpFile(DUMP_SERVER_PARA, DUMP_ENDPOINT_DCP_FILENAME, &dumpDcp_);
    return true;
}

int32_t AudioEndpointInner::GetAdapterBufferInfo(const DeviceInfo &deviceInfo)
{
    int32_t ret = 0;
    AUDIO_INFO_LOG("GetAdapterBufferInfo enter, deviceRole %{public}d.", deviceInfo.deviceRole);
    if (deviceInfo.deviceRole == INPUT_DEVICE) {
        CHECK_AND_RETURN_RET_LOG(fastSource_ != nullptr, ERR_INVALID_HANDLE,
            "fast source is null.");
        ret = fastSource_->GetMmapBufferInfo(dstBufferFd_, dstTotalSizeInframe_, dstSpanSizeInframe_,
        dstByteSizePerFrame_);
    } else {
        CHECK_AND_RETURN_RET_LOG(fastSink_ != nullptr, ERR_INVALID_HANDLE, "fast sink is null.");
        ret = fastSink_->GetMmapBufferInfo(dstBufferFd_, dstTotalSizeInframe_, dstSpanSizeInframe_,
        dstByteSizePerFrame_);
    }

    if (ret != SUCCESS || dstBufferFd_ == -1 || dstTotalSizeInframe_ == 0 || dstSpanSizeInframe_ == 0 ||
        dstByteSizePerFrame_ == 0) {
        AUDIO_ERR_LOG("get mmap buffer info fail, ret %{public}d, dstBufferFd %{public}d, \
            dstTotalSizeInframe %{public}d, dstSpanSizeInframe %{public}d, dstByteSizePerFrame %{public}d.",
            ret, dstBufferFd_, dstTotalSizeInframe_, dstSpanSizeInframe_, dstByteSizePerFrame_);
        return ERR_ILLEGAL_STATE;
    }
    AUDIO_DEBUG_LOG("end, fd %{public}d.", dstBufferFd_);
    return SUCCESS;
}

int32_t AudioEndpointInner::PrepareDeviceBuffer(const DeviceInfo &deviceInfo)
{
    AUDIO_INFO_LOG("enter, deviceRole %{public}d.", deviceInfo.deviceRole);
    if (dstAudioBuffer_ != nullptr) {
        AUDIO_INFO_LOG("endpoint buffer is preapred, fd:%{public}d", dstBufferFd_);
        return SUCCESS;
    }

    int32_t ret = GetAdapterBufferInfo(deviceInfo);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ERR_OPERATION_FAILED,
        "get adapter buffer Info fail, ret %{public}d.", ret);

    // spanDuration_ may be less than the correct time of dstSpanSizeInframe_.
    spanDuration_ = dstSpanSizeInframe_ * AUDIO_NS_PER_SECOND / dstStreamInfo_.samplingRate;
    int64_t temp = spanDuration_ / 5 * 3; // 3/5 spanDuration
    serverAheadReadTime_ = temp < ONE_MILLISECOND_DURATION ? ONE_MILLISECOND_DURATION : temp; // at least 1ms ahead.
    AUDIO_DEBUG_LOG("panDuration %{public}" PRIu64" ns, serverAheadReadTime %{public}" PRIu64" ns.",
        spanDuration_, serverAheadReadTime_);

    CHECK_AND_RETURN_RET_LOG(spanDuration_ > 0 && spanDuration_ < MAX_SPAN_DURATION_IN_NANO,
        ERR_INVALID_PARAM, "mmap span info error, spanDuration %{public}" PRIu64".", spanDuration_);
    dstAudioBuffer_ = OHAudioBuffer::CreateFromRemote(dstTotalSizeInframe_, dstSpanSizeInframe_, dstByteSizePerFrame_,
        AUDIO_SERVER_ONLY, dstBufferFd_, OHAudioBuffer::INVALID_BUFFER_FD);
    CHECK_AND_RETURN_RET_LOG(dstAudioBuffer_ != nullptr && dstAudioBuffer_->GetBufferHolder() ==
        AudioBufferHolder::AUDIO_SERVER_ONLY, ERR_ILLEGAL_STATE, "create buffer from remote fail.");
    dstAudioBuffer_->GetStreamStatus()->store(StreamStatus::STREAM_IDEL);

    // clear data buffer
    ret = memset_s(dstAudioBuffer_->GetDataBase(), dstAudioBuffer_->GetDataSize(), 0, dstAudioBuffer_->GetDataSize());
    if (ret != EOK) {
        AUDIO_WARNING_LOG("memset buffer fail, ret %{public}d, fd %{public}d.", ret, dstBufferFd_);
    }
    InitAudiobuffer(true);

    AUDIO_DEBUG_LOG("end, fd %{public}d.", dstBufferFd_);
    return SUCCESS;
}

void AudioEndpointInner::InitAudiobuffer(bool resetReadWritePos)
{
    CHECK_AND_RETURN_LOG((dstAudioBuffer_ != nullptr), "dst audio buffer is null.");
    if (resetReadWritePos) {
        dstAudioBuffer_->ResetCurReadWritePos(0, 0);
    }

    uint32_t spanCount = dstAudioBuffer_->GetSpanCount();
    for (uint32_t i = 0; i < spanCount; i++) {
        SpanInfo *spanInfo = dstAudioBuffer_->GetSpanInfoByIndex(i);
        CHECK_AND_RETURN_LOG(spanInfo != nullptr, "InitAudiobuffer failed.");
        if (deviceInfo_.deviceRole == INPUT_DEVICE) {
            spanInfo->spanStatus = SPAN_WRITE_DONE;
        } else {
            spanInfo->spanStatus = SPAN_READ_DONE;
        }
        spanInfo->offsetInFrame = 0;

        spanInfo->readStartTime = 0;
        spanInfo->readDoneTime = 0;

        spanInfo->writeStartTime = 0;
        spanInfo->writeDoneTime = 0;

        spanInfo->volumeStart = 1 << VOLUME_SHIFT_NUMBER; // 65536 for initialize
        spanInfo->volumeEnd = 1 << VOLUME_SHIFT_NUMBER; // 65536 for initialize
        spanInfo->isMute = false;
    }
    return;
}

int32_t AudioEndpointInner::GetPreferBufferInfo(uint32_t &totalSizeInframe, uint32_t &spanSizeInframe)
{
    totalSizeInframe = dstTotalSizeInframe_;
    spanSizeInframe = dstSpanSizeInframe_;
    return SUCCESS;
}

bool AudioEndpointInner::IsAnyProcessRunning()
{
    std::lock_guard<std::mutex> lock(listLock_);
    bool isRunning = false;
    for (size_t i = 0; i < processBufferList_.size(); i++) {
        if (processBufferList_[i]->GetStreamStatus()->load() == STREAM_RUNNING) {
            isRunning = true;
            break;
        }
    }
    return isRunning;
}

void AudioEndpointInner::RecordReSyncPosition()
{
    AUDIO_INFO_LOG("RecordReSyncPosition enter.");
    uint64_t curHdiWritePos = 0;
    int64_t writeTime = 0;
    CHECK_AND_RETURN_LOG(GetDeviceHandleInfo(curHdiWritePos, writeTime),
        "get device handle info fail.");
    AUDIO_DEBUG_LOG("get capturer info, curHdiWritePos %{public}" PRIu64", writeTime %{public}" PRId64".",
        curHdiWritePos, writeTime);
    int64_t temp = ClockTime::GetCurNano() - writeTime;
    if (temp > spanDuration_) {
        AUDIO_WARNING_LOG("GetDeviceHandleInfo cost long time %{public}" PRIu64".", temp);
    }

    writeTimeModel_.ResetFrameStamp(curHdiWritePos, writeTime);
    uint64_t nextDstReadPos = curHdiWritePos;
    uint64_t nextDstWritePos = curHdiWritePos;
    InitAudiobuffer(false);
    int32_t ret = dstAudioBuffer_->ResetCurReadWritePos(nextDstReadPos, nextDstWritePos);
    CHECK_AND_RETURN_LOG(ret == SUCCESS, "ResetCurReadWritePos failed.");

    SpanInfo *nextReadSapn = dstAudioBuffer_->GetSpanInfo(nextDstReadPos);
    CHECK_AND_RETURN_LOG(nextReadSapn != nullptr, "GetSpanInfo failed.");
    nextReadSapn->offsetInFrame = nextDstReadPos;
    nextReadSapn->spanStatus = SpanStatus::SPAN_WRITE_DONE;
}

void AudioEndpointInner::ReSyncPosition()
{
    Trace loopTrace("AudioEndpoint::ReSyncPosition");
    uint64_t curHdiReadPos = 0;
    int64_t readTime = 0;
    bool res = GetDeviceHandleInfo(curHdiReadPos, readTime);
    CHECK_AND_RETURN_LOG(res, "ReSyncPosition call GetDeviceHandleInfo failed.");
    int64_t curTime = ClockTime::GetCurNano();
    int64_t temp = curTime - readTime;
    if (temp > spanDuration_) {
        AUDIO_ERR_LOG("GetDeviceHandleInfo may cost long time.");
    }

    readTimeModel_.ResetFrameStamp(curHdiReadPos, readTime);
    uint64_t nextDstWritePos = curHdiReadPos + dstSpanSizeInframe_;
    InitAudiobuffer(false);
    int32_t ret = dstAudioBuffer_->ResetCurReadWritePos(nextDstWritePos, nextDstWritePos);
    CHECK_AND_RETURN_LOG(ret == SUCCESS, "ResetCurReadWritePos failed.");

    SpanInfo *nextWriteSapn = dstAudioBuffer_->GetSpanInfo(nextDstWritePos);
    CHECK_AND_RETURN_LOG(nextWriteSapn != nullptr, "GetSpanInfo failed.");
    nextWriteSapn->offsetInFrame = nextDstWritePos;
    nextWriteSapn->spanStatus = SpanStatus::SPAN_READ_DONE;
    return;
}

bool AudioEndpointInner::StartDevice()
{
    AUDIO_INFO_LOG("StartDevice enter.");
    // how to modify the status while unlinked and started?
    CHECK_AND_RETURN_RET_LOG(endpointStatus_ == IDEL, false,
        "Endpoint status is not IDEL");
    endpointStatus_ = STARTING;
    bool isStartSuccess = true;
    if (deviceInfo_.deviceRole == INPUT_DEVICE) {
        CHECK_AND_RETURN_RET_LOG(fastSource_ != nullptr && fastSource_->Start() == SUCCESS,
            false, "Source start failed.");
    } else {
        if (fastSink_ == nullptr || fastSink_->Start() != SUCCESS) {
            AUDIO_ERR_LOG("Sink start failed.");
            isStartSuccess = false;
        }
    }

    if (isStartSuccess == false) {
        endpointStatus_ = IDEL;
        workThreadCV_.notify_all();
        return false;
    }

    std::unique_lock<std::mutex> lock(loopThreadLock_);
    needReSyncPosition_ = true;
    endpointStatus_ = IsAnyProcessRunning() ? RUNNING : IDEL;
    workThreadCV_.notify_all();
    AUDIO_DEBUG_LOG("StartDevice out, status is %{public}s", GetStatusStr(endpointStatus_).c_str());
    return true;
}

bool AudioEndpointInner::StopDevice()
{
    AUDIO_INFO_LOG("StopDevice with status:%{public}s", GetStatusStr(endpointStatus_).c_str());
    // todo
    endpointStatus_ = STOPPING;
    // Clear data buffer to avoid noise in some case.
    if (dstAudioBuffer_ != nullptr) {
        int32_t ret = memset_s(dstAudioBuffer_->GetDataBase(), dstAudioBuffer_->GetDataSize(), 0,
            dstAudioBuffer_->GetDataSize());
        AUDIO_INFO_LOG("StopDevice clear buffer ret:%{public}d", ret);
    }
    if (deviceInfo_.deviceRole == INPUT_DEVICE) {
        CHECK_AND_RETURN_RET_LOG(fastSource_ != nullptr && fastSource_->Stop() == SUCCESS,
            false, "Source stop failed.");
    } else {
        CHECK_AND_RETURN_RET_LOG(fastSink_ != nullptr && fastSink_->Stop() == SUCCESS,
            false, "Sink stop failed.");
    }
    endpointStatus_ = STOPPED;
    return true;
}

int32_t AudioEndpointInner::OnStart(IAudioProcessStream *processStream)
{
    AUDIO_INFO_LOG("OnStart endpoint status:%{public}s", GetStatusStr(endpointStatus_).c_str());
    if (endpointStatus_ == RUNNING) {
        AUDIO_INFO_LOG("OnStart find endpoint already in RUNNING.");
        return SUCCESS;
    }
    if (endpointStatus_ == IDEL && !isDeviceRunningInIdel_) {
        // call sink start
        StartDevice();
        endpointStatus_ = RUNNING;
    }
    return SUCCESS;
}

int32_t AudioEndpointInner::OnPause(IAudioProcessStream *processStream)
{
    AUDIO_INFO_LOG("OnPause endpoint status:%{public}s", GetStatusStr(endpointStatus_).c_str());
    if (endpointStatus_ == RUNNING) {
        endpointStatus_ = IsAnyProcessRunning() ? RUNNING : IDEL;
    }
    if (endpointStatus_ == IDEL && !isDeviceRunningInIdel_) {
        // call sink stop when no process running?
        AUDIO_INFO_LOG("OnPause status is IDEL, call stop");
    }
    // todo
    return SUCCESS;
}

int32_t AudioEndpointInner::GetProcLastWriteDoneInfo(const std::shared_ptr<OHAudioBuffer> processBuffer,
    uint64_t curWriteFrame, uint64_t &proHandleFrame, int64_t &proHandleTime)
{
    CHECK_AND_RETURN_RET_LOG(processBuffer != nullptr, ERR_INVALID_HANDLE, "Process found but buffer is null");
    uint64_t curReadFrame = processBuffer->GetCurReadFrame();
    SpanInfo *curWriteSpan = processBuffer->GetSpanInfo(curWriteFrame);
    CHECK_AND_RETURN_RET_LOG(curWriteSpan != nullptr, ERR_INVALID_HANDLE,
        "curWriteSpan of curWriteFrame %{public}" PRIu64" is null", curWriteFrame);
    if (curWriteSpan->spanStatus == SpanStatus::SPAN_WRITE_DONE || curWriteFrame < dstSpanSizeInframe_ ||
        curWriteFrame < curReadFrame) {
        proHandleFrame = curWriteFrame;
        proHandleTime = curWriteSpan->writeDoneTime;
    } else {
        int32_t ret = GetProcLastWriteDoneInfo(processBuffer, curWriteFrame - dstSpanSizeInframe_,
            proHandleFrame, proHandleTime);
        CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret,
            "get process last write done info fail, ret %{public}d.", ret);
    }

    AUDIO_INFO_LOG("GetProcLastWriteDoneInfo end, curWriteFrame %{public}" PRIu64", proHandleFrame %{public}" PRIu64", "
        "proHandleTime %{public}" PRId64".", curWriteFrame, proHandleFrame, proHandleTime);
    return SUCCESS;
}

int32_t AudioEndpointInner::OnUpdateHandleInfo(IAudioProcessStream *processStream)
{
    Trace trace("AudioEndpoint::OnUpdateHandleInfo");
    bool isFind = false;
    std::lock_guard<std::mutex> lock(listLock_);
    auto processItr = processList_.begin();
    while (processItr != processList_.end()) {
        if (*processItr != processStream) {
            processItr++;
            continue;
        }
        std::shared_ptr<OHAudioBuffer> processBuffer = (*processItr)->GetStreamBuffer();
        CHECK_AND_RETURN_RET_LOG(processBuffer != nullptr, ERR_OPERATION_FAILED, "Process found but buffer is null");
        uint64_t proHandleFrame = 0;
        int64_t proHandleTime = 0;
        if (deviceInfo_.deviceRole == INPUT_DEVICE) {
            uint64_t curWriteFrame = processBuffer->GetCurWriteFrame();
            int32_t ret = GetProcLastWriteDoneInfo(processBuffer, curWriteFrame, proHandleFrame, proHandleTime);
            CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret,
                "get process last write done info fail, ret %{public}d.", ret);
            processBuffer->SetHandleInfo(proHandleFrame, proHandleTime);
        } else {
            // For output device, handle info is updated in CheckAllBufferReady
            processBuffer->GetHandleInfo(proHandleFrame, proHandleTime);
        }
        AUDIO_INFO_LOG("OnUpdateHandleInfo set process handle pos[%{public}" PRIu64"] time [%{public}" PRId64"], "
            "deviceRole %{public}d.", proHandleFrame, proHandleTime, deviceInfo_.deviceRole);
        isFind = true;
        break;
    }
    CHECK_AND_RETURN_RET_LOG(isFind, ERR_OPERATION_FAILED, "Can not find any process to UpdateHandleInfo");
    return SUCCESS;
}

int32_t AudioEndpointInner::LinkProcessStream(IAudioProcessStream *processStream)
{
    CHECK_AND_RETURN_RET_LOG(processStream != nullptr, ERR_INVALID_PARAM, "IAudioProcessStream is null");
    std::shared_ptr<OHAudioBuffer> processBuffer = processStream->GetStreamBuffer();
    CHECK_AND_RETURN_RET_LOG(processBuffer != nullptr, ERR_INVALID_PARAM, "processBuffer is null");

    CHECK_AND_RETURN_RET_LOG(processList_.size() < MAX_LINKED_PROCESS, ERR_OPERATION_FAILED, "reach link limit.");

    AUDIO_INFO_LOG("LinkProcessStream start status is:%{public}s.", GetStatusStr(endpointStatus_).c_str());

    bool needEndpointRunning = processBuffer->GetStreamStatus()->load() == STREAM_RUNNING;

    if (endpointStatus_ == STARTING) {
        AUDIO_INFO_LOG("LinkProcessStream wait start begin.");
        std::unique_lock<std::mutex> lock(loopThreadLock_);
        workThreadCV_.wait_for(lock, std::chrono::milliseconds(SLEEP_TIME_IN_DEFAULT), [this] {
            return endpointStatus_ != STARTING;
        });
        AUDIO_DEBUG_LOG("LinkProcessStream wait start end.");
    }

    if (endpointStatus_ == RUNNING) {
        std::lock_guard<std::mutex> lock(listLock_);
        processList_.push_back(processStream);
        processBufferList_.push_back(processBuffer);
        AUDIO_INFO_LOG("LinkProcessStream success in RUNNING.");
        return SUCCESS;
    }

    if (endpointStatus_ == UNLINKED) {
        endpointStatus_ = IDEL; // handle push_back in IDEL
        if (isDeviceRunningInIdel_) {
            StartDevice();
        }
    }

    if (endpointStatus_ == IDEL) {
        {
            std::lock_guard<std::mutex> lock(listLock_);
            processList_.push_back(processStream);
            processBufferList_.push_back(processBuffer);
        }
        if (!needEndpointRunning) {
            AUDIO_INFO_LOG("LinkProcessStream success, process stream status is not running.");
            return SUCCESS;
        }
        // needEndpointRunning = true
        if (isDeviceRunningInIdel_) {
            endpointStatus_ = IsAnyProcessRunning() ? RUNNING : IDEL;
        } else {
            // needEndpointRunning = true & isDeviceRunningInIdel_ = false
            // KeepWorkloopRunning will wait on IDEL
            StartDevice();
        }
        AUDIO_INFO_LOG("LinkProcessStream success with status:%{public}s", GetStatusStr(endpointStatus_).c_str());
        return SUCCESS;
    }

    AUDIO_INFO_LOG("LinkProcessStream success with status:%{public}s", GetStatusStr(endpointStatus_).c_str());
    return SUCCESS;
}

int32_t AudioEndpointInner::UnlinkProcessStream(IAudioProcessStream *processStream)
{
    AUDIO_INFO_LOG("UnlinkProcessStream in status:%{public}s.", GetStatusStr(endpointStatus_).c_str());
    CHECK_AND_RETURN_RET_LOG(processStream != nullptr, ERR_INVALID_PARAM, "IAudioProcessStream is null");
    std::shared_ptr<OHAudioBuffer> processBuffer = processStream->GetStreamBuffer();
    CHECK_AND_RETURN_RET_LOG(processBuffer != nullptr, ERR_INVALID_PARAM, "processBuffer is null");

    bool isFind = false;
    std::lock_guard<std::mutex> lock(listLock_);
    auto processItr = processList_.begin();
    auto bufferItr = processBufferList_.begin();
    while (processItr != processList_.end()) {
        if (*processItr == processStream && *bufferItr == processBuffer) {
            processList_.erase(processItr);
            processBufferList_.erase(bufferItr);
            isFind = true;
            break;
        } else {
            processItr++;
            bufferItr++;
        }
    }
    if (processList_.size() == 0) {
        StopDevice();
        endpointStatus_ = UNLINKED;
    }

    AUDIO_DEBUG_LOG("UnlinkProcessStream end, %{public}s the process.", (isFind ? "find and remove" : "not find"));
    return SUCCESS;
}

bool AudioEndpointInner::CheckAllBufferReady(int64_t checkTime, uint64_t curWritePos)
{
    bool isAllReady = true;
    {
        // lock list without sleep
        std::lock_guard<std::mutex> lock(listLock_);
        for (size_t i = 0; i < processBufferList_.size(); i++) {
            std::shared_ptr<OHAudioBuffer> tempBuffer = processBufferList_[i];
            uint64_t eachCurReadPos = processBufferList_[i]->GetCurReadFrame();
            lastHandleProcessTime_ = checkTime;
            processBufferList_[i]->SetHandleInfo(eachCurReadPos, lastHandleProcessTime_); // update handle info
            if (tempBuffer->GetStreamStatus()->load() != StreamStatus::STREAM_RUNNING) {
                // Process is not running, server will continue to check the same location in the next cycle.
                int64_t duration = 5000000; // 5ms
                processBufferList_[i]->SetHandleInfo(eachCurReadPos, lastHandleProcessTime_ + duration);
                continue; // process not running
            }
            uint64_t curRead = tempBuffer->GetCurReadFrame();
            SpanInfo *curReadSpan = tempBuffer->GetSpanInfo(curRead);
            if (curReadSpan == nullptr || curReadSpan->spanStatus != SpanStatus::SPAN_WRITE_DONE) {
                AUDIO_WARNING_LOG("Find one process not ready"); // print uid of the process?
                isAllReady = false;
                break;
            }
        }
    }

    if (!isAllReady) {
        Trace trace("AudioEndpoint::WaitAllProcessReady");
        int64_t tempWakeupTime = readTimeModel_.GetTimeOfPos(curWritePos) + WRITE_TO_HDI_AHEAD_TIME;
        if (tempWakeupTime - ClockTime::GetCurNano() < ONE_MILLISECOND_DURATION) {
            ClockTime::RelativeSleep(ONE_MILLISECOND_DURATION);
        } else {
            ClockTime::AbsoluteSleep(tempWakeupTime); // sleep to hdi read time ahead 1ms.
        }
    }
    return isAllReady;
}

void AudioEndpointInner::ProcessData(const std::vector<AudioStreamData> &srcDataList, const AudioStreamData &dstData)
{
    size_t srcListSize = srcDataList.size();

    for (size_t i = 0; i < srcListSize; i++) {
        if (srcDataList[i].streamInfo.format != SAMPLE_S16LE || srcDataList[i].streamInfo.channels != STEREO ||
            srcDataList[i].bufferDesc.bufLength != dstData.bufferDesc.bufLength ||
            srcDataList[i].bufferDesc.dataLength != dstData.bufferDesc.dataLength) {
            AUDIO_ERR_LOG("ProcessData failed, streamInfo are different");
            return;
        }
    }

    // Assum using the same format and same size
    CHECK_AND_RETURN_LOG(dstData.streamInfo.format == SAMPLE_S16LE && dstData.streamInfo.channels == STEREO,
        "ProcessData failed, streamInfo are not support");

    size_t dataLength = dstData.bufferDesc.dataLength;
    dataLength /= 2; // SAMPLE_S16LE--> 2 byte
    int16_t *dstPtr = reinterpret_cast<int16_t *>(dstData.bufferDesc.buffer);
    for (size_t offset = 0; dataLength > 0; dataLength--) {
        int32_t sum = 0;
        for (size_t i = 0; i < srcListSize; i++) {
            int32_t vol = srcDataList[i].volumeStart; // change to modify volume of each channel
            int16_t *srcPtr = reinterpret_cast<int16_t *>(srcDataList[i].bufferDesc.buffer) + offset;
            sum += (*srcPtr * static_cast<int64_t>(vol)) >> VOLUME_SHIFT_NUMBER; // 1/65536
        }
        offset++;
        *dstPtr++ = sum > INT16_MAX ? INT16_MAX : (sum < INT16_MIN ? INT16_MIN : sum);
    }
}

// call with listLock_ hold
void AudioEndpointInner::GetAllReadyProcessData(std::vector<AudioStreamData> &audioDataList)
{
    for (size_t i = 0; i < processBufferList_.size(); i++) {
        uint64_t curRead = processBufferList_[i]->GetCurReadFrame();
        Trace trace("AudioEndpoint::ReadProcessData->" + std::to_string(curRead));
        SpanInfo *curReadSpan = processBufferList_[i]->GetSpanInfo(curRead);
        CHECK_AND_CONTINUE_LOG(curReadSpan != nullptr, "GetSpanInfo failed, can not get client curReadSpan");
        AudioStreamData streamData;
        Volume vol = {true, 1.0f, 0};
        AudioStreamType streamType = processList_[i]->GetAudioStreamType();
        AudioVolumeType volumeType = PolicyHandler::GetInstance().GetVolumeTypeFromStreamType(streamType);
        DeviceType deviceType = PolicyHandler::GetInstance().GetActiveOutPutDevice();
        if (deviceInfo_.networkId == LOCAL_NETWORK_ID &&
            PolicyHandler::GetInstance().GetSharedVolume(volumeType, deviceType, vol)) {
            streamData.volumeStart = vol.isMute ? 0 : static_cast<int32_t>(curReadSpan->volumeStart * vol.volumeFloat);
        } else {
            streamData.volumeStart = curReadSpan->volumeStart;
        }
        streamData.volumeEnd = curReadSpan->volumeEnd;
        streamData.streamInfo = processList_[i]->GetStreamInfo();
        SpanStatus targetStatus = SpanStatus::SPAN_WRITE_DONE;
        if (curReadSpan->spanStatus.compare_exchange_strong(targetStatus, SpanStatus::SPAN_READING)) {
            processBufferList_[i]->GetReadbuffer(curRead, streamData.bufferDesc); // check return?
            audioDataList.push_back(streamData);
            curReadSpan->readStartTime = ClockTime::GetCurNano();
            DumpFileUtil::WriteDumpFile(dumpDcp_, static_cast<void *>(streamData.bufferDesc.buffer),
                streamData.bufferDesc.bufLength);
        }
    }
}

bool AudioEndpointInner::ProcessToEndpointDataHandle(uint64_t curWritePos)
{
    std::lock_guard<std::mutex> lock(listLock_);

    std::vector<AudioStreamData> audioDataList;
    GetAllReadyProcessData(audioDataList);

    AudioStreamData dstStreamData;
    dstStreamData.streamInfo = dstStreamInfo_;
    int32_t ret = dstAudioBuffer_->GetWriteBuffer(curWritePos, dstStreamData.bufferDesc);
    CHECK_AND_RETURN_RET_LOG(((ret == SUCCESS && dstStreamData.bufferDesc.buffer != nullptr)), false,
        "GetWriteBuffer failed, ret:%{public}d", ret);

    SpanInfo *curWriteSpan = dstAudioBuffer_->GetSpanInfo(curWritePos);
    CHECK_AND_RETURN_RET_LOG(curWriteSpan != nullptr, false, "GetSpanInfo failed, can not get curWriteSpan");

    dstStreamData.volumeStart = curWriteSpan->volumeStart;
    dstStreamData.volumeEnd = curWriteSpan->volumeEnd;

    Trace trace("AudioEndpoint::WriteDstBuffer=>" + std::to_string(curWritePos));
    // do write work
    if (audioDataList.size() == 0) {
        memset_s(dstStreamData.bufferDesc.buffer, dstStreamData.bufferDesc.bufLength, 0,
            dstStreamData.bufferDesc.bufLength);
    } else {
        ProcessData(audioDataList, dstStreamData);
    }

    DumpFileUtil::WriteDumpFile(dumpHdi_, static_cast<void *>(dstStreamData.bufferDesc.buffer),
        dstStreamData.bufferDesc.bufLength);
    return true;
}

int64_t AudioEndpointInner::GetPredictNextReadTime(uint64_t posInFrame)
{
    Trace trace("AudioEndpoint::GetPredictNextRead");
    uint64_t handleSpanCnt = posInFrame / dstSpanSizeInframe_;
    uint32_t startPeriodCnt = 20; // sync each time when start
    uint32_t oneBigPeriodCnt = 40; // 200ms
    if (handleSpanCnt < startPeriodCnt || handleSpanCnt % oneBigPeriodCnt == 0) {
        updateThreadCV_.notify_all();
    }
    uint64_t readFrame = 0;
    int64_t readtime = 0;
    if (readTimeModel_.GetFrameStamp(readFrame, readtime)) {
        if (readFrame != posInFrame_) {
            readTimeModel_.UpdataFrameStamp(posInFrame_, timeInNano_);
        }
    }

    int64_t nextHdiReadTime = readTimeModel_.GetTimeOfPos(posInFrame);
    return nextHdiReadTime;
}

int64_t AudioEndpointInner::GetPredictNextWriteTime(uint64_t posInFrame)
{
    uint64_t handleSpanCnt = posInFrame / dstSpanSizeInframe_;
    uint32_t startPeriodCnt = 20;
    uint32_t oneBigPeriodCnt = 40;
    if (handleSpanCnt < startPeriodCnt || handleSpanCnt % oneBigPeriodCnt == 0) {
        updateThreadCV_.notify_all();
    }
    uint64_t writeFrame = 0;
    int64_t writetime = 0;
    if (writeTimeModel_.GetFrameStamp(writeFrame, writetime)) {
        if (writeFrame != posInFrame_) {
            writeTimeModel_.UpdataFrameStamp(posInFrame_, timeInNano_);
        }
    }
    int64_t nextHdiWriteTime = writeTimeModel_.GetTimeOfPos(posInFrame);
    return nextHdiWriteTime;
}

bool AudioEndpointInner::RecordPrepareNextLoop(uint64_t curReadPos, int64_t &wakeUpTime)
{
    uint64_t nextHandlePos = curReadPos + dstSpanSizeInframe_;
    int64_t nextHdiWriteTime = GetPredictNextWriteTime(nextHandlePos);
    int64_t tempDelay = 4000000; // 4ms
    wakeUpTime = nextHdiWriteTime + tempDelay;

    int32_t ret = dstAudioBuffer_->SetCurWriteFrame(nextHandlePos);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, false, "set dst buffer write frame fail, ret %{public}d.", ret);
    ret = dstAudioBuffer_->SetCurReadFrame(nextHandlePos);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, false, "set dst buffer read frame fail, ret %{public}d.", ret);

    return true;
}

bool AudioEndpointInner::PrepareNextLoop(uint64_t curWritePos, int64_t &wakeUpTime)
{
    uint64_t nextHandlePos = curWritePos + dstSpanSizeInframe_;
    Trace prepareTrace("AudioEndpoint::PrepareNextLoop " + std::to_string(nextHandlePos));
    int64_t nextHdiReadTime = GetPredictNextReadTime(nextHandlePos);
    wakeUpTime = nextHdiReadTime - serverAheadReadTime_;

    SpanInfo *nextWriteSpan = dstAudioBuffer_->GetSpanInfo(nextHandlePos);
    CHECK_AND_RETURN_RET_LOG(nextWriteSpan != nullptr, false, "GetSpanInfo failed, can not get next write span");

    int32_t ret1 = dstAudioBuffer_->SetCurWriteFrame(nextHandlePos);
    int32_t ret2 = dstAudioBuffer_->SetCurReadFrame(nextHandlePos);
    CHECK_AND_RETURN_RET_LOG(ret1 == SUCCESS && ret2 == SUCCESS, false,
        "SetCurWriteFrame or SetCurReadFrame failed, ret1:%{public}d ret2:%{public}d", ret1, ret2);
    // handl each process buffer info
    int64_t curReadDoneTime = ClockTime::GetCurNano();
    for (size_t i = 0; i < processBufferList_.size(); i++) {
        uint64_t eachCurReadPos = processBufferList_[i]->GetCurReadFrame();
        SpanInfo *tempSpan = processBufferList_[i]->GetSpanInfo(eachCurReadPos);
        CHECK_AND_RETURN_RET_LOG(tempSpan != nullptr, false,
            "GetSpanInfo failed, can not get process read span");
        SpanStatus targetStatus = SpanStatus::SPAN_READING;
        if (tempSpan->spanStatus.compare_exchange_strong(targetStatus, SpanStatus::SPAN_READ_DONE)) {
            tempSpan->readDoneTime = curReadDoneTime;
            BufferDesc bufferReadDone = { nullptr, 0, 0};
            processBufferList_[i]->GetReadbuffer(eachCurReadPos, bufferReadDone);
            if (bufferReadDone.buffer != nullptr && bufferReadDone.bufLength != 0) {
                memset_s(bufferReadDone.buffer, bufferReadDone.bufLength, 0, bufferReadDone.bufLength);
            }
            processBufferList_[i]->SetCurReadFrame(eachCurReadPos + dstSpanSizeInframe_); // use client span size
        } else if (processBufferList_[i]->GetStreamStatus()->load() == StreamStatus::STREAM_RUNNING) {
            AUDIO_WARNING_LOG("Current %{public}" PRIu64" span not ready:%{public}d", eachCurReadPos, targetStatus);
        }
    }
    return true;
}

bool AudioEndpointInner::GetDeviceHandleInfo(uint64_t &frames, int64_t &nanoTime)
{
    Trace trace("AudioEndpoint::GetMmapHandlePosition");
    int64_t timeSec = 0;
    int64_t timeNanoSec = 0;
    int32_t ret = 0;
    if (deviceInfo_.deviceRole == INPUT_DEVICE) {
        CHECK_AND_RETURN_RET_LOG(fastSource_ != nullptr && fastSource_->IsInited(),
            false, "Source start failed.");
        // GetMmapHandlePosition will call using ipc.
        ret = fastSource_->GetMmapHandlePosition(frames, timeSec, timeNanoSec);
    } else {
        CHECK_AND_RETURN_RET_LOG(fastSink_ != nullptr && fastSink_->IsInited(),
            false, "GetDeviceHandleInfo failed: sink is not inited.");
        // GetMmapHandlePosition will call using ipc.
        ret = fastSink_->GetMmapHandlePosition(frames, timeSec, timeNanoSec);
    }
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, false, "Call adapter GetMmapHandlePosition failed: %{public}d", ret);
    trace.End();
    nanoTime = timeNanoSec + timeSec * AUDIO_NS_PER_SECOND;
    Trace infoTrace("AudioEndpoint::GetDeviceHandleInfo frames=>" + std::to_string(frames) + " " +
        std::to_string(nanoTime) + " at " + std::to_string(ClockTime::GetCurNano()));
    nanoTime += DELTA_TO_REAL_READ_START_TIME; // global delay in server
    return true;
}

void AudioEndpointInner::AsyncGetPosTime()
{
    AUDIO_INFO_LOG("AsyncGetPosTime thread start.");
    while (!stopUpdateThread_) {
        std::unique_lock<std::mutex> lock(updateThreadLock_);
        updateThreadCV_.wait_for(lock, std::chrono::milliseconds(UPDATE_THREAD_TIMEOUT));
        if (stopUpdateThread_) {
            break;
        }
        // get signaled, call get pos-time
        uint64_t curHdiHandlePos = posInFrame_;
        int64_t handleTime = timeInNano_;
        if (!GetDeviceHandleInfo(curHdiHandlePos, handleTime)) {
            AUDIO_WARNING_LOG("AsyncGetPosTime call GetDeviceHandleInfo failed.");
            continue;
        }
        // keep it
        if (posInFrame_ != curHdiHandlePos) {
            posInFrame_ = curHdiHandlePos;
            timeInNano_ = handleTime;
        }
    }
}

std::string AudioEndpointInner::GetStatusStr(EndpointStatus status)
{
    switch (status) {
        case INVALID:
            return "INVALID";
        case UNLINKED:
            return "UNLINKED";
        case IDEL:
            return "IDEL";
        case STARTING:
            return "STARTING";
        case RUNNING:
            return "RUNNING";
        case STOPPING:
            return "STOPPING";
        case STOPPED:
            return "STOPPED";
        default:
            break;
    }
    return "NO_SUCH_STATUS";
}

bool AudioEndpointInner::KeepWorkloopRunning()
{
    EndpointStatus targetStatus = INVALID;
    switch (endpointStatus_.load()) {
        case RUNNING:
            return true;
        case IDEL:
            if (isDeviceRunningInIdel_) {
                return true;
            } else {
                targetStatus = STARTING;
            }
            break;
        case UNLINKED:
            targetStatus = IDEL;
            break;
        case STARTING:
            targetStatus = RUNNING;
            break;
        case STOPPING:
            targetStatus = STOPPED;
            break;
        default:
            break;
    }

    // when return false, EndpointWorkLoopFuc will continue loop immediately. Wait to avoid a inifity loop.
    std::unique_lock<std::mutex> lock(loopThreadLock_);
    AUDIO_INFO_LOG("Status is %{public}s now, wait for %{public}s...", GetStatusStr(endpointStatus_).c_str(),
        GetStatusStr(targetStatus).c_str());
    threadStatus_ = WAITTING;
    workThreadCV_.wait_for(lock, std::chrono::milliseconds(SLEEP_TIME_IN_DEFAULT));
    AUDIO_DEBUG_LOG("Wait end. Cur is %{public}s now, target is %{public}s...", GetStatusStr(endpointStatus_).c_str(),
        GetStatusStr(targetStatus).c_str());

    return false;
}

int32_t AudioEndpointInner::WriteToSpecialProcBuf(const std::shared_ptr<OHAudioBuffer> &procBuf,
    const BufferDesc &readBuf)
{
    CHECK_AND_RETURN_RET_LOG(procBuf != nullptr, ERR_INVALID_HANDLE, "process buffer is null.");
    uint64_t curWritePos = procBuf->GetCurWriteFrame();
    Trace trace("AudioEndpoint::WriteProcessData-<" + std::to_string(curWritePos));
    SpanInfo *curWriteSpan = procBuf->GetSpanInfo(curWritePos);
    CHECK_AND_RETURN_RET_LOG(curWriteSpan != nullptr, ERR_INVALID_HANDLE,
        "get write span info of procBuf fail.");

    AUDIO_DEBUG_LOG("process buffer write start, curWritePos %{public}" PRIu64".", curWritePos);
    curWriteSpan->spanStatus.store(SpanStatus::SPAN_WRITTING);
    curWriteSpan->writeStartTime = ClockTime::GetCurNano();

    BufferDesc writeBuf;
    int32_t ret = procBuf->GetWriteBuffer(curWritePos, writeBuf);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "get write buffer fail, ret %{public}d.", ret);
    ret = memcpy_s(static_cast<void *>(writeBuf.buffer), writeBuf.bufLength,
        static_cast<void *>(readBuf.buffer), readBuf.bufLength);
    CHECK_AND_RETURN_RET_LOG(ret == EOK, ERR_WRITE_FAILED, "memcpy data to process buffer fail, "
        "curWritePos %{public}" PRIu64", ret %{public}d.", curWritePos, ret);

    curWriteSpan->writeDoneTime = ClockTime::GetCurNano();
    procBuf->SetHandleInfo(curWritePos, curWriteSpan->writeDoneTime);
    ret = procBuf->SetCurWriteFrame(curWritePos + dstSpanSizeInframe_);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "set procBuf next write frame fail, ret %{public}d.", ret);
    curWriteSpan->spanStatus.store(SpanStatus::SPAN_WRITE_DONE);
    return SUCCESS;
}

void AudioEndpointInner::WriteToProcessBuffers(const BufferDesc &readBuf)
{
    std::lock_guard<std::mutex> lock(listLock_);
    for (size_t i = 0; i < processBufferList_.size(); i++) {
        CHECK_AND_CONTINUE_LOG(processBufferList_[i] != nullptr,
            "process buffer %{public}zu is null.", i);
        if (processBufferList_[i]->GetStreamStatus()->load() != STREAM_RUNNING) {
            AUDIO_WARNING_LOG("process buffer %{public}zu not running, stream status %{public}d.",
                i, processBufferList_[i]->GetStreamStatus()->load());
            continue;
        }

        int32_t ret = WriteToSpecialProcBuf(processBufferList_[i], readBuf);
        CHECK_AND_CONTINUE_LOG(ret == SUCCESS,
            "endpoint write to process buffer %{public}zu fail, ret %{public}d.", i, ret);
        AUDIO_DEBUG_LOG("endpoint process buffer %{public}zu write success.", i);
    }
}

int32_t AudioEndpointInner::ReadFromEndpoint(uint64_t curReadPos)
{
    Trace trace("AudioEndpoint::ReadDstBuffer=<" + std::to_string(curReadPos));
    AUDIO_DEBUG_LOG("ReadFromEndpoint enter, dstAudioBuffer curReadPos %{public}" PRIu64".", curReadPos);
    CHECK_AND_RETURN_RET_LOG(dstAudioBuffer_ != nullptr, ERR_INVALID_HANDLE,
        "dst audio buffer is null.");
    SpanInfo *curReadSpan = dstAudioBuffer_->GetSpanInfo(curReadPos);
    CHECK_AND_RETURN_RET_LOG(curReadSpan != nullptr, ERR_INVALID_HANDLE,
        "get source read span info of source adapter fail.");
    curReadSpan->readStartTime = ClockTime::GetCurNano();
    curReadSpan->spanStatus.store(SpanStatus::SPAN_READING);
    BufferDesc readBuf;
    int32_t ret = dstAudioBuffer_->GetReadbuffer(curReadPos, readBuf);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "get read buffer fail, ret %{public}d.", ret);
    DumpFileUtil::WriteDumpFile(dumpHdi_, static_cast<void *>(readBuf.buffer), readBuf.bufLength);

    WriteToProcessBuffers(readBuf);
    ret = memset_s(readBuf.buffer, readBuf.bufLength, 0, readBuf.bufLength);
    if (ret != EOK) {
        AUDIO_WARNING_LOG("reset buffer fail, ret %{public}d.", ret);
    }
    curReadSpan->readDoneTime = ClockTime::GetCurNano();
    curReadSpan->spanStatus.store(SpanStatus::SPAN_READ_DONE);
    return SUCCESS;
}

void AudioEndpointInner::RecordEndpointWorkLoopFuc()
{
    ScheduleReportData(getpid(), gettid(), "audio_server");
    int64_t curTime = 0;
    uint64_t curReadPos = 0;
    int64_t wakeUpTime = ClockTime::GetCurNano();
    AUDIO_INFO_LOG("Record endpoint work loop fuc start.");
    while (isInited_.load()) {
        if (!KeepWorkloopRunning()) {
            continue;
        }
        threadStatus_ = INRUNNING;
        if (needReSyncPosition_) {
            RecordReSyncPosition();
            wakeUpTime = ClockTime::GetCurNano();
            needReSyncPosition_ = false;
            continue;
        }
        curTime = ClockTime::GetCurNano();
        Trace loopTrace("Record_loop_trace");
        if (curTime - wakeUpTime > THREE_MILLISECOND_DURATION) {
            AUDIO_WARNING_LOG("Wake up cost %{public}" PRId64" ms!", (curTime - wakeUpTime) / AUDIO_US_PER_SECOND);
        } else if (curTime - wakeUpTime > ONE_MILLISECOND_DURATION) {
            AUDIO_DEBUG_LOG("Wake up cost %{public}" PRId64" ms!", (curTime - wakeUpTime) / AUDIO_US_PER_SECOND);
        }

        curReadPos = dstAudioBuffer_->GetCurReadFrame();
        CHECK_AND_BREAK_LOG(ReadFromEndpoint(curReadPos) == SUCCESS, "read from endpoint to process service fail.");

        bool ret = RecordPrepareNextLoop(curReadPos, wakeUpTime);
        CHECK_AND_BREAK_LOG(ret, "PrepareNextLoop failed!");

        loopTrace.End();
        threadStatus_ = SLEEPING;
        ClockTime::AbsoluteSleep(wakeUpTime);
    }
}

void AudioEndpointInner::EndpointWorkLoopFuc()
{
    ScheduleReportData(getpid(), gettid(), "audio_server");
    int64_t curTime = 0;
    uint64_t curWritePos = 0;
    int64_t wakeUpTime = ClockTime::GetCurNano();
    AUDIO_INFO_LOG("Endpoint work loop fuc start");
    int32_t ret = 0;
    while (isInited_.load()) {
        if (!KeepWorkloopRunning()) {
            continue;
        }
        ret = 0;
        threadStatus_ = INRUNNING;
        curTime = ClockTime::GetCurNano();
        Trace loopTrace("AudioEndpoint::loop_trace");
        if (needReSyncPosition_) {
            ReSyncPosition();
            wakeUpTime = curTime;
            needReSyncPosition_ = false;
            continue;
        }
        if (curTime - wakeUpTime > THREE_MILLISECOND_DURATION) {
            AUDIO_WARNING_LOG("Wake up cost %{public}" PRId64" ms!", (curTime - wakeUpTime) / AUDIO_US_PER_SECOND);
        } else if (curTime - wakeUpTime > ONE_MILLISECOND_DURATION) {
            AUDIO_DEBUG_LOG("Wake up cost %{public}" PRId64" ms!", (curTime - wakeUpTime) / AUDIO_US_PER_SECOND);
        }

        // First, wake up at client may-write-done time, and check if all process write done.
        // If not, do another sleep to the possible latest write time.
        curWritePos = dstAudioBuffer_->GetCurWriteFrame();
        if (!CheckAllBufferReady(wakeUpTime, curWritePos)) {
            curTime = ClockTime::GetCurNano();
        }

        // then do mix & write to hdi buffer and prepare next loop
        if (!ProcessToEndpointDataHandle(curWritePos)) {
            AUDIO_ERR_LOG("ProcessToEndpointDataHandle failed!");
            break;
        }

        // prepare info of next loop
        if (!PrepareNextLoop(curWritePos, wakeUpTime)) {
            AUDIO_ERR_LOG("PrepareNextLoop failed!");
            break;
        }

        loopTrace.End();
        // start sleep
        threadStatus_ = SLEEPING;
        ClockTime::AbsoluteSleep(wakeUpTime);
    }
    AUDIO_DEBUG_LOG("Endpoint work loop fuc end, ret %{public}d", ret);
}
} // namespace AudioStandard
} // namespace OHOS
