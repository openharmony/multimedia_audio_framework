/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#include "media_log.h"
#include "securec.h"

namespace OHOS {
namespace AudioStandard {
AudioRendererCallbacks::~AudioRendererCallbacks() = default;
AudioRecorderCallbacks::~AudioRecorderCallbacks() = default;

const uint64_t LATENCY_IN_MSEC = 200UL;

#define CHECK_AND_RETURN_IFINVALID(expr) \
    if (!(expr)) {                       \
        return AUDIO_CLIENT_ERR;         \
    }

#define CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, error) \
    if (!context || !paStream || !mainLoop                              \
            || !PA_CONTEXT_IS_GOOD(pa_context_get_state(context))       \
            || !PA_STREAM_IS_GOOD(pa_stream_get_state(paStream))) {     \
                return error;                                           \
    }

#define CHECK_PA_STATUS_FOR_WRITE(mainLoop, context, paStream, pError, retVal) \
    if (!context || !paStream || !mainLoop                                     \
            || !PA_CONTEXT_IS_GOOD(pa_context_get_state(context))              \
            || !PA_STREAM_IS_GOOD(pa_stream_get_state(paStream))) {            \
                pError = pa_context_errno(context);                            \
                return retVal;                                                 \
    }

AudioStreamParams AudioServiceClient::ConvertFromPAAudioParams(pa_sample_spec paSampleSpec)
{
    AudioStreamParams audioParams;

    audioParams.channels = paSampleSpec.channels;
    audioParams.samplingRate = paSampleSpec.rate;

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
            paSampleSpec.format = (pa_sample_format_t) PA_SAMPLE_U8;
            break;
        case SAMPLE_S16LE:
            paSampleSpec.format = (pa_sample_format_t) PA_SAMPLE_S16LE;
            break;
        case SAMPLE_S24LE:
            paSampleSpec.format = (pa_sample_format_t) PA_SAMPLE_S24LE;
            break;
        case SAMPLE_S32LE:
            paSampleSpec.format = (pa_sample_format_t) PA_SAMPLE_S32LE;
            break;
        default:
            paSampleSpec.format = (pa_sample_format_t) PA_SAMPLE_INVALID;
            break;
    }

    return paSampleSpec;
}

static size_t AlignToAudioFrameSize(size_t l, const pa_sample_spec &ss)
{
    size_t fs;

    fs = pa_frame_size(&ss);
    if (fs == 0) {
        MEDIA_ERR_LOG(" Error: pa_frame_size returned  0");
        return 0;
    }

    return (l / fs) * fs;
}

void AudioServiceClient::PAStreamCmdSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    AudioServiceClient *asClient = (AudioServiceClient *)userdata;
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *) asClient->mainLoop;

    asClient->streamCmdStatus = success;
    pa_threaded_mainloop_signal(mainLoop, 0);
}

void AudioServiceClient::PAStreamRequestCb(pa_stream *stream, size_t length, void *userdata)
{
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *) userdata;
    pa_threaded_mainloop_signal(mainLoop, 0);
}

void AudioServiceClient::PAStreamUnderFlowCb(pa_stream *stream, void *userdata)
{
    AudioServiceClient *asClient = (AudioServiceClient *)userdata;
    asClient->underFlowCount++;
}

void AudioServiceClient::PAStreamLatencyUpdateCb(pa_stream *stream, void *userdata)
{
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *) userdata;
    MEDIA_INFO_LOG("Inside latency update callback");
    pa_threaded_mainloop_signal(mainLoop, 0);
}

void AudioServiceClient::PAStreamStateCb(pa_stream *stream, void *userdata)
{
    AudioServiceClient *asClient = (AudioServiceClient *)userdata;
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *) asClient->mainLoop;

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
    pa_threaded_mainloop *mainLoop = (pa_threaded_mainloop *) userdata;

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

    mAudioRendererCallbacks = NULL;
    mAudioRecorderCallbacks = NULL;
    internalReadBuffer = NULL;
    mainLoop = NULL;
    paStream = NULL;
    context  = NULL;
    api = NULL;

    internalRdBufIndex = 0;
    internalRdBufLen = 0;
    underFlowCount = 0;
}

void AudioServiceClient::ResetPAAudioClient()
{
    if (mainLoop && (isMainLoopStarted == true))
        pa_threaded_mainloop_stop(mainLoop);

    if (paStream) {
        pa_stream_set_state_callback(paStream, NULL, NULL);
        pa_stream_set_write_callback(paStream, NULL, NULL);
        pa_stream_set_read_callback(paStream, NULL, NULL);
        pa_stream_set_latency_update_callback(paStream, NULL, NULL);
        pa_stream_set_underflow_callback(paStream, NULL, NULL);

        if (isStreamConnected == true)
            pa_stream_disconnect(paStream);
        pa_stream_unref(paStream);
    }

    if (context) {
        pa_context_set_state_callback(context, NULL, NULL);
        if (isContextConnected == true)
            pa_context_disconnect(context);
        pa_context_unref(context);
    }

    if (mainLoop)
        pa_threaded_mainloop_free(mainLoop);

    isMainLoopStarted  = false;
    isContextConnected = false;
    isStreamConnected  = false;

    sinkDevices.clear();
    sourceDevices.clear();
    sinkInputs.clear();
    sourceOutputs.clear();
    clientInfo.clear();

    mAudioRendererCallbacks = NULL;
    mAudioRecorderCallbacks = NULL;
    internalReadBuffer      = NULL;

    mainLoop = NULL;
    paStream = NULL;
    context  = NULL;
    api      = NULL;

    internalRdBufIndex = 0;
    internalRdBufLen   = 0;
    underFlowCount     = 0;
}

AudioServiceClient::~AudioServiceClient()
{
    ResetPAAudioClient();
}

int32_t AudioServiceClient::Initialize(ASClientType eClientType)
{
    int error = PA_ERR_INTERNAL;
    eAudioClientType = eClientType;

    mainLoop = pa_threaded_mainloop_new();
    if (mainLoop == NULL)
        return AUDIO_CLIENT_INIT_ERR;

    api = pa_threaded_mainloop_get_api(mainLoop);
    if (api == NULL) {
        ResetPAAudioClient();
        return AUDIO_CLIENT_INIT_ERR;
    }

    context = pa_context_new(api, "AudioServiceClient");
    if (context == NULL) {
        ResetPAAudioClient();
        return AUDIO_CLIENT_INIT_ERR;
    }

    pa_context_set_state_callback(context, PAContextStateCb, mainLoop);

    if (pa_context_connect(context, NULL, PA_CONTEXT_NOFAIL, NULL) < 0) {
        error = pa_context_errno(context);
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
            // log the error
            pa_threaded_mainloop_unlock(mainLoop);
            ResetPAAudioClient();
            return AUDIO_CLIENT_INIT_ERR;
        }

        pa_threaded_mainloop_wait(mainLoop);
    }

    pa_threaded_mainloop_unlock(mainLoop);
    return AUDIO_CLIENT_SUCCESS;
}

const std::string AudioServiceClient::GetStreamName(AudioStreamType audioType)
{
    std::string name;
    switch (audioType) {
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
    bufferAttr.fragsize =  (uint32_t) -1;
    bufferAttr.prebuf = (uint32_t) -1;
    bufferAttr.maxlength = (uint32_t) -1;
    bufferAttr.tlength = (uint32_t) -1;
    bufferAttr.minreq = pa_usec_to_bytes(LATENCY_IN_MSEC * PA_USEC_PER_MSEC, &sampleSpec);
    if (eAudioClientType == AUDIO_SERVICE_CLIENT_PLAYBACK)
        result = pa_stream_connect_playback(paStream, NULL, &bufferAttr,
                                            (pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY
                                            | PA_STREAM_INTERPOLATE_TIMING
                                            | PA_STREAM_START_CORKED
                                            | PA_STREAM_AUTO_TIMING_UPDATE), NULL, NULL);
    else
        result = pa_stream_connect_record(paStream, NULL, NULL,
                                          (pa_stream_flags_t)(PA_STREAM_INTERPOLATE_TIMING
                                          | PA_STREAM_ADJUST_LATENCY
                                          | PA_STREAM_START_CORKED
                                          | PA_STREAM_AUTO_TIMING_UPDATE));

    if (result < 0) {
        error = pa_context_errno(context);
        MEDIA_ERR_LOG("error in connection to stream");
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
            MEDIA_ERR_LOG("error in connection to stream");
            ResetPAAudioClient();
            return AUDIO_CLIENT_CREATE_STREAM_ERR;
        }

        pa_threaded_mainloop_wait(mainLoop);
    }

    isStreamConnected = true;
    pa_threaded_mainloop_unlock(mainLoop);
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
    const std::string streamName = GetStreamName(audioType);

    sampleSpec = ConvertToPAAudioParams(audioParams);

    pa_proplist *propList = pa_proplist_new();
    if (propList == NULL) {
        MEDIA_ERR_LOG("pa_proplist_new failed");
        ResetPAAudioClient();
        return AUDIO_CLIENT_CREATE_STREAM_ERR;
    }

    pa_proplist_sets(propList, "stream.type", streamName.c_str());

    if (!(paStream = pa_stream_new_with_proplist(context, streamName.c_str(), &sampleSpec, NULL, propList))) {
        error = pa_context_errno(context);
        pa_proplist_free(propList);
        pa_threaded_mainloop_unlock(mainLoop);
        ResetPAAudioClient();
        return AUDIO_CLIENT_CREATE_STREAM_ERR;
    }

    pa_proplist_free(propList);
    pa_stream_set_state_callback(paStream, PAStreamStateCb, (void *) this);
    pa_stream_set_write_callback(paStream, PAStreamRequestCb, mainLoop);
    pa_stream_set_read_callback(paStream, PAStreamRequestCb, mainLoop);
    pa_stream_set_latency_update_callback(paStream, PAStreamLatencyUpdateCb, mainLoop);
    pa_stream_set_underflow_callback(paStream, PAStreamUnderFlowCb, (void *) this);

    pa_threaded_mainloop_unlock(mainLoop);

    error = ConnectStreamToPA();
    if (error < 0) {
        MEDIA_ERR_LOG("Create Stream Failed");
        ResetPAAudioClient();
        return AUDIO_CLIENT_CREATE_STREAM_ERR;
    }

    MEDIA_INFO_LOG("Created Stream");
    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::StartStream()
{
    int error;

    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    pa_operation *operation = NULL;

    pa_threaded_mainloop_lock(mainLoop);

    pa_stream_state_t state = pa_stream_get_state(paStream);
    if (state != PA_STREAM_READY) {
        error = pa_context_errno(context);
        pa_threaded_mainloop_unlock(mainLoop);
        MEDIA_ERR_LOG("Stream Start Failed");
        ResetPAAudioClient();
        return AUDIO_CLIENT_START_STREAM_ERR;
    }

    streamCmdStatus = 0;
    operation = pa_stream_cork(paStream, 0, PAStreamCmdSuccessCb, (void *) this);

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

int32_t AudioServiceClient::PauseStream(uint32_t sessionID)
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::StopStream()
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    pa_operation *operation = NULL;

    pa_threaded_mainloop_lock(mainLoop);
    pa_stream_state_t state = pa_stream_get_state(paStream);
    if (state != PA_STREAM_READY) {
        int32_t error = pa_context_errno(context);
        pa_threaded_mainloop_unlock(mainLoop);
        MEDIA_ERR_LOG("Stream Stop Failed : %{public}d", error);
        return AUDIO_CLIENT_ERR;
    }

    streamCmdStatus = 0;
    operation = pa_stream_cork(paStream, 1, PAStreamCmdSuccessCb, (void *) this);

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainLoop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mainLoop);

    if (!streamCmdStatus) {
        MEDIA_ERR_LOG("Stream Stop Failed");
        return AUDIO_CLIENT_ERR;
    } else {
        MEDIA_INFO_LOG("Stream Stopped Successfully");
        return AUDIO_CLIENT_SUCCESS;
    }
}

int32_t AudioServiceClient::FlushStream()
{
    int error;
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    pa_operation *operation = NULL;

    pa_threaded_mainloop_lock(mainLoop);

    pa_stream_state_t state = pa_stream_get_state(paStream);
    if (state != PA_STREAM_READY) {
        error = pa_context_errno(context);
        pa_threaded_mainloop_unlock(mainLoop);
        MEDIA_ERR_LOG("Stream Flush Failed");
        return AUDIO_CLIENT_ERR;
    }

    streamCmdStatus = 0;
    operation = pa_stream_flush(paStream, PAStreamCmdSuccessCb, (void *) this);

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainLoop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mainLoop);

    if (!streamCmdStatus) {
        MEDIA_ERR_LOG("Stream Flush Failed");
        return AUDIO_CLIENT_ERR;
    } else {
        MEDIA_INFO_LOG("Stream Flushed Successfully");
        return AUDIO_CLIENT_SUCCESS;
    }
}

int32_t AudioServiceClient::DrainStream()
{
    int error;
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    pa_operation *operation = NULL;

    pa_threaded_mainloop_lock(mainLoop);

    pa_stream_state_t state = pa_stream_get_state(paStream);
    if (state != PA_STREAM_READY) {
        error = pa_context_errno(context);
        pa_threaded_mainloop_unlock(mainLoop);
        MEDIA_ERR_LOG("Stream Drain Failed");
        return AUDIO_CLIENT_ERR;
    }

    streamCmdStatus = 0;
    operation = pa_stream_drain(paStream, PAStreamCmdSuccessCb, (void *) this);

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainLoop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mainLoop);

    if (!streamCmdStatus) {
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

size_t AudioServiceClient::WriteStream(const StreamBuffer &stream, int32_t &pError)
{
    const uint8_t* buffer = stream.buffer;
    size_t length = stream.bufferLen;
    int error = 0;

    CHECK_PA_STATUS_FOR_WRITE(mainLoop, context, paStream, pError, 0);

    pa_threaded_mainloop_lock(mainLoop);

    while (length > 0) {
        size_t writableSize;

        while (!(writableSize = pa_stream_writable_size(paStream))) {
            pa_threaded_mainloop_wait(mainLoop);
        }

        if (writableSize > length)
            writableSize = length;

        writableSize = AlignToAudioFrameSize(writableSize, sampleSpec);
        if (writableSize == 0) {
            break;
        }

        error = pa_stream_write(paStream, (void *)buffer, writableSize, NULL, 0LL, PA_SEEK_RELATIVE);
        if (error < 0) {
            break;
        }

        MEDIA_INFO_LOG("writable size: %{public}zu  bytes to write: %{public}zu  return val: %{public}d ", writableSize, length, error);

        buffer = (const uint8_t*) buffer + writableSize;
        length -= writableSize;
    }

    pa_threaded_mainloop_unlock(mainLoop);
    pError = error;
    return (stream.bufferLen - length);
}

int32_t AudioServiceClient::ReadStream(StreamBuffer &stream, bool isBlocking)
{
    uint8_t* buffer = stream.buffer;
    size_t length = stream.bufferLen;
    size_t readSize = 0;

    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);

    pa_threaded_mainloop_lock(mainLoop);
    while (length > 0) {
        size_t l;

        while (!internalReadBuffer) {
            int retVal = pa_stream_peek(paStream, &internalReadBuffer, &internalRdBufLen);
            if (retVal < 0) {
                MEDIA_ERR_LOG("pa_stream_peek failed, retVal: %d", retVal);
                pa_threaded_mainloop_unlock(mainLoop);
                return AUDIO_CLIENT_READ_STREAM_ERR;
            }

            if (internalRdBufLen <= 0) {
                if (isBlocking)
                    pa_threaded_mainloop_wait(mainLoop);
                else {
                    pa_threaded_mainloop_unlock(mainLoop);
                    return 0;
                }
            } else if (!internalReadBuffer) {
                retVal = pa_stream_drop(paStream);
                if (retVal < 0) {
                    MEDIA_ERR_LOG("pa_stream_drop failed, retVal: %d", retVal);
                    pa_threaded_mainloop_unlock(mainLoop);
                    return AUDIO_CLIENT_READ_STREAM_ERR;
                }
            } else {
                internalRdBufIndex = 0;
                MEDIA_INFO_LOG("buffer size from PA: %zu", internalRdBufLen);
            }
        }

        l = (internalRdBufLen < length) ? internalRdBufLen : length;
        memcpy_s(buffer, length, (const uint8_t*) internalReadBuffer + internalRdBufIndex, l);

        buffer = (uint8_t*) buffer + l;
        length -= l;

        internalRdBufIndex += l;
        internalRdBufLen -= l;
        readSize += l;

        if (!internalRdBufLen) {
            int retVal = pa_stream_drop(paStream);
            internalReadBuffer = NULL;
            internalRdBufLen = 0;
            internalRdBufIndex = 0;
            if (retVal < 0) {
                MEDIA_ERR_LOG("pa_stream_drop failed, retVal: %d", retVal);
                pa_threaded_mainloop_unlock(mainLoop);
                return AUDIO_CLIENT_READ_STREAM_ERR;
            }
        }
    }
    pa_threaded_mainloop_unlock(mainLoop);
    return readSize;
}

int32_t AudioServiceClient::ReleaseStream()
{
    ResetPAAudioClient();
    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::GetMinimumBufferSize(size_t &minBufferSize)
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);

    const pa_buffer_attr* bufferAttr = pa_stream_get_buffer_attr(paStream);

    if (bufferAttr == NULL) {
        MEDIA_ERR_LOG("pa_stream_get_buffer_attr returned NULL");
        return AUDIO_CLIENT_ERR;
    }

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_PLAYBACK) {
        minBufferSize = (size_t) bufferAttr->minreq;
    }

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_RECORD) {
        minBufferSize = (size_t) bufferAttr->fragsize;
    }

    MEDIA_INFO_LOG("buffer size: %zu", minBufferSize);
    return AUDIO_CLIENT_SUCCESS;
}

int32_t AudioServiceClient::GetMinimumFrameCount(uint32_t &frameCount)
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    size_t minBufferSize = 0;

    const pa_buffer_attr* bufferAttr = pa_stream_get_buffer_attr(paStream);

    if (bufferAttr == NULL) {
        MEDIA_ERR_LOG("pa_stream_get_buffer_attr returned NULL");
        return AUDIO_CLIENT_ERR;
    }

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_PLAYBACK) {
        minBufferSize = (size_t) bufferAttr->minreq;
    }

    if (eAudioClientType == AUDIO_SERVICE_CLIENT_RECORD) {
        minBufferSize = (size_t) bufferAttr->fragsize;
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

uint32_t AudioServiceClient::GetSamplingRate()
{
    return DEFAULT_SAMPLING_RATE;
}

uint8_t AudioServiceClient::GetChannelCount()
{
    return DEFAULT_CHANNEL_COUNT;
}

uint8_t AudioServiceClient::GetSampleSize()
{
    return DEFAULT_SAMPLE_SIZE;
}

int32_t AudioServiceClient::GetAudioStreamParams(AudioStreamParams& audioParams)
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    const pa_sample_spec* paSampleSpec = pa_stream_get_sample_spec(paStream);

    audioParams = ConvertFromPAAudioParams(*paSampleSpec);
    return AUDIO_CLIENT_SUCCESS;
}

uint32_t AudioServiceClient::GetStreamVolume(uint32_t sessionID)
{
    return DEFAULT_STREAM_VOLUME;
}

int32_t AudioServiceClient::GetCurrentTimeStamp(uint64_t &timeStamp)
{
    CHECK_PA_STATUS_RET_IF_FAIL(mainLoop, context, paStream, AUDIO_CLIENT_PA_ERR);
    int32_t retVal = 0;

    pa_threaded_mainloop_lock(mainLoop);
    retVal = pa_stream_get_time(paStream, &timeStamp);
    pa_threaded_mainloop_unlock(mainLoop);

    if (retVal >= 0)
        return AUDIO_CLIENT_SUCCESS;
    else
        return AUDIO_CLIENT_ERR;
}

void AudioServiceClient::RegisterAudioRendererCallbacks(const AudioRendererCallbacks &cb)
{
    MEDIA_INFO_LOG("Registering audio render callbacks");
    mAudioRendererCallbacks = (AudioRendererCallbacks *) &cb;
}

void AudioServiceClient::RegisterAudioRecorderCallbacks(const AudioRecorderCallbacks &cb)
{
    MEDIA_INFO_LOG("Registering audio record callbacks");
    mAudioRecorderCallbacks = (AudioRecorderCallbacks *) &cb;
}
} // namespace AudioStandard
} // namespace OHOS
