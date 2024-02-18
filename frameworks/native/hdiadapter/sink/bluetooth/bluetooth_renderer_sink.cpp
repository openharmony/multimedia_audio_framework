/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "bluetooth_renderer_sink.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <list>
#include <cinttypes>

#include <dlfcn.h>
#include <unistd.h>

#include "audio_proxy_manager.h"
#ifdef FEATURE_POWER_MANAGER
#include "running_lock.h"
#include "power_mgr_client.h"
#endif

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_utils.h"

using namespace std;
using namespace OHOS::HDI::Audio_Bluetooth;

namespace OHOS {
namespace AudioStandard {
namespace {
const int32_t HALF_FACTOR = 2;
const int32_t MAX_AUDIO_ADAPTER_NUM = 5;
const int32_t RENDER_FRAME_NUM = -4;
const float DEFAULT_VOLUME_LEVEL = 1.0f;
const uint32_t AUDIO_CHANNELCOUNT = 2;
const uint32_t AUDIO_SAMPLE_RATE_48K = 48000;
const uint32_t DEEP_BUFFER_RENDER_PERIOD_SIZE = 4096;
const uint32_t RENDER_FRAME_INTERVAL_IN_MICROSECONDS = 10000;
const uint32_t INT_32_MAX = 0x7fffffff;
const uint32_t PCM_8_BIT = 8;
const uint32_t PCM_16_BIT = 16;
const uint32_t PCM_24_BIT = 24;
const uint32_t PCM_32_BIT = 32;
const uint32_t STEREO_CHANNEL_COUNT = 2;
constexpr uint32_t BIT_TO_BYTES = 8;
#ifdef FEATURE_POWER_MANAGER
constexpr int32_t RUNNINGLOCK_LOCK_TIMEOUTMS_LASTING = -1;
#endif
}

typedef struct {
    HDI::Audio_Bluetooth::AudioFormat format;
    uint32_t sampleRate;
    uint32_t channel;
    float volume;
} BluetoothSinkAttr;

class BluetoothRendererSinkInner : public BluetoothRendererSink {
public:
    int32_t Init(const IAudioSinkAttr &attr) override;
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
    int32_t GetLatency(uint32_t *latency) override;
    int32_t GetTransactionId(uint64_t *transactionId) override;
    void SetAudioMonoState(bool audioMono) override;
    void SetAudioBalanceValue(float audioBalance) override;
    int32_t GetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec) override;

    int32_t SetVoiceVolume(float volume) override;
    int32_t SetAudioScene(AudioScene audioScene, DeviceType activeDevice) override;
    int32_t SetOutputRoute(DeviceType deviceType) override;
    void SetAudioParameter(const AudioParamKey key, const std::string &condition, const std::string &value) override;
    std::string GetAudioParameter(const AudioParamKey key, const std::string &condition) override;
    void RegisterParameterCallback(IAudioSinkCallback* callback) override;

    bool GetAudioMonoState();
    float GetAudioBalanceValue();

    BluetoothRendererSinkInner();
    ~BluetoothRendererSinkInner();
private:
    BluetoothSinkAttr attr_;
    bool rendererInited_;
    bool started_;
    bool paused_;
    float leftVolume_;
    float rightVolume_;
    struct HDI::Audio_Bluetooth::AudioProxyManager *audioManager_;
    struct HDI::Audio_Bluetooth::AudioAdapter *audioAdapter_;
    struct HDI::Audio_Bluetooth::AudioRender *audioRender_;
    struct HDI::Audio_Bluetooth::AudioPort audioPort = {};
    void *handle_;
    bool audioMonoState_ = false;
    bool audioBalanceState_ = false;
    float leftBalanceCoef_ = 1.0f;
    float rightBalanceCoef_ = 1.0f;
#ifdef FEATURE_POWER_MANAGER
    std::shared_ptr<PowerMgr::RunningLock> keepRunningLock_;
#endif

    int32_t CreateRender(struct HDI::Audio_Bluetooth::AudioPort &renderPort);
    int32_t InitAudioManager();
    void AdjustStereoToMono(char *data, uint64_t len);
    void AdjustAudioBalance(char *data, uint64_t len);
    AudioFormat ConvertToHdiFormat(HdiAdapterFormat format);
    int64_t BytesToNanoTime(size_t lens);
    FILE *dumpFile_ = nullptr;
};

BluetoothRendererSinkInner::BluetoothRendererSinkInner()
    : rendererInited_(false), started_(false), paused_(false), leftVolume_(DEFAULT_VOLUME_LEVEL),
      rightVolume_(DEFAULT_VOLUME_LEVEL), audioManager_(nullptr), audioAdapter_(nullptr),
      audioRender_(nullptr), handle_(nullptr)
{
    attr_ = {};
}

BluetoothRendererSinkInner::~BluetoothRendererSinkInner()
{
    BluetoothRendererSinkInner::DeInit();
}

BluetoothRendererSink *BluetoothRendererSink::GetInstance()
{
    static BluetoothRendererSinkInner audioRenderer;

    return &audioRenderer;
}

bool BluetoothRendererSinkInner::IsInited(void)
{
    return rendererInited_;
}

int32_t BluetoothRendererSinkInner::SetVoiceVolume(float volume)
{
    return ERR_NOT_SUPPORTED;
}

int32_t BluetoothRendererSinkInner::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    return ERR_NOT_SUPPORTED;
}

int32_t BluetoothRendererSinkInner::SetOutputRoute(DeviceType deviceType)
{
    return ERR_NOT_SUPPORTED;
}

void BluetoothRendererSinkInner::SetAudioParameter(const AudioParamKey key, const std::string &condition,
    const std::string &value)
{
    AUDIO_INFO_LOG("key %{public}d, condition: %{public}s, value: %{public}s", key,
        condition.c_str(), value.c_str());
    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("SetAudioParameter for render failed, audioRender_ is null");
        return;
    } else {
        int32_t ret = audioRender_->attr.SetExtraParams(reinterpret_cast<AudioHandle>(audioRender_), value.c_str());
        if (ret != SUCCESS) {
            AUDIO_WARNING_LOG("SetAudioParameter for render failed, error code: %d", ret);
        }
    }
}

std::string BluetoothRendererSinkInner::GetAudioParameter(const AudioParamKey key, const std::string &condition)
{
    AUDIO_ERR_LOG("BluetoothRendererSink GetAudioParameter not supported.");
    return "";
}

void BluetoothRendererSinkInner::RegisterParameterCallback(IAudioSinkCallback* callback)
{
    AUDIO_ERR_LOG("BluetoothRendererSink RegisterParameterCallback not supported.");
}

void BluetoothRendererSinkInner::DeInit()
{
    AUDIO_INFO_LOG("DeInit.");
    started_ = false;
    rendererInited_ = false;
    if ((audioRender_ != nullptr) && (audioAdapter_ != nullptr)) {
        audioAdapter_->DestroyRender(audioAdapter_, audioRender_);
    }
    audioRender_ = nullptr;

    if ((audioManager_ != nullptr) && (audioAdapter_ != nullptr)) {
        audioManager_->UnloadAdapter(audioManager_, audioAdapter_);
    }
    audioAdapter_ = nullptr;
    audioManager_ = nullptr;

    if (handle_ != nullptr) {
        dlclose(handle_);
        handle_ = nullptr;
    }

    DumpFileUtil::CloseDumpFile(&dumpFile_);
}

void InitAttrs(struct AudioSampleAttributes &attrs)
{
    /* Initialization of audio parameters for playback */
    attrs.format = AUDIO_FORMAT_TYPE_PCM_16_BIT;
    attrs.channelCount = AUDIO_CHANNELCOUNT;
    attrs.frameSize = PCM_16_BIT * attrs.channelCount / PCM_8_BIT;
    attrs.sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs.interleaved = 0;
    attrs.type = AUDIO_IN_MEDIA;
    attrs.period = DEEP_BUFFER_RENDER_PERIOD_SIZE;
    attrs.isBigEndian = false;
    attrs.isSignedData = true;
    attrs.startThreshold = DEEP_BUFFER_RENDER_PERIOD_SIZE / (attrs.frameSize);
    attrs.stopThreshold = INT_32_MAX;
    attrs.silenceThreshold = 0;
}

static int32_t SwitchAdapter(struct AudioAdapterDescriptor *descs, string adapterNameCase,
    enum AudioPortDirection portFlag, struct AudioPort &renderPort, int32_t size)
{
    AUDIO_INFO_LOG("SwitchAdapter: adapterNameCase: %{public}s", adapterNameCase.c_str());
    CHECK_AND_RETURN_RET(descs != nullptr, ERROR);

    for (int32_t index = 0; index < size; index++) {
        struct AudioAdapterDescriptor *desc = &descs[index];
        if (desc == nullptr) {
            continue;
        }
        AUDIO_DEBUG_LOG("SwitchAdapter: adapter name for %{public}d: %{public}s", index, desc->adapterName);
        if (!strcmp(desc->adapterName, adapterNameCase.c_str())) {
            for (uint32_t port = 0; port < desc->portNum; port++) {
                // Only find out the port of out in the sound card
                if (desc->ports[port].dir == portFlag) {
                    renderPort = desc->ports[port];
                    AUDIO_DEBUG_LOG("SwitchAdapter: index found %{public}d", index);
                    return index;
                }
            }
        }
    }
    AUDIO_ERR_LOG("SwitchAdapter Fail");

    return ERR_INVALID_INDEX;
}

int32_t BluetoothRendererSinkInner::InitAudioManager()
{
    AUDIO_INFO_LOG("Initialize audio proxy manager");

#if (defined(__aarch64__) || defined(__x86_64__))
    char resolvedPath[100] = "/vendor/lib64/chipsetsdk/libaudio_bluetooth_hdi_proxy_server.z.so";
#else
    char resolvedPath[100] = "/vendor/lib/chipsetsdk/libaudio_bluetooth_hdi_proxy_server.z.so";
#endif
    struct AudioProxyManager *(*getAudioManager)() = nullptr;

    handle_ = dlopen(resolvedPath, 1);
    CHECK_AND_RETURN_RET_LOG(handle_ != nullptr, ERR_INVALID_HANDLE, "Open so Fail");
    AUDIO_DEBUG_LOG("dlopen successful");

    getAudioManager = (struct AudioProxyManager *(*)())(dlsym(handle_, "GetAudioProxyManagerFuncs"));
    CHECK_AND_RETURN_RET(getAudioManager != nullptr, ERR_INVALID_HANDLE);
    AUDIO_DEBUG_LOG("getaudiomanager done");

    audioManager_ = getAudioManager();
    CHECK_AND_RETURN_RET(audioManager_ != nullptr, ERR_INVALID_HANDLE);
    AUDIO_DEBUG_LOG("audio manager created");

    return 0;
}

uint32_t PcmFormatToBits(AudioFormat format)
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
            return PCM_24_BIT;
    };
}

int32_t BluetoothRendererSinkInner::CreateRender(struct AudioPort &renderPort)
{
    AUDIO_DEBUG_LOG("Create render in");
    int32_t ret;
    struct AudioSampleAttributes param;
    InitAttrs(param);
    param.sampleRate = attr_.sampleRate;
    param.channelCount = attr_.channel;
    param.format = attr_.format;
    param.frameSize = PcmFormatToBits(param.format) * param.channelCount / PCM_8_BIT;
    param.startThreshold = DEEP_BUFFER_RENDER_PERIOD_SIZE / (param.frameSize);
    AUDIO_DEBUG_LOG("BluetoothRendererSink Create render format: %{public}d", param.format);
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
    AUDIO_DEBUG_LOG("create render done");

    return 0;
}

AudioFormat BluetoothRendererSinkInner::ConvertToHdiFormat(HdiAdapterFormat format)
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

int32_t BluetoothRendererSinkInner::Init(const IAudioSinkAttr &attr)
{
    AUDIO_INFO_LOG("Init: %{public}d", attr.format);

    attr_.format = ConvertToHdiFormat(attr.format);
    attr_.sampleRate = attr.sampleRate;
    attr_.channel = attr.channel;
    attr_.volume = attr.volume;

    string adapterNameCase = "bt_a2dp";  // Set sound card information
    enum AudioPortDirection port = PORT_OUT; // Set port information

    CHECK_AND_RETURN_RET_LOG(InitAudioManager() == 0, ERR_NOT_STARTED,
        "Init audio manager Fail");

    int32_t size = 0;
    struct AudioAdapterDescriptor *descs = nullptr;
    int32_t ret = audioManager_->GetAllAdapters(audioManager_, &descs, &size);
    CHECK_AND_RETURN_RET_LOG(size <= MAX_AUDIO_ADAPTER_NUM && size != 0 && descs != nullptr && ret == 0,
        ERR_NOT_STARTED, "Get adapters Fail");

    // Get qualified sound card and port
    int32_t index = SwitchAdapter(descs, adapterNameCase, port, audioPort, size);
    CHECK_AND_RETURN_RET_LOG(index >= 0, ERR_NOT_STARTED, "Switch Adapter Fail");

    struct AudioAdapterDescriptor *desc = &descs[index];
    int32_t loadAdapter = audioManager_->LoadAdapter(audioManager_, desc, &audioAdapter_);
    CHECK_AND_RETURN_RET_LOG(loadAdapter == 0, ERR_NOT_STARTED, "Load Adapter Fail");
    CHECK_AND_RETURN_RET_LOG(audioAdapter_ != nullptr, ERR_NOT_STARTED, "Load audio device failed");

    // Initialization port information, can fill through mode and other parameters
    ret = audioAdapter_->InitAllPorts(audioAdapter_);
    CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_NOT_STARTED, "InitAllPorts failed");

    int32_t result = CreateRender(audioPort);
    CHECK_AND_RETURN_RET_LOG(result == 0, ERR_NOT_STARTED, "Create render failed");

    rendererInited_ = true;

    return SUCCESS;
}

int32_t BluetoothRendererSinkInner::RenderFrame(char &data, uint64_t len, uint64_t &writeLen)
{
    int32_t ret = SUCCESS;
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE, "Bluetooth Render Handle is nullptr!");

    if (audioMonoState_) {
        AdjustStereoToMono(&data, len);
    }

    if (audioBalanceState_) {
        AdjustAudioBalance(&data, len);
    }

    DumpFileUtil::WriteDumpFile(dumpFile_, static_cast<void *>(&data), len);

    Trace trace("BluetoothRendererSinkInner::RenderFrame");
    while (true) {
        if (*reinterpret_cast<int8_t*>(&data) == 0) {
            Trace::Count("BluetoothRendererSinkInner::RenderFrame", PCM_MAYBE_SILENT);
        } else {
            Trace::Count("BluetoothRendererSinkInner::RenderFrame", PCM_MAYBE_NOT_SILENT);
        }
        ret = audioRender_->RenderFrame(audioRender_, (void*)&data, len, &writeLen);
        AUDIO_DEBUG_LOG("A2dp RenderFrame returns: %{public}x", ret);
        if (ret == RENDER_FRAME_NUM) {
            AUDIO_ERR_LOG("retry render frame...");
            usleep(RENDER_FRAME_INTERVAL_IN_MICROSECONDS);
            continue;
        }

        if (ret != 0) {
            AUDIO_ERR_LOG("A2dp RenderFrame failed ret: %{public}x", ret);
            ret = ERR_WRITE_FAILED;
        }

        break;
    }
    return ret;
}

int32_t BluetoothRendererSinkInner::Start(void)
{
    Trace trace("BluetoothRendererSinkInner::Start");
    AUDIO_INFO_LOG("Start.");
#ifdef FEATURE_POWER_MANAGER
    if (keepRunningLock_ == nullptr) {
        keepRunningLock_ = PowerMgr::PowerMgrClient::GetInstance().CreateRunningLock("AudioBluetoothBackgroundPlay",
            PowerMgr::RunningLockType::RUNNINGLOCK_BACKGROUND_AUDIO);
    }

    if (keepRunningLock_ != nullptr) {
        AUDIO_INFO_LOG("keepRunningLock lock");
        keepRunningLock_->Lock(RUNNINGLOCK_LOCK_TIMEOUTMS_LASTING); // -1 for lasting.
    } else {
        AUDIO_ERR_LOG("keepRunningLock is null, playback can not work well!");
    }
#endif
    DumpFileUtil::OpenDumpFile(DUMP_SERVER_PARA, DUMP_BLUETOOTH_RENDER_SINK_FILENAME, &dumpFile_);

    int32_t ret;

    if (!started_) {
        ret = audioRender_->control.Start(reinterpret_cast<AudioHandle>(audioRender_));
        if (!ret) {
            started_ = true;
            return SUCCESS;
        } else {
            AUDIO_ERR_LOG("Start failed!");
            return ERR_NOT_STARTED;
        }
    }

    return SUCCESS;
}

int32_t BluetoothRendererSinkInner::SetVolume(float left, float right)
{
    int32_t ret;
    float volume;

    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "SetVolume failed audioRender_ null");

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
        AUDIO_WARNING_LOG("Set volume failed!");
    }

    return ret;
}

int32_t BluetoothRendererSinkInner::GetVolume(float &left, float &right)
{
    left = leftVolume_;
    right = rightVolume_;
    return SUCCESS;
}

int32_t BluetoothRendererSinkInner::GetLatency(uint32_t *latency)
{
    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "GetLatency failed audio render null");

    CHECK_AND_RETURN_RET_LOG(latency, ERR_INVALID_PARAM, "GetLatency failed latency null");

    uint32_t hdiLatency;
    if (audioRender_->GetLatency(audioRender_, &hdiLatency) == 0) {
        *latency = hdiLatency;
        return SUCCESS;
    } else {
        return ERR_OPERATION_FAILED;
    }
}

int32_t BluetoothRendererSinkInner::GetTransactionId(uint64_t *transactionId)
{
    AUDIO_INFO_LOG("GetTransactionId in");

    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "GetTransactionId failed audio render null");

    CHECK_AND_RETURN_RET_LOG(transactionId, ERR_INVALID_PARAM,
        "GetTransactionId failed transactionId null");

    *transactionId = reinterpret_cast<uint64_t>(audioRender_);
    return SUCCESS;
}

int32_t BluetoothRendererSinkInner::Stop(void)
{
    Trace trace("BluetoothRendererSinkInner::Stop");
    AUDIO_INFO_LOG("Stop in");
#ifdef FEATURE_POWER_MANAGER
    if (keepRunningLock_ != nullptr) {
        AUDIO_INFO_LOG("keepRunningLock unLock");
        keepRunningLock_->UnLock();
    } else {
        AUDIO_ERR_LOG("keepRunningLock is null, playback can not work well!");
    }
#endif
    int32_t ret;

    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "Stop failed audioRender_ null");

    if (started_) {
        AUDIO_DEBUG_LOG("Stop control before");
        ret = audioRender_->control.Stop(reinterpret_cast<AudioHandle>(audioRender_));
        AUDIO_DEBUG_LOG("Stop control after");
        if (!ret) {
            started_ = false;
            paused_ = false;
            return SUCCESS;
        } else {
            AUDIO_ERR_LOG("Stop failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return SUCCESS;
}

int32_t BluetoothRendererSinkInner::Pause(void)
{
    int32_t ret;

    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "Pause failed audioRender_ null");

    CHECK_AND_RETURN_RET_LOG(started_, ERR_OPERATION_FAILED,
        "Pause invalid state!");

    if (!paused_) {
        ret = audioRender_->control.Pause(reinterpret_cast<AudioHandle>(audioRender_));
        if (!ret) {
            paused_ = true;
            return SUCCESS;
        } else {
            AUDIO_ERR_LOG("Pause failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return SUCCESS;
}

int32_t BluetoothRendererSinkInner::Resume(void)
{
    int32_t ret;

    CHECK_AND_RETURN_RET_LOG(audioRender_ != nullptr, ERR_INVALID_HANDLE,
        "Resume failed audioRender_ null");

    CHECK_AND_RETURN_RET_LOG(started_, ERR_OPERATION_FAILED,
        "Resume invalid state!");

    if (paused_) {
        ret = audioRender_->control.Resume(reinterpret_cast<AudioHandle>(audioRender_));
        if (!ret) {
            paused_ = false;
            return SUCCESS;
        } else {
            AUDIO_ERR_LOG("Resume failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return SUCCESS;
}

int32_t BluetoothRendererSinkInner::Reset(void)
{
    int32_t ret;

    if (started_ && audioRender_ != nullptr) {
        ret = audioRender_->control.Flush(reinterpret_cast<AudioHandle>(audioRender_));
        if (!ret) {
            return SUCCESS;
        } else {
            AUDIO_ERR_LOG("Reset failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return ERR_OPERATION_FAILED;
}

int32_t BluetoothRendererSinkInner::Flush(void)
{
    int32_t ret;

    if (started_ && audioRender_ != nullptr) {
        ret = audioRender_->control.Flush(reinterpret_cast<AudioHandle>(audioRender_));
        if (!ret) {
            return SUCCESS;
        } else {
            AUDIO_ERR_LOG("Flush failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return ERR_OPERATION_FAILED;
}

void BluetoothRendererSinkInner::SetAudioMonoState(bool audioMono)
{
    audioMonoState_ = audioMono;
}

void BluetoothRendererSinkInner::SetAudioBalanceValue(float audioBalance)
{
    // reset the balance coefficient value firstly
    leftBalanceCoef_ = 1.0f;
    rightBalanceCoef_ = 1.0f;

    if (std::abs(audioBalance) <= std::numeric_limits<float>::epsilon()) {
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

int32_t BluetoothRendererSinkInner::GetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec)
{
    AUDIO_ERR_LOG("BluetoothRendererSink GetPresentationPosition not supported.");
    return ERR_NOT_SUPPORTED;
}

void BluetoothRendererSinkInner::AdjustStereoToMono(char *data, uint64_t len)
{
    // only stereo is surpported now (stereo channel count is 2)
    CHECK_AND_RETURN_LOG(attr_.channel == STEREO_CHANNEL_COUNT,
        "AdjustStereoToMono: Unsupported channel number: %{public}d", attr_.channel);

    switch (attr_.format) {
        case AUDIO_FORMAT_TYPE_PCM_8_BIT: {
            // this function needs to be further tested for usability
            AdjustStereoToMonoForPCM8Bit(reinterpret_cast<int8_t *>(data), len);
            break;
        }
        case AUDIO_FORMAT_TYPE_PCM_16_BIT: {
            AdjustStereoToMonoForPCM16Bit(reinterpret_cast<int16_t *>(data), len);
            break;
        }
        case AUDIO_FORMAT_TYPE_PCM_24_BIT: {
            // this function needs to be further tested for usability
            AdjustStereoToMonoForPCM24Bit(reinterpret_cast<int8_t *>(data), len);
            break;
        }
        case AUDIO_FORMAT_TYPE_PCM_32_BIT: {
            AdjustStereoToMonoForPCM32Bit(reinterpret_cast<int32_t *>(data), len);
            break;
        }
        default: {
            // if the audio format is unsupported, the audio data will not be changed
            AUDIO_ERR_LOG("AdjustStereoToMono: Unsupported audio format: %{public}d",
                attr_.format);
            break;
        }
    }
}

void BluetoothRendererSinkInner::AdjustAudioBalance(char *data, uint64_t len)
{
    CHECK_AND_RETURN_LOG(attr_.channel == STEREO_CHANNEL_COUNT,
        "Unsupported channel number: %{public}d", attr_.channel);

    switch (attr_.format) {
        case AUDIO_FORMAT_TYPE_PCM_8_BIT: {
            // this function needs to be further tested for usability
            AdjustAudioBalanceForPCM8Bit(reinterpret_cast<int8_t *>(data), len, leftBalanceCoef_, rightBalanceCoef_);
            break;
        }
        case AUDIO_FORMAT_TYPE_PCM_16_BIT: {
            AdjustAudioBalanceForPCM16Bit(reinterpret_cast<int16_t *>(data), len, leftBalanceCoef_, rightBalanceCoef_);
            break;
        }
        case AUDIO_FORMAT_TYPE_PCM_24_BIT: {
            // this function needs to be further tested for usability
            AdjustAudioBalanceForPCM24Bit(reinterpret_cast<int8_t *>(data), len, leftBalanceCoef_, rightBalanceCoef_);
            break;
        }
        case AUDIO_FORMAT_TYPE_PCM_32_BIT: {
            AdjustAudioBalanceForPCM32Bit(reinterpret_cast<int32_t *>(data), len, leftBalanceCoef_, rightBalanceCoef_);
            break;
        }
        default: {
            // if the audio format is unsupported, the audio data will not be changed
            AUDIO_ERR_LOG("Unsupported audio format: %{public}d",
                attr_.format);
            break;
        }
    }
}

static uint32_t HdiFormatToByte(HDI::Audio_Bluetooth::AudioFormat format)
{
    return PcmFormatToBits(format) / BIT_TO_BYTES;
}

int64_t BluetoothRendererSinkInner::BytesToNanoTime(size_t lens)
{
    int64_t res = AUDIO_NS_PER_SECOND * lens / (attr_.sampleRate * attr_.channel * HdiFormatToByte(attr_.format));
    return res;
}
} // namespace AudioStandard
} // namespace OHOS
