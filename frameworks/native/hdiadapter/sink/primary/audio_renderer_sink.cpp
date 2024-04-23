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

#include "audio_renderer_sink.h"

#include <atomic>
#include <cstring>
#include <cinttypes>
#include <condition_variable>
#include <dlfcn.h>
#include <string>
#include <unistd.h>
#include <mutex>

#include "securec.h"

#include "power_mgr_client.h"
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
const uint32_t PRIMARY_OUTPUT_STREAM_ID = 13; // 13 + 0 * 8
const uint32_t PARAM_VALUE_LENTH = 10;
const uint32_t STEREO_CHANNEL_COUNT = 2;
constexpr int32_t RUNNINGLOCK_LOCK_TIMEOUTMS_LASTING = -1;
const int32_t SLEEP_TIME_FOR_RENDER_EMPTY = 120;
}
class AudioRendererSinkInner : public AudioRendererSink {
public:
    int32_t Init(IAudioSinkAttr attr) override;
    bool IsInited(void) override;
    void DeInit(void) override;

    int32_t Flush(void) override;
    int32_t Pause(void) override;
    int32_t Reset(void) override;
    int32_t Resume(void) override;
    int32_t Start(void) override;
    int32_t Stop(void) override;

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

    void SetAudioMonoState(bool audioMono) override;
    void SetAudioBalanceValue(float audioBalance) override;
    int32_t SetOutputRoute(DeviceType outputDevice) override;

    int32_t SetOutputRoute(DeviceType outputDevice, AudioPortPin &outputPortPin);
    AudioRendererSinkInner();
    ~AudioRendererSinkInner();
private:
    IAudioSinkAttr attr_;
    bool rendererInited_;
    bool started_;
    bool paused_;
    float leftVolume_;
    float rightVolume_;
    int32_t routeHandle_ = -1;
    uint32_t openSpeaker_;
    uint32_t renderId_ = 0;
    std::string adapterNameCase_;
    struct IAudioManager *audioManager_;
    struct IAudioAdapter *audioAdapter_;
    struct IAudioRender *audioRender_;
    struct AudioAdapterDescriptor adapterDesc_;
    struct AudioPort audioPort_ = {};
    bool audioMonoState_ = false;
    bool audioBalanceState_ = false;
    float leftBalanceCoef_ = 1.0f;
    float rightBalanceCoef_ = 1.0f;

    std::shared_ptr<PowerMgr::RunningLock> keepRunningLock_;

    // for device switch
    std::atomic<bool> inSwitch_ = false;
    std::atomic<int32_t> renderEmptyFrameCount_ = 0;
    std::mutex switchMutex_;
    std::condition_variable switchCV_;

private:
    int32_t CreateRender(const struct AudioPort &renderPort);
    int32_t InitAudioManager();
    AudioFormat ConverToHdiFormat(AudioSampleFormat format);
    void AdjustStereoToMono(char *data, uint64_t len);
    void AdjustAudioBalance(char *data, uint64_t len);
    void ReleaseRunningLock();
#ifdef DUMPFILE
    FILE *pfd;
    const char *g_audioOutTestFilePath = "/data/data/.pulse_dir/dump_audiosink.pcm";
#endif // DUMPFILE
};

AudioRendererSinkInner::AudioRendererSinkInner()
    : rendererInited_(false), started_(false), paused_(false), leftVolume_(DEFAULT_VOLUME_LEVEL),
      rightVolume_(DEFAULT_VOLUME_LEVEL), openSpeaker_(0), audioManager_(nullptr), audioAdapter_(nullptr),
      audioRender_(nullptr)
{
    attr_ = {};
#ifdef DUMPFILE
    pfd = nullptr;
#endif // DUMPFILE
}

AudioRendererSinkInner::~AudioRendererSinkInner()
{
    AUDIO_ERR_LOG("~AudioRendererSinkInner");
}

AudioRendererSink *AudioRendererSink::GetInstance()
{
    static AudioRendererSinkInner audioRenderer;

    return &audioRenderer;
}

void AudioRendererSinkInner::SetAudioParameter(const AudioParamKey key, const std::string& condition,
    const std::string& value)
{
    AUDIO_INFO_LOG("SetAudioParameter: key %{public}d, condition: %{public}s, value: %{public}s", key,
        condition.c_str(), value.c_str());
    AudioExtParamKey hdiKey = AudioExtParamKey(key);
    if (audioAdapter_ == nullptr) {
        AUDIO_ERR_LOG("SetAudioParameter failed, audioAdapter_ is null");
        return;
    }
    int32_t ret = audioAdapter_->SetExtraParams(audioAdapter_, hdiKey, condition.c_str(), value.c_str());
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("SetAudioParameter failed, error code: %d", ret);
    }
}

std::string AudioRendererSinkInner::GetAudioParameter(const AudioParamKey key, const std::string& condition)
{
    AUDIO_INFO_LOG("GetAudioParameter: key %{public}d, condition: %{public}s", key,
        condition.c_str());
    AudioExtParamKey hdiKey = AudioExtParamKey(key);
    char value[PARAM_VALUE_LENTH];
    if (audioAdapter_ == nullptr) {
        AUDIO_ERR_LOG("GetAudioParameter failed, audioAdapter_ is null");
        return "";
    }
    int32_t ret = audioAdapter_->GetExtraParams(audioAdapter_, hdiKey, condition.c_str(), value, PARAM_VALUE_LENTH);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("GetAudioParameter failed, error code: %d", ret);
        return "";
    }
    return value;
}

void AudioRendererSinkInner::SetAudioMonoState(bool audioMono)
{
    audioMonoState_ = audioMono;
}

void AudioRendererSinkInner::SetAudioBalanceValue(float audioBalance)
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

void AudioRendererSinkInner::AdjustStereoToMono(char *data, uint64_t len)
{
    if (attr_.channel != STEREO_CHANNEL_COUNT) {
        // only stereo is surpported now (stereo channel count is 2)
        AUDIO_ERR_LOG("AdjustStereoToMono: Unsupported channel number: %{public}d", attr_.channel);
        return;
    }

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
            AUDIO_ERR_LOG("AdjustStereoToMono: Unsupported audio format: %{public}d", attr_.format);
            break;
        }
    }
}

void AudioRendererSinkInner::AdjustAudioBalance(char *data, uint64_t len)
{
    if (attr_.channel != STEREO_CHANNEL_COUNT) {
        // only stereo is surpported now (stereo channel count is 2)
        AUDIO_ERR_LOG("AdjustAudioBalance: Unsupported channel number: %{public}d", attr_.channel);
        return;
    }

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
            AUDIO_ERR_LOG("AdjustAudioBalance: Unsupported audio format: %{public}d", attr_.format);
            break;
        }
    }
}

bool AudioRendererSinkInner::IsInited()
{
    return rendererInited_;
}

void AudioRendererSinkInner::RegisterParameterCallback(IAudioSinkCallback* callback)
{
    AUDIO_ERR_LOG("RegisterParameterCallback not supported.");
}

void AudioRendererSinkInner::DeInit()
{
    AUDIO_INFO_LOG("DeInit.");
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
    attrs.streamId = PRIMARY_OUTPUT_STREAM_ID;
    attrs.type = AUDIO_IN_MEDIA;
    attrs.period = DEEP_BUFFER_RENDER_PERIOD_SIZE;
    attrs.isBigEndian = false;
    attrs.isSignedData = true;
    attrs.stopThreshold = INT_32_MAX;
    attrs.silenceThreshold = 0;
}

static int32_t SwitchAdapterRender(struct AudioAdapterDescriptor *descs, string adapterNameCase,
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

int32_t AudioRendererSinkInner::InitAudioManager()
{
    AUDIO_INFO_LOG("Initialize audio proxy manager");

    audioManager_ = IAudioManagerGet(false);
    if (audioManager_ == nullptr) {
        return ERR_INVALID_HANDLE;
    }

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
            AUDIO_INFO_LOG("PcmFormatToBits: Unkown format type,set it to default");
            return PCM_24_BIT;
    }
}

AudioFormat AudioRendererSinkInner::ConverToHdiFormat(AudioSampleFormat format)
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

int32_t AudioRendererSinkInner::CreateRender(const struct AudioPort &renderPort)
{
    int32_t ret;
    struct AudioSampleAttributes param;
    struct AudioDeviceDescriptor deviceDesc;
    InitAttrs(param);
    param.sampleRate = attr_.sampleRate;
    param.channelCount = attr_.channel;
    param.format = ConverToHdiFormat(attr_.format);
    param.frameSize = PcmFormatToBits(param.format) * param.channelCount / PCM_8_BIT;
    param.startThreshold = DEEP_BUFFER_RENDER_PERIOD_SIZE / (param.frameSize);
    AUDIO_DEBUG_LOG("Create render format: %{public}d", param.format);
    deviceDesc.portId = renderPort.portId;
    deviceDesc.desc = const_cast<char *>("");
    deviceDesc.pins = PIN_OUT_SPEAKER;
    ret = audioAdapter_->CreateRender(audioAdapter_, &deviceDesc, &param, &audioRender_, &renderId_);
    if (ret != 0 || audioRender_ == nullptr) {
        AUDIO_ERR_LOG("AudioDeviceCreateRender failed.");
        audioManager_->UnloadAdapter(audioManager_, adapterDesc_.adapterName);
        return ERR_NOT_STARTED;
    }

    return 0;
}

int32_t AudioRendererSinkInner::Init(IAudioSinkAttr attr)
{
    attr_ = attr;
    adapterNameCase_ = attr_.adapterName;  // Set sound card information
    openSpeaker_ = attr_.openMicSpeaker;
    enum AudioPortDirection port = PORT_OUT; // Set port information

    if (InitAudioManager() != 0) {
        AUDIO_ERR_LOG("Init audio manager Fail.");
        return ERR_NOT_STARTED;
    }

    uint32_t size = MAX_AUDIO_ADAPTER_NUM;
    int32_t ret;
    AudioAdapterDescriptor descs[MAX_AUDIO_ADAPTER_NUM];
    ret = audioManager_->GetAllAdapters(audioManager_, (struct AudioAdapterDescriptor *)&descs, &size);
    if (size > MAX_AUDIO_ADAPTER_NUM || size == 0 || ret != 0) {
        AUDIO_ERR_LOG("Get adapters Fail.");
        return ERR_NOT_STARTED;
    }

    // Get qualified sound card and port
    int32_t index =
        SwitchAdapterRender((struct AudioAdapterDescriptor *)&descs, adapterNameCase_, port, audioPort_, size);
    CHECK_AND_RETURN_RET_LOG((index >= 0), ERR_NOT_STARTED, "Switch Adapter Fail.");

    adapterDesc_ = descs[index];
    CHECK_AND_RETURN_RET_LOG((audioManager_->LoadAdapter(audioManager_, &adapterDesc_, &audioAdapter_) == SUCCESS),
        ERR_NOT_STARTED, "Load Adapter Fail.");

    CHECK_AND_RETURN_RET_LOG((audioAdapter_ != nullptr), ERR_NOT_STARTED, "Load audio device failed.");

    // Initialization port information, can fill through mode and other parameters
    CHECK_AND_RETURN_RET_LOG((audioAdapter_->InitAllPorts(audioAdapter_) == SUCCESS),
        ERR_NOT_STARTED, "InitAllPorts failed");

    if (CreateRender(audioPort_) != 0) {
        AUDIO_ERR_LOG("Create render failed, Audio Port: %{public}d", audioPort_.portId);
        return ERR_NOT_STARTED;
    }
    if (openSpeaker_) {
        ret = SetOutputRoute(DEVICE_TYPE_SPEAKER);
        if (ret < 0) {
            AUDIO_ERR_LOG("Update route FAILED: %{public}d", ret);
        }
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

int32_t AudioRendererSinkInner::RenderFrame(char &data, uint64_t len, uint64_t &writeLen)
{
    int64_t stamp = ClockTime::GetCurNano();
    int32_t ret;
    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("Audio Render Handle is nullptr!");
        return ERR_INVALID_HANDLE;
    }

    if (audioMonoState_) {
        AdjustStereoToMono(&data, len);
    }

    if (audioBalanceState_) {
        AdjustAudioBalance(&data, len);
    }

#ifdef DUMPFILE
    if (pfd) {
        size_t writeResult = fwrite((void*)&data, 1, len, pfd);
        if (writeResult != len) {
            AUDIO_ERR_LOG("Failed to write the file.");
        }
    }
#endif // DUMPFILE
    if (inSwitch_) {
        Trace traceInSwitch("AudioRendererSinkInner::RenderFrame::inSwitch");
        writeLen = len;
        return SUCCESS;
    }
    if (renderEmptyFrameCount_ > 0) {
        Trace traceEmpty("AudioRendererSinkInner::RenderFrame::renderEmpty");
        CHECK_AND_BREAK_LOG(memset_s(reinterpret_cast<void*>(&data), static_cast<size_t>(len), 0,
            static_cast<size_t>(len)) == EOK, "call memset_s failed");
        renderEmptyFrameCount_--;
        if (renderEmptyFrameCount_ == 0) {
            switchCV_.notify_all();
        }
    }
    Trace trace("AudioRendererSinkInner::RenderFrame");
    ret = audioRender_->RenderFrame(audioRender_, reinterpret_cast<int8_t*>(&data), static_cast<uint32_t>(len),
        &writeLen);
    if (ret != 0) {
        AUDIO_ERR_LOG("RenderFrame failed ret: %{public}x", ret);
        return ERR_WRITE_FAILED;
    }

    stamp = (ClockTime::GetCurNano() - stamp) / AUDIO_US_PER_SECOND;
    AUDIO_DEBUG_LOG("RenderFrame len[%{public}" PRIu64 "] cost[%{public}" PRId64 "]ms", len, stamp);
    return SUCCESS;
}

int32_t AudioRendererSinkInner::Start(void)
{
    AUDIO_INFO_LOG("Start.");
    Trace trace("Sink::Start");
    if (keepRunningLock_ == nullptr) {
        keepRunningLock_ = PowerMgr::PowerMgrClient::GetInstance().CreateRunningLock("AudioPrimaryBackgroundPlay",
            PowerMgr::RunningLockType::RUNNINGLOCK_BACKGROUND_AUDIO);
    }

    if (keepRunningLock_ != nullptr) {
        AUDIO_DEBUG_LOG("AudioRendererSink call KeepRunningLock lock");
        keepRunningLock_->Lock(RUNNINGLOCK_LOCK_TIMEOUTMS_LASTING); // -1 for lasting.
    } else {
        AUDIO_ERR_LOG("keepRunningLock_ is null, playback can not work well!");
    }

    int32_t ret;
    if (!started_) {
        ret = audioRender_->Start(audioRender_);
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

int32_t AudioRendererSinkInner::SetVolume(float left, float right)
{
    int32_t ret;
    float volume;

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("SetVolume failed audioRender_ null");
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
        AUDIO_ERR_LOG("Set volume failed!");
    }

    return ret;
}

int32_t AudioRendererSinkInner::GetVolume(float &left, float &right)
{
    left = leftVolume_;
    right = rightVolume_;
    return SUCCESS;
}

int32_t AudioRendererSinkInner::SetVoiceVolume(float volume)
{
    Trace trace("AudioRendererSinkInner::SetVoiceVolume");
    if (audioAdapter_ == nullptr) {
        AUDIO_ERR_LOG("SetVoiceVolume failed, audioAdapter_ is null");
        return ERR_INVALID_HANDLE;
    }
    AUDIO_DEBUG_LOG("SetVoiceVolume %{public}f", volume);
    return audioAdapter_->SetVoiceVolume(audioAdapter_, volume);
}

int32_t AudioRendererSinkInner::GetLatency(uint32_t *latency)
{
    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("GetLatency failed audio render null");
        return ERR_INVALID_HANDLE;
    }

    if (!latency) {
        AUDIO_ERR_LOG("GetLatency failed latency null");
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

static AudioCategory GetAudioCategory(AudioScene audioScene)
{
    AudioCategory audioCategory;
    switch (audioScene) {
        case AUDIO_SCENE_DEFAULT:
            audioCategory = AUDIO_IN_MEDIA;
            break;
        case AUDIO_SCENE_RINGING:
            audioCategory = AUDIO_IN_RINGTONE;
            break;
        case AUDIO_SCENE_PHONE_CALL:
            audioCategory = AUDIO_IN_CALL;
            break;
        case AUDIO_SCENE_PHONE_CHAT:
            audioCategory = AUDIO_IN_COMMUNICATION;
            break;
        default:
            audioCategory = AUDIO_IN_MEDIA;
            break;
    }
    AUDIO_DEBUG_LOG("Audio category returned is: %{public}d", audioCategory);

    return audioCategory;
}

static int32_t SetOutputPortPin(DeviceType outputDevice, AudioRouteNode &sink)
{
    int32_t ret = SUCCESS;

    switch (outputDevice) {
        case DEVICE_TYPE_EARPIECE:
            sink.ext.device.type = PIN_OUT_EARPIECE;
            sink.ext.device.desc = (char *)"pin_out_earpiece";
            break;
        case DEVICE_TYPE_SPEAKER:
            sink.ext.device.type = PIN_OUT_SPEAKER;
            sink.ext.device.desc = (char *)"pin_out_speaker";
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
            sink.ext.device.type = PIN_OUT_HEADSET;
            sink.ext.device.desc = (char *)"pin_out_headset";
            break;
        case DEVICE_TYPE_USB_HEADSET:
            sink.ext.device.type = PIN_OUT_USB_EXT;
            sink.ext.device.desc = (char *)"pin_out_usb_ext";
            break;
        case DEVICE_TYPE_BLUETOOTH_SCO:
            sink.ext.device.type = PIN_OUT_BLUETOOTH_SCO;
            sink.ext.device.desc = (char *)"pin_out_bluetooth_sco";
            break;
        default:
            ret = ERR_NOT_SUPPORTED;
            break;
    }

    return ret;
}

int32_t AudioRendererSinkInner::SetOutputRoute(DeviceType outputDevice)
{
    AudioPortPin outputPortPin = PIN_OUT_SPEAKER;
    return SetOutputRoute(outputDevice, outputPortPin);
}

int32_t AudioRendererSinkInner::SetOutputRoute(DeviceType outputDevice, AudioPortPin &outputPortPin)
{
    AudioRouteNode source = {};
    AudioRouteNode sink = {};

    int32_t ret = SetOutputPortPin(outputDevice, sink);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("SetOutputRoute FAILED: %{public}d", ret);
        return ret;
    }

    outputPortPin = sink.ext.device.type;
    AUDIO_INFO_LOG("Output PIN is: 0x%{public}X", outputPortPin);
    source.portId = 0;
    source.role = AUDIO_PORT_SOURCE_ROLE;
    source.type = AUDIO_PORT_MIX_TYPE;
    source.ext.mix.moduleId = 0;
    source.ext.mix.streamId = PRIMARY_OUTPUT_STREAM_ID;
    source.ext.device.desc = (char *)"";

    sink.portId = static_cast<int32_t>(audioPort_.portId);
    sink.role = AUDIO_PORT_SINK_ROLE;
    sink.type = AUDIO_PORT_DEVICE_TYPE;
    sink.ext.device.moduleId = 0;
    sink.ext.device.desc = (char *)"";

    AudioRoute route = {
        .sources = &source,
        .sourcesLen = 1,
        .sinks = &sink,
        .sinksLen = 1,
    };

    renderEmptyFrameCount_ = 5; // preRender 5 frames
    std::unique_lock<std::mutex> lock(switchMutex_);
    switchCV_.wait_for(lock, std::chrono::milliseconds(SLEEP_TIME_FOR_RENDER_EMPTY), [this] {
        if (renderEmptyFrameCount_ == 0) {
            AUDIO_INFO_LOG("Wait for preRender end.");
            return true;
        }
        AUDIO_DEBUG_LOG("Current renderEmptyFrameCount_ is %{public}d", renderEmptyFrameCount_.load());
        return false;
    });
    int64_t stamp = ClockTime::GetCurNano();
    CHECK_AND_RETURN_RET_LOG(audioAdapter_ != nullptr, ERR_INVALID_HANDLE, "SetOutputRoute failed with null adapter");
    inSwitch_.store(true);
    ret = audioAdapter_->UpdateAudioRoute(audioAdapter_, &route, &routeHandle_);
    inSwitch_.store(false);
    stamp = (ClockTime::GetCurNano() - stamp) / AUDIO_US_PER_SECOND;
    AUDIO_INFO_LOG("UpdateAudioRoute cost[%{public}" PRId64 "]ms", stamp);
    renderEmptyFrameCount_ = 5; // render 5 empty frame
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ERR_OPERATION_FAILED, "UpdateAudioRoute failed");

    return SUCCESS;
}

int32_t AudioRendererSinkInner::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    AUDIO_INFO_LOG("SetAudioScene scene: %{public}d, device: %{public}d",
        audioScene, activeDevice);
    CHECK_AND_RETURN_RET_LOG(audioScene >= AUDIO_SCENE_DEFAULT && audioScene <= AUDIO_SCENE_PHONE_CHAT,
        ERR_INVALID_PARAM, "invalid audioScene");
    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("SetAudioScene failed audio render handle is null!");
        return ERR_INVALID_HANDLE;
    }
    if (openSpeaker_) {
        AudioPortPin audioSceneOutPort = PIN_OUT_SPEAKER;
        int32_t ret = SetOutputRoute(activeDevice, audioSceneOutPort);
        if (ret < 0) {
            AUDIO_ERR_LOG("Update route FAILED: %{public}d", ret);
        }

        AUDIO_DEBUG_LOG("OUTPUT port is %{public}d", audioSceneOutPort);
        struct AudioSceneDescriptor scene;
        scene.scene.id = GetAudioCategory(audioScene);
        scene.desc.pins = audioSceneOutPort;
        scene.desc.desc = (char *)"";

        ret = audioRender_->SelectScene(audioRender_, &scene);
        if (ret < 0) {
            AUDIO_ERR_LOG("Select scene FAILED: %{public}d", ret);
            return ERR_OPERATION_FAILED;
        }
    }
    return SUCCESS;
}

int32_t AudioRendererSinkInner::GetTransactionId(uint64_t *transactionId)
{
    AUDIO_INFO_LOG("GetTransactionId in");

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("GetTransactionId failed audio render null");
        return ERR_INVALID_HANDLE;
    }

    if (!transactionId) {
        AUDIO_ERR_LOG("GetTransactionId failed transactionId null");
        return ERR_INVALID_PARAM;
    }

    *transactionId = reinterpret_cast<uint64_t>(audioRender_);
    return SUCCESS;
}

void AudioRendererSinkInner::ReleaseRunningLock()
{
    if (keepRunningLock_ != nullptr) {
        AUDIO_INFO_LOG("AudioRendererSink call KeepRunningLock UnLock");
        keepRunningLock_->UnLock();
    } else {
        AUDIO_ERR_LOG("keepRunningLock_ is null, playback can not work well!");
    }
}

int32_t AudioRendererSinkInner::Stop(void)
{
    AUDIO_INFO_LOG("Stop.");
    int32_t ret;

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("Stop failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    if (started_) {
        ret = audioRender_->Stop(audioRender_);
        if (!ret) {
            started_ = false;
            ReleaseRunningLock();
            return SUCCESS;
        } else {
            AUDIO_ERR_LOG("Stop failed!");
            ReleaseRunningLock();
            return ERR_OPERATION_FAILED;
        }
    }
    ReleaseRunningLock();
    return SUCCESS;
}

int32_t AudioRendererSinkInner::Pause(void)
{
    int32_t ret;

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("Pause failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    if (!started_) {
        AUDIO_ERR_LOG("Pause invalid state!");
        return ERR_OPERATION_FAILED;
    }

    if (!paused_) {
        ret = audioRender_->Pause(audioRender_);
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

int32_t AudioRendererSinkInner::Resume(void)
{
    int32_t ret;

    if (audioRender_ == nullptr) {
        AUDIO_ERR_LOG("Resume failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    if (!started_) {
        AUDIO_ERR_LOG("Resume invalid state!");
        return ERR_OPERATION_FAILED;
    }

    if (paused_) {
        ret = audioRender_->Resume(audioRender_);
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

int32_t AudioRendererSinkInner::Reset(void)
{
    int32_t ret;

    if (started_ && audioRender_ != nullptr) {
        ret = audioRender_->Flush(audioRender_);
        if (!ret) {
            return SUCCESS;
        } else {
            AUDIO_ERR_LOG("Reset failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return ERR_OPERATION_FAILED;
}

int32_t AudioRendererSinkInner::Flush(void)
{
    int32_t ret;

    if (started_ && audioRender_ != nullptr) {
        ret = audioRender_->Flush(audioRender_);
        if (!ret) {
            return SUCCESS;
        } else {
            AUDIO_ERR_LOG("Flush failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return ERR_OPERATION_FAILED;
}
} // namespace AudioStandard
} // namespace OHOS
