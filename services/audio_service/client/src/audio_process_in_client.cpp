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

#include "audio_process_in_client.h"

#include <atomic>
#include <cinttypes>
#include <condition_variable>
#include <sstream>
#include <string>
#include <mutex>
#include <thread>

#include "iservice_registry.h"
#include "system_ability_definition.h"

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_system_manager.h"
#include "audio_utils.h"
#include "securec.h"

#include "audio_manager_base.h"
#include "audio_process_cb_stub.h"
#include "audio_server_death_recipient.h"
#include "i_audio_process.h"
#include "linear_pos_time_model.h"

namespace OHOS {
namespace AudioStandard {

namespace {
static constexpr int32_t VOLUME_SHIFT_NUMBER = 16; // 1 >> 16 = 65536, max volume
}

class ProcessCbImpl;
class AudioProcessInClientInner : public AudioProcessInClient,
    public std::enable_shared_from_this<AudioProcessInClientInner> {
public:
    explicit AudioProcessInClientInner(const sptr<IAudioProcess> &ipcProxy);
    ~AudioProcessInClientInner();

    int32_t SaveDataCallback(const std::shared_ptr<AudioDataCallback> &dataCallback) override;

    int32_t SaveUnderrunCallback(const std::shared_ptr<ClientUnderrunCallBack> &underrunCallback) override;

    int32_t GetBufferDesc(BufferDesc &bufDesc) const override;

    int32_t Enqueue(const BufferDesc &bufDesc) const override;

    int32_t SetVolume(int32_t vol) override;

    int32_t Start() override;

    int32_t Pause(bool isFlush) override;

    int32_t Resume() override;

    int32_t Stop() override;

    int32_t Release() override;

    // methods for support IAudioStream
    int32_t GetSessionID(uint32_t &sessionID) override;

    bool GetAudioTime(uint32_t &framePos, int64_t &sec, int64_t &nanoSec) override;

    int32_t GetBufferSize(size_t &bufferSize) override;

    int32_t GetFrameCount(uint32_t &frameCount) override;

    int32_t GetLatency(uint64_t &latency) override;

    int32_t SetVolume(float vol) override;

    float GetVolume() override;

    uint32_t GetUnderflowCount() override;

    int64_t GetFramesWritten() override;

    int64_t GetFramesRead() override;

    void SetApplicationCachePath(const std::string &cachePath) override;

    void SetPreferredFrameSize(int32_t frameSize) override;
    
    bool Init(const AudioProcessConfig &config);

    static const sptr<IStandardAudioService> GetAudioServerProxy();
    static void AudioServerDied(pid_t pid);
    static constexpr AudioStreamInfo g_targetStreamInfo = {SAMPLE_RATE_48000, ENCODING_PCM, SAMPLE_S16LE, STEREO};

private:
    // move it to a common folder
    static bool ChannelFormatConvert(const AudioStreamData &srcData, const AudioStreamData &dstData);

    bool InitAudioBuffer();
    void ResetAudioBuffer();

    bool PrepareCurrent(uint64_t curWritePos);
    void CallClientHandleCurrent();
    bool FinishHandleCurrent(uint64_t &curWritePos, int64_t &clientWriteCost);
    int32_t ReadFromProcessClient() const;
    int32_t RecordReSyncServicePos();
    int32_t RecordPrepareCurrent(uint64_t curReadPos);
    int32_t RecordFinishHandleCurrent(uint64_t &curReadPos, int64_t &clientReadCost);
    bool PrepareCurrentLoop(uint64_t curWritePos);
    bool FinishHandleCurrentLoop(uint64_t &curWritePos, int64_t &clientWriteCost);

    void UpdateHandleInfo(bool isAysnc = true, bool resetReadWritePos = false);
    int64_t GetPredictNextHandleTime(uint64_t posInFrame, bool isIndependent = false);
    bool PrepareNext(uint64_t curHandPos, int64_t &wakeUpTime);
    bool ClientPrepareNextLoop(uint64_t curWritePos, int64_t &wakeUpTime);
    bool PrepareNextIndependent(uint64_t curWritePos, int64_t &wakeUpTime);

    std::string GetStatusInfo(StreamStatus status);
    bool KeepLoopRunning();
    bool KeepLoopRunningIndependent();

    void ProcessCallbackFuc();
    void ProcessCallbackFucIndependent();
    void RecordProcessCallbackFuc();
    void CopyWithVolume(const BufferDesc &srcDesc, const BufferDesc &dstDesc) const;
    void ProcessVolume(const AudioStreamData &targetData) const;
    int32_t ProcessData(const BufferDesc &srcDesc, const BufferDesc &dstDesc) const;

private:
    static constexpr int64_t ONE_MILLISECOND_DURATION = 1000000; // 1ms
    static constexpr int64_t THREE_MILLISECOND_DURATION = 3000000; // 3ms
    static constexpr int64_t MAX_WRITE_COST_DURATION_NANO = 5000000; // 5ms
    static constexpr int64_t MAX_READ_COST_DURATION_NANO = 5000000; // 5ms
    static constexpr int64_t WRITE_BEFORE_DURATION_NANO = 2000000; // 2ms
    static constexpr int64_t RECORD_RESYNC_SLEEP_NANO = 2000000; // 2ms
    static constexpr int64_t RECORD_HANDLE_DELAY_NANO = 3000000; // 3ms
    static constexpr size_t MAX_TIMES = 4; // 4 times spanSizeInFrame_
    static constexpr size_t DIV = 2; // halt of span
    enum ThreadStatus : uint32_t {
        WAITTING = 0,
        SLEEPING,
        INRUNNING,
        INVALID
    };
    AudioProcessConfig processConfig_;
    bool needConvert_ = false;
    size_t clientByteSizePerFrame_ = 0;
    size_t clientSpanSizeInByte_ = 0;
    size_t clientSpanSizeInFrame_ = 240;
    sptr<IAudioProcess> processProxy_ = nullptr;
    std::shared_ptr<OHAudioBuffer> audioBuffer_ = nullptr;

    uint32_t totalSizeInFrame_ = 0;
    uint32_t spanSizeInFrame_ = 0;
    uint32_t byteSizePerFrame_ = 0;
    size_t spanSizeInByte_ = 0;
    std::weak_ptr<AudioDataCallback> audioDataCallback_;
    std::weak_ptr<ClientUnderrunCallBack> underrunCallback_;

    std::unique_ptr<uint8_t[]> callbackBuffer_ = nullptr;

    std::mutex statusSwitchLock_;
    std::atomic<StreamStatus> *streamStatus_ = nullptr;
    bool isInited_ = false;
    bool needReSyncPosition_ = true;

    float volumeInFloat_ = 1.0f;
    int32_t processVolume_ = PROCESS_VOLUME_MAX; // 0 ~ 65536
    LinearPosTimeModel handleTimeModel_;

    std::thread callbackLoop_; // thread for callback to client and write.
    bool isCallbackLoopEnd_ = false;
    std::atomic<ThreadStatus> threadStatus_ = INVALID;
    std::mutex loopThreadLock_;
    std::condition_variable threadStatusCV_;

    std::atomic<uint32_t> underflowCount_ = 0;
    std::string cachePath_;
    FILE *dumpFile_ = nullptr;

    sptr<ProcessCbImpl> processCbImpl_ = nullptr;
};

// ProcessCbImpl --> sptr | AudioProcessInClientInner --> shared_ptr
class ProcessCbImpl : public ProcessCbStub {
public:
    explicit ProcessCbImpl(std::shared_ptr<AudioProcessInClientInner> processInClientInner);
    virtual ~ProcessCbImpl() = default;

    int32_t OnEndpointChange(int32_t status) override;

private:
    std::weak_ptr<AudioProcessInClientInner> processInClientInner_;
};

ProcessCbImpl::ProcessCbImpl(std::shared_ptr<AudioProcessInClientInner> processInClientInner)
{
    if (processInClientInner == nullptr) {
        AUDIO_ERR_LOG("ProcessCbImpl() find null processInClientInner");
    }
    processInClientInner_ = processInClientInner;
}

int32_t ProcessCbImpl::OnEndpointChange(int32_t status)
{
    AUDIO_INFO_LOG("OnEndpointChange: %{public}d", status);
    return SUCCESS;
}

std::mutex g_audioServerProxyMutex;
sptr<IStandardAudioService> gAudioServerProxy = nullptr;

AudioProcessInClientInner::AudioProcessInClientInner(const sptr<IAudioProcess> &ipcProxy) : processProxy_(ipcProxy)
{
    AUDIO_INFO_LOG("AudioProcessInClient construct.");
}

const sptr<IStandardAudioService> AudioProcessInClientInner::GetAudioServerProxy()
{
    std::lock_guard<std::mutex> lock(g_audioServerProxyMutex);
    if (gAudioServerProxy == nullptr) {
        auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        CHECK_AND_RETURN_RET_LOG(samgr != nullptr, nullptr, "get sa manager failed");
        sptr<IRemoteObject> object = samgr->GetSystemAbility(AUDIO_DISTRIBUTED_SERVICE_ID);
        CHECK_AND_RETURN_RET_LOG(object != nullptr, nullptr, "get audio service remote object failed");
        gAudioServerProxy = iface_cast<IStandardAudioService>(object);
        CHECK_AND_RETURN_RET_LOG(gAudioServerProxy != nullptr, nullptr, "get audio service proxy failed");

        // register death recipent to restore proxy
        sptr<AudioServerDeathRecipient> asDeathRecipient = new(std::nothrow) AudioServerDeathRecipient(getpid());
        if (asDeathRecipient != nullptr) {
            asDeathRecipient->SetNotifyCb(std::bind(&AudioProcessInClientInner::AudioServerDied,
                std::placeholders::_1));
            bool result = object->AddDeathRecipient(asDeathRecipient);
            if (!result) {
                AUDIO_WARNING_LOG("failed to add deathRecipient");
            }
        }
    }
    sptr<IStandardAudioService> gasp = gAudioServerProxy;
    return gasp;
}

/**
 * When AudioServer died, all stream in client should be notified. As they were proxy stream ,the stub stream
 * has been destoried in server.
*/
void AudioProcessInClientInner::AudioServerDied(pid_t pid)
{
    AUDIO_INFO_LOG("audio server died, will restore proxy in next call");
    std::lock_guard<std::mutex> lock(g_audioServerProxyMutex);
    gAudioServerProxy = nullptr;
}

std::shared_ptr<AudioProcessInClient> AudioProcessInClient::Create(const AudioProcessConfig &config)
{
    AUDIO_INFO_LOG("Create with config: render flag %{public}d, capturer flag %{public}d, streamType %{public}d.",
        config.rendererInfo.rendererFlags, config.capturerInfo.capturerFlags, config.streamType);
    bool ret = AudioProcessInClient::CheckIfSupport(config);
    CHECK_AND_RETURN_RET_LOG(config.audioMode != AUDIO_MODE_PLAYBACK || ret, nullptr,
        "CheckIfSupport failed!");
    sptr<IStandardAudioService> gasp = AudioProcessInClientInner::GetAudioServerProxy();
    CHECK_AND_RETURN_RET_LOG(gasp != nullptr, nullptr, "Create failed, can not get service.");
    AudioProcessConfig resetConfig = config;
    resetConfig.streamInfo = AudioProcessInClientInner::g_targetStreamInfo;
    sptr<IRemoteObject> ipcProxy = gasp->CreateAudioProcess(resetConfig);
    CHECK_AND_RETURN_RET_LOG(ipcProxy != nullptr, nullptr, "Create failed with null ipcProxy.");
    sptr<IAudioProcess> iProcessProxy = iface_cast<IAudioProcess>(ipcProxy);
    CHECK_AND_RETURN_RET_LOG(iProcessProxy != nullptr, nullptr, "Create failed when iface_cast.");
    std::shared_ptr<AudioProcessInClientInner> process = std::make_shared<AudioProcessInClientInner>(iProcessProxy);
    if (!process->Init(config)) {
        AUDIO_ERR_LOG("Init failed!");
        process = nullptr;
    }

    return process;
}

AudioProcessInClientInner::~AudioProcessInClientInner()
{
    AUDIO_INFO_LOG("AudioProcessInClient deconstruct.");
    if (callbackLoop_.joinable()) {
        std::unique_lock<std::mutex> lock(loopThreadLock_);
        isCallbackLoopEnd_ = true; // change it with lock to break the loop
        threadStatusCV_.notify_all();
        lock.unlock(); // should call unlock before join
        callbackLoop_.join();
    }
    if (isInited_) {
        AudioProcessInClientInner::Release();
    }
    DumpFileUtil::CloseDumpFile(&dumpFile_);
}

int32_t AudioProcessInClientInner::GetSessionID(uint32_t &sessionID)
{
    // note: Get the session id from server.
    int32_t pid = processConfig_.appInfo.appPid;
    CHECK_AND_RETURN_RET_LOG(pid >= 0, ERR_OPERATION_FAILED, "GetSessionID failed:%{public}d", pid);
    sessionID = static_cast<uint32_t>(pid); // using pid as sessionID temporarily
    return SUCCESS;
}

bool AudioProcessInClientInner::GetAudioTime(uint32_t &framePos, int64_t &sec, int64_t &nanoSec)
{
    CHECK_AND_RETURN_RET_LOG(audioBuffer_ != nullptr, false, "buffer is null, maybe not inited.");
    uint64_t pos = 0;
    if (processConfig_.audioMode == AUDIO_MODE_PLAYBACK) {
        pos = audioBuffer_->GetCurWriteFrame();
    } else {
        pos = audioBuffer_->GetCurReadFrame();
    }

    if (pos > UINT32_MAX) {
        framePos = pos % UINT32_MAX;
    } else {
        framePos = static_cast<uint32_t>(pos);
    }
    int64_t time = handleTimeModel_.GetTimeOfPos(pos);
    int64_t deltaTime = 20000000; // note: 20ms
    time += deltaTime;

    sec = time / AUDIO_NS_PER_SECOND;
    nanoSec = time % AUDIO_NS_PER_SECOND;
    return true;
}

int32_t AudioProcessInClientInner::GetBufferSize(size_t &bufferSize)
{
    bufferSize = clientSpanSizeInByte_;
    return SUCCESS;
}

int32_t AudioProcessInClientInner::GetFrameCount(uint32_t &frameCount)
{
    frameCount = static_cast<uint32_t>(clientSpanSizeInFrame_);
    return SUCCESS;
}

int32_t AudioProcessInClientInner::GetLatency(uint64_t &latency)
{
    latency = 20; // 20ms for debug
    return SUCCESS;
}

int32_t AudioProcessInClientInner::SetVolume(float vol)
{
    float minVol = 0.0f;
    float maxVol = 1.0f;
    CHECK_AND_RETURN_RET_LOG(vol >= minVol && vol <= maxVol, ERR_INVALID_PARAM,
        "SetVolume failed to with invalid volume:%{public}f", vol);
    int32_t volumeInt = static_cast<int32_t>(vol * PROCESS_VOLUME_MAX);
    int32_t ret = SetVolume(volumeInt);
    if (ret == SUCCESS) {
        volumeInFloat_ = vol;
    }
    return ret;
}

float AudioProcessInClientInner::GetVolume()
{
    return volumeInFloat_;
}

uint32_t AudioProcessInClientInner::GetUnderflowCount()
{
    return underflowCount_.load();
}

int64_t AudioProcessInClientInner::GetFramesWritten()
{
    CHECK_AND_RETURN_RET_LOG(processConfig_.audioMode == AUDIO_MODE_PLAYBACK, -1, "Playback not support.");
    CHECK_AND_RETURN_RET_LOG(audioBuffer_ != nullptr, -1, "buffer is null, maybe not inited.");
    return audioBuffer_->GetCurWriteFrame();
}

int64_t AudioProcessInClientInner::GetFramesRead()
{
    CHECK_AND_RETURN_RET_LOG(processConfig_.audioMode == AUDIO_MODE_RECORD, -1, "Record not support.");
    CHECK_AND_RETURN_RET_LOG(audioBuffer_ != nullptr, -1, "buffer is null, maybe not inited.");
    return audioBuffer_->GetCurReadFrame();
}

void AudioProcessInClientInner::SetApplicationCachePath(const std::string &cachePath)
{
    AUDIO_INFO_LOG("Using cachePath:%{public}s", cachePath.c_str());
    cachePath_ = cachePath;
}

void AudioProcessInClientInner::SetPreferredFrameSize(int32_t frameSize)
{
    size_t originalSpanSizeInFrame = static_cast<size_t>(spanSizeInFrame_);
    size_t tmp = static_cast<size_t>(frameSize);
    size_t count = frameSize / spanSizeInFrame_;
    size_t rest = frameSize % spanSizeInFrame_;
    if (tmp <= originalSpanSizeInFrame) {
        clientSpanSizeInFrame_ = originalSpanSizeInFrame;
    } else if (tmp >= MAX_TIMES * originalSpanSizeInFrame) {
        clientSpanSizeInFrame_ = MAX_TIMES * originalSpanSizeInFrame;
    } else {
        if (rest <= originalSpanSizeInFrame / DIV) {
            clientSpanSizeInFrame_ = count * spanSizeInFrame_;
        } else {
            count++;
            clientSpanSizeInFrame_ = count * spanSizeInFrame_;
        }
    }
    if (clientByteSizePerFrame_ == 0) {
        clientSpanSizeInByte_ = count * spanSizeInByte_;
    } else {
        clientSpanSizeInByte_ = clientSpanSizeInFrame_ * clientByteSizePerFrame_;
    }
    callbackBuffer_ = std::make_unique<uint8_t[]>(clientSpanSizeInByte_);
    memset_s(callbackBuffer_.get(), clientSpanSizeInByte_, 0, clientSpanSizeInByte_);
}

bool AudioProcessInClientInner::InitAudioBuffer()
{
    CHECK_AND_RETURN_RET_LOG(processProxy_ != nullptr, false, "Init failed with null ipcProxy.");
    processCbImpl_ = sptr<ProcessCbImpl>::MakeSptr(shared_from_this());
    CHECK_AND_RETURN_RET_LOG(processProxy_->RegisterProcessCb(processCbImpl_) == SUCCESS, false,
        "RegisterProcessCb failed.");
    int32_t ret = processProxy_->ResolveBuffer(audioBuffer_);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS && audioBuffer_ != nullptr, false,
        "Init failed to call ResolveBuffer");
    streamStatus_ = audioBuffer_->GetStreamStatus();
    CHECK_AND_RETURN_RET_LOG(streamStatus_ != nullptr, false, "Init failed, access buffer failed.");

    audioBuffer_->GetSizeParameter(totalSizeInFrame_, spanSizeInFrame_, byteSizePerFrame_);
    spanSizeInByte_ = spanSizeInFrame_ * byteSizePerFrame_;

    if (processConfig_.audioMode == AUDIO_MODE_PLAYBACK && clientByteSizePerFrame_ != 0) {
        clientSpanSizeInByte_ = spanSizeInFrame_ * clientByteSizePerFrame_;
    } else {
        clientSpanSizeInByte_ = spanSizeInByte_;
    }

    AUDIO_INFO_LOG("Using totalSizeInFrame_ %{public}d spanSizeInFrame_ %{public}d byteSizePerFrame_ %{public}d "
        "spanSizeInByte_ %{public}zu clientSpanSizeInByte_ %{public}zu", totalSizeInFrame_, spanSizeInFrame_,
        byteSizePerFrame_, spanSizeInByte_, clientSpanSizeInByte_);

    callbackBuffer_ = std::make_unique<uint8_t[]>(clientSpanSizeInByte_);
    CHECK_AND_RETURN_RET_LOG(callbackBuffer_ != nullptr, false, "Init callbackBuffer_ failed.");
    memset_s(callbackBuffer_.get(), clientSpanSizeInByte_, 0, clientSpanSizeInByte_);

    return true;
}

static size_t GetFormatSize(const AudioStreamInfo &info)
{
    size_t result = 0;
    size_t bitWidthSize = 0;
    switch (info.format) {
        case SAMPLE_U8:
            bitWidthSize = 1; // size is 1
            break;
        case SAMPLE_S16LE:
            bitWidthSize = 2; // size is 2
            break;
        case SAMPLE_S24LE:
            bitWidthSize = 3; // size is 3
            break;
        case SAMPLE_S32LE:
            bitWidthSize = 4; // size is 4
            break;
        default:
            bitWidthSize = 2; // size is 2
            break;
    }

    size_t channelSize = 0;
    switch (info.channels) {
        case MONO:
            channelSize = 1; // size is 1
            break;
        case STEREO:
            channelSize = 2; // size is 2
            break;
        default:
            channelSize = 2; // size is 2
            break;
    }
    result = bitWidthSize * channelSize;
    return result;
}

bool AudioProcessInClientInner::Init(const AudioProcessConfig &config)
{
    AUDIO_INFO_LOG("Call Init.");
    processConfig_ = config;
    if (config.streamInfo.format != g_targetStreamInfo.format ||
        config.streamInfo.channels != g_targetStreamInfo.channels) {
        needConvert_ = true;
    }
    if (config.audioMode == AUDIO_MODE_PLAYBACK) {
        clientByteSizePerFrame_ = GetFormatSize(config.streamInfo);
    }
    AUDIO_DEBUG_LOG("Using clientByteSizePerFrame_:%{public}zu", clientByteSizePerFrame_);
    bool isBufferInited = InitAudioBuffer();
    CHECK_AND_RETURN_RET_LOG(isBufferInited, isBufferInited, "%{public}s init audio buffer fail.", __func__);

    bool ret = handleTimeModel_.ConfigSampleRate(processConfig_.streamInfo.samplingRate);
    CHECK_AND_RETURN_RET_LOG(ret != false, false, "Init LinearPosTimeModel failed.");
    uint64_t handlePos = 0;
    int64_t handleTime = 0;
    audioBuffer_->GetHandleInfo(handlePos, handleTime);
    handleTimeModel_.ResetFrameStamp(handlePos, handleTime);

    streamStatus_->store(StreamStatus::STREAM_IDEL);

    AudioBufferHolder bufferHolder = audioBuffer_->GetBufferHolder();
    bool isIndependent = bufferHolder == AudioBufferHolder::AUDIO_SERVER_INDEPENDENT;
    if (config.audioMode == AUDIO_MODE_RECORD) {
        callbackLoop_ = std::thread(&AudioProcessInClientInner::RecordProcessCallbackFuc, this);
        pthread_setname_np(callbackLoop_.native_handle(), "OS_AudioRecCb");
    } else if (isIndependent) {
        callbackLoop_ = std::thread(&AudioProcessInClientInner::ProcessCallbackFucIndependent, this);
        pthread_setname_np(callbackLoop_.native_handle(), "OS_AudioPlayCb");
    } else {
        callbackLoop_ = std::thread(&AudioProcessInClientInner::ProcessCallbackFuc, this);
        pthread_setname_np(callbackLoop_.native_handle(), "OS_AudioPlayCb");
    }

    int waitThreadStartTime = 5; // wait for thread start.
    while (threadStatus_.load() == INVALID) {
        AUDIO_DEBUG_LOG("%{public}s wait %{public}d ms for %{public}s started...", __func__, waitThreadStartTime,
            config.audioMode == AUDIO_MODE_RECORD ? "RecordProcessCallbackFuc" : "ProcessCallbackFuc");
        ClockTime::RelativeSleep(ONE_MILLISECOND_DURATION * waitThreadStartTime);
    }

    isInited_ = true;
    return true;
}

int32_t AudioProcessInClientInner::SaveDataCallback(const std::shared_ptr<AudioDataCallback> &dataCallback)
{
    AUDIO_INFO_LOG("%{public}s enter.", __func__);
    CHECK_AND_RETURN_RET_LOG(isInited_, ERR_ILLEGAL_STATE, "not inited!");

    CHECK_AND_RETURN_RET_LOG(dataCallback != nullptr, ERR_INVALID_PARAM,
        "data callback is null.");
    audioDataCallback_ = dataCallback;
    return SUCCESS;
}

int32_t AudioProcessInClientInner::SaveUnderrunCallback(const std::shared_ptr<ClientUnderrunCallBack> &underrunCallback)
{
    AUDIO_INFO_LOG("%{public}s enter.", __func__);
    CHECK_AND_RETURN_RET_LOG(isInited_, ERR_ILLEGAL_STATE, "not inited!");

    CHECK_AND_RETURN_RET_LOG(underrunCallback != nullptr, ERR_INVALID_PARAM,
        "underrun callback is null.");
    underrunCallback_ = underrunCallback;
    return SUCCESS;
}

int32_t AudioProcessInClientInner::ReadFromProcessClient() const
{
    CHECK_AND_RETURN_RET_LOG(audioBuffer_ != nullptr, ERR_INVALID_HANDLE,
        "%{public}s audio buffer is null.", __func__);
    uint64_t curReadPos = audioBuffer_->GetCurReadFrame();
    Trace trace("AudioProcessInClient::ReadProcessData-<" + std::to_string(curReadPos));
    BufferDesc readbufDesc = {nullptr, 0, 0};
    int32_t ret = audioBuffer_->GetReadbuffer(curReadPos, readbufDesc);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS && readbufDesc.buffer != nullptr &&
        readbufDesc.bufLength == spanSizeInByte_ && readbufDesc.dataLength == spanSizeInByte_,
        ERR_OPERATION_FAILED, "get client mmap read buffer failed, ret %{public}d.", ret);
    ret = memcpy_s(static_cast<void *>(callbackBuffer_.get()), spanSizeInByte_,
        static_cast<void *>(readbufDesc.buffer), spanSizeInByte_);
    CHECK_AND_RETURN_RET_LOG(ret == EOK, ERR_OPERATION_FAILED, "%{public}s memcpy fail, ret %{public}d,"
        " spanSizeInByte %{public}zu.", __func__, ret, spanSizeInByte_);
    DumpFileUtil::WriteDumpFile(dumpFile_, static_cast<void *>(readbufDesc.buffer), spanSizeInByte_);

    ret = memset_s(readbufDesc.buffer, readbufDesc.bufLength, 0, readbufDesc.bufLength);
    if (ret != EOK) {
        AUDIO_WARNING_LOG("reset buffer fail, ret %{public}d.", ret);
    }
    return SUCCESS;
}

// the buffer will be used by client
int32_t AudioProcessInClientInner::GetBufferDesc(BufferDesc &bufDesc) const
{
    CHECK_AND_RETURN_RET_LOG(isInited_, ERR_ILLEGAL_STATE, "%{public}s not inited!", __func__);
    Trace trace("AudioProcessInClient::GetBufferDesc");

    if (processConfig_.audioMode == AUDIO_MODE_RECORD) {
        ReadFromProcessClient();
    }

    bufDesc.buffer = callbackBuffer_.get();
    bufDesc.dataLength = clientSpanSizeInByte_;
    bufDesc.bufLength = clientSpanSizeInByte_;
    return SUCCESS;
}

bool AudioProcessInClient::CheckIfSupport(const AudioProcessConfig &config)
{
    if (config.streamInfo.encoding != ENCODING_PCM || config.streamInfo.samplingRate != SAMPLE_RATE_48000) {
        return false;
    }

    if (config.streamInfo.format != SAMPLE_S16LE && config.streamInfo.format != SAMPLE_S32LE) {
        return false;
    }

    if (config.streamInfo.channels != MONO && config.streamInfo.channels != STEREO) {
        return false;
    }
    return true;
}

inline bool S16MonoToS16Stereo(const BufferDesc &srcDesc, const BufferDesc &dstDesc)
{
    size_t half = 2;
    if (srcDesc.bufLength != dstDesc.bufLength / half || srcDesc.buffer == nullptr || dstDesc.buffer == nullptr) {
        return false;
    }
    int16_t *stcPtr = reinterpret_cast<int16_t *>(srcDesc.buffer);
    int16_t *dstPtr = reinterpret_cast<int16_t *>(dstDesc.buffer);
    size_t count = srcDesc.bufLength / half;
    for (size_t idx = 0; idx < count; idx++) {
        *(dstPtr++) = *stcPtr;
        *(dstPtr++) = *stcPtr++;
    }
    return true;
}

inline bool S32MonoToS16Stereo(const BufferDesc &srcDesc, const BufferDesc &dstDesc)
{
    size_t quarter = 4;
    if (srcDesc.bufLength != dstDesc.bufLength || srcDesc.buffer == nullptr || dstDesc.buffer == nullptr ||
        srcDesc.bufLength % quarter != 0) {
        return false;
    }
    int32_t *stcPtr = reinterpret_cast<int32_t *>(srcDesc.buffer);
    int16_t *dstPtr = reinterpret_cast<int16_t *>(dstDesc.buffer);
    size_t count = srcDesc.bufLength / quarter;

    double maxInt32 = INT32_MAX;
    double maxInt16 = INT16_MAX;
    for (size_t idx = 0; idx < count; idx++) {
        int16_t temp = static_cast<int16_t>((static_cast<double>(*stcPtr) / maxInt32) * maxInt16);
        stcPtr++;
        *(dstPtr++) = temp;
        *(dstPtr++) = temp;
    }
    return true;
}

inline bool S32StereoS16Stereo(const BufferDesc &srcDesc, const BufferDesc &dstDesc)
{
    size_t half = 2;
    if (srcDesc.bufLength / half != dstDesc.bufLength || srcDesc.buffer == nullptr || dstDesc.buffer == nullptr ||
        dstDesc.bufLength % half != 0) {
        return false;
    }
    int32_t *stcPtr = reinterpret_cast<int32_t *>(srcDesc.buffer);
    int16_t *dstPtr = reinterpret_cast<int16_t *>(dstDesc.buffer);
    size_t count = srcDesc.bufLength / half / half;
    double maxInt32 = INT32_MAX;
    double maxInt16 = INT16_MAX;
    for (size_t idx = 0; idx < count; idx++) {
        int16_t temp = static_cast<int16_t>((static_cast<double>(*stcPtr) / maxInt32) * maxInt16);
        stcPtr++;
        *(dstPtr++) = temp;
    }
    return true;
}

// only support MONO to STEREO and SAMPLE_S32LE to SAMPLE_S16LE
bool AudioProcessInClientInner::ChannelFormatConvert(const AudioStreamData &srcData, const AudioStreamData &dstData)
{
    if (srcData.streamInfo.samplingRate != dstData.streamInfo.samplingRate ||
        srcData.streamInfo.encoding != dstData.streamInfo.encoding) {
        return false;
    }
    if (srcData.streamInfo.format == SAMPLE_S16LE && srcData.streamInfo.channels == STEREO) {
        return true; // no need convert
    }
    if (srcData.streamInfo.format == SAMPLE_S16LE && srcData.streamInfo.channels == MONO) {
        return S16MonoToS16Stereo(srcData.bufferDesc, dstData.bufferDesc);
    }
    if (srcData.streamInfo.format == SAMPLE_S32LE && srcData.streamInfo.channels == MONO) {
        return S32MonoToS16Stereo(srcData.bufferDesc, dstData.bufferDesc);
    }
    if (srcData.streamInfo.format == SAMPLE_S32LE && srcData.streamInfo.channels == STEREO) {
        return S32StereoS16Stereo(srcData.bufferDesc, dstData.bufferDesc);
    }

    return false;
}

void AudioProcessInClientInner::CopyWithVolume(const BufferDesc &srcDesc, const BufferDesc &dstDesc) const
{
    size_t len = dstDesc.dataLength;
    len /= 2; // SAMPLE_S16LE--> 2 byte
    int16_t *dstPtr = reinterpret_cast<int16_t *>(dstDesc.buffer);
    for (size_t pos = 0; len > 0; len--) {
        int32_t sum = 0;
        int16_t *srcPtr = reinterpret_cast<int16_t *>(srcDesc.buffer) + pos;
        sum += (*srcPtr * static_cast<int64_t>(processVolume_)) >> VOLUME_SHIFT_NUMBER; // 1/65536
        pos++;
        *dstPtr++ = sum > INT16_MAX ? INT16_MAX : (sum < INT16_MIN ? INT16_MIN : sum);
    }
}

void AudioProcessInClientInner::ProcessVolume(const AudioStreamData &targetData) const
{
    size_t half = 2;
    size_t len = targetData.bufferDesc.dataLength;
    len /= half;
    int16_t *dstPtr = reinterpret_cast<int16_t *>(targetData.bufferDesc.buffer);
    for (; len > 0; len--) {
        int32_t sum = 0;
        sum += (*dstPtr * static_cast<int64_t>(processVolume_)) >> VOLUME_SHIFT_NUMBER;
        *dstPtr++ = sum > INT16_MAX ? INT16_MAX : (sum < INT16_MIN ? INT16_MIN : sum);
    }
}

int32_t AudioProcessInClientInner::ProcessData(const BufferDesc &srcDesc, const BufferDesc &dstDesc) const
{
    int32_t ret = 0;
    size_t round = (spanSizeInFrame_ == 0 ? 1 : clientSpanSizeInFrame_ / spanSizeInFrame_);
    size_t offSet = clientSpanSizeInByte_ / round;
    if (!needConvert_) {
        AudioBufferHolder bufferHolder = audioBuffer_->GetBufferHolder();
        if (bufferHolder == AudioBufferHolder::AUDIO_SERVER_INDEPENDENT) {
            CopyWithVolume(srcDesc, dstDesc);
        } else {
            ret = memcpy_s(static_cast<void *>(dstDesc.buffer), spanSizeInByte_,
                static_cast<void *>(srcDesc.buffer), offSet);
            CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ERR_OPERATION_FAILED, "Copy data failed!");
        }
    } else {
        Trace traceConvert("APIC::FormatConvert");
        AudioStreamData srcData = {processConfig_.streamInfo, srcDesc, 0, 0};
        AudioStreamData dstData = {g_targetStreamInfo, dstDesc, 0, 0};
        bool succ = ChannelFormatConvert(srcData, dstData);
        CHECK_AND_RETURN_RET_LOG(succ == true, ERR_OPERATION_FAILED, "Convert data failed!");
        AudioBufferHolder bufferHolder = audioBuffer_->GetBufferHolder();
        if (bufferHolder == AudioBufferHolder::AUDIO_SERVER_INDEPENDENT) {
            ProcessVolume(dstData);
        }
    }
    return SUCCESS;
}

int32_t AudioProcessInClientInner::Enqueue(const BufferDesc &bufDesc) const
{
    Trace trace("AudioProcessInClient::Enqueue");
    CHECK_AND_RETURN_RET_LOG(isInited_, ERR_ILLEGAL_STATE, "not inited!");

    CHECK_AND_RETURN_RET_LOG(bufDesc.buffer != nullptr && bufDesc.bufLength == clientSpanSizeInByte_ &&
        bufDesc.dataLength == clientSpanSizeInByte_, ERR_INVALID_PARAM,
        "bufDesc error, bufLen %{public}zu, dataLen %{public}zu, spanSize %{public}zu.",
        bufDesc.bufLength, bufDesc.dataLength, clientSpanSizeInByte_);
    // check if this buffer is form us.
    if (bufDesc.buffer != callbackBuffer_.get()) {
        AUDIO_WARNING_LOG("the buffer is not created by client.");
    }

    size_t round = (spanSizeInFrame_ == 0 ? 1 : clientSpanSizeInFrame_ / spanSizeInFrame_);
    uint64_t curWritePos = audioBuffer_->GetCurWriteFrame();
    for (size_t count = 0 ; count < round ; count++) {
        BufferDesc curCallbackBuffer = {nullptr, 0, 0};
        size_t offSet = clientSpanSizeInByte_ / round;
        curCallbackBuffer.buffer = bufDesc.buffer + count * offSet;
        curCallbackBuffer.bufLength = offSet;
        curCallbackBuffer.dataLength = offSet;
        uint64_t curPos = curWritePos + count * spanSizeInFrame_;
        if (processConfig_.audioMode == AUDIO_MODE_PLAYBACK) {
            BufferDesc curWriteBuffer = {nullptr, 0, 0};
            Trace writeProcessDataTrace("AudioProcessInClient::WriteProcessData->" + std::to_string(curPos));
            int32_t ret = audioBuffer_->GetWriteBuffer(curPos, curWriteBuffer);
            CHECK_AND_RETURN_RET_LOG(ret == SUCCESS && curWriteBuffer.buffer != nullptr &&
                curWriteBuffer.bufLength == spanSizeInByte_ && curWriteBuffer.dataLength == spanSizeInByte_,
                ERR_OPERATION_FAILED, "get write buffer fail, ret:%{public}d", ret);
            ret = ProcessData(curCallbackBuffer, curWriteBuffer);
            if (ret != SUCCESS) {
                return ERR_OPERATION_FAILED;
            }
            writeProcessDataTrace.End();

            DumpFileUtil::WriteDumpFile(dumpFile_, static_cast<void *>(curCallbackBuffer.buffer), offSet);
        }
    }

    if (memset_s(callbackBuffer_.get(), clientSpanSizeInByte_, 0, clientSpanSizeInByte_) != EOK) {
        AUDIO_WARNING_LOG("reset callback buffer fail.");
    }

    return SUCCESS;
}

int32_t AudioProcessInClientInner::SetVolume(int32_t vol)
{
    AUDIO_INFO_LOG("proc client mode %{public}d to %{public}d.", processConfig_.audioMode, vol);
    Trace trace("AudioProcessInClient::SetVolume " + std::to_string(vol));
    CHECK_AND_RETURN_RET_LOG(vol >= 0 && vol <= PROCESS_VOLUME_MAX, ERR_INVALID_PARAM,
        "SetVolume failed, invalid volume:%{public}d", vol);
    processVolume_ = vol;
    return SUCCESS;
}

int32_t AudioProcessInClientInner::Start()
{
    Trace traceWithLog("AudioProcessInClient::Start", true);
    CHECK_AND_RETURN_RET_LOG(isInited_, ERR_ILLEGAL_STATE, "not inited!");

    DumpFileUtil::OpenDumpFile(DUMP_CLIENT_PARA, DUMP_PROCESS_IN_CLIENT_FILENAME, &dumpFile_);

    std::lock_guard<std::mutex> lock(statusSwitchLock_);
    if (streamStatus_->load() == StreamStatus::STREAM_RUNNING) {
        AUDIO_INFO_LOG("Start find already started");
        return SUCCESS;
    }

    StreamStatus targetStatus = StreamStatus::STREAM_IDEL;
    bool ret = streamStatus_->compare_exchange_strong(targetStatus, StreamStatus::STREAM_STARTING);
    CHECK_AND_RETURN_RET_LOG(ret, ERR_ILLEGAL_STATE, "Start failed, invalid status: %{public}s",
        GetStatusInfo(targetStatus).c_str());

    if (processProxy_->Start() != SUCCESS) {
        streamStatus_->store(StreamStatus::STREAM_IDEL);
        AUDIO_ERR_LOG("Start failed to call process proxy, reset status to IDEL.");
        threadStatusCV_.notify_all();
        return ERR_OPERATION_FAILED;
    }
    UpdateHandleInfo();
    streamStatus_->store(StreamStatus::STREAM_RUNNING);
    threadStatusCV_.notify_all();
    return SUCCESS;
}

int32_t AudioProcessInClientInner::Pause(bool isFlush)
{
    Trace traceWithLog("AudioProcessInClient::Pause", true);
    CHECK_AND_RETURN_RET_LOG(isInited_, ERR_ILLEGAL_STATE, "not inited!");

    std::lock_guard<std::mutex> lock(statusSwitchLock_);
    if (streamStatus_->load() == StreamStatus::STREAM_PAUSED) {
        AUDIO_INFO_LOG("Pause find already paused");
        return SUCCESS;
    }
    StreamStatus targetStatus = StreamStatus::STREAM_RUNNING;
    bool ret = streamStatus_->compare_exchange_strong(targetStatus, StreamStatus::STREAM_PAUSING);
    CHECK_AND_RETURN_RET_LOG(ret, ERR_ILLEGAL_STATE, "Pause failed, invalid status : %{public}s",
        GetStatusInfo(targetStatus).c_str());

    if (processProxy_->Pause(isFlush) != SUCCESS) {
        streamStatus_->store(StreamStatus::STREAM_RUNNING);
        AUDIO_ERR_LOG("Pause failed to call process proxy, reset status to RUNNING");
        threadStatusCV_.notify_all(); // avoid thread blocking with status PAUSING
        return ERR_OPERATION_FAILED;
    }
    streamStatus_->store(StreamStatus::STREAM_PAUSED);

    return SUCCESS;
}

int32_t AudioProcessInClientInner::Resume()
{
    Trace traceWithLog("AudioProcessInClient::Resume", true);
    CHECK_AND_RETURN_RET_LOG(isInited_, ERR_ILLEGAL_STATE, "not inited!");
    std::lock_guard<std::mutex> lock(statusSwitchLock_);

    if (streamStatus_->load() == StreamStatus::STREAM_RUNNING) {
        AUDIO_INFO_LOG("Resume find already running");
        return SUCCESS;
    }

    StreamStatus targetStatus = StreamStatus::STREAM_PAUSED;
    bool ret = streamStatus_->compare_exchange_strong(targetStatus, StreamStatus::STREAM_STARTING);
    CHECK_AND_RETURN_RET_LOG(ret, ERR_ILLEGAL_STATE, "Resume failed, invalid status : %{public}s",
        GetStatusInfo(targetStatus).c_str());

    if (processProxy_->Resume() != SUCCESS) {
        streamStatus_->store(StreamStatus::STREAM_PAUSED);
        AUDIO_ERR_LOG("Resume failed to call process proxy, reset status to PAUSED.");
        threadStatusCV_.notify_all();
        return ERR_OPERATION_FAILED;
    }
    UpdateHandleInfo();
    streamStatus_->store(StreamStatus::STREAM_RUNNING);
    threadStatusCV_.notify_all();

    return SUCCESS;
}

int32_t AudioProcessInClientInner::Stop()
{
    Trace traceWithLog("AudioProcessInClient::Stop", true);
    CHECK_AND_RETURN_RET_LOG(isInited_, ERR_ILLEGAL_STATE, "not inited!");
    std::lock_guard<std::mutex> lock(statusSwitchLock_);
    if (streamStatus_->load() == StreamStatus::STREAM_STOPPED) {
        AUDIO_INFO_LOG("Stop find already stopped");
        return SUCCESS;
    }

    StreamStatus oldStatus = streamStatus_->load();
    CHECK_AND_RETURN_RET_LOG(oldStatus != STREAM_IDEL && oldStatus != STREAM_RELEASED && oldStatus != STREAM_INVALID,
        ERR_ILLEGAL_STATE, "Stop failed, invalid status : %{public}s", GetStatusInfo(oldStatus).c_str());

    streamStatus_->store(StreamStatus::STREAM_STOPPING);
    if (processProxy_->Stop() != SUCCESS) {
        streamStatus_->store(oldStatus);
        AUDIO_ERR_LOG("Stop failed in server, reset status to %{public}s", GetStatusInfo(oldStatus).c_str());
        threadStatusCV_.notify_all(); // avoid thread blocking with status RUNNING
        return ERR_OPERATION_FAILED;
    }
    isCallbackLoopEnd_ = true;
    threadStatusCV_.notify_all();

    streamStatus_->store(StreamStatus::STREAM_STOPPED);
    AUDIO_INFO_LOG("Success stop proc client mode %{public}d form %{public}s.",
        processConfig_.audioMode, GetStatusInfo(oldStatus).c_str());
    return SUCCESS;
}

int32_t AudioProcessInClientInner::Release()
{
    Trace traceWithLog("AudioProcessInClient::Release", true);
    CHECK_AND_RETURN_RET_LOG(isInited_, ERR_ILLEGAL_STATE, "not inited!");
    AUDIO_INFO_LOG("AudioProcessInClientInner::Release()");
    // not lock as status is already released
    if (streamStatus_->load() == StreamStatus::STREAM_RELEASED) {
        AUDIO_INFO_LOG("Stream status is already released");
        return SUCCESS;
    }
    Stop();
    std::lock_guard<std::mutex> lock(statusSwitchLock_);
    StreamStatus currentStatus = streamStatus_->load();
    if (currentStatus != STREAM_STOPPED) {
        AUDIO_WARNING_LOG("Release in currentStatus:%{public}s", GetStatusInfo(currentStatus).c_str());
    }

    if (processProxy_->Release() != SUCCESS) {
        AUDIO_ERR_LOG("Release may failed in server");
        threadStatusCV_.notify_all(); // avoid thread blocking with status RUNNING
        return ERR_OPERATION_FAILED;
    }

    streamStatus_->store(StreamStatus::STREAM_RELEASED);
    AUDIO_INFO_LOG("Success release proc client mode %{public}d.", processConfig_.audioMode);
    isInited_ = false;
    return SUCCESS;
}

// client should call GetBufferDesc and Enqueue in OnHandleData
void AudioProcessInClientInner::CallClientHandleCurrent()
{
    Trace trace("AudioProcessInClient::CallClientHandleCurrent");
    std::shared_ptr<AudioDataCallback> cb = audioDataCallback_.lock();
    CHECK_AND_RETURN_LOG(cb != nullptr, "audio data callback is null.");

    int64_t stamp = ClockTime::GetCurNano();
    cb->OnHandleData(clientSpanSizeInByte_);
    stamp = ClockTime::GetCurNano() - stamp;
    if (stamp > THREE_MILLISECOND_DURATION) {
        AUDIO_WARNING_LOG("Client handle callback too slow, cost %{public}" PRId64"us", stamp / AUDIO_MS_PER_SECOND);
        return;
    }
}

void AudioProcessInClientInner::UpdateHandleInfo(bool isAysnc, bool resetReadWritePos)
{
    Trace traceSync("AudioProcessInClient::UpdateHandleInfo");
    uint64_t serverHandlePos = 0;
    int64_t serverHandleTime = 0;
    int32_t ret = processProxy_->RequestHandleInfo(isAysnc);
    CHECK_AND_RETURN_LOG(ret == SUCCESS, "RequestHandleInfo failed ret:%{public}d", ret);
    audioBuffer_->GetHandleInfo(serverHandlePos, serverHandleTime);

    bool isSuccess = handleTimeModel_.UpdataFrameStamp(serverHandlePos, serverHandleTime);
    if (!isSuccess) {
        handleTimeModel_.ResetFrameStamp(serverHandlePos, serverHandleTime);
    }

    if (resetReadWritePos) {
        uint64_t nextWritePos = serverHandlePos + spanSizeInFrame_;
        ResetAudioBuffer();
        ret = audioBuffer_->ResetCurReadWritePos(nextWritePos, nextWritePos);
        CHECK_AND_RETURN_LOG(ret == SUCCESS, "ResetCurReadWritePos failed ret:%{public}d", ret);
    }
}

void AudioProcessInClientInner::ResetAudioBuffer()
{
    CHECK_AND_RETURN_LOG((audioBuffer_ != nullptr), "%{public}s: audio buffer is null.", __func__);

    uint32_t spanCount = audioBuffer_->GetSpanCount();
    for (uint32_t i = 0; i < spanCount; i++) {
        SpanInfo *spanInfo = audioBuffer_->GetSpanInfoByIndex(i);
        if (spanInfo == nullptr) {
            AUDIO_ERR_LOG("ResetAudiobuffer failed.");
            return;
        }
        spanInfo->spanStatus = SPAN_READ_DONE;
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

int64_t AudioProcessInClientInner::GetPredictNextHandleTime(uint64_t posInFrame, bool isIndependent)
{
    Trace trace("AudioProcessInClient::GetPredictNextRead");
    uint64_t handleSpanCnt = posInFrame / spanSizeInFrame_;
    uint32_t startPeriodCnt = 20; // sync each time when start
    uint32_t oneBigPeriodCnt = 40; // 200ms
    if (isIndependent) {
        if (handleSpanCnt % oneBigPeriodCnt == 0) {
            UpdateHandleInfo(true);
        }
    } else {
        if (handleSpanCnt < startPeriodCnt || handleSpanCnt % oneBigPeriodCnt == 0) {
            UpdateHandleInfo();
        }
    }

    int64_t nextHandleTime = handleTimeModel_.GetTimeOfPos(posInFrame);

    return nextHandleTime;
}

bool AudioProcessInClientInner::PrepareNext(uint64_t curHandPos, int64_t &wakeUpTime)
{
    Trace trace("AudioProcessInClient::PrepareNext " + std::to_string(curHandPos));
    int64_t handleModifyTime = 0;
    if (processConfig_.audioMode == AUDIO_MODE_RECORD) {
        handleModifyTime = RECORD_HANDLE_DELAY_NANO;
    } else {
        handleModifyTime = -WRITE_BEFORE_DURATION_NANO;
    }

    int64_t nextServerHandleTime = GetPredictNextHandleTime(curHandPos) + handleModifyTime;
    if (nextServerHandleTime < ClockTime::GetCurNano()) {
        wakeUpTime = ClockTime::GetCurNano() + ONE_MILLISECOND_DURATION; // make sure less than duration
    } else {
        wakeUpTime = nextServerHandleTime;
    }
    AUDIO_DEBUG_LOG("%{public}s end, audioMode %{public}d, curReadPos %{public}" PRIu64", nextServerHandleTime "
        "%{public}" PRId64" wakeUpTime %{public}" PRId64".", __func__, processConfig_.audioMode, curHandPos,
        nextServerHandleTime, wakeUpTime);
    return true;
}

bool AudioProcessInClientInner::ClientPrepareNextLoop(uint64_t curWritePos, int64_t &wakeUpTime)
{
    size_t round = (spanSizeInFrame_ == 0 ? 1 : clientSpanSizeInFrame_ / spanSizeInFrame_);
    for (size_t count = 0; count < round; count++) {
        bool ret = PrepareNext(curWritePos + count * spanSizeInFrame_, wakeUpTime);
        CHECK_AND_RETURN_RET_LOG(ret, false, "PrepareNextLoop in process failed!");
    }
    return true;
}

std::string AudioProcessInClientInner::GetStatusInfo(StreamStatus status)
{
    switch (status) {
        case STREAM_IDEL:
            return "STREAM_IDEL";
        case STREAM_STARTING:
            return "STREAM_STARTING";
        case STREAM_RUNNING:
            return "STREAM_RUNNING";
        case STREAM_PAUSING:
            return "STREAM_PAUSING";
        case STREAM_PAUSED:
            return "STREAM_PAUSED";
        case STREAM_STOPPING:
            return "STREAM_STOPPING";
        case STREAM_STOPPED:
            return "STREAM_STOPPED";
        case STREAM_RELEASED:
            return "STREAM_RELEASED";
        case STREAM_INVALID:
            return "STREAM_INVALID";
        default:
            break;
    }
    return "NO_SUCH_STATUS";
}

bool AudioProcessInClientInner::KeepLoopRunning()
{
    StreamStatus targetStatus = STREAM_INVALID;

    switch (streamStatus_->load()) {
        case STREAM_RUNNING:
            return true;
        case STREAM_STARTING:
            targetStatus = STREAM_RUNNING;
            break;
        case STREAM_IDEL:
            targetStatus = STREAM_STARTING;
            break;
        case STREAM_PAUSING:
            targetStatus = STREAM_PAUSED;
            break;
        case STREAM_PAUSED:
            targetStatus = STREAM_STARTING;
            break;
        case STREAM_STOPPING:
            targetStatus = STREAM_STOPPED;
            break;
        case STREAM_STOPPED:
            targetStatus = STREAM_RELEASED;
            break;
        default:
            break;
    }

    Trace trace("AudioProcessInClient::InWaitStatus");
    std::unique_lock<std::mutex> lock(loopThreadLock_);
    AUDIO_DEBUG_LOG("Process status is %{public}s now, wait for %{public}s...",
        GetStatusInfo(streamStatus_->load()).c_str(), GetStatusInfo(targetStatus).c_str());
    threadStatus_ = WAITTING;
    threadStatusCV_.wait(lock);
    AUDIO_DEBUG_LOG("Process wait end. Cur is %{public}s now, target is %{public}s...",
        GetStatusInfo(streamStatus_->load()).c_str(), GetStatusInfo(targetStatus).c_str());

    return false;
}

void AudioProcessInClientInner::RecordProcessCallbackFuc()
{
    AUDIO_INFO_LOG("%{public}s enter.", __func__);
    AudioSystemManager::GetInstance()->RequestThreadPriority(gettid());
    uint64_t curReadPos = 0;
    int64_t wakeUpTime = ClockTime::GetCurNano();
    int64_t clientReadCost = 0;

    while (!isCallbackLoopEnd_ && audioBuffer_ != nullptr) {
        if (!KeepLoopRunning()) {
            continue;
        }
        threadStatus_ = INRUNNING;
        Trace traceLoop("AudioProcessInClient Record InRunning");
        if (needReSyncPosition_ && RecordReSyncServicePos() == SUCCESS) {
            wakeUpTime = ClockTime::GetCurNano();
            needReSyncPosition_ = false;
            continue;
        }
        int64_t curTime = ClockTime::GetCurNano();
        int64_t wakeupCost = curTime - wakeUpTime;
        if (wakeupCost > ONE_MILLISECOND_DURATION) {
            AUDIO_WARNING_LOG("loop wake up too late, cost %{public}" PRId64"us", wakeupCost / AUDIO_MS_PER_SECOND);
            wakeUpTime = curTime;
        }

        curReadPos = audioBuffer_->GetCurReadFrame();
        int32_t recordPrepare = RecordPrepareCurrent(curReadPos);
        CHECK_AND_CONTINUE_LOG(recordPrepare == SUCCESS, "prepare current fail.");
        CallClientHandleCurrent();
        int32_t recordFinish = RecordFinishHandleCurrent(curReadPos, clientReadCost);
        CHECK_AND_CONTINUE_LOG(recordFinish == SUCCESS, "finish handle current fail.");

        bool ret = PrepareNext(curReadPos, wakeUpTime);
        CHECK_AND_BREAK_LOG(ret, "prepare next loop in process fail.");

        threadStatus_ = SLEEPING;
        curTime = ClockTime::GetCurNano();
        if (wakeUpTime > curTime && wakeUpTime - curTime < MAX_READ_COST_DURATION_NANO + clientReadCost) {
            ClockTime::AbsoluteSleep(wakeUpTime);
        } else {
            Trace trace("RecordBigWakeUpTime");
            AUDIO_WARNING_LOG("%{public}s wakeUpTime is too late...", __func__);
            ClockTime::RelativeSleep(MAX_READ_COST_DURATION_NANO);
        }
    }
}

int32_t AudioProcessInClientInner::RecordReSyncServicePos()
{
    CHECK_AND_RETURN_RET_LOG(processProxy_ != nullptr && audioBuffer_ != nullptr, ERR_INVALID_HANDLE,
        "%{public}s process proxy or audio buffer is null.", __func__);
    uint64_t serverHandlePos = 0;
    int64_t serverHandleTime = 0;
    int32_t tryTimes = 3;
    int32_t ret = 0;
    while (tryTimes > 0) {
        ret = processProxy_->RequestHandleInfo();
        CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "%{public}s request handle info fail, ret %{public}d.",
            __func__, ret);

        CHECK_AND_RETURN_RET_LOG(audioBuffer_->GetHandleInfo(serverHandlePos, serverHandleTime), ERR_OPERATION_FAILED,
            "%{public}s get handle info fail.", __func__);
        if (serverHandlePos > 0) {
            break;
        }
        ClockTime::RelativeSleep(MAX_READ_COST_DURATION_NANO);
        tryTimes--;
    }
    AUDIO_INFO_LOG("%{public}s get handle info OK, tryTimes %{public}d, serverHandlePos %{public}" PRIu64", "
        "serverHandleTime %{public}" PRId64".", __func__, tryTimes, serverHandlePos, serverHandleTime);
    ClockTime::AbsoluteSleep(serverHandleTime + RECORD_HANDLE_DELAY_NANO);

    ret = audioBuffer_->SetCurReadFrame(serverHandlePos);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "%{public}s set curReadPos fail, ret %{public}d.", __func__, ret);
    return SUCCESS;
}

int32_t AudioProcessInClientInner::RecordPrepareCurrent(uint64_t curReadPos)
{
    CHECK_AND_RETURN_RET_LOG(audioBuffer_ != nullptr, ERR_INVALID_HANDLE,
        "%{public}s audio buffer is null.", __func__);
    SpanInfo *curReadSpan = audioBuffer_->GetSpanInfo(curReadPos);
    CHECK_AND_RETURN_RET_LOG(curReadSpan != nullptr, ERR_INVALID_HANDLE,
        "%{public}s get read span info of process client fail.", __func__);

    int tryCount = 10;
    SpanStatus targetStatus = SpanStatus::SPAN_WRITE_DONE;
    while (!curReadSpan->spanStatus.compare_exchange_strong(targetStatus, SpanStatus::SPAN_READING)
        && tryCount > 0) {
        AUDIO_WARNING_LOG("%{public}s unready, curReadSpan %{public}" PRIu64", curSpanStatus %{public}d, wait 2ms.",
            __func__, curReadPos, curReadSpan->spanStatus.load());
        targetStatus = SpanStatus::SPAN_WRITE_DONE;
        tryCount--;
        ClockTime::RelativeSleep(RECORD_RESYNC_SLEEP_NANO);
    }
    CHECK_AND_RETURN_RET_LOG(tryCount > 0, ERR_INVALID_READ,
        "%{public}s wait too long, curReadSpan %{public}" PRIu64".", __func__, curReadPos);

    curReadSpan->readStartTime = ClockTime::GetCurNano();
    return SUCCESS;
}

int32_t AudioProcessInClientInner::RecordFinishHandleCurrent(uint64_t &curReadPos, int64_t &clientReadCost)
{
    CHECK_AND_RETURN_RET_LOG(audioBuffer_ != nullptr, ERR_INVALID_HANDLE,
        "%{public}s audio buffer is null.", __func__);
    SpanInfo *curReadSpan = audioBuffer_->GetSpanInfo(curReadPos);
    CHECK_AND_RETURN_RET_LOG(curReadSpan != nullptr, ERR_INVALID_HANDLE,
        "%{public}s get read span info of process client fail.", __func__);

    SpanStatus targetStatus = SpanStatus::SPAN_READING;
    if (!curReadSpan->spanStatus.compare_exchange_strong(targetStatus, SpanStatus::SPAN_READ_DONE)) {
        AUDIO_ERR_LOG("%{public}s status error, curReadSpan %{public}" PRIu64", curSpanStatus %{public}d.",
            __func__, curReadPos, curReadSpan->spanStatus.load());
        return ERR_INVALID_OPERATION;
    }
    curReadSpan->readDoneTime = ClockTime::GetCurNano();

    clientReadCost = curReadSpan->readDoneTime - curReadSpan->readStartTime;
    if (clientReadCost > MAX_READ_COST_DURATION_NANO) {
        AUDIO_WARNING_LOG("Client write cost too long...");
    }

    uint64_t nextWritePos = curReadPos + spanSizeInFrame_;
    int32_t ret = audioBuffer_->SetCurReadFrame(nextWritePos);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "%{public}s set next hand frame %{public}" PRIu64" fail, "
        "ret %{public}d.", __func__, nextWritePos, ret);
    curReadPos = nextWritePos;

    return SUCCESS;
}

bool AudioProcessInClientInner::PrepareCurrent(uint64_t curWritePos)
{
    Trace trace("AudioProcessInClient::PrepareCurrent " + std::to_string(curWritePos));
    SpanInfo *tempSpan = audioBuffer_->GetSpanInfo(curWritePos);
    if (tempSpan == nullptr) {
        AUDIO_ERR_LOG("GetSpanInfo failed!");
        return false;
    }

    int tryCount = 10; // try 10 * 2 = 20ms
    SpanStatus targetStatus = SpanStatus::SPAN_READ_DONE;
    while (!tempSpan->spanStatus.compare_exchange_strong(targetStatus, SpanStatus::SPAN_WRITTING) && tryCount-- > 0) {
        AUDIO_WARNING_LOG("span %{public}" PRIu64" not ready, status: %{public}d, wait 2ms.", curWritePos,
            targetStatus);
        targetStatus = SpanStatus::SPAN_READ_DONE;
        ClockTime::RelativeSleep(ONE_MILLISECOND_DURATION + ONE_MILLISECOND_DURATION);
    }
    // If the last attempt is successful, tryCount will be equal to zero.
    CHECK_AND_RETURN_RET_LOG(tryCount >= 0, false,
        "wait on current span  %{public}" PRIu64" too long...", curWritePos);
    tempSpan->writeStartTime = ClockTime::GetCurNano();
    return true;
}

bool AudioProcessInClientInner::PrepareCurrentLoop(uint64_t curWritePos)
{
    size_t round = (spanSizeInFrame_ == 0 ? 1 : clientSpanSizeInFrame_ / spanSizeInFrame_);
    for (size_t count = 0; count < round; count++) {
        uint64_t tmp = curWritePos + count * static_cast<uint64_t>(spanSizeInFrame_);
        bool ret = PrepareCurrent(tmp);
        CHECK_AND_RETURN_RET_LOG(ret, false, "PrepareCurrent failed at %{public}" PRIu64" ", tmp);
    }
    return true;
}

bool AudioProcessInClientInner::FinishHandleCurrent(uint64_t &curWritePos, int64_t &clientWriteCost)
{
    Trace trace("AudioProcessInClient::FinishHandleCurrent " + std::to_string(curWritePos));
    SpanInfo *tempSpan = audioBuffer_->GetSpanInfo(curWritePos);
    CHECK_AND_RETURN_RET_LOG(tempSpan != nullptr, false, "GetSpanInfo failed!");

    int32_t ret = ERROR;
    // mark status write-done and then server can read
    SpanStatus targetStatus = SpanStatus::SPAN_WRITTING;
    if (tempSpan->spanStatus.load() == targetStatus) {
        uint64_t nextWritePos = curWritePos + spanSizeInFrame_;
        ret = audioBuffer_->SetCurWriteFrame(nextWritePos); // move ahead before writedone
        curWritePos = nextWritePos;
        tempSpan->spanStatus.store(SpanStatus::SPAN_WRITE_DONE);
    }
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, false,
        "SetCurWriteFrame %{public}" PRIu64" failed, ret:%{public}d", curWritePos, ret);
    tempSpan->writeDoneTime = ClockTime::GetCurNano();
    tempSpan->volumeStart = processVolume_;
    tempSpan->volumeEnd = processVolume_;
    clientWriteCost = tempSpan->writeDoneTime - tempSpan->writeStartTime;
    if (clientWriteCost > MAX_WRITE_COST_DURATION_NANO) {
        AUDIO_WARNING_LOG("Client write cost too long...");
        underflowCount_++;
        // todo
        // handle write time out: send underrun msg to client, reset time model with latest server handle time.
    }

    return true;
}

bool AudioProcessInClientInner::FinishHandleCurrentLoop(uint64_t &curWritePos, int64_t &clientWriteCost)
{
    size_t round = (spanSizeInFrame_ == 0 ? 1 : clientSpanSizeInFrame_ / spanSizeInFrame_);
    for (size_t count = 0; count < round; count++) {
        uint64_t tmp = curWritePos + count * static_cast<uint64_t>(spanSizeInFrame_);
        bool ret = FinishHandleCurrent(tmp, clientWriteCost);
        CHECK_AND_RETURN_RET_LOG(ret, false, "FinishHandleCurrent failed at %{public}" PRIu64" ", tmp);
    }
    return true;
}

void AudioProcessInClientInner::ProcessCallbackFuc()
{
    AUDIO_INFO_LOG("Callback loop start.");
    AudioSystemManager::GetInstance()->RequestThreadPriority(gettid());

    uint64_t curWritePos = 0;
    int64_t curTime = 0;
    int64_t wakeUpTime = ClockTime::GetCurNano();
    int64_t clientWriteCost = 0;

    while (!isCallbackLoopEnd_) {
        if (!KeepLoopRunning()) {
            continue;
        }
        threadStatus_ = INRUNNING;
        Trace traceLoop("AudioProcessInClient::InRunning");
        curTime = ClockTime::GetCurNano();
        int64_t wakeupCost = curTime - wakeUpTime;
        if (wakeupCost > ONE_MILLISECOND_DURATION) {
            AUDIO_WARNING_LOG("loop wake up too late, cost %{public}" PRId64"us", wakeupCost / AUDIO_MS_PER_SECOND);
            wakeUpTime = curTime;
        }
        curWritePos = audioBuffer_->GetCurWriteFrame();
        bool prepared = true;
        prepared = PrepareCurrentLoop(curWritePos);
        if (!prepared) {
            continue;
        }
        // call client write
        CallClientHandleCurrent();
        // client write done, check if time out
        bool finished = true;
        finished = FinishHandleCurrentLoop(curWritePos, clientWriteCost);
        if (!finished) {
            continue;
        }
        prepared = ClientPrepareNextLoop(curWritePos, wakeUpTime);
        if (!prepared) {
            break;
        }
        traceLoop.End();
        // start safe sleep
        threadStatus_ = SLEEPING;
        curTime = ClockTime::GetCurNano();
        if (wakeUpTime - curTime > MAX_WRITE_COST_DURATION_NANO + clientWriteCost) {
            Trace trace("BigWakeUpTime curTime[" + std::to_string(curTime) + "] target[" + std::to_string(wakeUpTime) +
                "] delay " + std::to_string(wakeUpTime - curTime) + "ns");
            AUDIO_WARNING_LOG("wakeUpTime is too late...");
        }
        ClockTime::AbsoluteSleep(wakeUpTime);
    }
}

void AudioProcessInClientInner::ProcessCallbackFucIndependent()
{
    AUDIO_INFO_LOG("multi play loop start");
    AudioSystemManager::GetInstance()->RequestThreadPriority(gettid());
    int64_t curTime = 0;
    uint64_t curWritePos = 0;
    int64_t wakeUpTime = ClockTime::GetCurNano();
    while (!isCallbackLoopEnd_) {
        if (!KeepLoopRunningIndependent()) {
            continue;
        }
        int32_t ret = 0;
        threadStatus_ = INRUNNING;
        curTime = ClockTime::GetCurNano();
        Trace traceLoop("AudioProcessInClient::InRunning");
        if (needReSyncPosition_) {
            UpdateHandleInfo(true, true);
            wakeUpTime = curTime;
            needReSyncPosition_ = false;
            continue;
        }
        curWritePos = audioBuffer_->GetCurWriteFrame();
        if (streamStatus_->load() == STREAM_RUNNING) {
            CallClientHandleCurrent();
        } else {
            AudioStreamData writeStreamData;
            ret = audioBuffer_->GetWriteBuffer(curWritePos, writeStreamData.bufferDesc);
            if (ret != SUCCESS || writeStreamData.bufferDesc.buffer == nullptr) {
                AUDIO_INFO_LOG("ret is fail or buffer is nullptr");
                return;
            }
            memset_s(writeStreamData.bufferDesc.buffer, writeStreamData.bufferDesc.bufLength,
                0, writeStreamData.bufferDesc.bufLength);
        }
        bool prepared = true;
        size_t round = (spanSizeInFrame_ == 0 ? 1 : clientSpanSizeInFrame_ / spanSizeInFrame_);
        for (size_t count = 0; count < round; count++) {
            if (!PrepareNextIndependent(curWritePos + count * spanSizeInFrame_, wakeUpTime)) {
                prepared = false;
                AUDIO_ERR_LOG("PrepareNextLoop failed!");
                break;
            }
        }
        if (!prepared) {
            break;
        }
        traceLoop.End();
        // start sleep
        threadStatus_ = SLEEPING;

        ClockTime::AbsoluteSleep(wakeUpTime);
    }
}

bool AudioProcessInClientInner::KeepLoopRunningIndependent()
{
    switch (streamStatus_->load()) {
        case STREAM_RUNNING:
            return true;
        case STREAM_IDEL:
            return true;
        case STREAM_PAUSED:
            return true;
        default:
            break;
    }

    return false;
}

bool AudioProcessInClientInner::PrepareNextIndependent(uint64_t curWritePos, int64_t &wakeUpTime)
{
    uint64_t nextHandlePos = curWritePos + spanSizeInFrame_;
    Trace prepareTrace("AudioEndpoint::PrepareNextLoop " + std::to_string(nextHandlePos));
    int64_t nextHdiReadTime = GetPredictNextHandleTime(nextHandlePos, true);
    int64_t aheadTime = spanSizeInFrame_ * AUDIO_NS_PER_SECOND / processConfig_.streamInfo.samplingRate;
    int64_t nextServerHandleTime = nextHdiReadTime - aheadTime;
    if (nextServerHandleTime < ClockTime::GetCurNano()) {
        wakeUpTime = ClockTime::GetCurNano() + ONE_MILLISECOND_DURATION; // make sure less than duration
    } else {
        wakeUpTime = nextServerHandleTime;
    }

    SpanInfo *nextWriteSpan = audioBuffer_->GetSpanInfo(nextHandlePos);
    if (nextWriteSpan == nullptr) {
        AUDIO_ERR_LOG("GetSpanInfo failed, can not get next write span");
        return false;
    }

    int32_t ret1 = audioBuffer_->SetCurWriteFrame(nextHandlePos);
    int32_t ret2 = audioBuffer_->SetCurReadFrame(nextHandlePos);
    if (ret1 != SUCCESS || ret2 != SUCCESS) {
        AUDIO_ERR_LOG("SetCurWriteFrame or SetCurReadFrame failed, ret1:%{public}d ret2:%{public}d", ret1, ret2);
        return false;
    }
    return true;
}
} // namespace AudioStandard
} // namespace OHOS
