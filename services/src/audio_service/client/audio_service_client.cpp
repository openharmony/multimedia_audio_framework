/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "audio_service_client.h"

#include <fstream>

#include "iservice_registry.h"
#include "media_log.h"
#include "securec.h"
#include "system_ability_definition.h"
#include "unistd.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
AudioRendererCallbacks::~AudioRendererCallbacks() = default;
AudioCapturerCallbacks::~AudioCapturerCallbacks() = default;

#ifdef PRODUCT_M40
const uint64_t LATENCY_IN_MSEC = 45UL;
#else
const uint64_t LATENCY_IN_MSEC = 50UL;
#endif
const uint32_t READ_TIMEOUT_IN_SEC = 5;
const uint32_t DOUBLE_VALUE = 2;
const uint32_t MAX_LENGTH_FACTOR = 5;
const uint32_t T_LENGTH_FACTOR = 4;
const uint64_t MIN_BUF_DURATION_IN_USEC = 92880;

const string PATH_SEPARATOR = "/";
const string COOKIE_FILE_NAME = "cookie";

#define CHECK_AND_RETURN_IFINVALID(expr) \
do {                                     \
    if (!(expr)) {                       \
        return AUDIO_CLIENT_ERR;         \
    }                                    \
} while (false)

#define CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, error) \
do {                                                                    \
    if (!context || !paStream || !mainLoop                              \
        || !PA_CONTEXT_IS_GOOD(pa_context_get_state(context))           \
        || !PA_STREAM_IS_GOOD(pa_stream_get_state(paStream))) {         \
        return error;                                                   \
    }                                                                   \
} while (false)

#define CHECK_PA_STATUS_FOR_WRITE(mainLoop, context, paStream, pError, retVal) \
do {                                                                           \
    if (!context || !paStream || !mainLoop                                     \
        || !PA_CONTEXT_IS_GOOD(pa_context_get_state(context))                  \
        || !PA_STREAM_IS_GOOD(pa_stream_get_state(paStream))) {                \
            pError = pa_context_errno(context);                                \
        return retVal;                                                         \
    }                                                                          \
} while (false)

AudioStreamParams AudioServiceClient::ConvertFromPAAudioParams(pa_sample_spec paSampleSpec)
{
    AudioStreamParams audioParams;

    audioParams.channels = paSampleSpec.channels;
    audioParams.samplingRate = paSampleSpec.rate;
    audioParams.encoding = ENCODING_PCM;

    switch (paSampleSpec.format) {
        case PA_SAMPLE_U8:
            audioParams.format = SAMPLE_U8;
            break;
        case PA_SAMPLE_S16LE:
            audioParams.format = SAMPLE_S16LE;
            break;
        case PA_SAMPLE_S24LE:
            audioParams.format = SAMPLE_S24LE;
            break;
        case PA_SAMPLE_S32LE:
            audioParams.format = SAMPLE_S32LE;
            break;
        default:
            audioParams.format = INVALID_WIDTH;
            break;
    }

    return audioParams;
}

pa_sample_spec AudioServiceClient::ConvertToPAAudioParams(AudioStreamParams audioParams)
{
    pa_sample_spec paSampleSpec;

    paSampleSpec.channels = audioParams.channels;
    paSampleSpec.rate     = audioParams.samplingRate;

    switch ((AudioSampleFormat)audioParams.format) {
        case SAMPLE_U8:
            paSampleSpec.format = (pa_sample_format_t)PA_SAMPLE_U8;
            break;
        case SAMPLE_S16LE:
            paSampleSpec.format = (pa_sample_format_t)PA_SAMPLE_S16LE;
            break;
        case SAMPLE_S24LE:
            paSampleSpec.format = (pa_sample_format_t)PA_SAMPLE_S24LE;
            break;
        case SAMPLE_S32LE:
            paSampleSpec.format = (pa_sample_format_t)PA_SAMPLE_S32LE;
            break;
        default:
            paSampleSpec.format = (pa_sample_format_t)PA_SAMPLE_INVALID;
            break;
    }

    return paSampleSpec;
}

static size_t AlignToAudioFrameSize(size_t l, const pa_sample_spec &ss)
{
    size_t fs = pa_frame_size(&ss);
    if (fs == 0) {
        MEDIA_ERR_LOG(" Error: pa_frame_size returned  0");
        return 0;
    }

    return (l / fs) * fs;
}

void AudioServiceClient::PAStreamStartSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    if (!userdata) {
        MEDIA_ERR_LOG("AudioServiceClient::PAStreamStartSuccessCb: userdata is null");
        return;
    }

    AudioServiceClient *asClient = static_cast<AudioServiceClient *>(userdata);
    pa_threaded_mainloop *mainLoop = static_cast<pa_threaded_mainloop *>(asClient->mainLoop);

    asClient->state_ = RUNNING;
    std::shared_ptr<AudioStreamCallback> streamCb = asClient->streamCallback_.lock();
    if (streamCb != nullptr) {
        streamCb->OnStateChange(asClient->state_);
    }
    asClient->streamCmdStatus = success;
    pa_threaded_mainloop_signal(mainLoop, 0);
}

void AudioServiceClient::PAStreamStopSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    if (!userdata) {
        MEDIA_ERR_LOG("AudioServiceClient::PAStreamStopSuccessCb: userdata is null");
        return;
    }

    AudioServiceClient *asClient = static_cast<AudioServiceClient *>(userdata);
    pa_threaded_mainloop *mainLoop = static_cast<pa_threaded_mainloop *>(asClient->mainLoop);

    asClient->state_ = STOPPED;
    std::shared_ptr<AudioStreamCallback> streamCb = asClient->streamCallback_.lock();
    if (streamCb != nullptr) {
        streamCb->OnStateChange(asClient->state_);
    }
    asClient->streamCmdStatus = success;
    pa_threaded_mainloop_signal(mainLoop, 0);
}

void AudioServiceClient::PAStreamPauseSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    if (!userdata) {
        MEDIA_ERR_LOG("AudioServiceClient::PAStreamPauseSuccessCb: userdata is null");
        return;
    }

    AudioServiceClient *asClient = static_cast<AudioServiceClient *>(userdata);
    pa_threaded_mainloop *mainLoop = static_cast<pa_threaded_mainloop *>(asClient->mainLoop);

    asClient->state_ = PAUSED;
    std::shared_ptr<AudioStreamCallback> streamCb = asClient->streamCallback_.lock();
    if (streamCb != nullptr) {
        streamCb->OnStateChange(asClient->state_);
    }
    asClient->streamCmdStatus = success;
    pa_threaded_mainloop_signal(mainLoop, 0);
}

void AudioServiceClient::PAStreamDrainSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    if (!userdata) {
        MEDIA_ERR_LOG("AudioServiceClient::PAStreamDrainSuccessCb: userdata is null");
        return;
    }

    AudioServiceClient *asClient = (AudioServiceClient *)userdata;
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *)asClient->mainLoop;

    asClient->streamDrainStatus = success;
    pa_threaded_mainloop_signal(mainLoop, 0);
}

void AudioServiceClient::PAStreamFlushSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    if (!userdata) {
        MEDIA_ERR_LOG("AudioServiceClient::PAStreamFlushSuccessCb: userdata is null");
        return;
    }
    AudioServiceClient *asClient = (AudioServiceClient *)userdata;
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *)asClient->mainLoop;

    asClient->streamFlushStatus = success;
    pa_threaded_mainloop_signal(mainLoop, 0);
}

void AudioServiceClient::PAStreamReadCb(pa_stream *stream, size_t length, void *userdata)
{
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *)userdata;
    pa_threaded_mainloop_signal(mainLoop, 0);
}

void AudioServiceClient::PAStreamSetBufAttrSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    if (!userdata) {
        MEDIA_ERR_LOG("AudioServiceClient::PAStreamSetBufAttrSuccessCb: userdata is null");
        return;
    }
    AudioServiceClient *asClient = (AudioServiceClient *)userdata;
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *)asClient->mainLoop;

    MEDIA_DEBUG_LOG("AAudioServiceClient::PAStreamSetBufAttrSuccessCb is called");
    if (!success) {
        MEDIA_ERR_LOG("AAudioServiceClient::PAStreamSetBufAttrSuccessCb SetBufAttr failed");
    } else {
        MEDIA_ERR_LOG("AAudioServiceClient::PAStreamSetBufAttrSuccessCb SetBufAttr success");
    }
    pa_threaded_mainloop_signal(mainLoop, 0);
}

int32_t AudioServiceClient::SetAudioRenderMode(AudioRenderMode renderMode)
{
    MEDIA_DEBUG_LOG("AudioServiceClient::SetAudioRenderMode begin");
    renderMode_ = renderMode;

    if (renderMode_ != RENDER_MODE_CALLBACK) {
        return AUDIO_CLIENT_SUCCESS;
    }

    CHECK_AND_RETURN_IFINVALID(mainLoop && context && paStream);
    pa_threaded_mainloop_lock(mainLoop);
    pa_buffer_attr bufferAttr;
    bufferAttr.fragsize = static_cast<uint32_t>(-1);
    bufferAttr.prebuf = AlignToAudioFrameSize(pa_usec_to_bytes(MIN_BUF_DURATION_IN_USEC, &sampleSpec), sampleSpec);
    bufferAttr.maxlength = static_cast<uint32_t>(-1);
    bufferAttr.tlength = static_cast<uint32_t>(-1);
    bufferAttr.minreq = bufferAttr.prebuf;
    pa_operation *operation = pa_stream_set_buffer_attr(paStream, &bufferAttr,
        PAStreamSetBufAttrSuccessCb, (void *)this);
        while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainLoop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mainLoop);

    MEDIA_DEBUG_LOG("AudioServiceClient::SetAudioRenderMode end");

    return AUDIO_CLIENT_SUCCESS;
}

AudioRenderMode AudioServiceClient::GetAudioRenderMode()
{
    return renderMode_;
}

int32_t AudioServiceClient::SaveWriteCallback(const std::weak_ptr<AudioRendererWriteCallback> &callback)
{
    if (callback.lock() == nullptr) {
        MEDIA_ERR_LOG("AudioServiceClient::SaveWriteCallback callback == nullptr");
        return AUDIO_CLIENT_INIT_ERR;
    }
    writeCallback_ = callback;

    return AUDIO_CLIENT_SUCCESS;
}

void AudioServiceClient::PAStreamWriteCb(pa_stream *stream, size_t length, void *userdata)
{
    MEDIA_INFO_LOG("AudioServiceClient::Inside PA write callback");
    if (!userdata) {
        MEDIA_ERR_LOG("AudioServiceClient::PAStreamWriteCb: userdata is null");
        return;
    }

    auto asClient = static_cast<AudioServiceClient *>(userdata);
    auto mainLoop = static_cast<pa_threaded_mainloop *>(asClient->mainLoop);
    pa_threaded_mainloop_signal(mainLoop, 0);

    if (asClient->renderMode_ != RENDER_MODE_CALLBACK) {
        return;
    }

    std::shared_ptr<AudioRendererWriteCallback> cb = asClient->writeCallback_.lock();
    if (cb != nullptr) {
        size_t requestSize;
        asClient->GetMinimumBufferSize(requestSize);
        MEDIA_INFO_LOG("AudioServiceClient::PAStreamWriteCb: cb != nullptr firing OnWriteData");
        MEDIA_INFO_LOG("AudioServiceClient::OnWriteData requestSize : %{public}zu", requestSize);
        cb->OnWriteData(requestSize);
    } else {
        MEDIA_ERR_LOG("AudioServiceClient::PAStreamWriteCb: cb == nullptr not firing OnWriteData");
    }
}

void AudioServiceClient::PAStreamUnderFlowCb(pa_stream *stream, void *userdata)
{
    if (!userdata) {
        MEDIA_ERR_LOG("AudioServiceClient::PAStreamUnderFlowCb: userdata is null");
        return;
    }

    AudioServiceClient *asClient = (AudioServiceClient *)userdata;
    asClient->underFlowCount++;
}

void AudioServiceClient::PAStreamLatencyUpdateCb(pa_stream *stream, void *userdata)
{
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *)userdata;
    pa_threaded_mainloop_signal(mainLoop, 0);
}

void AudioServiceClient::PAStreamStateCb(pa_stream *stream, void *userdata)
{
    if (!userdata) {
        MEDIA_ERR_LOG("AudioServiceClient::PAStreamStateCb: userdata is null");
        return;
    }

    AudioServiceClient *asClient = (AudioServiceClient *)userdata;
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *)asClient->mainLoop;

    if (asClient->mAudioRendererCallbacks)
        asClient->mAudioRendererCallbacks->OnStreamStateChangeCb();

    MEDIA_INFO_LOG("Current Stream State: %d", pa_stream_get_state(stream));

    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_READY:
        case PA_STREAM_FAILED:
        case PA_STREAM_TERMINATED:
            pa_threaded_mainloop_signal(mainLoop, 0);
            break;

        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        default:
            break;
    }
}

void AudioServiceClient::PAContextStateCb(pa_context *context, void *userdata)
{
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *)userdata;

    MEDIA_INFO_LOG("Current Context State: %d", pa_context_get_state(context));

    switch (pa_context_get_state(context)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(mainLoop, 0);
            break;

        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
        default:
            break;
    }
}

AudioServiceClient::AudioServiceClient()
{
    isMainLoopStarted = false;
    isContextConnected = false;
    isStreamConnected = false;

    sinkDevices.clear();
    sourceDevices.clear();
    sinkInputs.clear();
    sourceOutputs.clear();
    clientInfo.clear();

    mVolumeFactor = 1.0f;
    mStreamType = STREAM_MUSIC;
    mAudioSystemMgr = nullptr;

    streamIndex = 0;
    volumeChannels = STEREO;
    streamInfoUpdated = false;

    renderRate = RENDER_RATE_NORMAL;
    renderMode_ = RENDER_MODE_NORMAL;

    eAudioClientType = AUDIO_SERVICE_CLIENT_PLAYBACK;

    mFrameSize = 0;
    mFrameMarkPosition = 0;
    mMarkReached = false;
    mFramePeriodNumber = 0;

    mTotalBytesWritten = 0;
    mFramePeriodWritten = 0;
    mTotalBytesRead = 0;
    mFramePeriodRead = 0;
    mRenderPositionCb = nullptr;
    mRenderPeriodPositionCb = nullptr;

    mAudioRendererCallbacks = nullptr;
    mAudioCapturerCallbacks = nullptr;
    internalReadBuffer = nullptr;
    mainLoop = nullptr;
    paStream = nullptr;
    context  = nullptr;
    api = nullptr;

    internalRdBufIndex = 0;
    internalRdBufLen = 0;
    streamCmdStatus = 0;
    streamDrainStatus = 0;
    streamFlushStatus = 0;
    underFlowCount = 0;

    acache.readIndex = 0;
    acache.writeIndex = 0;
    acache.isFull = false;
    acache.totalCacheSize = 0;
    acache.buffer = nullptr;

    setBufferSize = 0;
    PAStreamCorkSuccessCb = PAStreamStopSuccessCb;
}

void AudioServiceClient::ResetPAAudioClient()
{
    lock_guard<mutex> lock(ctrlMutex);
    if (mainLoop && (isMainLoopStarted == true))
        pa_threaded_mainloop_stop(mainLoop);

    if (paStream) {
        pa_stream_set_state_callback(paStream, nullptr, nullptr);
        pa_stream_set_write_callback(paStream, nullptr, nullptr);
        pa_stream_set_read_callback(paStream, nullptr, nullptr);
        pa_stream_set_latency_update_callback(paStream, nullptr, nullptr);
        pa_stream_set_underflow_callback(paStream, nullptr, nullptr);

        if (isStreamConnected == true)
            pa_stream_disconnect(paStream);
        pa_stream_unref(paStream);
    }

    if (context) {
        pa_context_set_state_callback(context, nullptr, nullptr);
        if (isContextConnected == true)
            pa_context_disconnect(context);
        pa_context_unref(context);
    }

    if (mainLoop)
        pa_threaded_mainloop_free(mainLoop);

    for (auto &thread : mPositionCBThreads) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }

    for (auto &thread : mPeriodPositionCBThreads) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }

    if (appCookiePath.compare("")) {
        remove(appCookiePath.c_str());
        appCookiePath = "";
    }

    isMainLoopStarted  = false;
    isContextConnected = false;
    isStreamConnected  = false;

    sinkDevices.clear();
    sourceDevices.clear();
    sinkInputs.clear();
    sourceOutputs.clear();
    clientInfo.clear();

    mAudioRendererCallbacks = nullptr;
    mAudioCapturerCallbacks = nullptr;
    internalReadBuffer      = nullptr;

    mainLoop = nullptr;
    paStream = nullptr;
    context  = nullptr;
    api      = nullptr;

    internalRdBufIndex = 0;
    internalRdBufLen   = 0;
    underFlowCount     = 0;

    acache.buffer = nullptr;
    acache.readIndex = 0;
    acache.writeIndex = 0;
    acache.isFull = false;
    acache.totalCacheSize = 0;

    setBufferSize = 0;
    PAStreamCorkSuccessCb = nullptr;
}

AudioServiceClient::~AudioServiceClient()
{
    ResetPAAudioClient();
}

void AudioServiceClient::SetEnv()
{
    MEDIA_INFO_LOG("SetEnv called");
    int ret = 0;
    const char *env_home_pa = getenv("HOME");
    if (!env_home_pa) {
        ret = setenv("HOME", PA_HOME_DIR, 1);
        MEDIA_INFO_LOG("set env HOME: %{public}d", ret);
    }

    const char *env_runtime_pa = getenv("PULSE_RUNTIME_PATH");
    if (!env_runtime_pa) {
        ret = setenv("PULSE_RUNTIME_PATH", PA_RUNTIME_DIR, 1);
        MEDIA_INFO_LOG("set env PULSE_RUNTIME_DIR: %{public}d", ret);
    }

    const char *env_state_pa = getenv("PULSE_STATE_PATH");
    if (!env_state_pa) {
        ret = setenv("PULSE_STATE_PATH", PA_STATE_DIR, 1);
        MEDIA_INFO_LOG("set env PULSE_STATE_PATH: %{public}d", ret);
    }
}

void AudioServiceClient::SetApplicationCachePath(const std::string cachePath)
{
    MEDIA_DEBUG_LOG("SetApplicationCachePath in");
    cachePath_ = cachePath;
}

int32_t AudioServiceClient::Initialize(ASClientType eClientType)
{
    int error = PA_ERR_INTERNAL;
    eAudioClientType = eClientType;

    mMarkReached = false;
    mTotalBytesWritten = 0;
    mFramePeriodWritten = 0;
    mTotalBytesRead = 0;
    mFramePeriodRead = 0;

    SetEnv();
    mAudioSystemMgr = AudioSystemManager::GetInstance();

    mainLoop = pa_threaded_mainloop_new();
    if (mainLoop == nullptr)
        return AUDIO_CLIENT_INIT_ERR;

    api = pa_threaded_mainloop_get_api(mainLoop);
    if (api == nullptr) {
        ResetPAAudioClient();
        return AUDIO_CLIENT_INIT_ERR;
    }

    context = pa_context_new(api, "AudioServiceClient");
    if (context == nullptr) {
        ResetPAAudioClient();
        return AUDIO_CLIENT_INIT_ERR;
    }

    pa_context_set_state_callback(context, PAContextStateCb, mainLoop);

    if (!cachePath_.empty()) {
        MEDIA_DEBUG_LOG("abilityContext not null");
        int32_t size = 0;
        const char *cookieData = mAudioSystemMgr->RetrieveCookie(size);
        if (size <= 0) {
            MEDIA_ERR_LOG("Error retrieving cookie");
            return AUDIO_CLIENT_INIT_ERR;
        }

        appCookiePath = cachePath_ + PATH_SEPARATOR + COOKIE_FILE_NAME;

        ofstream cookieCache(appCookiePath.c_str(), std::ofstream::binary);
        cookieCache.write(cookieData, size);
        cookieCache.flush();
        cookieCache.close();

        pa_context_load_cookie_from_file(context, appCookiePath.c_str());
    }

    if (pa_context_connect(context, nullptr, PA_CONTEXT_NOFAIL, nullptr) < 0) {
        error = pa_context_errno(context);
        MEDIA_ERR_LOG("context connect error: %{public}s", pa_strerror(error));
        ResetPAAudioClient();
        return AUDIO_CLIENT_INIT_ERR;
    }

    isContextConnected = true;
    pa_threaded_mainloop_lock(mainLoop);

    if (pa_threaded_mainloop_start(mainLoop) < 0) {
        pa_threaded_mainloop_unlock(mainLoop);
        ResetPAAudioClient();
        return AUDIO_CLIENT_INIT_ERR;
    }

    isMainLoopStarted = true;
    while (true) {
        pa_context_state_t state = pa_context_get_state(context);
        if (state == PA_CONTEXT_READY)
            break;

        if (!PA_CONTEXT_IS_GOOD(state)) {
            error = pa_context_errno(context);
            MEDIA_ERR_LOG("context bad state error: %{public}s", pa_strerror(error));
            pa_threaded_mainloop_unlock(mainLoop);
            ResetPAAudioClient();
            return AUDIO_CLIENT_INIT_ERR;
        }

        pa_threaded_mainloop_wait(mainLoop);
    }

    if (appCookiePath.compare("")) {
        remove(appCookiePath.c_str());
        appCookiePath = "";
    }

    pa_threaded_mainloop_unlock(mainLoop);
    return AUDIO_CLIENT_SUCCESS;
}

const std::string AudioServiceClient::GetStreamName(AudioStreamType audioType)
{
    std::string name;
    switch (audioType) {
        case STREAM_VOICE_ASSISTANT:
            name = "voice_assistant";
            break;
        case STREAM_VOICE_CALL:
            name = "voice_call";
            break;
        case STREAM_SYSTEM:
            name = "system";
            break;
        case STREAM_RING:
            name = "ring";
            break;
        case STREAM_MUSIC:
            name = "music";
            break;
        case STREAM_ALARM:
            name = "alarm";
            break;
        case STREAM_NOTIFICATION:
            name = "notification";
            break;
        case STREAM_BLUETOOTH_SCO:
            name = "bluetooth_sco";
            break;
        case STREAM_DTMF:
            name = "dtmf";
            break;
        case STREAM_TTS:
            name = "tts";
            break;
        case STREAM_ACCESSIBILITY:
            name = "accessibility";
            break;
        default:
            name = "unknown";
    }

    const std::string streamName = name;
    return streamName;
}

int32_t AudioServiceClient::ConnectStreamToPA()
{
    int error, result;

    CHECK_AND_RETURN_IFINVALID(mainLoop && context && paStream);
    pa_threaded_mainloop_lock(mainLoop);

    pa_buffer_attr bufferAttr;
    bufferAttr.fragsize = static_cast<uint32_t>(-1);

    bufferAttr.prebuf = pa_usec_to_bytes(LATENCY_IN_MSEC * PA_USEC_PER_MSEC, &sampleSpec);
    bufferAttr.maxlength = pa_usec_to_bytes(LATENCY_IN_MSEC * PA_USEC_PER_MSEC * MAX_LENGTH_FACTOR, &sampleSpec);
    bufferAttr.tlength = pa_usec_to_bytes(LATENCY_IN_MSEC * PA_USEC_PER_MSEC * T_LENGTH_FACTOR, &sampleSpec);
    bufferAttr.minreq = pa_usec_to_bytes(LATENCY_IN_MSEC * PA_USEC_PER_MSEC, &sampleSpec);

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_PLAYBACK)
        result = pa_stream_connect_playback(paStream, nullptr, &bufferAttr,
                                            (pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY
                                            | PA_STREAM_INTERPOLATE_TIMING
                                            | PA_STREAM_START_CORKED
                                            | PA_STREAM_AUTO_TIMING_UPDATE
                                            | PA_STREAM_VARIABLE_RATE), nullptr, nullptr);
    else
        result = pa_stream_connect_record(paStream, nullptr, nullptr,
                                          (pa_stream_flags_t)(PA_STREAM_INTERPOLATE_TIMING
                                          | PA_STREAM_ADJUST_LATENCY
                                          | PA_STREAM_START_CORKED
                                          | PA_STREAM_AUTO_TIMING_UPDATE));

    if (result < 0) {
        error = pa_context_errno(context);
        MEDIA_ERR_LOG("connection to stream error: %{public}d", error);
        pa_threaded_mainloop_unlock(mainLoop);
        ResetPAAudioClient();
        return AUDIO_CLIENT_CREATE_STREAM_ERR;
    }

    while (true) {
        pa_stream_state_t state = pa_stream_get_state(paStream);
        if (state == PA_STREAM_READY)
            break;

        if (!PA_STREAM_IS_GOOD(state)) {
            error = pa_context_errno(context);
            pa_threaded_mainloop_unlock(mainLoop);
            MEDIA_ERR_LOG("connection to stream error: %{public}d", error);
            ResetPAAudioClient();
            return AUDIO_CLIENT_CREATE_STREAM_ERR;
        }

        pa_threaded_mainloop_wait(mainLoop);
    }

    isStreamConnected = true;
    pa_threaded_mainloop_unlock(mainLoop);
    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::InitializeAudioCache()
{
    MEDIA_INFO_LOG("Initializing internal audio cache");
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);

    const pa_buffer_attr *bufferAttr = pa_stream_get_buffer_attr(paStream);
    if (bufferAttr == nullptr) {
        MEDIA_ERR_LOG("pa stream get buffer attribute returned null");
        return AUDIO_CLIENT_INIT_ERR;
    }

    acache.buffer = make_unique<uint8_t[]>(bufferAttr->minreq);
    if (acache.buffer == nullptr) {
        MEDIA_ERR_LOG("Allocate memory for buffer failed");
        return AUDIO_CLIENT_INIT_ERR;
    }

    acache.readIndex = 0;
    acache.writeIndex = 0;
    acache.totalCacheSize = bufferAttr->minreq;
    acache.isFull = false;
    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::CreateStream(AudioStreamParams audioParams, AudioStreamType audioType)
{
    int error;

    CHECK_AND_RETURN_IFINVALID(mainLoop && context);

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_CONTROLLER) {
        return AUDIO_CLIENT_INVALID_PARAMS_ERR;
    }

    pa_threaded_mainloop_lock(mainLoop);
    mStreamType = audioType;
    const std::string streamName = GetStreamName(audioType);

    auto timenow = chrono::system_clock::to_time_t(chrono::system_clock::now());
    const std::string streamStartTime = ctime(&timenow);

    sampleSpec = ConvertToPAAudioParams(audioParams);
    mFrameSize = pa_frame_size(&sampleSpec);

    pa_proplist *propList = pa_proplist_new();
    if (propList == nullptr) {
        MEDIA_ERR_LOG("pa_proplist_new failed");
        ResetPAAudioClient();
        return AUDIO_CLIENT_CREATE_STREAM_ERR;
    }

    pa_proplist_sets(propList, "stream.type", streamName.c_str());
    pa_proplist_sets(propList, "stream.volumeFactor", std::to_string(mVolumeFactor).c_str());
    pa_proplist_sets(propList, "stream.sessionID", std::to_string(pa_context_get_index(context)).c_str());
    pa_proplist_sets(propList, "stream.startTime", streamStartTime.c_str());

    if (!(paStream = pa_stream_new_with_proplist(context, streamName.c_str(), &sampleSpec, nullptr, propList))) {
        error = pa_context_errno(context);
        pa_proplist_free(propList);
        pa_threaded_mainloop_unlock(mainLoop);
        ResetPAAudioClient();
        return AUDIO_CLIENT_CREATE_STREAM_ERR;
    }

    pa_proplist_free(propList);
    pa_stream_set_state_callback(paStream, PAStreamStateCb, (void *)this);
    pa_stream_set_write_callback(paStream, PAStreamWriteCb, (void *)this);
    pa_stream_set_read_callback(paStream, PAStreamReadCb, mainLoop);
    pa_stream_set_latency_update_callback(paStream, PAStreamLatencyUpdateCb, mainLoop);
    pa_stream_set_underflow_callback(paStream, PAStreamUnderFlowCb, (void *)this);

    pa_threaded_mainloop_unlock(mainLoop);

    error = ConnectStreamToPA();
    if (error < 0) {
        MEDIA_ERR_LOG("Create Stream Failed");
        ResetPAAudioClient();
        return AUDIO_CLIENT_CREATE_STREAM_ERR;
    }

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_PLAYBACK) {
        error = InitializeAudioCache();
        if (error < 0) {
            MEDIA_ERR_LOG("Initialize audio cache failed");
            ResetPAAudioClient();
            return AUDIO_CLIENT_CREATE_STREAM_ERR;
        }

        if (SetStreamRenderRate(renderRate) != AUDIO_CLIENT_SUCCESS) {
            MEDIA_ERR_LOG("Set render rate failed");
        }
    }

    state_ = PREPARED;
    std::shared_ptr<AudioStreamCallback> streamCb = streamCallback_.lock();
    if (streamCb != nullptr) {
        streamCb->OnStateChange(state_);
    }

    MEDIA_INFO_LOG("Created Stream");
    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::GetSessionID(uint32_t &sessionID) const
{
    MEDIA_DEBUG_LOG("AudioServiceClient: GetSessionID");
    uint32_t client_index = pa_context_get_index(context);
    if (client_index == PA_INVALID_INDEX) {
        return AUDIO_CLIENT_ERR;
    }

    sessionID = client_index;

    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::StartStream()
{
    int error;

    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    pa_operation *operation = nullptr;

    pa_threaded_mainloop_lock(mainLoop);

    pa_stream_state_t state = pa_stream_get_state(paStream);
    if (state != PA_STREAM_READY) {
        error = pa_context_errno(context);
        pa_threaded_mainloop_unlock(mainLoop);
        MEDIA_ERR_LOG("Stream Start Failed, error: %{public}d", error);
        ResetPAAudioClient();
        return AUDIO_CLIENT_START_STREAM_ERR;
    }

    streamCmdStatus = 0;
    operation = pa_stream_cork(paStream, 0, PAStreamStartSuccessCb, (void *)this);

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainLoop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mainLoop);

    if (!streamCmdStatus) {
        MEDIA_ERR_LOG("Stream Start Failed");
        ResetPAAudioClient();
        return AUDIO_CLIENT_START_STREAM_ERR;
    } else {
        MEDIA_INFO_LOG("Stream Started Successfully");
        return AUDIO_CLIENT_SUCCESS;
    }
}

int32_t AudioServiceClient::PauseStream()
{
    lock_guard<mutex> lock(ctrlMutex);
    PAStreamCorkSuccessCb = PAStreamPauseSuccessCb;
    int32_t ret = CorkStream();
    if (ret) {
        return ret;
    }

    if (!streamCmdStatus) {
        MEDIA_ERR_LOG("Stream Pasue Failed");
        return AUDIO_CLIENT_ERR;
    } else {
        MEDIA_INFO_LOG("Stream Pasued Successfully");
        return AUDIO_CLIENT_SUCCESS;
    }
}

int32_t AudioServiceClient::StopStream()
{
    lock_guard<mutex> lock(ctrlMutex);
    PAStreamCorkSuccessCb = PAStreamStopSuccessCb;
    int32_t ret = CorkStream();
    if (ret) {
        return ret;
    }

    if (!streamCmdStatus) {
        MEDIA_ERR_LOG("Stream Stop Failed");
        return AUDIO_CLIENT_ERR;
    } else {
        MEDIA_INFO_LOG("Stream Stopped Successfully");
        if (internalRdBufLen) {
            (void)pa_stream_drop(paStream);
            internalReadBuffer = nullptr;
            internalRdBufLen = 0;
            internalRdBufIndex = 0;
        }
        return AUDIO_CLIENT_SUCCESS;
    }
}

int32_t AudioServiceClient::CorkStream()
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    pa_operation *operation = nullptr;

    pa_threaded_mainloop_lock(mainLoop);
    pa_stream_state_t state = pa_stream_get_state(paStream);
    if (state != PA_STREAM_READY) {
        int32_t error = pa_context_errno(context);
        pa_threaded_mainloop_unlock(mainLoop);
        MEDIA_ERR_LOG("Stream Stop Failed : %{public}d", error);
        return AUDIO_CLIENT_ERR;
    }

    streamCmdStatus = 0;
    operation = pa_stream_cork(paStream, 1, PAStreamCorkSuccessCb, (void *)this);

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainLoop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mainLoop);

    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::FlushStream()
{
    int error;
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    pa_operation *operation = nullptr;

    lock_guard<mutex> lock(dataMutex);
    pa_threaded_mainloop_lock(mainLoop);

    pa_stream_state_t state = pa_stream_get_state(paStream);
    if (state != PA_STREAM_READY) {
        error = pa_context_errno(context);
        pa_threaded_mainloop_unlock(mainLoop);
        MEDIA_ERR_LOG("Stream Flush Failed, error: %{public}d", error);
        return AUDIO_CLIENT_ERR;
    }

    streamFlushStatus = 0;
    operation = pa_stream_flush(paStream, PAStreamFlushSuccessCb, (void *)this);
    if (operation == nullptr) {
        MEDIA_ERR_LOG("Stream Flush Operation Failed");
        pa_threaded_mainloop_unlock(mainLoop);
        return AUDIO_CLIENT_ERR;
    }

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainLoop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mainLoop);

    if (!streamFlushStatus) {
        MEDIA_ERR_LOG("Stream Flush Failed");
        return AUDIO_CLIENT_ERR;
    } else {
        MEDIA_INFO_LOG("Stream Flushed Successfully");
        acache.readIndex = 0;
        acache.writeIndex = 0;
        acache.isFull = false;
        return AUDIO_CLIENT_SUCCESS;
    }
}

int32_t AudioServiceClient::DrainStream()
{
    uint32_t error;

    if (eAudioClientType != AUDIO_SERVICE_CLIENT_PLAYBACK) {
        MEDIA_ERR_LOG("Drain is not supported");
        return AUDIO_CLIENT_ERR;
    }

    lock_guard<mutex> lock(dataMutex);

    error = DrainAudioCache();
    if (error != AUDIO_CLIENT_SUCCESS) {
        MEDIA_ERR_LOG("Audio cache drain failed");
        return AUDIO_CLIENT_ERR;
    }

    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    pa_operation *operation = nullptr;

    pa_threaded_mainloop_lock(mainLoop);

    pa_stream_state_t state = pa_stream_get_state(paStream);
    if (state != PA_STREAM_READY) {
        error = pa_context_errno(context);
        pa_threaded_mainloop_unlock(mainLoop);
        MEDIA_ERR_LOG("Stream Drain Failed");
        return AUDIO_CLIENT_ERR;
    }

    streamDrainStatus = 0;
    operation = pa_stream_drain(paStream, PAStreamDrainSuccessCb, (void *)this);

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainLoop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mainLoop);

    if (!streamDrainStatus) {
        MEDIA_ERR_LOG("Stream Drain Failed");
        return AUDIO_CLIENT_ERR;
    } else {
        MEDIA_INFO_LOG("Stream Drained Successfully");
        return AUDIO_CLIENT_SUCCESS;
    }
}

int32_t AudioServiceClient::SetStreamVolume(uint32_t sessionID, uint32_t volume)
{
    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::PaWriteStream(const uint8_t *buffer, size_t &length)
{
    int error = 0;

    while (length > 0) {
        size_t writableSize;

        while (!(writableSize = pa_stream_writable_size(paStream))) {
            pa_threaded_mainloop_wait(mainLoop);
        }

        MEDIA_INFO_LOG("Write stream: writable size = %{public}zu, length = %{public}zu",
                       writableSize, length);
        if (writableSize > length) {
            writableSize = length;
        }

        writableSize = AlignToAudioFrameSize(writableSize, sampleSpec);
        if (writableSize == 0) {
            MEDIA_ERR_LOG("Align to frame size failed");
            error = AUDIO_CLIENT_WRITE_STREAM_ERR;
            break;
        }

        error = pa_stream_write(paStream, (void *)buffer, writableSize, nullptr, 0LL,
                                PA_SEEK_RELATIVE);
        if (error < 0) {
            MEDIA_ERR_LOG("Write stream failed");
            error = AUDIO_CLIENT_WRITE_STREAM_ERR;
            break;
        }

        MEDIA_INFO_LOG("Writable size: %{public}zu, bytes to write: %{public}zu, return val: %{public}d",
                       writableSize, length, error);
        buffer = buffer + writableSize;
        length -= writableSize;

        HandleRenderPositionCallbacks(writableSize);
    }

    return error;
}

void AudioServiceClient::HandleRenderPositionCallbacks(size_t bytesWritten)
{
    mTotalBytesWritten += bytesWritten;
    if (mFrameSize == 0) {
        MEDIA_ERR_LOG("HandleRenderPositionCallbacks: capturePeriodPositionCb not set");
        return;
    }

    uint64_t writtenFrameNumber = mTotalBytesWritten / mFrameSize;
    MEDIA_DEBUG_LOG("frame size: %{public}d", mFrameSize);

    {
        std::lock_guard<std::mutex> lock(rendererMarkReachedMutex_);
        if (!mMarkReached && mRenderPositionCb) {
            MEDIA_DEBUG_LOG("frame mark position: %{public}" PRIu64 ", Total frames written: %{public}" PRIu64,
                static_cast<uint64_t>(mFrameMarkPosition), static_cast<uint64_t>(writtenFrameNumber));
            if (writtenFrameNumber >= mFrameMarkPosition) {
                MEDIA_DEBUG_LOG("audio service client OnMarkReached");
                mPositionCBThreads.emplace_back(std::make_unique<std::thread>(&RendererPositionCallback::OnMarkReached,
                                                mRenderPositionCb, mFrameMarkPosition));
                mMarkReached = true;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(rendererPeriodReachedMutex_);
        if (mRenderPeriodPositionCb) {
            mFramePeriodWritten += (bytesWritten / mFrameSize);
            MEDIA_DEBUG_LOG("frame period number: %{public}" PRIu64 ", Total frames written: %{public}" PRIu64,
                static_cast<uint64_t>(mFramePeriodNumber), static_cast<uint64_t>(writtenFrameNumber));
            if (mFramePeriodWritten >= mFramePeriodNumber) {
                mFramePeriodWritten %= mFramePeriodNumber;
                MEDIA_DEBUG_LOG("OnPeriodReached, remaining frames: %{public}" PRIu64,
                    static_cast<uint64_t>(mFramePeriodWritten));
                mPeriodPositionCBThreads.emplace_back(std::make_unique<std::thread>(
                    &RendererPeriodPositionCallback::OnPeriodReached, mRenderPeriodPositionCb, mFramePeriodNumber));
            }
        }
    }
}

int32_t AudioServiceClient::DrainAudioCache()
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    pa_threaded_mainloop_lock(mainLoop);

    int32_t error = 0;
    if (acache.buffer == nullptr) {
        MEDIA_ERR_LOG("Drain cache failed");
        return AUDIO_CLIENT_ERR;
    }

    size_t length = acache.writeIndex - acache.readIndex;
    const uint8_t *buffer = acache.buffer.get();

    error = PaWriteStream(buffer, length);

    acache.readIndex = 0;
    acache.writeIndex = 0;

    pa_threaded_mainloop_unlock(mainLoop);
    return error;
}

size_t AudioServiceClient::WriteToAudioCache(const StreamBuffer &stream)
{
    if (stream.buffer == nullptr) {
        return 0;
    }

    const uint8_t *inputBuffer = stream.buffer;
    uint8_t *cacheBuffer = acache.buffer.get() + acache.writeIndex;

    size_t inputLen = stream.bufferLen;

    while (inputLen > 0) {
        size_t writableSize = acache.totalCacheSize - acache.writeIndex;

        if (writableSize > inputLen) {
            writableSize = inputLen;
        }

        if (writableSize == 0) {
            break;
        }

        if (memcpy_s(cacheBuffer, acache.totalCacheSize, inputBuffer, writableSize)) {
            break;
        }

        inputBuffer = inputBuffer + writableSize;
        cacheBuffer = cacheBuffer + writableSize;
        inputLen -= writableSize;
        acache.writeIndex += writableSize;
    }

    if ((acache.writeIndex - acache.readIndex) == acache.totalCacheSize) {
        acache.isFull = true;
    }

    return (stream.bufferLen - inputLen);
}

size_t AudioServiceClient::WriteStreamInCb(const StreamBuffer &stream, int32_t &pError)
{
    lock_guard<mutex> lock(dataMutex);
    int error = 0;

    CHECK_PA_STATUS_FOR_WRITE(mainLoop, context, paStream, pError, 0);
    pa_threaded_mainloop_lock(mainLoop);

    const uint8_t *buffer = stream.buffer;
    size_t length = stream.bufferLen;
    error = PaWriteStream(buffer, length);
    pa_threaded_mainloop_unlock(mainLoop);
    pError = error;
    return (stream.bufferLen - length);
}

size_t AudioServiceClient::WriteStream(const StreamBuffer &stream, int32_t &pError)
{
    lock_guard<mutex> lock(dataMutex);
    int error = 0;
    size_t cachedLen = WriteToAudioCache(stream);

    if (!acache.isFull) {
        pError = error;
        return cachedLen;
    }

    CHECK_PA_STATUS_FOR_WRITE(mainLoop, context, paStream, pError, 0);
    pa_threaded_mainloop_lock(mainLoop);

    if (acache.buffer == nullptr) {
        MEDIA_ERR_LOG("Buffer is null");
        pError = AUDIO_CLIENT_WRITE_STREAM_ERR;
        return cachedLen;
    }

    const uint8_t *buffer = acache.buffer.get();
    size_t length = acache.totalCacheSize;

    error = PaWriteStream(buffer, length);
    acache.readIndex += acache.totalCacheSize;
    acache.isFull = false;

    if (!error && (length >= 0) && !acache.isFull) {
        uint8_t *cacheBuffer = acache.buffer.get();
        uint32_t offset = acache.readIndex;
        uint32_t size = (acache.writeIndex - acache.readIndex);
        if (size > 0) {
            if (memcpy_s(cacheBuffer, acache.totalCacheSize, cacheBuffer + offset, size)) {
                MEDIA_ERR_LOG("Update cache failed");
                pError = AUDIO_CLIENT_WRITE_STREAM_ERR;
                return cachedLen;
            }
            MEDIA_INFO_LOG("rearranging the audio cache");
        }
        acache.readIndex = 0;
        acache.writeIndex = 0;

        if (cachedLen < stream.bufferLen) {
            StreamBuffer str;
            str.buffer = stream.buffer + cachedLen;
            str.bufferLen = stream.bufferLen - cachedLen;
            MEDIA_INFO_LOG("writing pending data to audio cache: %{public}d", str.bufferLen);
            cachedLen += WriteToAudioCache(str);
        }
    }

    pa_threaded_mainloop_unlock(mainLoop);
    pError = error;
    return cachedLen;
}

int32_t AudioServiceClient::UpdateReadBuffer(uint8_t *buffer, size_t &length, size_t &readSize)
{
    size_t l = (internalRdBufLen < length) ? internalRdBufLen : length;
    if (memcpy_s(buffer, length, (const uint8_t*)internalReadBuffer + internalRdBufIndex, l)) {
        MEDIA_ERR_LOG("Update read buffer failed");
        return AUDIO_CLIENT_READ_STREAM_ERR;
    }

    length -= l;
    internalRdBufIndex += l;
    internalRdBufLen -= l;
    readSize += l;

    if (!internalRdBufLen) {
        int retVal = pa_stream_drop(paStream);
        internalReadBuffer = nullptr;
        internalRdBufLen = 0;
        internalRdBufIndex = 0;
        if (retVal < 0) {
            MEDIA_ERR_LOG("pa_stream_drop failed, retVal: %{public}d", retVal);
            return AUDIO_CLIENT_READ_STREAM_ERR;
        }
    }

    return 0;
}

void AudioServiceClient::OnTimeOut()
{
    MEDIA_ERR_LOG("Inside read timeout callback");
    pa_threaded_mainloop_lock(mainLoop);
    pa_threaded_mainloop_signal(mainLoop, 0);
    pa_threaded_mainloop_unlock(mainLoop);
}

void AudioServiceClient::HandleCapturePositionCallbacks(size_t bytesRead)
{
    mTotalBytesRead += bytesRead;
    if (mFrameSize == 0) {
        MEDIA_ERR_LOG("HandleCapturePositionCallbacks: capturePeriodPositionCb not set");
        return;
    }

    uint64_t readFrameNumber = mTotalBytesRead / mFrameSize;
    MEDIA_DEBUG_LOG("frame size: %{public}d", mFrameSize);
    {
        std::lock_guard<std::mutex> lock(capturerMarkReachedMutex_);
        if (!mMarkReached && mCapturePositionCb) {
            MEDIA_DEBUG_LOG("frame mark position: %{public}" PRIu64 ", Total frames read: %{public}" PRIu64,
                static_cast<uint64_t>(mFrameMarkPosition), static_cast<uint64_t>(readFrameNumber));
            if (readFrameNumber >= mFrameMarkPosition) {
                MEDIA_DEBUG_LOG("audio service client capturer OnMarkReached");
                mPositionCBThreads.emplace_back(std::make_unique<std::thread>(&CapturerPositionCallback::OnMarkReached,
                                                mCapturePositionCb, mFrameMarkPosition));
                mMarkReached = true;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(capturerPeriodReachedMutex_);
        if (mCapturePeriodPositionCb) {
            mFramePeriodRead += (bytesRead / mFrameSize);
            MEDIA_DEBUG_LOG("frame period number: %{public}" PRIu64 ", Total frames read: %{public}" PRIu64,
                static_cast<uint64_t>(mFramePeriodNumber), static_cast<uint64_t>(readFrameNumber));
            if (mFramePeriodRead >= mFramePeriodNumber) {
                mFramePeriodRead %= mFramePeriodNumber;
                MEDIA_DEBUG_LOG("audio service client OnPeriodReached, remaining frames: %{public}" PRIu64,
                    static_cast<uint64_t>(mFramePeriodRead));
                mPeriodPositionCBThreads.emplace_back(std::make_unique<std::thread>(
                    &CapturerPeriodPositionCallback::OnPeriodReached, mCapturePeriodPositionCb, mFramePeriodNumber));
            }
        }
    }
}

int32_t AudioServiceClient::ReadStream(StreamBuffer &stream, bool isBlocking)
{
    uint8_t *buffer = stream.buffer;
    size_t length = stream.bufferLen;
    size_t readSize = 0;

    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);

    lock_guard<mutex> lock(dataMutex);
    pa_threaded_mainloop_lock(mainLoop);
    while (length > 0) {
        while (!internalReadBuffer) {
            int retVal = pa_stream_peek(paStream, &internalReadBuffer, &internalRdBufLen);
            if (retVal < 0) {
                MEDIA_ERR_LOG("pa_stream_peek failed, retVal: %{public}d", retVal);
                pa_threaded_mainloop_unlock(mainLoop);
                return AUDIO_CLIENT_READ_STREAM_ERR;
            }

            if (internalRdBufLen <= 0) {
                if (isBlocking) {
                    StartTimer(READ_TIMEOUT_IN_SEC);
                    pa_threaded_mainloop_wait(mainLoop);
                    StopTimer();
                    if (IsTimeOut()) {
                        MEDIA_ERR_LOG("Read timeout");
                        pa_threaded_mainloop_unlock(mainLoop);
                        return AUDIO_CLIENT_READ_STREAM_ERR;
                    }
                } else {
                    pa_threaded_mainloop_unlock(mainLoop);
                    HandleCapturePositionCallbacks(readSize);
                    return readSize;
                }
            } else if (!internalReadBuffer) {
                retVal = pa_stream_drop(paStream);
                if (retVal < 0) {
                    MEDIA_ERR_LOG("pa_stream_drop failed, retVal: %{public}d", retVal);
                    pa_threaded_mainloop_unlock(mainLoop);
                    return AUDIO_CLIENT_READ_STREAM_ERR;
                }
            } else {
                internalRdBufIndex = 0;
                MEDIA_INFO_LOG("buffer size from PA: %{public}zu", internalRdBufLen);
            }
        }

        if (UpdateReadBuffer(buffer, length, readSize) != 0) {
            pa_threaded_mainloop_unlock(mainLoop);
            return AUDIO_CLIENT_READ_STREAM_ERR;
        }
        buffer = stream.buffer + readSize;
    }
    pa_threaded_mainloop_unlock(mainLoop);
    HandleCapturePositionCallbacks(readSize);

    return readSize;
}

int32_t AudioServiceClient::ReleaseStream()
{
    ResetPAAudioClient();
    state_ = RELEASED;

    std::shared_ptr<AudioStreamCallback> streamCb = streamCallback_.lock();
    if (streamCb != nullptr) {
        streamCb->OnStateChange(state_);
    }

    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::SetBufferSizeInMsec(int32_t bufferSizeInMsec)
{
    size_t bufferSize =  pa_usec_to_bytes(bufferSizeInMsec * PA_USEC_PER_MSEC, &sampleSpec);
    setBufferSize = bufferSize;
    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::GetMinimumBufferSize(size_t &minBufferSize) const
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);

    const pa_buffer_attr *bufferAttr = pa_stream_get_buffer_attr(paStream);

    if (bufferAttr == nullptr) {
        MEDIA_ERR_LOG("pa_stream_get_buffer_attr returned nullptr");
        return AUDIO_CLIENT_ERR;
    }

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_PLAYBACK) {
        if (renderMode_ == RENDER_MODE_CALLBACK) {
            minBufferSize = (size_t)bufferAttr->minreq;
        } else {
            if (setBufferSize) {
                minBufferSize = setBufferSize;
            } else {
                minBufferSize = (size_t)bufferAttr->minreq;
            }
        }
    }

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_RECORD) {
        minBufferSize = (size_t)bufferAttr->fragsize;
    }

    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::GetMinimumFrameCount(uint32_t &frameCount) const
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    size_t minBufferSize = 0;

    const pa_buffer_attr *bufferAttr = pa_stream_get_buffer_attr(paStream);

    if (bufferAttr == nullptr) {
        MEDIA_ERR_LOG("pa_stream_get_buffer_attr returned nullptr");
        return AUDIO_CLIENT_ERR;
    }

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_PLAYBACK) {
        if (renderMode_ == RENDER_MODE_CALLBACK) {
            minBufferSize = (size_t)bufferAttr->minreq;
        } else {
            if (setBufferSize) {
                minBufferSize = setBufferSize;
            } else {
                minBufferSize = (size_t)bufferAttr->minreq;
            }
        }
    }

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_RECORD) {
        minBufferSize = (size_t)bufferAttr->fragsize;
    }

    uint32_t bytesPerSample = pa_frame_size(&sampleSpec);
    if (bytesPerSample == 0) {
        MEDIA_ERR_LOG("GetMinimumFrameCount Failed");
        return AUDIO_CLIENT_ERR;
    }

    frameCount = minBufferSize / bytesPerSample;
    MEDIA_INFO_LOG("frame count: %d", frameCount);
    return AUDIO_CLIENT_SUCCESS;
}

uint32_t AudioServiceClient::GetSamplingRate() const
{
    return DEFAULT_SAMPLING_RATE;
}

uint8_t AudioServiceClient::GetChannelCount() const
{
    return DEFAULT_CHANNEL_COUNT;
}

uint8_t AudioServiceClient::GetSampleSize() const
{
    return DEFAULT_SAMPLE_SIZE;
}

int32_t AudioServiceClient::GetAudioStreamParams(AudioStreamParams& audioParams) const
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    const pa_sample_spec *paSampleSpec = pa_stream_get_sample_spec(paStream);

    if (!paSampleSpec) {
        MEDIA_ERR_LOG("GetAudioStreamParams Failed");
        return AUDIO_CLIENT_ERR;
    }

    audioParams = ConvertFromPAAudioParams(*paSampleSpec);
    return AUDIO_CLIENT_SUCCESS;
}

uint32_t AudioServiceClient::GetStreamVolume(uint32_t sessionID)
{
    return DEFAULT_STREAM_VOLUME;
}

int32_t AudioServiceClient::GetCurrentTimeStamp(uint64_t &timeStamp) const
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    int32_t retVal = AUDIO_CLIENT_SUCCESS;

    pa_threaded_mainloop_lock(mainLoop);
    const pa_timing_info *info = pa_stream_get_timing_info(paStream);
    if (info == nullptr) {
        retVal = AUDIO_CLIENT_ERR;
    } else {
        if (eAudioClientType == AUDIO_SERVICE_CLIENT_PLAYBACK) {
            timeStamp = pa_bytes_to_usec(info->write_index, &sampleSpec);
        } else if (eAudioClientType == AUDIO_SERVICE_CLIENT_RECORD) {
            if (pa_stream_get_time(paStream, &timeStamp)) {
                MEDIA_ERR_LOG("AudioServiceClient::GetCurrentTimeStamp failed for AUDIO_SERVICE_CLIENT_RECORD");
            }
        }
    }
    pa_threaded_mainloop_unlock(mainLoop);

    return retVal;
}

int32_t AudioServiceClient::GetAudioLatency(uint64_t &latency) const
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    pa_usec_t paLatency;
    pa_usec_t cacheLatency;
    int32_t retVal = AUDIO_CLIENT_SUCCESS;
    int negative;
    bool getPALatency = false;

    // Get PA latency
    pa_threaded_mainloop_lock(mainLoop);
    while (!getPALatency) {
        if (pa_stream_get_latency(paStream, &paLatency, &negative) >= 0) {
            if (negative) {
                latency = 0;
                retVal = AUDIO_CLIENT_ERR;
                return retVal;
            }
            getPALatency = true;
            break;
        }
        MEDIA_INFO_LOG("waiting for audio latency information");
        pa_threaded_mainloop_wait(mainLoop);
    }
    pa_threaded_mainloop_unlock(mainLoop);

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_PLAYBACK) {
        // Get audio write cache latency
        cacheLatency = pa_bytes_to_usec((acache.totalCacheSize - acache.writeIndex), &sampleSpec);

        // Total latency will be sum of audio write cache latency + PA latency
        latency = paLatency + cacheLatency;
        MEDIA_INFO_LOG("total latency: %{public}" PRIu64 ", pa latency: %{public}"
            PRIu64 ", cache latency: %{public}" PRIu64, latency, paLatency, cacheLatency);
    } else if (eAudioClientType == AUDIO_SERVICE_CLIENT_RECORD) {
        // Get audio read cache latency
        cacheLatency = pa_bytes_to_usec(internalRdBufLen, &sampleSpec);

        // Total latency will be sum of audio read cache latency + PA latency
        latency = paLatency + cacheLatency;
        MEDIA_INFO_LOG("total latency: %{public}" PRIu64 ", pa latency: %{public}" PRIu64, latency, paLatency);
    }

    return retVal;
}

void AudioServiceClient::RegisterAudioRendererCallbacks(const AudioRendererCallbacks &cb)
{
    MEDIA_INFO_LOG("Registering audio render callbacks");
    mAudioRendererCallbacks = (AudioRendererCallbacks *)&cb;
}

void AudioServiceClient::RegisterAudioCapturerCallbacks(const AudioCapturerCallbacks &cb)
{
    MEDIA_INFO_LOG("Registering audio record callbacks");
    mAudioCapturerCallbacks = (AudioCapturerCallbacks *)&cb;
}

void AudioServiceClient::SetRendererPositionCallback(int64_t markPosition,
    const std::shared_ptr<RendererPositionCallback> &callback)
{
    MEDIA_INFO_LOG("Registering render frame position callback");
    MEDIA_INFO_LOG("mark position: %{public}" PRIu64, markPosition);
    mFrameMarkPosition = markPosition;
    mRenderPositionCb = callback;
    mMarkReached = false;
}

void AudioServiceClient::UnsetRendererPositionCallback()
{
    MEDIA_INFO_LOG("Unregistering render frame position callback");
    std::lock_guard<std::mutex> lock(rendererMarkReachedMutex_);
    mRenderPositionCb = nullptr;
    mMarkReached = false;
    mFrameMarkPosition = 0;
}

void AudioServiceClient::SetRendererPeriodPositionCallback(int64_t periodPosition,
    const std::shared_ptr<RendererPeriodPositionCallback> &callback)
{
    MEDIA_INFO_LOG("Registering render period position callback");
    mFramePeriodNumber = periodPosition;
    if ((mFrameSize != 0) && (mFramePeriodNumber != 0)) {
        mFramePeriodWritten = (mTotalBytesWritten / mFrameSize) % mFramePeriodNumber;
    } else {
        MEDIA_ERR_LOG("AudioServiceClient::SetRendererPeriodPositionCallback failed");
        return;
    }
    mRenderPeriodPositionCb = callback;
}

void AudioServiceClient::UnsetRendererPeriodPositionCallback()
{
    MEDIA_INFO_LOG("Unregistering render period position callback");
    std::lock_guard<std::mutex> lock(rendererPeriodReachedMutex_);
    mRenderPeriodPositionCb = nullptr;
    mFramePeriodWritten = 0;
    mFramePeriodNumber = 0;
}

void AudioServiceClient::SetCapturerPositionCallback(int64_t markPosition,
    const std::shared_ptr<CapturerPositionCallback> &callback)
{
    MEDIA_INFO_LOG("Registering capture frame position callback");
    MEDIA_INFO_LOG("mark position: %{public}" PRIu64, markPosition);
    mFrameMarkPosition = markPosition;
    mCapturePositionCb = callback;
    mMarkReached = false;
}

void AudioServiceClient::UnsetCapturerPositionCallback()
{
    MEDIA_INFO_LOG("Unregistering capture frame position callback");
    std::lock_guard<std::mutex> lock(capturerMarkReachedMutex_);
    mCapturePositionCb = nullptr;
    mMarkReached = false;
    mFrameMarkPosition = 0;
}

void AudioServiceClient::SetCapturerPeriodPositionCallback(int64_t periodPosition,
    const std::shared_ptr<CapturerPeriodPositionCallback> &callback)
{
    MEDIA_INFO_LOG("Registering period position callback");
    mFramePeriodNumber = periodPosition;
    if ((mFrameSize != 0) && (mFramePeriodNumber) != 0) {
        mFramePeriodRead = (mTotalBytesRead / mFrameSize) % mFramePeriodNumber;
    } else {
        MEDIA_INFO_LOG("AudioServiceClient::SetCapturerPeriodPositionCallback failed");
        return;
    }
    mCapturePeriodPositionCb = callback;
}

void AudioServiceClient::UnsetCapturerPeriodPositionCallback()
{
    MEDIA_INFO_LOG("Unregistering period position callback");
    std::lock_guard<std::mutex> lock(capturerPeriodReachedMutex_);
    mCapturePeriodPositionCb = nullptr;
    mFramePeriodRead = 0;
    mFramePeriodNumber = 0;
}

int32_t AudioServiceClient::SetStreamType(AudioStreamType audioStreamType)
{
    MEDIA_INFO_LOG("SetStreamType: %{public}d", audioStreamType);

    if (context == nullptr) {
        MEDIA_ERR_LOG("context is null");
        return AUDIO_CLIENT_ERR;
    }

    pa_threaded_mainloop_lock(mainLoop);

    mStreamType = audioStreamType;
    const std::string streamName = GetStreamName(audioStreamType);

    pa_proplist *propList = pa_proplist_new();
    if (propList == nullptr) {
        MEDIA_ERR_LOG("pa_proplist_new failed");
        pa_threaded_mainloop_unlock(mainLoop);
        return AUDIO_CLIENT_ERR;
    }

    pa_proplist_sets(propList, "stream.type", streamName.c_str());
    pa_proplist_sets(propList, "media.name", streamName.c_str());
    pa_operation *updatePropOperation = pa_stream_proplist_update(paStream, PA_UPDATE_REPLACE, propList,
        nullptr, nullptr);
    pa_proplist_free(propList);
    pa_operation_unref(updatePropOperation);

    pa_threaded_mainloop_unlock(mainLoop);

    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::SetStreamVolume(float volume)
{
    lock_guard<mutex> lock(ctrlMutex);
    MEDIA_INFO_LOG("SetVolume volume: %{public}f", volume);

    if (context == nullptr) {
        MEDIA_ERR_LOG("context is null");
        return AUDIO_CLIENT_ERR;
    }

    /* Validate and return INVALID_PARAMS error */
    if ((volume < MIN_STREAM_VOLUME_LEVEL) || (volume > MAX_STREAM_VOLUME_LEVEL)) {
        MEDIA_ERR_LOG("Invalid Volume Input!");
        return AUDIO_CLIENT_INVALID_PARAMS_ERR;
    }

    pa_threaded_mainloop_lock(mainLoop);

    mVolumeFactor = volume;
    pa_proplist *propList = pa_proplist_new();
    if (propList == nullptr) {
        MEDIA_ERR_LOG("pa_proplist_new failed");
        pa_threaded_mainloop_unlock(mainLoop);
        return AUDIO_CLIENT_ERR;
    }

    pa_proplist_sets(propList, "stream.volumeFactor", std::to_string(mVolumeFactor).c_str());
    pa_operation *updatePropOperation = pa_stream_proplist_update(paStream, PA_UPDATE_REPLACE, propList,
        nullptr, nullptr);
    pa_proplist_free(propList);
    pa_operation_unref(updatePropOperation);

    if (mAudioSystemMgr == nullptr) {
        MEDIA_ERR_LOG("System manager instance is null");
        pa_threaded_mainloop_unlock(mainLoop);
        return AUDIO_CLIENT_ERR;
    }

    if (!streamInfoUpdated) {
        uint32_t idx = pa_stream_get_index(paStream);
        pa_operation *operation = pa_context_get_sink_input_info(context, idx, AudioServiceClient::GetSinkInputInfoCb,
            reinterpret_cast<void *>(this));
        if (operation == nullptr) {
            MEDIA_ERR_LOG("pa_context_get_sink_input_info_list returned null");
            pa_threaded_mainloop_unlock(mainLoop);
            return AUDIO_CLIENT_ERR;
        }

        pa_threaded_mainloop_accept(mainLoop);

        pa_operation_unref(operation);
    } else {
        SetPaVolume(*this);
    }

    pa_threaded_mainloop_unlock(mainLoop);

    return AUDIO_CLIENT_SUCCESS;
}

float AudioServiceClient::GetStreamVolume()
{
    return mVolumeFactor;
}

void AudioServiceClient::GetSinkInputInfoCb(pa_context *context, const pa_sink_input_info *info, int eol,
    void *userdata)
{
    MEDIA_INFO_LOG("GetSinkInputInfoVolumeCb in");
    AudioServiceClient *thiz = reinterpret_cast<AudioServiceClient *>(userdata);

    if (eol < 0) {
        MEDIA_ERR_LOG("Failed to get sink input information: %{public}s", pa_strerror(pa_context_errno(context)));
        return;
    }

    if (eol) {
        pa_threaded_mainloop_signal(thiz->mainLoop, 1);
        return;
    }

    if (info->proplist == nullptr) {
        MEDIA_ERR_LOG("Invalid prop list for sink input (%{public}d).", info->index);
        return;
    }

    thiz->cvolume = info->volume;
    thiz->streamIndex = info->index;
    thiz->volumeChannels = info->channel_map.channels;
    thiz->streamInfoUpdated = true;

    SetPaVolume(*thiz);

    return;
}

void AudioServiceClient::SetPaVolume(const AudioServiceClient &client)
{
    pa_cvolume cv = client.cvolume;
    int32_t systemVolumeInt
        = client.mAudioSystemMgr->GetVolume(static_cast<AudioSystemManager::AudioVolumeType>(client.mStreamType));
    float systemVolume = AudioSystemManager::MapVolumeToHDI(systemVolumeInt);
    float vol = systemVolume * client.mVolumeFactor;

    AudioRingerMode ringerMode = client.mAudioSystemMgr->GetRingerMode();
    if ((client.mStreamType == STREAM_RING) && (ringerMode != RINGER_MODE_NORMAL)) {
        vol = MIN_STREAM_VOLUME_LEVEL;
    }

    uint32_t volume = pa_sw_volume_from_linear(vol);
    pa_cvolume_set(&cv, client.volumeChannels, volume);
    pa_operation_unref(pa_context_set_sink_input_volume(client.context, client.streamIndex, &cv, nullptr, nullptr));

    MEDIA_INFO_LOG("Applied volume : %{public}f, pa volume: %{public}d", vol, volume);
}

int32_t AudioServiceClient::SetStreamRenderRate(AudioRendererRate audioRendererRate)
{
    MEDIA_INFO_LOG("SetStreamRenderRate in");
    renderRate = audioRendererRate;
    if (!paStream) {
        return AUDIO_CLIENT_SUCCESS;
    }

    uint32_t rate = sampleSpec.rate;
    switch (audioRendererRate) {
        case RENDER_RATE_NORMAL:
            break;
        case RENDER_RATE_DOUBLE:
            rate *= DOUBLE_VALUE;
            break;
        case RENDER_RATE_HALF:
            rate /= DOUBLE_VALUE;
            break;
        default:
            return AUDIO_CLIENT_INVALID_PARAMS_ERR;
    }

    pa_threaded_mainloop_lock(mainLoop);
    pa_operation *operation = pa_stream_update_sample_rate(paStream, rate, nullptr, nullptr);
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mainLoop);

    return AUDIO_CLIENT_SUCCESS;
}

AudioRendererRate AudioServiceClient::GetStreamRenderRate()
{
    return renderRate;
}

void AudioServiceClient::SaveStreamCallback(const std::weak_ptr<AudioStreamCallback> &callback)
{
    streamCallback_ = callback;

    if (state_ != PREPARED) {
        return;
    }

    std::shared_ptr<AudioStreamCallback> streamCb = streamCallback_.lock();
    if (streamCb != nullptr) {
        streamCb->OnStateChange(state_);
    }
}
} // namespace AudioStandard
} // namespace OHOS
