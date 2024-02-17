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

#include "offload_audio_renderer_sink.h"

#include <cstring>
#include <cinttypes>
#include <dlfcn.h>
#include <string>
#include <unistd.h>
#include <future>

#ifdef FEATURE_POWER_MANAGER
#include "power_mgr_client.h"
#include "running_lock.h"
#endif
#include "v2_0/iaudio_manager.h"

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
const uint32_t DEEP_BUFFER_RENDER_PERIOD_SIZE = 4096;
const uint32_t INT_32_MAX = 0x7fffffff;
const uint32_t PCM_8_BIT = 8;
const uint32_t PCM_16_BIT = 16;
const uint32_t PCM_24_BIT = 24;
const uint32_t PCM_32_BIT = 32;
const uint32_t OFFLOAD_OUTPUT_STREAM_ID = 53; // 13+5*8
const uint32_t STEREO_CHANNEL_COUNT = 2;
#ifdef FEATURE_POWER_MANAGER
constexpr int32_t RUNNINGLOCK_LOCK_TIMEOUTMS_LASTING = -1;
#endif
const int64_t SECOND_TO_NANOSECOND = 1000000000;
const int64_t SECOND_TO_MICROSECOND = 1000000;
const int64_t SECOND_TO_MILLISECOND = 1000;
const uint32_t BIT_IN_BYTE = 8;
}

struct AudioCallbackService {
    struct IAudioCallback interface;
    void *cookie;
    OnRenderCallback* renderCallback;
    void *userdata;
    bool registered = false;
};

class OffloadAudioRendererSinkInner : public OffloadRendererSink {
public:
    int32_t Init(const IAudioSinkAttr& attr) override;
    bool IsInited(void) override;
    void DeInit(void) override;

    int32_t Flush(void) override;
    int32_t Pause(void) override;
    int32_t Reset(void) override;
    int32_t Resume(void) override;
    int32_t Start(void) override;
    int32_t Stop(void) override;
    int32_t Drain(AudioDrainType type) override;
    int32_t SetBufferSize(uint32_t sizeMs) override;

    int32_t OffloadRunningLockInit(void) override;
    int32_t OffloadRunningLockLock(void) override;
    int32_t OffloadRunningLockUnlock(void) override;

    int32_t RenderFrame(char &data, uint64_t len, uint64_t &writeLen) override;
    int32_t SetVolume(float left, float right) override;
    int32_t GetVolume(float &left, float &right) override;
    int32_t SetVoiceVolume(float volume) override;
    int32_t GetLatency(uint32_t *latency) override;
    int32_t GetTransactionId(uint64_t *transactionId) override;
    int32_t SetAudioScene(AudioScene audioScene, DeviceType activeDevice) override;

    void SetAudioParameter(const AudioParamKey key, const std::string& condition, const std::string& value) override;
    std::string GetAudioParameter(const AudioParamKey key, const std::string& condition) override;
    void RegisterParameterCallback(IAudioSinkCallback* callback) override;
    int32_t RegisterRenderCallback(OnRenderCallback (*callback), int8_t *userdata) override;
    int32_t GetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec) override;

    static int32_t RenderEventCallback(struct IAudioCallback *self, RenderCallbackType type, int8_t *reserved,
        int8_t *cookie);

    void SetAudioMonoState(bool audioMono) override;
    void SetAudioBalanceValue(float audioBalance) override;
    int32_t SetOutputRoute(DeviceType outputDevice) override;

    OffloadAudioRendererSinkInner();
    ~OffloadAudioRendererSinkInner();
private:
    IAudioSinkAttr attr_ = {};
    bool rendererInited_;
    bool started_;
    bool isFlushing_;
    bool startDuringFlush_;
    uint64_t renderPos_;
    float leftVolume_;
    float rightVolume_;
    uint32_t renderId_ = 0;
    std::string adapterNameCase_;
    struct IAudioManager *audioManager_;
    struct IAudioAdapter *audioAdapter_;
    struct IAudioRender *audioRender_;
    struct AudioAdapterDescriptor adapterDesc_;
    struct AudioPort audioPort_ = {};
    struct AudioCallbackService callbackServ = {};
    bool audioMonoState_ = false;
    bool audioBalanceState_ = false;
    float leftBalanceCoef_ = 1.0f;
    float rightBalanceCoef_ = 1.0f;

    int32_t CreateRender(const struct AudioPort &renderPort);
    int32_t InitAudioManager();
    AudioFormat ConverToHdiFormat(HdiAdapterFormat format);
    void AdjustStereoToMono(char *data, uint64_t len);
    void AdjustAudioBalance(char *data, uint64_t len);

#ifdef FEATURE_POWER_MANAGER
    std::shared_ptr<PowerMgr::RunningLock> OffloadKeepRunningLock;
    bool runninglocked;
#endif

    FILE *dumpFile_ = nullptr;
};
    
OffloadAudioRendererSinkInner::OffloadAudioRendererSinkInner()
    : rendererInited_(false), started_(false), isFlushing_(false), startDuringFlush_(false), renderPos_(0),
      leftVolume_(DEFAULT_VOLUME_LEVEL), rightVolume_(DEFAULT_VOLUME_LEVEL),
      audioManager_(nullptr), audioAdapter_(nullptr), audioRender_(nullptr)
{
#ifdef FEATURE_POWER_MANAGER
    runninglocked = false;
#endif
}

OffloadAudioRendererSinkInner::~OffloadAudioRendererSinkInner()
{
    AUDIO_DEBUG_LOG("~OffloadAudioRendererSinkInner");
}

OffloadRendererSink *OffloadRendererSink::GetInstance()
{
    static OffloadAudioRendererSinkInner audioRenderer;

    return &audioRenderer;
}

void OffloadAudioRendererSinkInner::SetAudioParameter(const AudioParamKey key, const std::string& condition,
    const std::string& value)
{
    AUDIO_INFO_LOG("key %{public}d, condition: %{public}s, value: %{public}s", key,
        condition.c_str(), value.c_str());
    AudioExtParamKey hdiKey = AudioExtParamKey(key);
    CHECK_AND_RETURN_LOG(audioAdapter_ != nullptr, "SetAudioParameter failed, audioAdapter_ is null");
    int32_t ret = audioAdapter_->SetExtraParams(audioAdapter_, hdiKey, condition.c_str(), value.c_str());
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("SetAudioParameter failed, error code: %{public}d", ret);
    }
}

std::string OffloadAudioRendererSinkInner::GetAudioParameter(const AudioParamKey key, const std::string& condition)
{
    AUDIO_INFO_LOG("key %{public}d, condition: %{public}s", key,
        condition.c_str());
    AudioExtParamKey hdiKey = AudioExtParamKey(key);
    char value[PARAM_VALUE_LENTH];
    CHECK_AND_RETURN_RET_LOG(audioAdapter_ != nullptr, "", "GetAudioParameter failed, audioAdapter_ is null");
    int32_t ret = audioAdapter_->GetExtraParams(audioAdapter_, hdiKey, condition.c_str(), value, PARAM_VALUE_LENTH);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, "", "GetAudioParameter failed, error code: %{public}d", ret);
    return value;
}

void OffloadAudioRendererSinkInner::SetAudioMonoState(bool audioMono)
{
    audioMonoState_ = audioMono;
}

void OffloadAudioRendererSinkInner::SetAudioBalanceValue(float audioBalance)
{
    // reset the balance coefficient value firstly
    leftBalanceCoef_ = 1.0f;
    rightBalanceCoef_ = 1.0f;

    if (std::abs(audioBalance - 0.0f) <= std::numeric_limits<float>::epsilon()) {
        // audioBalance is equal to 0.0f
        audioBalanceState_ = false;
    } else {
        // audioBalance is not equal to 0.0f
        audioBalanceState_ = true;
        // calculate the balance coefficient
        if (audioBalance > 0.0f) {
            leftBalanceCoef_ -= audioBalance;
        } else if (audioBalance < 0.0f) {
            rightBalanceCoef_ += audioBalance;
        }
    }
}

void OffloadAudioRendererSinkInner::AdjustStereoToMono(char *data, uint64_t len)
{
    // only stereo is surpported now (stereo channel count is 2)
    CHECK_AND_RETURN_LOG(attr_.channel == STEREO_CHANNEL_COUNT, "Unspport channel number: %{public}d", attr_.channel);

    switch (attr_.format) {
        case SAMPLE_U8: {
            // this function needs to be further tested for usability
            AdjustStereoToMonoForPCM8Bit(reinterpret_cast<int8_t *>(data), len);
            break;
        }
        case SAMPLE_S16LE: {
            AdjustStereoToMonoForPCM16Bit(reinterpret_cast<int16_t *>(data), len);
            break;
        }
        case SAMPLE_S24LE: {
            // this function needs to be further tested for usability
            AdjustStereoToMonoForPCM24Bit(reinterpret_cast<int8_t *>(data), len);
            break;
        }
        case SAMPLE_S32LE: {
            AdjustStereoToMonoForPCM32Bit(reinterpret_cast<int32_t *>(data), len);
            break;
        }
        default: {
            // if the audio format is unsupported, the audio data will not be changed
            AUDIO_ERR_LOG("Unsupported audio format: %{public}d", attr_.format);
            break;
        }
    }
}

void OffloadAudioRendererSinkInner::AdjustAudioBalance(char *data, uint64_t len)
{
    // only stereo is surpported now (stereo channel count is 2)
    CHECK_AND_RETURN_LOG(attr_.channel == STEREO_CHANNEL_COUNT, "Unspport channel number: %{public}d", attr_.channel);

    switch (attr_.format) {
        case SAMPLE_U8: {
            // this function needs to be further tested for usability
            AdjustAudioBalanceForPCM8Bit(reinterpret_cast<int8_t *>(data), len, leftBalanceCoef_, rightBalanceCoef_);
            break;
        }
        case SAMPLE_S16LE: {
            AdjustAudioBalanceForPCM16Bit(reinterpret_cast<int16_t *>(data), len, leftBalanceCoef_, rightBalanceCoef_);
            break;
        }
        case SAMPLE_S24LE: {
            // this function needs to be further tested for usability
            AdjustAudioBalanceForPCM24Bit(reinterpret_cast<int8_t *>(data), len, leftBalanceCoef_, rightBalanceCoef_);
            break;
        }
        case SAMPLE_S32LE: {
            AdjustAudioBalanceForPCM32Bit(reinterpret_cast<int32_t *>(data), len, leftBalanceCoef_, rightBalanceCoef_);
            break;
        }
        default: {
            // if the audio format is unsupported, the audio data will not be changed
            AUDIO_ERR_LOG("Unsupported audio format: %{public}d", attr_.format);
            break;
        }
    }
}

bool OffloadAudioRendererSinkInner::IsInited()
{
    return rendererInited_;
}

void OffloadAudioRendererSinkInner::RegisterParameterCallback(IAudioSinkCallback* callback)
{
    AUDIO_WARNING_LOG("not supported.");
}

typedef int32_t (*RenderCallback)(struct IAudioCallback *self, enum AudioCallbackType type, int8_t* reserved,
    int8_t* cookie);

int32_t OffloadAudioRendererSinkInner::RegisterRenderCallback(OnRenderCallback (*callback), int8_t *userdata)
{
    callbackServ.renderCallback = callback;
    callbackServ.userdata = userdata;
    if (callbackServ.registered) {
        AUDIO_DEBUG_LOG("update callback");
        return SUCCESS;
    }
    // register to adapter
    auto renderCallback = (RenderCallback) &OffloadAudioRendererSinkInner::RenderEventCallback;
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE, "audioRender_ is null");
    callbackServ.interface.RenderCallback = renderCallback;
    callbackServ.cookie = this;
    int32_t ret = audioRender_->RegCallback(audioRender_, &callbackServ.interface, (int8_t)0);
    if (ret != SUCCESS) {
        AUDIO_WARNING_LOG("failed, error code: %{public}d", ret);
    } else {
        callbackServ.registered = true;
    }
    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::RenderEventCallback(struct IAudioCallback* self, RenderCallbackType type,
    int8_t* reserved, int8_t* cookie)
{
    // reserved and cookie should be null
    if (self == nullptr) {
        AUDIO_WARNING_LOG("self is null!");
    }
    auto *impl = reinterpret_cast<struct AudioCallbackService *>(self);
    if (!impl->registered || impl->cookie == nullptr || impl->renderCallback == nullptr) {
        AUDIO_ERR_LOG("impl invalid, %{public}d, %{public}d, %{public}d",
            impl->registered, impl->cookie == nullptr, impl->renderCallback == nullptr);
    }
    auto *sink = reinterpret_cast<OffloadAudioRendererSinkInner *>(impl->cookie);
    if (!sink->started_ || sink->isFlushing_) {
        AUDIO_DEBUG_LOG("invalid renderCallback call, started_ %d, isFlushing_ %d", sink->started_, sink->isFlushing_);
        return 0;
    }

    auto cbType = RenderCallbackType(type);
    impl->renderCallback(cbType, reinterpret_cast<int8_t*>(impl->userdata));
    return 0;
}

int32_t OffloadAudioRendererSinkInner::GetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec)
{
    int32_t ret;
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE, "failed audioRender_ is NULL");
    uint64_t frames_;
    struct AudioTimeStamp timestamp = {};
    ret = audioRender_->GetRenderPosition(audioRender_, &frames_, &timestamp);
    CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_OPERATION_FAILED, "offload failed");
    int64_t maxSec = 9223372036; // (9223372036 + 1) * 10^9 > INT64_MAX, seconds should not bigger than it;
    CHECK_AND_RETURN_RET_LOG(timestamp.tvSec >= 0 && timestamp.tvSec <= maxSec && timestamp.tvNSec >= 0 &&
        timestamp.tvNSec <= SECOND_TO_NANOSECOND, ERR_OPERATION_FAILED,
        "Hdi GetRenderPosition get invaild second:%{public}" PRIu64 " or nanosecond:%{public}" PRIu64 " !",
        timestamp.tvSec, timestamp.tvNSec);
    frames = frames_ * SECOND_TO_MICROSECOND / attr_.sampleRate;
    timeSec = timestamp.tvSec;
    timeNanoSec = timestamp.tvNSec;
    return ret;
}

void OffloadAudioRendererSinkInner::DeInit()
{
    AUDIO_DEBUG_LOG("DeInit.");
    started_ = false;
    rendererInited_ = false;
    if (audioAdapter_ != nullptr) {
        audioAdapter_->DestroyRender(audioAdapter_, renderId_);
    }
    audioRender_ = nullptr;

    if (audioManager_ != nullptr) {
        audioManager_->UnloadAdapter(audioManager_, adapterDesc_.adapterName);
    }
    audioAdapter_ = nullptr;
    audioManager_ = nullptr;
    callbackServ = {};

    DumpFileUtil::CloseDumpFile(&dumpFile_);
}

void InitAttrs(struct AudioSampleAttributes &attrs)
{
    /* Initialization of audio parameters for playback*/
    attrs.channelCount = AUDIO_CHANNELCOUNT;
    attrs.sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs.interleaved = true;
    attrs.streamId = OFFLOAD_OUTPUT_STREAM_ID;
    attrs.type = AUDIO_OFFLOAD;
    attrs.period = DEEP_BUFFER_RENDER_PERIOD_SIZE;
    attrs.isBigEndian = false;
    attrs.isSignedData = true;
    attrs.stopThreshold = INT_32_MAX;
    attrs.silenceThreshold = 0;
    // AudioOffloadInfo attr
    attrs.offloadInfo.sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs.offloadInfo.channelCount = AUDIO_CHANNELCOUNT;
    attrs.offloadInfo.bitRate = AUDIO_SAMPLE_RATE_48K * BIT_IN_BYTE;
    attrs.offloadInfo.bitWidth = PCM_32_BIT;
}

static int32_t SwitchAdapterRender(struct AudioAdapterDescriptor *descs, const string &adapterNameCase,
    enum AudioPortDirection portFlag, struct AudioPort &renderPort, uint32_t size)
{
    if (descs == nullptr) {
        return ERROR;
    }
    for (uint32_t index = 0; index < size; index++) {
        struct AudioAdapterDescriptor *desc = &descs[index];
        if (desc == nullptr || desc->adapterName == nullptr) {
            continue;
        }
        AUDIO_DEBUG_LOG("index %{public}u, adapterName %{public}s", index, desc->adapterName);
        if (strcmp(desc->adapterName, adapterNameCase.c_str())) {
            continue;
        }
        for (uint32_t port = 0; port < desc->portsLen; port++) {
            // Only find out the port of out in the sound card
            if (desc->ports[port].dir == portFlag) {
                renderPort = desc->ports[port];
                return index;
            }
        }
    }
    AUDIO_ERR_LOG("switch adapter render fail");
    return ERR_INVALID_INDEX;
}

int32_t OffloadAudioRendererSinkInner::InitAudioManager()
{
    audioManager_ = IAudioManagerGet(false);
    CHECK_AND_RETURN_RET(audioManager_ != nullptr, ERR_INVALID_HANDLE);
    return 0;
}

uint32_t PcmFormatToBits(enum AudioFormat format)
{
    switch (format) {
        case AUDIO_FORMAT_TYPE_PCM_8_BIT:
            return PCM_8_BIT;
        case AUDIO_FORMAT_TYPE_PCM_16_BIT:
            return PCM_16_BIT;
        case AUDIO_FORMAT_TYPE_PCM_24_BIT:
            return PCM_24_BIT;
        case AUDIO_FORMAT_TYPE_PCM_32_BIT:
            return PCM_32_BIT;
        default:
            AUDIO_DEBUG_LOG("Unkown format type, set it to defalut");
            return PCM_24_BIT;
    }
}

AudioFormat OffloadAudioRendererSinkInner::ConverToHdiFormat(HdiAdapterFormat format)
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
        case SAMPLE_F32:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_FLOAT;
            break;
        default:
            hdiFormat = AUDIO_FORMAT_TYPE_PCM_16_BIT;
            break;
    }

    return hdiFormat;
}

int32_t OffloadAudioRendererSinkInner::CreateRender(const struct AudioPort &renderPort)
{
    int32_t ret;
    struct AudioSampleAttributes param;
    struct AudioDeviceDescriptor deviceDesc;
    InitAttrs(param);
    param.sampleRate = attr_.sampleRate;
    param.channelCount = attr_.channel;
    if (param.channelCount == MONO) {
        param.channelLayout = CH_LAYOUT_MONO;
    } else if (param.channelCount == STEREO) {
        param.channelLayout = CH_LAYOUT_STEREO;
    }
    param.format = ConverToHdiFormat(attr_.format);
    param.offloadInfo.format = ConverToHdiFormat(attr_.format);
    param.frameSize = PcmFormatToBits(param.format) * param.channelCount / PCM_8_BIT;
    param.startThreshold = DEEP_BUFFER_RENDER_PERIOD_SIZE / (param.frameSize);

    deviceDesc.portId = renderPort.portId;
    deviceDesc.desc = const_cast<char *>("");
    deviceDesc.pins = PIN_OUT_SPEAKER;
    ret = audioAdapter_->CreateRender(audioAdapter_, &deviceDesc, &param, &audioRender_, &renderId_);
    if (ret != 0 || audioRender_ == nullptr) {
        AUDIO_ERR_LOG("not started failed.");
        audioManager_->UnloadAdapter(audioManager_, adapterDesc_.adapterName);
        return ERR_NOT_STARTED;
    }

    return 0;
}

int32_t OffloadAudioRendererSinkInner::Init(const IAudioSinkAttr &attr)
{
    attr_ = attr;
    adapterNameCase_ = attr_.adapterName; // Set sound card information
    enum AudioPortDirection port = PORT_OUT; // Set port information

    CHECK_AND_RETURN_RET_LOG(InitAudioManager() == 0, ERR_NOT_STARTED, "Init audio manager Fail.");

    uint32_t size = MAX_AUDIO_ADAPTER_NUM;
    int32_t ret;
    AudioAdapterDescriptor descs[MAX_AUDIO_ADAPTER_NUM];
    ret = audioManager_->GetAllAdapters(audioManager_, (struct AudioAdapterDescriptor *)&descs, &size);
    CHECK_AND_RETURN_RET_LOG(size <= MAX_AUDIO_ADAPTER_NUM && size != 0 && ret == 0, ERR_NOT_STARTED,
        "Get adapters Fail.");

    // Get qualified sound card and port
    int32_t index =
        SwitchAdapterRender((struct AudioAdapterDescriptor *)&descs, adapterNameCase_, port, audioPort_, size);
    CHECK_AND_RETURN_RET_LOG(index >= 0, ERR_NOT_STARTED, "Switch Adapter Fail.");

    adapterDesc_ = descs[index];
    ret = audioManager_->LoadAdapter(audioManager_, &adapterDesc_, &audioAdapter_);
    CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_NOT_STARTED, "Load Adapter Fail.");

    CHECK_AND_RETURN_RET_LOG(audioAdapter_ != nullptr, ERR_NOT_STARTED, "Load audio device failed.");

    // Initialization port information, can fill through mode and other parameters
    int32_t result = audioAdapter_->InitAllPorts(audioAdapter_);
    CHECK_AND_RETURN_RET_LOG(result == 0, ERR_NOT_STARTED, "InitAllPorts failed.");

    int32_t tmp = CreateRender(audioPort_);
    CHECK_AND_RETURN_RET_LOG(tmp == 0, ERR_NOT_STARTED,
        "Create render failed, Audio Port: %{public}d", audioPort_.portId);
    rendererInited_ = true;

    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::RenderFrame(char &data, uint64_t len, uint64_t &writeLen)
{
    CHECK_AND_RETURN_RET_LOG(!isFlushing_, ERR_OPERATION_FAILED, "failed! during flushing");
    CHECK_AND_RETURN_RET_LOG(started_, ERR_OPERATION_FAILED, "failed! state not in started");
    int32_t ret;
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE, "Audio Render Handle is nullptr!");

    if (audioMonoState_) {
        AdjustStereoToMono(&data, len);
    }

    if (audioBalanceState_) {
        AdjustAudioBalance(&data, len);
    }

    Trace trace("RenderFrameOffload");
    ret = audioRender_->RenderFrame(audioRender_, reinterpret_cast<int8_t*>(&data), static_cast<uint32_t>(len),
        &writeLen);
    if (ret == 0 && writeLen != 0) {
        DumpFileUtil::WriteDumpFile(dumpFile_, static_cast<void *>(&data), writeLen);
    }
    CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_WRITE_FAILED, "RenderFrameOffload failed! ret: %{public}x", ret);
    renderPos_ += writeLen;
    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::Start(void)
{
    Trace trace("Sink::Start");
    if (started_) {
        if (isFlushing_) {
            AUDIO_ERR_LOG("start failed! during flushing");
            startDuringFlush_ = true;
            return ERR_OPERATION_FAILED;
        } else {
            AUDIO_WARNING_LOG("start duplicate!"); // when start while flushing, this will use
            return SUCCESS;
        }
    }

    int32_t ret = audioRender_->Start(audioRender_);
    if (ret) {
        AUDIO_ERR_LOG("Start failed! ret %d", ret);
        return ERR_NOT_STARTED;
    }

    DumpFileUtil::OpenDumpFile(DUMP_SERVER_PARA, DUMP_OFFLOAD_RENDER_SINK_FILENAME, &dumpFile_);

    started_ = true;
    renderPos_ = 0;
    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::SetVolume(float left, float right)
{
    int32_t ret;
    float thevolume;

    if (audioRender_ == nullptr) {
        AUDIO_WARNING_LOG("OffloadAudioRendererSinkInner::SetVolume failed, audioRender_ null, "
                          "this will happen when set volume on devices which offload not available");
        return ERR_INVALID_HANDLE;
    }

    leftVolume_ = left;
    rightVolume_ = right;
    if ((leftVolume_ == 0) && (rightVolume_ !=0)) {
        thevolume = rightVolume_;
    } else if ((leftVolume_ != 0) && (rightVolume_ ==0)) {
        thevolume = leftVolume_;
    } else {
        thevolume = (leftVolume_ + rightVolume_) / HALF_FACTOR;
    }

    ret = audioRender_->SetVolume(audioRender_, thevolume);
    if (ret) {
        AUDIO_ERR_LOG("Set volume failed!");
    }
    return ret;
}

int32_t OffloadAudioRendererSinkInner::GetVolume(float &left, float &right)
{
    left = leftVolume_;
    right = rightVolume_;
    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::SetVoiceVolume(float volume)
{
    CHECK_AND_RETURN_RET_LOG(audioAdapter_ != nullptr, ERR_INVALID_HANDLE,
        "failed, audioAdapter_ is null");
    AUDIO_DEBUG_LOG("Set void volume %{public}f", volume);
    return audioAdapter_->SetVoiceVolume(audioAdapter_, volume);
}

int32_t OffloadAudioRendererSinkInner::GetLatency(uint32_t *latency)
{
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "GetLatency failed audio render null");

    CHECK_AND_RETURN_RET_LOG(latency, ERR_INVALID_PARAM,
        "GetLatency failed latency null");

    uint32_t hdiLatency;
    if (audioRender_->GetLatency(audioRender_, &hdiLatency) == 0) {
        *latency = hdiLatency;
        return SUCCESS;
    } else {
        return ERR_OPERATION_FAILED;
    }
}

int32_t OffloadAudioRendererSinkInner::SetOutputRoute(DeviceType outputDevice)
{
    AUDIO_WARNING_LOG("not supported.");
    return ERR_NOT_SUPPORTED;
}

int32_t OffloadAudioRendererSinkInner::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    AUDIO_WARNING_LOG("not supported.");
    return ERR_NOT_SUPPORTED;
}

int32_t OffloadAudioRendererSinkInner::GetTransactionId(uint64_t *transactionId)
{
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        " failed audio render null");

    CHECK_AND_RETURN_RET_LOG(transactionId, ERR_INVALID_PARAM,
        "failed transaction Id null");

    *transactionId = reinterpret_cast<uint64_t>(audioRender_);
    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::Drain(AudioDrainType type)
{
    int32_t ret;
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "failed audio render null");

    ret = audioRender_->DrainBuffer(audioRender_, (AudioDrainNotifyType*)&type);
    if (!ret) {
        return SUCCESS;
    } else {
        AUDIO_ERR_LOG("DrainBuffer failed!");
        return ERR_OPERATION_FAILED;
    }

    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::Stop(void)
{
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "failed audio render null");

    if (started_) {
        CHECK_AND_RETURN_RET_LOG(!Flush(), ERR_OPERATION_FAILED, "Flush failed!");
        int32_t ret = audioRender_->Stop(audioRender_);
        if (!ret) {
            started_ = false;
            return SUCCESS;
        } else {
            AUDIO_ERR_LOG("Stop failed!");
            return ERR_OPERATION_FAILED;
        }
    }
    OffloadRunningLockUnlock();
    AUDIO_WARNING_LOG("Stop duplicate");

    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::Pause(void)
{
    AUDIO_ERR_LOG("Pause not use yet");
    return ERR_NOT_SUPPORTED;
}

int32_t OffloadAudioRendererSinkInner::Resume(void)
{
    AUDIO_ERR_LOG("Resume not use yet");
    return ERR_NOT_SUPPORTED;
}

int32_t OffloadAudioRendererSinkInner::Reset(void)
{
    if (started_ && audioRender_ != nullptr) {
        startDuringFlush_ = true;
        if (!Flush()) {
            return SUCCESS;
        } else {
            startDuringFlush_ = false;
            AUDIO_ERR_LOG("Reset failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return ERR_OPERATION_FAILED;
}

int32_t OffloadAudioRendererSinkInner::Flush(void)
{
    CHECK_AND_RETURN_RET_LOG(!isFlushing_, ERR_OPERATION_FAILED,
        "failed call flush during flushing");
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "failed audio render null");
    CHECK_AND_RETURN_RET_LOG(started_, ERR_INVALID_HANDLE,
        "failed state is not started");
    isFlushing_ = true;
    thread([&] {
        auto future = async(launch::async, [&] { return audioRender_->Flush(audioRender_); });
        if (future.wait_for(250ms) == future_status::timeout) { // max wait 250ms
            AUDIO_ERR_LOG("Flush failed! timeout of 250ms");
        } else {
            int32_t ret = future.get();
            if (ret) {
                AUDIO_ERR_LOG("Flush failed! ret %{public}d", ret);
            }
        }
        isFlushing_ = false;
        if (startDuringFlush_) {
            startDuringFlush_ = false;
            Start();
        }
    }).detach();
    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::SetBufferSize(uint32_t sizeMs)
{
    int32_t ret;

    // bytewidth is 4
    uint32_t size = (int64_t) sizeMs * AUDIO_SAMPLE_RATE_48K * 4 * STEREO_CHANNEL_COUNT / SECOND_TO_MILLISECOND;
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "failed audio render null");

    ret = audioRender_->SetBufferSize(audioRender_, size);
    CHECK_AND_RETURN_RET_LOG(!ret, ERR_OPERATION_FAILED,
        "SetBufferSize failed!");

    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::OffloadRunningLockInit(void)
{
#ifdef FEATURE_POWER_MANAGER
    CHECK_AND_RETURN_RET_LOG(OffloadKeepRunningLock == nullptr, ERR_OPERATION_FAILED,
        "OffloadKeepRunningLock is not null, init failed!");
    OffloadKeepRunningLock = PowerMgr::PowerMgrClient::GetInstance().CreateRunningLock("AudioOffloadBackgroudPlay",
        PowerMgr::RunningLockType::RUNNINGLOCK_BACKGROUND_AUDIO);
#endif
    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::OffloadRunningLockLock(void)
{
#ifdef FEATURE_POWER_MANAGER
    if (OffloadKeepRunningLock == nullptr) {
        OffloadKeepRunningLock = PowerMgr::PowerMgrClient::GetInstance().CreateRunningLock("AudioOffloadBackgroudPlay",
            PowerMgr::RunningLockType::RUNNINGLOCK_BACKGROUND_AUDIO);
    }
    CHECK_AND_RETURN_RET_LOG(OffloadKeepRunningLock != nullptr, ERR_OPERATION_FAILED,
        "OffloadKeepRunningLock is null, playback can not work well!");
    CHECK_AND_RETURN_RET(!runninglocked, SUCCESS);
    runninglocked = true;
    OffloadKeepRunningLock->Lock(RUNNINGLOCK_LOCK_TIMEOUTMS_LASTING); // -1 for lasting.
#endif

    return SUCCESS;
}

int32_t OffloadAudioRendererSinkInner::OffloadRunningLockUnlock(void)
{
#ifdef FEATURE_POWER_MANAGER
    CHECK_AND_RETURN_RET_LOG(OffloadKeepRunningLock != nullptr, ERR_OPERATION_FAILED,
        "OffloadKeepRunningLock is null, playback can not work well!");
    CHECK_AND_RETURN_RET(runninglocked, SUCCESS);
    runninglocked = false;
    OffloadKeepRunningLock->UnLock();
#endif

    return SUCCESS;
}

} // namespace AudioStandard
} // namespace OHOS
