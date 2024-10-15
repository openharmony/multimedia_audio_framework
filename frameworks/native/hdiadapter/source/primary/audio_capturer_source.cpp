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
#ifndef LOG_TAG
#define LOG_TAG "AudioCapturerSourceInner"
#endif

#include "audio_capturer_source.h"

#include <cstring>
#include <dlfcn.h>
#include <string>
#include <cinttypes>
#include <thread>
#include <future>

#include "securec.h"
#ifdef FEATURE_POWER_MANAGER
#include "power_mgr_client.h"
#include "running_lock.h"
#include "audio_running_lock_manager.h"
#endif
#include "v4_0/iaudio_manager.h"

#include "audio_hdi_log.h"
#include "audio_errors.h"
#include "audio_utils.h"
#include "parameters.h"
#include "media_monitor_manager.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
namespace {
    const int64_t SECOND_TO_NANOSECOND = 1000000000;
    const unsigned int DEINIT_TIME_OUT_SECONDS = 5;
    const uint16_t GET_MAX_AMPLITUDE_FRAMES_THRESHOLD = 10;
}
class AudioCapturerSourceInner : public AudioCapturerSource {
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

    int32_t SetInputRoute(DeviceType inputDevice) override;
    uint64_t GetTransactionId() override;
    std::string GetAudioParameter(const AudioParamKey key, const std::string &condition) override;

    int32_t GetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec) override;

    void RegisterWakeupCloseCallback(IAudioSourceCallback *callback) override;
    void RegisterAudioCapturerSourceCallback(std::unique_ptr<ICapturerStateCallback> callback) override;
    void RegisterParameterCallback(IAudioSourceCallback *callback) override;

    int32_t Preload(const std::string &usbInfoStr) override;
    float GetMaxAmplitude() override;

    int32_t UpdateAppsUid(const int32_t appsUid[PA_MAX_OUTPUTS_PER_SOURCE],
        const size_t size) final;
    int32_t UpdateAppsUid(const std::vector<int32_t> &appsUid) final;

    int32_t UpdateSourceType(SourceType souceType) final;

    explicit AudioCapturerSourceInner(const std::string &halName = "primary");
    ~AudioCapturerSourceInner();

private:
    static constexpr int32_t HALF_FACTOR = 2;
    static constexpr uint32_t MAX_AUDIO_ADAPTER_NUM = 5;
    static constexpr float MAX_VOLUME_LEVEL = 15.0f;
    static constexpr uint32_t USB_DEFAULT_BUFFERSIZE = 3840;
    static constexpr uint32_t STEREO_CHANNEL_COUNT = 2;

    int32_t CreateCapture(struct AudioPort &capturePort);
    int32_t InitAudioManager();
    void InitAttrsCapture(struct AudioSampleAttributes &attrs);
    AudioFormat ConvertToHdiFormat(HdiAdapterFormat format);

    int32_t UpdateUsbAttrs(const std::string &usbInfoStr);
    int32_t InitManagerAndAdapter();
    int32_t InitAdapterAndCapture();

    void InitLatencyMeasurement();
    void DeinitLatencyMeasurement();
    void CheckLatencySignal(uint8_t *frame, size_t replyBytes);

    void CheckUpdateState(char *frame, uint64_t replyBytes);
    void DfxOperation(BufferDesc &buffer, AudioSampleFormat format, AudioChannel channel) const;
    int32_t SetInputRoute(DeviceType inputDevice, AudioPortPin &inputPortPin);
    int32_t DoSetInputRoute(DeviceType inputDevice, AudioPortPin &inputPortPin);
    int32_t DoStop();

    IAudioSourceAttr attr_ = {};
    bool sourceInited_ = false;
    bool captureInited_ = false;
    bool started_ = false;
    bool paused_ = false;
    float leftVolume_ = 0.0f;
    float rightVolume_ = 0.0f;

    int32_t routeHandle_ = -1;
    int32_t logMode_ = 0;
    uint32_t openMic_ = 0;
    uint32_t captureId_ = 0;
    std::string adapterNameCase_ = "";

    // for get amplitude
    float maxAmplitude_ = 0;
    int64_t lastGetMaxAmplitudeTime_ = 0;
    int64_t last10FrameStartTime_ = 0;
    bool startUpdate_ = false;
    int capFrameNum_ = 0;

    struct IAudioManager *audioManager_ = nullptr;
    std::atomic<bool> adapterLoaded_ = false;
    struct IAudioAdapter *audioAdapter_ = nullptr;
    struct IAudioCapture *audioCapture_ = nullptr;
    const std::string halName_ = "";
    struct AudioAdapterDescriptor adapterDesc_ = {};
    struct AudioPort audioPort_ = {};
#ifdef FEATURE_POWER_MANAGER
    std::shared_ptr<AudioRunningLockManager<PowerMgr::RunningLock>> runningLockManager_;
#endif
    IAudioSourceCallback* wakeupCloseCallback_ = nullptr;
    std::mutex wakeupClosecallbackMutex_;

    std::unique_ptr<ICapturerStateCallback> audioCapturerSourceCallback_ = nullptr;
    FILE *dumpFile_ = nullptr;
    std::string dumpFileName_ = "";
    bool muteState_ = false;
    DeviceType currentActiveDevice_ = DEVICE_TYPE_INVALID;
    AudioScene currentAudioScene_ = AUDIO_SCENE_INVALID;
    bool latencyMeasEnabled_ = false;
    bool signalDetected_ = false;
    std::shared_ptr<SignalDetectAgent> signalDetectAgent_ = nullptr;
    std::mutex signalDetectAgentMutex_;

    std::mutex statusMutex_;

    std::mutex sourceAttrMutex_;
};

class AudioCapturerSourceWakeup : public AudioCapturerSource {
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

    int32_t SetInputRoute(DeviceType inputDevice) override;
    uint64_t GetTransactionId() override;
    int32_t GetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec) override;
    std::string GetAudioParameter(const AudioParamKey key, const std::string &condition) override;

    void RegisterWakeupCloseCallback(IAudioSourceCallback *callback) override;
    void RegisterAudioCapturerSourceCallback(std::unique_ptr<ICapturerStateCallback> callback) override;
    void RegisterParameterCallback(IAudioSourceCallback *callback) override;
    float GetMaxAmplitude() override;

    int32_t UpdateAppsUid(const int32_t appsUid[PA_MAX_OUTPUTS_PER_SOURCE],
        const size_t size) final;
    int32_t UpdateAppsUid(const std::vector<int32_t> &appsUid) final;

    AudioCapturerSourceWakeup() = default;
    ~AudioCapturerSourceWakeup() = default;

private:
    static inline void MemcpysAndCheck(void *dest, size_t destMax, const void *src, size_t count)
    {
        if (memcpy_s(dest, destMax, src, count)) {
            AUDIO_ERR_LOG("memcpy_s error");
        }
    }
    class WakeupBuffer {
    public:
        explicit WakeupBuffer(size_t sizeMax = BUFFER_SIZE_MAX)
            : sizeMax_(sizeMax),
              buffer_(std::make_unique<char[]>(sizeMax))
        {
        }

        ~WakeupBuffer() = default;

        int32_t Poll(char *frame, uint64_t requestBytes, uint64_t &replyBytes, uint64_t &noStart)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (noStart < headNum_) {
                noStart = headNum_;
            }

            if (noStart >= (headNum_ + size_)) {
                if (requestBytes > sizeMax_) {
                    requestBytes = sizeMax_;
                }

                int32_t res = audioCapturerSource_.CaptureFrame(frame, requestBytes, replyBytes);
                Offer(frame, replyBytes);

                return res;
            }

            if (requestBytes > size_) { // size_!=0
                replyBytes = size_;
            } else {
                replyBytes = requestBytes;
            }

            uint64_t tail = (head_ + size_) % sizeMax_;

            if (tail > head_) {
                MemcpysAndCheck(frame, replyBytes, buffer_.get() + head_, replyBytes);
                headNum_ += replyBytes;
                size_ -= replyBytes;
                head_ = (head_ + replyBytes) % sizeMax_;
            } else {
                uint64_t copySize = min((sizeMax_ - head_), replyBytes);
                if (copySize != 0) {
                    MemcpysAndCheck(frame, replyBytes, buffer_.get() + head_, copySize);
                    headNum_ += copySize;
                    size_ -= copySize;
                    head_ = (head_ + copySize) % sizeMax_;
                }

                uint64_t remainCopySize = replyBytes - copySize;
                if (remainCopySize != 0) {
                    MemcpysAndCheck(frame + copySize, remainCopySize, buffer_.get(), remainCopySize);
                    headNum_ += remainCopySize;
                    size_ -= remainCopySize;
                    head_ = (head_ + remainCopySize) % sizeMax_;
                }
            }

            return SUCCESS;
        }
    private:
        static constexpr size_t BUFFER_SIZE_MAX = 32000; // 2 seconds

        const size_t sizeMax_;
        size_t size_ = 0;

        std::unique_ptr<char[]> buffer_;
        std::mutex mutex_;

        uint64_t head_ = 0;

        uint64_t headNum_ = 0;

        void Offer(const char *frame, const uint64_t bufferBytes)
        {
            if ((size_ + bufferBytes) > sizeMax_) { // head_ need shift
                u_int64_t shift = (size_ + bufferBytes) - sizeMax_; // 1 to sizeMax_
                headNum_ += shift;
                if (size_ > shift) {
                    size_ -= shift;
                    head_ = ((head_ + shift) % sizeMax_);
                } else {
                    size_ = 0;
                    head_ = 0;
                }
            }

            uint64_t tail = (head_ + size_) % sizeMax_;
            if (tail < head_) {
                MemcpysAndCheck((buffer_.get() + tail), bufferBytes, frame, bufferBytes);
            } else {
                uint64_t copySize = min(sizeMax_ - tail, bufferBytes);
                MemcpysAndCheck((buffer_.get() + tail), sizeMax_ - tail, frame, copySize);

                if (copySize < bufferBytes) {
                    MemcpysAndCheck((buffer_.get()), bufferBytes - copySize, frame + copySize, bufferBytes - copySize);
                }
            }
            size_ += bufferBytes;
        }
    };

    uint64_t noStart_ = 0;
    std::atomic<bool> isInited = false;
    static inline int initCount = 0;

    std::atomic<bool> isStarted = false;
    static inline int startCount = 0;

    static inline std::unique_ptr<WakeupBuffer> wakeupBuffer_;
    static inline std::mutex wakeupMutex_;

    static inline AudioCapturerSourceInner audioCapturerSource_;
};
#ifdef FEATURE_POWER_MANAGER
constexpr int32_t RUNNINGLOCK_LOCK_TIMEOUTMS_LASTING = -1;
#endif

AudioCapturerSourceInner::AudioCapturerSourceInner(const std::string &halName)
    : sourceInited_(false), captureInited_(false), started_(false), paused_(false),
      leftVolume_(MAX_VOLUME_LEVEL), rightVolume_(MAX_VOLUME_LEVEL), openMic_(0),
      audioManager_(nullptr), audioAdapter_(nullptr), audioCapture_(nullptr), halName_(halName)
{
    attr_ = {};
}

AudioCapturerSourceInner::~AudioCapturerSourceInner()
{
    AUDIO_WARNING_LOG("~AudioCapturerSourceInner");
}

AudioCapturerSource *AudioCapturerSource::GetInstance(const std::string &halName,
    const SourceType sourceType, const char *sourceName)
{
    Trace trace("AudioCapturerSourceInner:GetInstance");
    if (halName == "usb") {
        static AudioCapturerSourceInner audioCapturerUsb(halName);
        return &audioCapturerUsb;
    }

    switch (sourceType) {
        case SourceType::SOURCE_TYPE_MIC:
        case SourceType::SOURCE_TYPE_VOICE_CALL:
            return GetMicInstance();
        case SourceType::SOURCE_TYPE_WAKEUP:
            if (!strcmp(sourceName, "Built_in_wakeup_mirror")) {
                return GetWakeupInstance(true);
            } else {
                return GetWakeupInstance(false);
            }
        default:
            AUDIO_ERR_LOG("sourceType error %{public}d", sourceType);
            return GetMicInstance();
    }
}

static enum AudioInputType ConvertToHDIAudioInputType(const int32_t currSourceType)
{
    enum AudioInputType hdiAudioInputType;
    switch (currSourceType) {
        case SOURCE_TYPE_INVALID:
            hdiAudioInputType = AUDIO_INPUT_DEFAULT_TYPE;
            break;
        case SOURCE_TYPE_MIC:
        case SOURCE_TYPE_PLAYBACK_CAPTURE:
        case SOURCE_TYPE_ULTRASONIC:
            hdiAudioInputType = AUDIO_INPUT_MIC_TYPE;
            break;
        case SOURCE_TYPE_WAKEUP:
            hdiAudioInputType = AUDIO_INPUT_SPEECH_WAKEUP_TYPE;
            break;
        case SOURCE_TYPE_VOICE_COMMUNICATION:
            hdiAudioInputType = AUDIO_INPUT_VOICE_COMMUNICATION_TYPE;
            break;
        case SOURCE_TYPE_VOICE_RECOGNITION:
            hdiAudioInputType = AUDIO_INPUT_VOICE_RECOGNITION_TYPE;
            break;
        case SOURCE_TYPE_VOICE_CALL:
            hdiAudioInputType = AUDIO_INPUT_VOICE_CALL_TYPE;
            break;
        default:
            hdiAudioInputType = AUDIO_INPUT_MIC_TYPE;
            break;
    }
    return hdiAudioInputType;
}

static uint32_t GenerateUniqueIDBySource(int32_t source)
{
    uint32_t sourceId = 0;
    switch (source) {
        case SOURCE_TYPE_WAKEUP:
            sourceId = GenerateUniqueID(AUDIO_HDI_CAPTURE_ID_BASE, HDI_CAPTURE_OFFSET_WAKEUP);
            break;
        default:
            sourceId = GenerateUniqueID(AUDIO_HDI_CAPTURE_ID_BASE, HDI_CAPTURE_OFFSET_PRIMARY);
            break;
    }
    return sourceId;
}

AudioCapturerSource *AudioCapturerSource::GetMicInstance()
{
    static AudioCapturerSourceInner audioCapturer;
    return &audioCapturer;
}

AudioCapturerSource *AudioCapturerSource::GetWakeupInstance(bool isMirror)
{
    if (isMirror) {
        static AudioCapturerSourceWakeup audioCapturerMirror;
        return &audioCapturerMirror;
    }
    static AudioCapturerSourceWakeup audioCapturer;
    return &audioCapturer;
}

bool AudioCapturerSourceInner::IsInited(void)
{
    return sourceInited_;
}

void AudioCapturerSourceInner::DeInit()
{
    std::lock_guard<std::mutex> statusLock(statusMutex_);
    Trace trace("AudioCapturerSourceInner::DeInit");
    AudioXCollie sourceXCollie("AudioCapturerSourceInner::DeInit", DEINIT_TIME_OUT_SECONDS);
    AUDIO_INFO_LOG("Start deinit of source inner");
    started_ = false;
    sourceInited_ = false;

    if (audioAdapter_ != nullptr) {
        audioAdapter_->DestroyCapture(audioAdapter_, captureId_);
    }
    captureInited_ = false;

    IAudioSourceCallback* callback = nullptr;
    {
        std::lock_guard<std::mutex> lck(wakeupClosecallbackMutex_);
        callback = wakeupCloseCallback_;
    }
    if (callback != nullptr) {
        callback->OnWakeupClose();
    }

    audioCapture_ = nullptr;
    currentActiveDevice_ = DEVICE_TYPE_INVALID; // the current device must be rest when closing capturer.

    // Only the usb hal needs to be unloadadapter at the moment.
    if (halName_ == "usb") {
        adapterLoaded_ = false;
        if (audioManager_ != nullptr) {
            audioManager_->UnloadAdapter(audioManager_, adapterDesc_.adapterName);
        }
        audioAdapter_ = nullptr;
        audioManager_ = nullptr;
    }

    DumpFileUtil::CloseDumpFile(&dumpFile_);
}

void AudioCapturerSourceInner::InitAttrsCapture(struct AudioSampleAttributes &attrs)
{
    /* Initialization of audio parameters for playback */
    attrs.format = AUDIO_FORMAT_TYPE_PCM_16_BIT;
    attrs.channelCount = AUDIO_CHANNELCOUNT;
    attrs.sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs.interleaved = true;
    attrs.streamId = GenerateUniqueIDBySource(attr_.sourceType);
    attrs.type = AUDIO_IN_MEDIA;
    attrs.period = DEEP_BUFFER_CAPTURE_PERIOD_SIZE;
    attrs.frameSize = PCM_16_BIT * attrs.channelCount / PCM_8_BIT;
    attrs.isBigEndian = false;
    attrs.isSignedData = true;
    attrs.startThreshold = DEEP_BUFFER_CAPTURE_PERIOD_SIZE / (attrs.frameSize);
    attrs.stopThreshold = INT_32_MAX;
    /* 16 * 1024 */
    attrs.silenceThreshold = AUDIO_BUFF_SIZE;
    attrs.sourceType = SOURCE_TYPE_MIC;
}

int32_t SwitchAdapterCapture(struct AudioAdapterDescriptor *descs, uint32_t size, const std::string &adapterNameCase,
    enum AudioPortDirection portFlag, struct AudioPort &capturePort)
{
    if (descs == nullptr) {
        return ERROR;
    }

    for (uint32_t index = 0; index < size; index++) {
        struct AudioAdapterDescriptor *desc = &descs[index];
        if (desc == nullptr || desc->adapterName == nullptr) {
            continue;
        }
        AUDIO_INFO_LOG("size: %{public}d, adapterNameCase %{public}s, adapterName %{public}s",
            size, adapterNameCase.c_str(), desc->adapterName);
        if (!adapterNameCase.compare(desc->adapterName)) {
            for (uint32_t port = 0; port < desc->portsLen; port++) {
                // Only find out the port of out in the sound card
                if (desc->ports[port].dir == portFlag) {
                    capturePort = desc->ports[port];
                    return index;
                }
            }
        }
    }
    AUDIO_ERR_LOG("SwitchAdapterCapture Fail");

    return ERR_INVALID_INDEX;
}

int32_t AudioCapturerSourceInner::InitAudioManager()
{
    AUDIO_INFO_LOG("Initialize audio proxy manager");

    if (audioManager_ == nullptr) {
        audioManager_ = IAudioManagerGet(false);
    }

    if (audioManager_ == nullptr) {
        return ERR_INVALID_HANDLE;
    }

    return 0;
}

AudioFormat AudioCapturerSourceInner::ConvertToHdiFormat(HdiAdapterFormat format)
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

int32_t AudioCapturerSourceInner::CreateCapture(struct AudioPort &capturePort)
{
    Trace trace("AudioCapturerSourceInner:CreateCapture");

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
    } else if (param.channelCount == CHANNEL_4) {
        param.channelLayout = CH_LAYOUT_QUAD;
    }
    param.silenceThreshold = attr_.bufferSize;
    param.frameSize = param.format * param.channelCount;
    param.startThreshold = DEEP_BUFFER_CAPTURE_PERIOD_SIZE / (param.frameSize);
    param.sourceType = static_cast<int32_t>(ConvertToHDIAudioInputType(attr_.sourceType));

    struct AudioDeviceDescriptor deviceDesc;
    deviceDesc.portId = capturePort.portId;
    deviceDesc.pins = PIN_IN_MIC;
    if (halName_ == "usb") {
        deviceDesc.pins = PIN_IN_USB_HEADSET;
    }
    deviceDesc.desc = (char *)"";

    AUDIO_INFO_LOG("Create capture sourceName:%{public}s, hdisource:%{public}d, " \
        "rate:%{public}u channel:%{public}u format:%{public}u, devicePin:%{public}u",
        halName_.c_str(), param.sourceType, param.sampleRate, param.channelCount, param.format, deviceDesc.pins);
    int32_t ret = audioAdapter_->CreateCapture(audioAdapter_, &deviceDesc, &param, &audioCapture_, &captureId_);
    CHECK_AND_RETURN_RET_LOG(audioCapture_ != nullptr && ret >= 0, ERR_NOT_STARTED, "Create capture failed");

    return 0;
}

int32_t AudioCapturerSourceInner::Init(const IAudioSourceAttr &attr)
{
    std::lock_guard<std::mutex> statusLock(statusMutex_);
    attr_ = attr;
    adapterNameCase_ = attr_.adapterName;
    openMic_ = attr_.openMicSpeaker;
    logMode_ = system::GetIntParameter("persist.multimedia.audiolog.switch", 0);

    int32_t ret = InitAdapterAndCapture();
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "Init adapter and capture failed");

    sourceInited_ = true;

    SetMute(muteState_);

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::CaptureFrame(char *frame, uint64_t requestBytes, uint64_t &replyBytes)
{
    CHECK_AND_RETURN_RET_LOG(audioCapture_ != nullptr, ERR_INVALID_HANDLE, "Audio capture Handle is nullptr!");

    Trace trace("AudioCapturerSourceInner::CaptureFrame");

    int64_t stamp = ClockTime::GetCurNano();
    uint32_t frameLen = static_cast<uint32_t>(requestBytes);
    int32_t ret = audioCapture_->CaptureFrame(audioCapture_, reinterpret_cast<int8_t*>(frame), &frameLen, &replyBytes);
    CHECK_AND_RETURN_RET_LOG(ret >= 0, ERR_READ_FAILED, "Capture Frame Fail");
    CheckLatencySignal(reinterpret_cast<uint8_t*>(frame), replyBytes);

    DumpFileUtil::WriteDumpFile(dumpFile_, frame, replyBytes);
    if (AudioDump::GetInstance().GetVersionType() == BETA_VERSION) {
        Media::MediaMonitor::MediaMonitorManager::GetInstance().WriteAudioBuffer(dumpFileName_,
            static_cast<void*>(frame), replyBytes);
    }
    CheckUpdateState(frame, requestBytes);

    stamp = (ClockTime::GetCurNano() - stamp) / AUDIO_US_PER_SECOND;
    if (logMode_) {
        AUDIO_DEBUG_LOG("RenderFrame len[%{public}" PRIu64 "] cost[%{public}" PRId64 "]ms", requestBytes, stamp);
    }
    return SUCCESS;
}

void AudioCapturerSourceInner::CheckUpdateState(char *frame, uint64_t replyBytes)
{
    if (startUpdate_) {
        std::lock_guard<std::mutex> lock(statusMutex_);
        if (capFrameNum_ == 0) {
            last10FrameStartTime_ = ClockTime::GetCurNano();
        }
        capFrameNum_++;
        maxAmplitude_ = UpdateMaxAmplitude(static_cast<ConvertHdiFormat>(attr_.format), frame, replyBytes);
        if (capFrameNum_ == GET_MAX_AMPLITUDE_FRAMES_THRESHOLD) {
            capFrameNum_ = 0;
            if (last10FrameStartTime_ > lastGetMaxAmplitudeTime_) {
                startUpdate_ = false;
                maxAmplitude_ = 0;
            }
        }
    }
}

float AudioCapturerSourceInner::GetMaxAmplitude()
{
    lastGetMaxAmplitudeTime_ = ClockTime::GetCurNano();
    startUpdate_ = true;
    return maxAmplitude_;
}

int32_t AudioCapturerSourceInner::Start(void)
{
    std::lock_guard<std::mutex> statusLock(statusMutex_);

    AUDIO_INFO_LOG("sourceName %{public}s", halName_.c_str());
    Trace trace("AudioCapturerSourceInner::Start");

    InitLatencyMeasurement();
#ifdef FEATURE_POWER_MANAGER
    std::shared_ptr<PowerMgr::RunningLock> keepRunningLock;
    if (runningLockManager_ == nullptr) {
        switch (attr_.sourceType) {
            case SOURCE_TYPE_WAKEUP:
                keepRunningLock = PowerMgr::PowerMgrClient::GetInstance().CreateRunningLock("AudioWakeupCapturer",
                    PowerMgr::RunningLockType::RUNNINGLOCK_BACKGROUND_AUDIO);
                break;
            case SOURCE_TYPE_MIC:
            default:
                keepRunningLock = PowerMgr::PowerMgrClient::GetInstance().CreateRunningLock("AudioPrimaryCapturer",
                    PowerMgr::RunningLockType::RUNNINGLOCK_BACKGROUND_AUDIO);
        }
        if (keepRunningLock) {
            runningLockManager_ = std::make_shared<AudioRunningLockManager<PowerMgr::RunningLock>> (keepRunningLock);
        }
    }
    if (runningLockManager_ != nullptr) {
        AUDIO_INFO_LOG("keepRunningLock lock result: %{public}d",
            runningLockManager_->Lock(RUNNINGLOCK_LOCK_TIMEOUTMS_LASTING)); // -1 for lasting.
    } else {
        AUDIO_WARNING_LOG("keepRunningLock is null, capture can not work well!");
    }
#endif
    // eg: primary_0_20240527202236189_source_44100_2_1.pcm
    dumpFileName_ = halName_ + "_" + std::to_string(attr_.sourceType) + "_" + GetTime()
        + "_source_" + std::to_string(attr_.sampleRate) + "_" + std::to_string(attr_.channel)
        + "_" + std::to_string(attr_.format) + ".pcm";
    DumpFileUtil::OpenDumpFile(DUMP_SERVER_PARA, dumpFileName_, &dumpFile_);

    if (!started_) {
        if (audioCapturerSourceCallback_ != nullptr) {
            audioCapturerSourceCallback_->OnCapturerState(true);
        }

        int32_t ret = audioCapture_->Start(audioCapture_);
        if (ret < 0) {
            return ERR_NOT_STARTED;
        }
        started_ = true;
    }

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::SetVolume(float left, float right)
{
    float volume;
    CHECK_AND_RETURN_RET_LOG(audioCapture_ != nullptr, ERR_INVALID_HANDLE,
        "SetVolume failed audioCapture_ null");

    rightVolume_ = right;
    leftVolume_ = left;
    if ((leftVolume_ == 0) && (rightVolume_ != 0)) {
        volume = rightVolume_;
    } else if ((leftVolume_ != 0) && (rightVolume_ == 0)) {
        volume = leftVolume_;
    } else {
        volume = (leftVolume_ + rightVolume_) / HALF_FACTOR;
    }

    audioCapture_->SetVolume(audioCapture_, volume);

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::GetVolume(float &left, float &right)
{
    float val = 0.0;
    audioCapture_->GetVolume(audioCapture_, &val);
    left = val;
    right = val;

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::SetMute(bool isMute)
{
    muteState_ = isMute;

    if (IsInited() && audioCapture_) {
        int32_t ret = audioCapture_->SetMute(audioCapture_, isMute);
        if (ret != 0) {
            AUDIO_WARNING_LOG("SetMute for hdi capturer failed");
        } else {
            AUDIO_INFO_LOG("SetMute for hdi capture success");
        }
    }

    if ((halName_ == "primary") && !adapterLoaded_) {
        InitManagerAndAdapter();
    }

    if (audioAdapter_ != nullptr) {
        int32_t ret = audioAdapter_->SetMicMute(audioAdapter_, isMute);
        if (ret != 0) {
            AUDIO_WARNING_LOG("SetMicMute for hdi adapter failed");
        } else {
            AUDIO_INFO_LOG("SetMicMute for hdi adapter success");
        }
    }

    AUDIO_INFO_LOG("end isMute=%{public}d", isMute);

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::GetMute(bool &isMute)
{
    CHECK_AND_RETURN_RET_LOG(audioCapture_ != nullptr, ERR_INVALID_HANDLE,
        "GetMute failed audioCapture_ handle is null!");

    bool isHdiMute = false;
    int32_t ret = audioCapture_->GetMute(audioCapture_, &isHdiMute);
    if (ret != 0) {
        AUDIO_WARNING_LOG("GetMute failed from hdi");
    }

    isMute = muteState_;

    return SUCCESS;
}

static AudioCategory GetAudioCategory(AudioScene audioScene)
{
    AudioCategory audioCategory;
    switch (audioScene) {
        case AUDIO_SCENE_PHONE_CALL:
            audioCategory = AUDIO_IN_CALL;
            break;
        case AUDIO_SCENE_PHONE_CHAT:
            audioCategory = AUDIO_IN_COMMUNICATION;
            break;
        case AUDIO_SCENE_RINGING:
        case AUDIO_SCENE_VOICE_RINGING:
            audioCategory = AUDIO_IN_RINGTONE;
            break;
        case AUDIO_SCENE_DEFAULT:
            audioCategory = AUDIO_IN_MEDIA;
            break;
        default:
            audioCategory = AUDIO_IN_MEDIA;
            break;
    }
    AUDIO_DEBUG_LOG("Audio category returned is: %{public}d", audioCategory);

    return audioCategory;
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
        case DEVICE_TYPE_USB_ARM_HEADSET:
            source.ext.device.type = PIN_IN_USB_HEADSET;
            source.ext.device.desc = (char *)"pin_in_usb_headset";
            break;
        case DEVICE_TYPE_USB_HEADSET:
            source.ext.device.type = PIN_IN_USB_EXT;
            source.ext.device.desc = (char *)"pin_in_usb_ext";
            break;
        case DEVICE_TYPE_BLUETOOTH_SCO:
            source.ext.device.type = PIN_IN_BLUETOOTH_SCO_HEADSET;
            source.ext.device.desc = (char *)"pin_in_bluetooth_sco_headset";
            break;
        default:
            ret = ERR_NOT_SUPPORTED;
            break;
    }

    return ret;
}

int32_t AudioCapturerSourceInner::SetInputRoute(DeviceType inputDevice)
{
    std::lock_guard<std::mutex> statusLock(statusMutex_);
    AudioPortPin inputPortPin = PIN_IN_MIC;
    return SetInputRoute(inputDevice, inputPortPin);
}

int32_t AudioCapturerSourceInner::SetInputRoute(DeviceType inputDevice, AudioPortPin &inputPortPin)
{
    if (inputDevice == currentActiveDevice_) {
        AUDIO_INFO_LOG("input device not change. currentActiveDevice %{public}d sourceType %{public}d",
            currentActiveDevice_, attr_.sourceType);

        return SUCCESS;
    }

    return DoSetInputRoute(inputDevice, inputPortPin);
}

int32_t AudioCapturerSourceInner::DoSetInputRoute(DeviceType inputDevice, AudioPortPin &inputPortPin)
{
    AudioRouteNode source = {};
    AudioRouteNode sink = {};

    int32_t ret = SetInputPortPin(inputDevice, source);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "DoSetInputRoute FAILED: %{public}d", ret);

    inputPortPin = source.ext.device.type;
    AUDIO_INFO_LOG("Input PIN is: 0x%{public}X", inputPortPin);
    source.portId = static_cast<int32_t>(audioPort_.portId);
    source.role = AUDIO_PORT_SOURCE_ROLE;
    source.type = AUDIO_PORT_DEVICE_TYPE;
    source.ext.device.moduleId = 0;
    source.ext.device.desc = (char *)"";

    sink.portId = 0;
    sink.role = AUDIO_PORT_SINK_ROLE;
    sink.type = AUDIO_PORT_MIX_TYPE;
    sink.ext.mix.moduleId = 0;
    sink.ext.mix.streamId = GenerateUniqueIDBySource(attr_.sourceType);
    sink.ext.mix.source = static_cast<int32_t>(ConvertToHDIAudioInputType(attr_.sourceType));
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
    CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_OPERATION_FAILED, "UpdateAudioRoute failed");

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    AUDIO_INFO_LOG("SetAudioScene scene: %{public}d, device: %{public}d",
        audioScene, activeDevice);
    CHECK_AND_RETURN_RET_LOG(audioScene >= AUDIO_SCENE_DEFAULT && audioScene < AUDIO_SCENE_MAX,
        ERR_INVALID_PARAM, "invalid audioScene");
    CHECK_AND_RETURN_RET_LOG(audioCapture_ != nullptr, ERR_INVALID_HANDLE,
        "SetAudioScene failed audioCapture_ handle is null!");
    if (openMic_) {
        AudioPortPin audioSceneInPort = PIN_IN_MIC;
        if (halName_ == "usb") {
            audioSceneInPort = PIN_IN_USB_HEADSET;
        }

        int32_t ret = SUCCESS;
        if (audioScene != currentAudioScene_) {
            struct AudioSceneDescriptor scene;
            scene.scene.id = GetAudioCategory(audioScene);
            scene.desc.pins = audioSceneInPort;
            scene.desc.desc = const_cast<char *>("");

            ret = audioCapture_->SelectScene(audioCapture_, &scene);
            CHECK_AND_RETURN_RET_LOG(ret >= 0, ERR_OPERATION_FAILED,
                "Select scene FAILED: %{public}d", ret);
            currentAudioScene_ = audioScene;
        }

        std::lock_guard<std::mutex> statusLock(statusMutex_);
        ret = SetInputRoute(activeDevice, audioSceneInPort);
        if (ret < 0) {
            AUDIO_WARNING_LOG("Update route FAILED: %{public}d", ret);
        }
    }
    AUDIO_DEBUG_LOG("Select audio scene SUCCESS: %{public}d", audioScene);
    return SUCCESS;
}

uint64_t AudioCapturerSourceInner::GetTransactionId()
{
    return reinterpret_cast<uint64_t>(audioCapture_);
}

int32_t AudioCapturerSourceInner::GetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec)
{
    if (audioCapture_ == nullptr) {
        AUDIO_ERR_LOG("failed audioCapture_ is NULL");
        return ERR_INVALID_HANDLE;
    }
    struct AudioTimeStamp timestamp = {};
    int32_t ret = audioCapture_->GetCapturePosition(audioCapture_, &frames, &timestamp);
    if (ret != 0) {
        AUDIO_ERR_LOG("get position failed");
        return ERR_OPERATION_FAILED;
    }
    int64_t maxSec = 9223372036; // (9223372036 + 1) * 10^9 > INT64_MAX, seconds should not bigger than it;
    if (timestamp.tvSec < 0 || timestamp.tvSec > maxSec || timestamp.tvNSec < 0 ||
        timestamp.tvNSec > SECOND_TO_NANOSECOND) {
        AUDIO_ERR_LOG(
            "Hdi GetRenderPosition get invaild second:%{public}" PRIu64 " or nanosecond:%{public}" PRIu64 " !",
            timestamp.tvSec, timestamp.tvNSec);
        return ERR_OPERATION_FAILED;
    }

    timeSec = timestamp.tvSec;
    timeNanoSec = timestamp.tvNSec;
    return ret;
}

int32_t AudioCapturerSourceInner::DoStop()
{
    AUDIO_INFO_LOG("sourceName %{public}s", halName_.c_str());

    Trace trace("AudioCapturerSourceInner::DoStop");

    DeinitLatencyMeasurement();

#ifdef FEATURE_POWER_MANAGER
    if (runningLockManager_ != nullptr) {
        AUDIO_INFO_LOG("keepRunningLock unlock");
        runningLockManager_->UnLock();
    } else {
        AUDIO_WARNING_LOG("keepRunningLock is null, stop can not work well!");
    }
#endif

    if (started_ && audioCapture_ != nullptr) {
        int32_t ret = audioCapture_->Stop(audioCapture_);
        CHECK_AND_RETURN_RET_LOG(ret >= 0, ERR_OPERATION_FAILED, "Stop capture Failed");
    }
    started_ = false;

    if (audioCapturerSourceCallback_ != nullptr) {
        audioCapturerSourceCallback_->OnCapturerState(false);
    }

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::Stop(void)
{
    Trace trace("AudioCapturerSourceInner::Stop");
    std::promise<void> promiseEnsueThreadLock;
    auto futureWaitThreadLock = promiseEnsueThreadLock.get_future();
    std::thread threadAsyncStop([&promiseEnsueThreadLock, this] {
        std::lock_guard<std::mutex> statusLock(statusMutex_);
        promiseEnsueThreadLock.set_value();
        DoStop();
    });
    futureWaitThreadLock.get();
    threadAsyncStop.detach();

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::Pause(void)
{
    std::lock_guard<std::mutex> statusLock(statusMutex_);
    AUDIO_INFO_LOG("sourceName %{public}s", halName_.c_str());

    Trace trace("AudioCapturerSourceInner::Pause");
    if (started_ && audioCapture_ != nullptr) {
        int32_t ret = audioCapture_->Pause(audioCapture_);
        CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_OPERATION_FAILED, "pause capture Failed");
    }
    paused_ = true;

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::Resume(void)
{
    std::lock_guard<std::mutex> statusLock(statusMutex_);
    AUDIO_INFO_LOG("sourceName %{public}s", halName_.c_str());
    Trace trace("AudioCapturerSourceInner::Resume");
    if (paused_ && audioCapture_ != nullptr) {
        int32_t ret = audioCapture_->Resume(audioCapture_);
        CHECK_AND_RETURN_RET_LOG(ret == 0, ERR_OPERATION_FAILED, "resume capture Failed");
    }
    paused_ = false;

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::Reset(void)
{
    std::lock_guard<std::mutex> statusLock(statusMutex_);
    AUDIO_INFO_LOG("sourceName %{public}s", halName_.c_str());
    Trace trace("AudioCapturerSourceInner::Reset");
    if (started_ && audioCapture_ != nullptr) {
        audioCapture_->Flush(audioCapture_);
    }

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::Flush(void)
{
    std::lock_guard<std::mutex> statusLock(statusMutex_);
    AUDIO_INFO_LOG("sourceName %{public}s", halName_.c_str());
    Trace trace("AudioCapturerSourceInner::Flush");
    if (started_ && audioCapture_ != nullptr) {
        audioCapture_->Flush(audioCapture_);
    }

    return SUCCESS;
}

void AudioCapturerSourceInner::RegisterWakeupCloseCallback(IAudioSourceCallback *callback)
{
    AUDIO_INFO_LOG("Register WakeupClose Callback");
    std::lock_guard<std::mutex> lck(wakeupClosecallbackMutex_);
    wakeupCloseCallback_ = callback;
}

void AudioCapturerSourceInner::RegisterAudioCapturerSourceCallback(std::unique_ptr<ICapturerStateCallback> callback)
{
    AUDIO_INFO_LOG("Register AudioCapturerSource Callback");
    audioCapturerSourceCallback_ = std::move(callback);
}

void AudioCapturerSourceInner::RegisterParameterCallback(IAudioSourceCallback *callback)
{
    AUDIO_WARNING_LOG("RegisterParameterCallback is not supported!");
}

int32_t AudioCapturerSourceInner::Preload(const std::string &usbInfoStr)
{
    CHECK_AND_RETURN_RET_LOG(halName_ == "usb", ERR_INVALID_OPERATION, "Preload only supported for usb");

    std::lock_guard<std::mutex> statusLock(statusMutex_);
    int32_t ret = UpdateUsbAttrs(usbInfoStr);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "Preload failed when init attr");

    ret = InitAdapterAndCapture();
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ret, "Preload failed when init adapter and capture");

    return SUCCESS;
}

static HdiAdapterFormat ParseAudioFormat(const std::string &format)
{
    if (format == "AUDIO_FORMAT_PCM_16_BIT") {
        return HdiAdapterFormat::SAMPLE_S16;
    } else if (format == "AUDIO_FORMAT_PCM_24_BIT") {
        return HdiAdapterFormat::SAMPLE_S24;
    } else if (format == "AUDIO_FORMAT_PCM_32_BIT") {
        return HdiAdapterFormat::SAMPLE_S32;
    } else {
        return HdiAdapterFormat::SAMPLE_S16;
    }
}

int32_t AudioCapturerSourceInner::UpdateUsbAttrs(const std::string &usbInfoStr)
{
    CHECK_AND_RETURN_RET_LOG(usbInfoStr != "", ERR_INVALID_PARAM, "usb info string error");

    auto sourceRate_begin = usbInfoStr.find("source_rate:");
    auto sourceRate_end = usbInfoStr.find_first_of(";", sourceRate_begin);
    std::string sampleRateStr = usbInfoStr.substr(sourceRate_begin + std::strlen("source_rate:"),
        sourceRate_end - sourceRate_begin - std::strlen("source_rate:"));
    auto sourceFormat_begin = usbInfoStr.find("source_format:");
    auto sourceFormat_end = usbInfoStr.find_first_of(";", sourceFormat_begin);
    std::string formatStr = usbInfoStr.substr(sourceFormat_begin + std::strlen("source_format:"),
        sourceFormat_end - sourceFormat_begin - std::strlen("source_format:"));

    // usb default config
    attr_.sampleRate = static_cast<uint32_t>(stoi(sampleRateStr));
    attr_.channel = STEREO_CHANNEL_COUNT;
    attr_.format = ParseAudioFormat(formatStr);
    attr_.isBigEndian = false;
    attr_.bufferSize = USB_DEFAULT_BUFFERSIZE;
    attr_.sourceType = SOURCE_TYPE_MIC;

    adapterNameCase_ = "usb";
    openMic_ = 0;

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::InitManagerAndAdapter()
{
    int32_t err = InitAudioManager();
    CHECK_AND_RETURN_RET_LOG(err == 0, ERR_NOT_STARTED, "Init audio manager Fail");

    AudioAdapterDescriptor descs[MAX_AUDIO_ADAPTER_NUM];
    uint32_t size = MAX_AUDIO_ADAPTER_NUM;
    int32_t ret = audioManager_->GetAllAdapters(audioManager_, (struct AudioAdapterDescriptor *)&descs, &size);
    CHECK_AND_RETURN_RET_LOG(size <= MAX_AUDIO_ADAPTER_NUM && size != 0 && ret == 0,
        ERR_NOT_STARTED, "Get adapters Fail");
    if (adapterNameCase_ == "" && halName_ == "primary") {
        adapterNameCase_ = "primary";
    }
    // Get qualified sound card and port
    int32_t index = SwitchAdapterCapture((struct AudioAdapterDescriptor *)&descs,
        size, adapterNameCase_, PORT_IN, audioPort_);
    CHECK_AND_RETURN_RET_LOG(index >= 0, ERR_NOT_STARTED, "Switch Adapter Capture Fail");
    adapterDesc_ = descs[index];

    if (audioAdapter_ == nullptr) {
        struct IAudioAdapter *iAudioAdapter = nullptr;
        int32_t loadAdapter = audioManager_->LoadAdapter(audioManager_, &adapterDesc_, &iAudioAdapter);
        CHECK_AND_RETURN_RET_LOG(loadAdapter == 0, ERR_NOT_STARTED, "Load Adapter Fail");
        CHECK_AND_RETURN_RET_LOG(iAudioAdapter != nullptr, ERR_NOT_STARTED, "Load audio device failed");

        // Inittialization port information, can fill through mode and other parameters
        int32_t initAllPorts = iAudioAdapter->InitAllPorts(iAudioAdapter);
        CHECK_AND_RETURN_RET_LOG(initAllPorts == 0, ERR_DEVICE_INIT, "InitAllPorts failed");
        audioAdapter_ = iAudioAdapter;
        adapterLoaded_ = true;
    }
    return SUCCESS;
}

int32_t AudioCapturerSourceInner::InitAdapterAndCapture()
{
    AUDIO_INFO_LOG("Init adapter start sourceName %{public}s", halName_.c_str());

    if (captureInited_) {
        AUDIO_INFO_LOG("Adapter already inited");
        return SUCCESS;
    }

    int32_t err = InitManagerAndAdapter();
    CHECK_AND_RETURN_RET_LOG(err == 0, err, "Init audio manager and adapater failed");

    int32_t createCapture = CreateCapture(audioPort_);
    CHECK_AND_RETURN_RET_LOG(createCapture == 0, ERR_NOT_STARTED, "Create capture failed");
    if (openMic_) {
        int32_t ret;
        AudioPortPin inputPortPin = PIN_IN_MIC;
        if (halName_ == "usb") {
            ret = SetInputRoute(DEVICE_TYPE_USB_ARM_HEADSET, inputPortPin);
        } else {
            ret = SetInputRoute(DEVICE_TYPE_MIC, inputPortPin);
        }
        if (ret < 0) {
            AUDIO_WARNING_LOG("update route FAILED: %{public}d", ret);
        }
    }

    captureInited_ = true;

    return SUCCESS;
}

std::string AudioCapturerSourceInner::GetAudioParameter(const AudioParamKey key,
                                                        const std::string &condition)
{
    AUDIO_WARNING_LOG("not supported yet");
    return "";
}

void AudioCapturerSourceInner::InitLatencyMeasurement()
{
    std::lock_guard<std::mutex> lock(signalDetectAgentMutex_);

    if (!AudioLatencyMeasurement::CheckIfEnabled()) {
        return;
    }
    signalDetectAgent_ = std::make_shared<SignalDetectAgent>();
    CHECK_AND_RETURN_LOG(signalDetectAgent_ != nullptr, "LatencyMeas signalDetectAgent_ is nullptr");
    signalDetectAgent_->sampleFormat_ = attr_.format;
    signalDetectAgent_->formatByteSize_ = GetFormatByteSize(attr_.format);
    latencyMeasEnabled_ = true;
}

void AudioCapturerSourceInner::DeinitLatencyMeasurement()
{
    std::lock_guard<std::mutex> lock(signalDetectAgentMutex_);

    signalDetected_ = false;
    signalDetectAgent_ = nullptr;
}

void AudioCapturerSourceInner::CheckLatencySignal(uint8_t *frame, size_t replyBytes)
{
    std::lock_guard<std::mutex> lock(signalDetectAgentMutex_);
    if (!latencyMeasEnabled_) {
        return;
    }
    CHECK_AND_RETURN_LOG(signalDetectAgent_ != nullptr, "LatencyMeas signalDetectAgent_ is nullptr");
    signalDetected_ = signalDetectAgent_->CheckAudioData(frame, replyBytes);
    if (signalDetected_) {
        char value[GET_EXTRA_PARAM_LEN];
        AudioParamKey key = NONE;
        AudioExtParamKey hdiKey = AudioExtParamKey(key);
        std::string condition = "debug_audio_latency_measurement";
        int32_t ret = audioAdapter_->GetExtraParams(audioAdapter_, hdiKey, condition.c_str(),
            value, PARAM_VALUE_LENTH);
        AUDIO_INFO_LOG("GetExtraParam ret:%{public}d", ret);
        LatencyMonitor::GetInstance().UpdateDspTime(value);
        LatencyMonitor::GetInstance().UpdateSinkOrSourceTime(false,
            signalDetectAgent_->lastPeakBufferTime_);
        AUDIO_INFO_LOG("LatencyMeas primarySource signal detected");
        signalDetected_ = false;
    }
}

int32_t AudioCapturerSourceInner::UpdateAppsUid(const int32_t appsUid[PA_MAX_OUTPUTS_PER_SOURCE],
    const size_t size)
{
#ifdef FEATURE_POWER_MANAGER
    if (!runningLockManager_) {
        return ERROR;
    }

    runningLockManager_->UpdateAppsUid(appsUid, appsUid + size);
    runningLockManager_->UpdateAppsUidToPowerMgr();
#endif

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::UpdateAppsUid(const std::vector<int32_t> &appsUid)
{
#ifdef FEATURE_POWER_MANAGER
    if (!runningLockManager_) {
        return ERROR;
    }

    runningLockManager_->UpdateAppsUid(appsUid.cbegin(), appsUid.cend());
    runningLockManager_->UpdateAppsUidToPowerMgr();
#endif

    return SUCCESS;
}

int32_t AudioCapturerSourceInner::UpdateSourceType(SourceType sourceType)
{
    std::lock_guard<std::mutex> lock(statusMutex_);
    if (attr_.sourceType == sourceType) {
        AUDIO_INFO_LOG("input sourceType not change. currentActiveDevice %{public}d sourceType %{public}d",
            currentActiveDevice_, attr_.sourceType);
        return SUCCESS;
    }

    attr_.sourceType = sourceType;
    AudioPortPin inputPortPin = PIN_IN_MIC;
    return DoSetInputRoute(currentActiveDevice_, inputPortPin);
}

int32_t AudioCapturerSourceWakeup::Init(const IAudioSourceAttr &attr)
{
    std::lock_guard<std::mutex> lock(wakeupMutex_);
    int32_t res = SUCCESS;
    if (isInited) {
        return res;
    }
    noStart_ = 0;
    if (initCount == 0) {
        if (wakeupBuffer_ == nullptr) {
            wakeupBuffer_ = std::make_unique<WakeupBuffer>();
        }
        res = audioCapturerSource_.Init(attr);
    }
    if (res == SUCCESS) {
        isInited = true;
        initCount++;
    }
    return res;
}

bool AudioCapturerSourceWakeup::IsInited(void)
{
    return isInited;
}

void AudioCapturerSourceWakeup::DeInit(void)
{
    AudioXCollie wakeupXCollie("AudioCapturerSourceWakeup::DeInit", DEINIT_TIME_OUT_SECONDS);
    AUDIO_INFO_LOG("Start deinit of source wakeup");
    std::lock_guard<std::mutex> lock(wakeupMutex_);
    if (!isInited) {
        return;
    }
    isInited = false;
    initCount--;
    if (initCount == 0) {
        wakeupBuffer_.reset();
        audioCapturerSource_.DeInit();
    }
}

int32_t AudioCapturerSourceWakeup::Start(void)
{
    std::lock_guard<std::mutex> lock(wakeupMutex_);
    int32_t res = SUCCESS;
    if (isStarted) {
        return res;
    }
    if (startCount == 0) {
        res = audioCapturerSource_.Start();
    }
    if (res == SUCCESS) {
        isStarted = true;
        startCount++;
    }
    return res;
}

int32_t AudioCapturerSourceWakeup::Stop(void)
{
    std::lock_guard<std::mutex> lock(wakeupMutex_);
    int32_t res = SUCCESS;
    if (!isStarted) {
        return res;
    }
    if (startCount == 1) {
        res = audioCapturerSource_.Stop();
    }
    if (res == SUCCESS) {
        isStarted = false;
        startCount--;
    }
    return res;
}

int32_t AudioCapturerSourceWakeup::Flush(void)
{
    return audioCapturerSource_.Flush();
}

int32_t AudioCapturerSourceWakeup::Reset(void)
{
    return audioCapturerSource_.Reset();
}

int32_t AudioCapturerSourceWakeup::Pause(void)
{
    return audioCapturerSource_.Pause();
}

int32_t AudioCapturerSourceWakeup::Resume(void)
{
    return audioCapturerSource_.Resume();
}

int32_t AudioCapturerSourceWakeup::CaptureFrame(char *frame, uint64_t requestBytes, uint64_t &replyBytes)
{
    int32_t res = wakeupBuffer_->Poll(frame, requestBytes, replyBytes, noStart_);
    noStart_ += replyBytes;
    return res;
}

int32_t AudioCapturerSourceWakeup::SetVolume(float left, float right)
{
    return audioCapturerSource_.SetVolume(left, right);
}

int32_t AudioCapturerSourceWakeup::GetVolume(float &left, float &right)
{
    return audioCapturerSource_.GetVolume(left, right);
}

int32_t AudioCapturerSourceWakeup::SetMute(bool isMute)
{
    return audioCapturerSource_.SetMute(isMute);
}

int32_t AudioCapturerSourceWakeup::GetMute(bool &isMute)
{
    return audioCapturerSource_.GetMute(isMute);
}

int32_t AudioCapturerSourceWakeup::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    return audioCapturerSource_.SetAudioScene(audioScene, activeDevice);
}

int32_t AudioCapturerSourceWakeup::SetInputRoute(DeviceType inputDevice)
{
    return audioCapturerSource_.SetInputRoute(inputDevice);
}

uint64_t AudioCapturerSourceWakeup::GetTransactionId()
{
    return audioCapturerSource_.GetTransactionId();
}

int32_t AudioCapturerSourceWakeup::GetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec)
{
    return audioCapturerSource_.GetPresentationPosition(frames, timeSec, timeNanoSec);
}

std::string AudioCapturerSourceWakeup::GetAudioParameter(const AudioParamKey key,
                                                         const std::string &condition)
{
    AUDIO_WARNING_LOG("not supported yet");
    return "";
}

void AudioCapturerSourceWakeup::RegisterWakeupCloseCallback(IAudioSourceCallback *callback)
{
    audioCapturerSource_.RegisterWakeupCloseCallback(callback);
}

void AudioCapturerSourceWakeup::RegisterAudioCapturerSourceCallback(std::unique_ptr<ICapturerStateCallback> callback)
{
    audioCapturerSource_.RegisterAudioCapturerSourceCallback(std::move(callback));
}

void AudioCapturerSourceWakeup::RegisterParameterCallback(IAudioSourceCallback *callback)
{
    AUDIO_WARNING_LOG("AudioCapturerSourceWakeup: RegisterParameterCallback is not supported!");
}

float AudioCapturerSourceWakeup::GetMaxAmplitude()
{
    return audioCapturerSource_.GetMaxAmplitude();
}

int32_t AudioCapturerSourceWakeup::UpdateAppsUid(const int32_t appsUid[PA_MAX_OUTPUTS_PER_SOURCE],
    const size_t size)
{
    return audioCapturerSource_.UpdateAppsUid(appsUid, size);
}

int32_t AudioCapturerSourceWakeup::UpdateAppsUid(const std::vector<int32_t> &appsUid)
{
    return audioCapturerSource_.UpdateAppsUid(appsUid);
}
} // namespace AudioStandard
} // namesapce OHOS
