/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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
#ifndef AUDIO_UTILS_H
#define AUDIO_UTILS_H

#include <cstdint>
#include <string>
#include <map>
#include <unordered_map>
#include <mutex>
#include <ctime>
#include <sys/time.h>
#include <atomic>
#include <cstdio>
#include <queue>
#include <climits>
#include <condition_variable>
#include "securec.h"

#include "audio_info.h"

#define AUDIO_MS_PER_SECOND 1000
#define AUDIO_US_PER_SECOND 1000000
#define AUDIO_NS_PER_SECOND ((uint64_t)1000000000)

#define FLOAT_EPS 1e-9f
#define OFFSET_BIT_24 3
#define BIT_DEPTH_TWO 2
#define BIT_8 8
#define BIT_16 16
#define BIT_24 24
#define BIT_32 32
namespace OHOS {
namespace AudioStandard {
const int64_t PCM_MAYBE_SILENT = 1;
const int64_t PCM_MAYBE_NOT_SILENT = 5;
const int32_t SIGNAL_DATA_SIZE = 96;
const int32_t SIGNAL_THRESHOLD = 10;
const int32_t BLANK_THRESHOLD_MS = 100;
const int32_t DETECTED_ZERO_THRESHOLD = 1;
const size_t MILLISECOND_PER_SECOND = 1000;
const size_t MOCK_INTERVAL = 2000;
const int32_t GET_EXTRA_PARAM_LEN = 200;
const int32_t YEAR_BASE = 1900;
const int32_t DECIMAL_EXPONENT = 10;
const size_t DATE_LENGTH = 17;
static uint32_t g_sessionToMock = 0;
const uint32_t MAX_VALUE_OF_SIGNED_24_BIT = 8388607;
const uint32_t STRING_BUFFER_SIZE = 4096;

// Ringer or alarmer dual tone
const size_t AUDIO_CONCURRENT_ACTIVE_DEVICES_LIMIT = 2;

/* Define AudioXcollie timeout flag, consistent with xcollie_define.h in hiviewdfx  */
static constexpr unsigned int AUDIO_XCOLLIE_FLAG_DEFAULT = (~0); // do all callback function
static constexpr unsigned int AUDIO_XCOLLIE_FLAG_NOOP = (0); // do nothing but the caller defined function
static constexpr unsigned int AUDIO_XCOLLIE_FLAG_LOG = (1 << 0); // generate log file
static constexpr unsigned int AUDIO_XCOLLIE_FLAG_RECOVERY = (1 << 1); // die when timeout

class Util {
public:
    static bool IsDualToneStreamType(const AudioStreamType streamType)
    {
        return streamType == STREAM_RING || streamType == STREAM_VOICE_RING || streamType == STREAM_ALARM;
    }

    static bool IsRingerOrAlarmerStreamUsage(const StreamUsage &usage)
    {
        return usage == STREAM_USAGE_ALARM || usage == STREAM_USAGE_VOICE_RINGTONE || usage == STREAM_USAGE_RINGTONE;
    }

    static bool IsRingerAudioScene(const AudioScene &audioScene)
    {
        return audioScene == AUDIO_SCENE_RINGING || audioScene == AUDIO_SCENE_VOICE_RINGING;
    }
};

class Trace {
public:
    static void Count(const std::string &value, int64_t count);
    // Show if data is silent.
    static void CountVolume(const std::string &value, uint8_t data);
    Trace(const std::string &value);
    void End();
    ~Trace();
private:
    std::string value_;
    bool isFinished_;
};

class AudioXCollie {
public:
    AudioXCollie(const std::string &tag, uint32_t timeoutSeconds,
        std::function<void(void *)> func = nullptr, void *arg = nullptr, uint32_t flag = 1);
    ~AudioXCollie();
    void CancelXCollieTimer();
private:
    int32_t id_;
    std::string tag_;
    bool isCanceled_;
};

class ClockTime {
public:
    static int64_t GetCurNano();
    static int32_t AbsoluteSleep(int64_t nanoTime);
    static int32_t RelativeSleep(int64_t nanoTime);
};

class PermissionUtil {
public:
    static bool VerifyIsAudio();
    static bool VerifyIsShell();
    static bool VerifyIsSystemApp();
    static bool VerifySelfPermission();
    static bool VerifySystemPermission();
    static bool VerifyPermission(const std::string &permissionName, uint32_t tokenId);
    static bool NeedVerifyBackgroundCapture(int32_t callingUid, SourceType sourceType);
    static bool VerifyBackgroundCapture(uint32_t tokenId, uint64_t fullTokenId);
    static bool NotifyStart(uint32_t targetTokenId, uint32_t sessionId);
    static bool NotifyStop(uint32_t targetTokenId, uint32_t sessionId);
};

void AdjustStereoToMonoForPCM8Bit(int8_t *data, uint64_t len);
void AdjustStereoToMonoForPCM16Bit(int16_t *data, uint64_t len);
void AdjustStereoToMonoForPCM24Bit(int8_t *data, uint64_t len);
void AdjustStereoToMonoForPCM32Bit(int32_t *data, uint64_t len);
void AdjustAudioBalanceForPCM8Bit(int8_t *data, uint64_t len, float left, float right);
void AdjustAudioBalanceForPCM16Bit(int16_t *data, uint64_t len, float left, float right);
void AdjustAudioBalanceForPCM24Bit(int8_t *data, uint64_t len, float left, float right);
void AdjustAudioBalanceForPCM32Bit(int32_t *data, uint64_t len, float left, float right);

void ConvertFrom24BitToFloat(unsigned n, const uint8_t *a, float *b);
void ConvertFrom32BitToFloat(unsigned n, const int32_t *a, float *b);
void ConvertFromFloatTo24Bit(unsigned n, const float *a, uint8_t *b);
void ConvertFromFloatTo32Bit(unsigned n, const float *a, int32_t *b);

std::string GetEncryptStr(const std::string &str);
std::string ConvertNetworkId(const std::string &networkId);

enum ConvertHdiFormat {
    SAMPLE_U8_C = 0,
    SAMPLE_S16_C = 1,
    SAMPLE_S24_C = 2,
    SAMPLE_S32_C = 3,
    SAMPLE_F32_C = 4,
    INVALID_WIDTH_C = -1
}; // same with HdiAdapterFormat

float UpdateMaxAmplitude(ConvertHdiFormat adapterFormat, char *frame, uint64_t replyBytes);
float CalculateMaxAmplitudeForPCM8Bit(int8_t *frame, uint64_t nSamples);
float CalculateMaxAmplitudeForPCM16Bit(int16_t *frame, uint64_t nSamples);
float CalculateMaxAmplitudeForPCM24Bit(char *frame, uint64_t nSamples);
float CalculateMaxAmplitudeForPCM32Bit(int32_t *frame, uint64_t nSamples);

template <typename T>
bool isEqual(T a, T b, double precision = 0.01)
{
    return std::abs(a - b) < precision;
}

// return true if value is not in the array.
template <typename V>
inline bool NotContain(const std::vector<V> &array, const V &value)
{
    return std::find(array.begin(), array.end(), value) == array.end();
}

template <typename T>
bool GetSysPara(const char *key, T &value);

enum AudioDumpFileType {
    AUDIO_APP = 0,
    OTHER_NATIVE_SERVICE = 1,
    AUDIO_PULSE = 2,
};

const std::string DUMP_SERVER_PARA = "sys.audio.dump.writeserver.enable";
const std::string DUMP_CLIENT_PARA = "sys.audio.dump.writeclient.enable";
const std::string DUMP_PULSE_DIR = "/data/data/.pulse_dir/";
const std::string DUMP_SERVICE_DIR = "/data/local/tmp/";
const std::string DUMP_APP_DIR = "/data/storage/el2/base/cache/";
const std::string DUMP_BLUETOOTH_RENDER_SINK_FILENAME = "dump_bluetooth_audiosink.pcm";
const std::string DUMP_RENDER_SINK_FILENAME = "dump_audiosink.pcm";
const std::string DUMP_MCH_SINK_FILENAME = "dump_mchaudiosink.pcm";
const std::string DUMP_DIRECT_RENDER_SINK_FILENAME = "dump_direct_audiosink.pcm";
const std::string DUMP_OFFLOAD_RENDER_SINK_FILENAME = "dump_offloadaudiosink.pcm";
const std::string DUMP_CAPTURER_SOURCE_FILENAME = "dump_capture_audiosource.pcm";
const std::string DUMP_TONEPLAYER_FILENAME = "dump_toneplayer_audio.pcm";
const std::string DUMP_PROCESS_IN_CLIENT_FILENAME = "dump_process_client_audio.pcm";
const std::string DUMP_REMOTE_RENDER_SINK_FILENAME = "dump_remote_audiosink";
const std::string DUMP_REMOTE_CAPTURE_SOURCE_FILENAME = "dump_remote_capture_audiosource.pcm";
const uint32_t PARAM_VALUE_LENTH = 150;
const std::string BETA_VERSION = "beta";

class DumpFileUtil {
public:
    static void WriteDumpFile(FILE *dumpFile, void *buffer, size_t bufferSize);
    static void CloseDumpFile(FILE **dumpFile);
    static std::map<std::string, std::string> g_lastPara;
    static void OpenDumpFile(std::string para, std::string fileName, FILE **file);
private:
    static FILE *OpenDumpFileInner(std::string para, std::string fileName, AudioDumpFileType fileType);
    static void ChangeDumpFileState(std::string para, FILE **dumpFile, std::string fileName);
};

template <typename...Args>
void AppendFormat(std::string& out, const char* fmt, Args&& ... args)
{
    char buf[STRING_BUFFER_SIZE] = {0};
    int len = ::sprintf_s(buf, sizeof(buf), fmt, args...);
    if (len <= 0) {
        return;
    }
    out += buf;
}

class AudioInfoDumpUtils {
public:
    static const std::string GetStreamName(AudioStreamType streamType);
    static const std::string GetDeviceTypeName(DeviceType deviceType);
    static const std::string GetConnectTypeName(ConnectType connectType);
    static const std::string GetSourceName(SourceType sourceType);
    static const std::string GetDeviceVolumeTypeName(DeviceVolumeType deviceType);
};

class VolumeUtils {
public:
    static AudioVolumeType GetVolumeTypeFromStreamType(AudioStreamType streamType);

private:
    static std::unordered_map<AudioStreamType, AudioVolumeType> defaultVolumeMap_;
    static std::unordered_map<AudioStreamType, AudioVolumeType>& GetVolumeMap();
};

template<typename T>
class ObjectRefMap {
public:
    static std::mutex allObjLock;
    static std::map<T*, uint32_t> refMap;
    static void Insert(T *obj);
    static void Erase(T *obj);
    static T *IncreaseRef(T *obj);
    static void DecreaseRef(T *obj);

    ObjectRefMap(T *obj);
    ~ObjectRefMap();
    T *GetPtr();

private:
    T *obj_ = nullptr;
};

template <typename T>
std::mutex ObjectRefMap<T>::allObjLock;

template <typename T>
std::map<T *, uint32_t> ObjectRefMap<T>::refMap;

template <typename T>
void ObjectRefMap<T>::Insert(T *obj)
{
    std::lock_guard<std::mutex> lock(allObjLock);
    refMap[obj] = 1;
}

template <typename T>
void ObjectRefMap<T>::Erase(T *obj)
{
    std::lock_guard<std::mutex> lock(allObjLock);
    auto it = refMap.find(obj);
    if (it != refMap.end()) {
        refMap.erase(it);
    }
}

template <typename T>
T *ObjectRefMap<T>::IncreaseRef(T *obj)
{
    std::lock_guard<std::mutex> lock(allObjLock);
    if (refMap.count(obj)) {
        refMap[obj]++;
        return obj;
    } else {
        return nullptr;
    }
}

template <typename T>
void ObjectRefMap<T>::DecreaseRef(T *obj)
{
    std::lock_guard<std::mutex> lock(allObjLock);
    if (refMap.count(obj) && --refMap[obj] == 0) {
        refMap.erase(obj);
        delete obj;
        obj = nullptr;
    }
}

template <typename T>
ObjectRefMap<T>::ObjectRefMap(T *obj)
{
    if (obj != nullptr) {
        obj_ = ObjectRefMap::IncreaseRef(obj);
    }
}

template <typename T>
ObjectRefMap<T>::~ObjectRefMap()
{
    if (obj_ != nullptr) {
        ObjectRefMap::DecreaseRef(obj_);
    }
}

template <typename T>
T *ObjectRefMap<T>::GetPtr()
{
    return obj_;
}

std::string GetTime();

int32_t GetFormatByteSize(int32_t format);

struct SignalDetectAgent {
    bool CheckAudioData(uint8_t *buffer, size_t bufferLen);
    bool DetectSignalData(int32_t *buffer, size_t bufferLen);
    void ResetDetectResult();
    int32_t channels_ = STEREO;
    int32_t sampleRate_ = SAMPLE_RATE_48000;
    int32_t sampleFormat_ = SAMPLE_S16LE;
    int32_t formatByteSize_;
    int32_t lastPeakSignal_ = SHRT_MIN;
    int32_t lastPeakSignalPos_ = 0;
    int32_t blankPeriod_ = 0;
    size_t frameCountIgnoreChannel_;
    bool hasFirstNoneZero_ = false;
    bool blankHaveOutput_ = true;
    bool dspTimestampGot_ = false;
    bool signalDetected_ = false;
    std::string lastPeakBufferTime_ = "";
    std::vector<int32_t> cacheAudioData_;
};

class AudioLatencyMeasurement {
public:
    // static methods, invoked without instantiation in sinks and sources
    static bool CheckIfEnabled();
    AudioLatencyMeasurement(const int32_t &sampleRate, const int32_t &channelCount,
        const int32_t &sampleFormat, const std::string &appName, const uint32_t &sessionId);
    ~AudioLatencyMeasurement();

    // non-static methods, invoked after instantiation in AudioRenderer and AudioCapturer
    void InitSignalData();
    bool MockPcmData(uint8_t *buffer, size_t bufferLen); // mute data and insert signal data
private:
    int32_t format_ = SAMPLE_S16LE;
    int32_t formatByteSize_;
    int32_t sampleRate_;
    int32_t channelCount_;
    uint32_t sessionId_;
    size_t mockedTime_ = 0;
    bool mockThisStream_ = false;
    std::string appName_;
    std::unique_ptr<int16_t[]> signalData_ = nullptr;
};

class LatencyMonitor {
public:
    static LatencyMonitor& GetInstance();
    void ShowTimestamp(bool isRenderer);
    void ShowBluetoothTimestamp();
    void UpdateClientTime(bool isRenderer, std::string &timestamp);
    void UpdateSinkOrSourceTime(bool isRenderer, std::string &timestamp);
    void UpdateDspTime(std::string dspTime);
private:
    std::string rendererMockTime_ = "";
    std::string sinkDetectedTime_ = "";
    std::string dspDetectedTime_ = "";
    std::string capturerDetectedTime_ = "";
    std::string sourceDetectedTime_ = "";
    std::string dspBeforeSmartPa_ = "";
    std::string dspAfterSmartPa_ = "";
    std::string dspMockTime_ = "";
    size_t extraStrLen_ = 0;
};

class AudioDump {
public:
    static AudioDump& GetInstance();
    void SetVersionType(const std::string& versionType);
    std::string GetVersionType();
private:
    AudioDump() {}
    ~AudioDump() {}
    std::string versionType_ = "commercial";
};

template <typename EnumType, typename V>
int32_t GetKeyFromValue(const std::unordered_map<EnumType, V> &map, const V &value)
{
    for (auto it : map) {
        if (it.second == value) {
            return it.first;
        }
    }
    return -1;
}

template <typename T, typename Compare>
bool CasWithCompare(std::atomic<T> &atomicVar, T newValue, Compare compare)
{
    T old = atomicVar;
    do {
        if (!compare(old, newValue)) {
            return false;
        }
    } while (!atomicVar.compare_exchange_weak(old, newValue));

    return true;
}

/**
 * @brief Provides interfaces for thread-safe blocking queues.
 *
 * The interfaces can be used to perform blocking and non-blocking push and
 * pop operations on queues.
 */
template <typename T>
class AudioSafeBlockQueue {
public:
    explicit AudioSafeBlockQueue(int capacity) : maxSize_(capacity)
    {
    }

    /**
     * @brief Inserts an element at the end of this queue in blocking mode.
     *
     * If the queue is full, the thread of the push operation will be blocked
     * until the queue has space.
     * If the queue is not full, the push operation can be performed and one of the
     * pop threads (blocked when the queue is empty) is woken up.
     *
     * @param elem Indicates the element to insert.
     */
    virtual void Push(T const& elem)
    {
        std::unique_lock<std::mutex> lock(mutexLock_);
        while (queueT_.size() >= maxSize_) {
            // If the queue is full, wait for jobs to be taken.
            cvNotFull_.wait(lock, [&]() { return (queueT_.size() < maxSize_); });
        }

        // Insert the element into the queue if the queue is not full.
        queueT_.push(elem);
        cvNotEmpty_.notify_all();
    }

    /**
     * @brief Removes the first element from this queue in blocking mode.
     *
     * If the queue is empty, the thread of the pop operation will be blocked
     * until the queue has elements.
     * If the queue is not empty, the pop operation can be performed, the first
     * element of the queue is returned, and one of the push threads (blocked
     * when the queue is full) is woken up.
     */
    T Pop()
    {
        std::unique_lock<std::mutex> lock(mutexLock_);

        while (queueT_.empty()) {
            // If the queue is empty, wait for elements to be pushed in.
            cvNotEmpty_.wait(lock, [&] { return !queueT_.empty(); });
        }

        T elem = queueT_.front();
        queueT_.pop();
        cvNotFull_.notify_all();
        return elem;
    }

    /**
     * @brief Inserts an element at the end of this queue in non-blocking mode.
     *
     * If the queue is full, <b>false</b> is returned directly.
     * If the queue is not full, the push operation can be performed, one of the
     * pop threads (blocked when the queue is empty) is woken up, and <b>true</b>
     * is returned.
     *
     * @param elem Indicates the element to insert.
     */
    virtual bool PushNoWait(T const& elem)
    {
        std::unique_lock<std::mutex> lock(mutexLock_);
        if (queueT_.size() >= maxSize_) {
            return false;
        }
        // Insert the element if the queue is not full.
        queueT_.push(elem);
        cvNotEmpty_.notify_all();
        return true;
    }

    /**
     * @brief Removes the first element from this queue in non-blocking mode.
     *
     * If the queue is empty, <b>false</b> is returned directly.
     * If the queue is not empty, the pop operation can be performed, one of the
     * push threads (blocked when the queue is full) is woken up, and <b>true</b>
     * is returned.
     *
     * @param outtask Indicates the data of the pop operation.
     */
    bool PopNotWait(T& outtask)
    {
        std::unique_lock<std::mutex> lock(mutexLock_);
        if (queueT_.empty()) {
            return false;
        }
        outtask = queueT_.front();
        queueT_.pop();

        cvNotFull_.notify_all();

        return true;
    }

    std::queue<T> PopAllNotWait()
    {
        std::queue<T> retQueue = {};
        std::unique_lock<std::mutex> lock(mutexLock_);
        retQueue.swap(queueT_);

        cvNotFull_.notify_all();

        return retQueue;
    }

    unsigned int Size()
    {
        std::unique_lock<std::mutex> lock(mutexLock_);
        return queueT_.size();
    }

    template< class Rep, class Period >
    void WaitNotEmptyFor(const std::chrono::duration<Rep, Period>& rel_time)
    {
        std::unique_lock<std::mutex> lock(mutexLock_);
        cvNotEmpty_.wait_for(lock, rel_time, [this] {
            return !queueT_.empty();
        });
    }

    bool IsEmpty()
    {
        std::unique_lock<std::mutex> lock(mutexLock_);
        return queueT_.empty();
    }

    bool IsFull()
    {
        std::unique_lock<std::mutex> lock(mutexLock_);
        return queueT_.size() == maxSize_;
    }

    void Clear()
    {
        std::unique_lock<std::mutex> lock(mutexLock_);
        queueT_ = {};
        cvNotFull_.notify_all();
    }

    virtual ~AudioSafeBlockQueue() {}

protected:
    unsigned long maxSize_;  // Capacity of the queue
    std::mutex mutexLock_;
    std::condition_variable cvNotEmpty_;
    std::condition_variable cvNotFull_;
    std::queue<T> queueT_;
};

enum AudioHdiUniqueIDBase : uint32_t {
    // 0-4 is reserved for other modules
    AUDIO_HDI_RENDER_ID_BASE = 5,
    AUDIO_HDI_CAPTURE_ID_BASE = 6,
};

enum HdiCaptureOffset : uint32_t {
    HDI_CAPTURE_OFFSET_PRIMARY = 1,
    HDI_CAPTURE_OFFSET_FAST = 2,
    HDI_CAPTURE_OFFSET_REMOTE = 3,
    HDI_CAPTURE_OFFSET_REMOTE_FAST = 4,
    HDI_CAPTURE_OFFSET_USB = 5,
    HDI_CAPTURE_OFFSET_EC = 6,
    HDI_CAPTURE_OFFSET_MIC_REF = 7,
    HDI_CAPTURE_OFFSET_WAKEUP = 8,
};

enum HdiRenderOffset : uint32_t {
    HDI_RENDER_OFFSET_PRIMARY = 1,
    HDI_RENDER_OFFSET_FAST = 2,
    HDI_RENDER_OFFSET_REMOTE = 3,
    HDI_RENDER_OFFSET_REMOTE_FAST = 4,
    HDI_RENDER_OFFSET_BLUETOOTH = 5,
    HDI_RENDER_OFFSET_OFFLOAD = 6,
    HDI_RENDER_OFFSET_MULTICHANNEL = 7,
    HDI_RENDER_OFFSET_DIRECT = 8,
    HDI_RENDER_OFFSET_VOIP = 9,
    HDI_RENDER_OFFSET_DP = 10,
    HDI_RENDER_OFFSET_USB = 11,
    HDI_RENDER_OFFSET_VOIP_FAST = 12,
};

uint32_t GenerateUniqueID(AudioHdiUniqueIDBase base, uint32_t offset);
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_UTILS_H
