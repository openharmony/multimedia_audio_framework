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

#ifdef FEATURE_POWER_MANAGER
#include "power_mgr_client.h"
#endif

#include "pa_renderer_stream_impl.h"

#include <chrono>

#include "safe_map.h"
#include "pa_adapter_tools.h"
#include "audio_errors.h"
#include "audio_log.h"
#include "audio_utils.h"
#include "i_audio_renderer_sink.h"

namespace OHOS {
namespace AudioStandard {
static SafeMap<PaRendererStreamImpl *, bool> rendererStreamInstanceMap_;
const uint32_t DOUBLE_VALUE = 2;
const uint32_t MAX_LENGTH_OFFLOAD = 500;
const int32_t OFFLOAD_HDI_CACHE1 = 200; // ms, should equal with val in hdi_sink.c
const int32_t OFFLOAD_HDI_CACHE2 = 7000; // ms, should equal with val in hdi_sink.c
const uint32_t OFFLOAD_BUFFER = 50;
const uint64_t AUDIO_US_PER_MS = 1000;
const uint64_t AUDIO_NS_PER_US = 1000;

static int32_t CheckReturnIfStreamInvalid(pa_stream *paStream, const int32_t retVal)
{
    do {
        if (!(paStream && PA_STREAM_IS_GOOD(pa_stream_get_state(paStream)))) {
            return retVal;
        }
    } while (false);
    return SUCCESS;
}

PaRendererStreamImpl::PaRendererStreamImpl(pa_stream *paStream, AudioProcessConfig processConfig,
    pa_threaded_mainloop *mainloop)
{
    mainloop_ = mainloop;
    paStream_ = paStream;
    processConfig_ = processConfig;
}

PaRendererStreamImpl::~PaRendererStreamImpl()
{
    AUDIO_DEBUG_LOG("~PaRendererStreamImpl");
    std::lock_guard<std::mutex> lock(rendererStreamLock_);
    rendererStreamInstanceMap_.Erase(this);
}

int32_t PaRendererStreamImpl::InitParams()
{
    rendererStreamInstanceMap_.Insert(this, true);
    PaLockGuard lock(mainloop_);
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }

    sinkInputIndex_ = pa_stream_get_index(paStream_);
    pa_stream_set_moved_callback(paStream_, PAStreamMovedCb,
        reinterpret_cast<void *>(this)); // used to notify sink/source moved
    pa_stream_set_write_callback(paStream_, PAStreamWriteCb, reinterpret_cast<void *>(this));
    pa_stream_set_underflow_callback(paStream_, PAStreamUnderFlowCb, reinterpret_cast<void *>(this));
    pa_stream_set_started_callback(paStream_, PAStreamSetStartedCb, reinterpret_cast<void *>(this));

    // Get byte size per frame
    const pa_sample_spec *sampleSpec = pa_stream_get_sample_spec(paStream_);
    AUDIO_INFO_LOG("sampleSpec: channels: %{public}u, formats: %{public}d, rate: %{public}d", sampleSpec->channels,
        sampleSpec->format, sampleSpec->rate);

    if (sampleSpec->channels != processConfig_.streamInfo.channels) {
        AUDIO_WARNING_LOG("Unequal channels, in server: %{public}d, in client: %{public}d", sampleSpec->channels,
            processConfig_.streamInfo.channels);
    }
    if (static_cast<uint8_t>(sampleSpec->format) != processConfig_.streamInfo.format) { // In plan
        AUDIO_WARNING_LOG("Unequal format, in server: %{public}d, in client: %{public}d", sampleSpec->format,
            processConfig_.streamInfo.format);
    }
    byteSizePerFrame_ = pa_frame_size(sampleSpec);

    // Get min buffer size in frame
    const pa_buffer_attr *bufferAttr = pa_stream_get_buffer_attr(paStream_);
    if (bufferAttr == nullptr) {
        AUDIO_ERR_LOG("pa_stream_get_buffer_attr returned nullptr");
        return ERR_OPERATION_FAILED;
    }
    minBufferSize_ = (size_t)bufferAttr->minreq;
    if (byteSizePerFrame_ == 0) {
        AUDIO_ERR_LOG("byteSizePerFrame_ should not be zero.");
        return ERR_INVALID_PARAM;
    }
    spanSizeInFrame_ = minBufferSize_ / byteSizePerFrame_;

    lock.Unlock();
    // In plan: Get data from xml
    effectSceneName_ = GetEffectSceneName(processConfig_.streamType);

    ResetOffload();

    DumpFileUtil::OpenDumpFile(DUMP_SERVER_PARA, DUMP_RENDERER_STREAM_FILENAME, &dumpFile_);
    return SUCCESS;
}

int32_t PaRendererStreamImpl::Start()
{
    AUDIO_INFO_LOG("Enter PaRendererStreamImpl::Start");
    PaLockGuard lock(mainloop_);
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }
    pa_operation *operation = nullptr;

    pa_stream_state_t state = pa_stream_get_state(paStream_);
    if (state != PA_STREAM_READY) {
        return ERR_OPERATION_FAILED;
    }

    streamCmdStatus_ = 0;
    operation = pa_stream_cork(paStream_, 0, PAStreamStartSuccessCb, reinterpret_cast<void *>(this));
    pa_operation_unref(operation);
    return SUCCESS;
}

int32_t PaRendererStreamImpl::Pause()
{
    AUDIO_INFO_LOG("Enter PaRendererStreamImpl::Pause");
    PaLockGuard lock(mainloop_);
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }

    pa_operation *operation = nullptr;
    pa_stream_state_t state = pa_stream_get_state(paStream_);
    if (state != PA_STREAM_READY) {
        AUDIO_ERR_LOG("Stream Stop Failed");
        return ERR_OPERATION_FAILED;
    }
    operation = pa_stream_cork(paStream_, 1, PAStreamPauseSuccessCb, reinterpret_cast<void *>(this));
    pa_operation_unref(operation);
    return SUCCESS;
}

int32_t PaRendererStreamImpl::Flush()
{
    AUDIO_INFO_LOG("Enter PaRendererStreamImpl::Flush");
    PaLockGuard lock(mainloop_);
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }

    pa_operation *operation = nullptr;
    pa_stream_state_t state = pa_stream_get_state(paStream_);
    if (state != PA_STREAM_READY) {
        AUDIO_ERR_LOG("Stream Flush Failed");
        return ERR_OPERATION_FAILED;
    }

    streamFlushStatus_ = 0;
    operation = pa_stream_flush(paStream_, PAStreamFlushSuccessCb, reinterpret_cast<void *>(this));
    if (operation == nullptr) {
        AUDIO_ERR_LOG("Stream Flush Operation Failed");
        return ERR_OPERATION_FAILED;
    }
    pa_operation_unref(operation);
    return SUCCESS;
}

int32_t PaRendererStreamImpl::Drain()
{
    AUDIO_INFO_LOG("Enter PaRendererStreamImpl::Drain");
    PaLockGuard lock(mainloop_);
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }
    isDrain_ = true;

    pa_operation *operation = nullptr;
    pa_stream_state_t state = pa_stream_get_state(paStream_);
    if (state != PA_STREAM_READY) {
        AUDIO_ERR_LOG("Stream drain failed, state is not ready");
        return ERR_OPERATION_FAILED;
    }
    streamDrainStatus_ = 0;
    operation = pa_stream_drain(paStream_, PAStreamDrainSuccessCb, reinterpret_cast<void *>(this));
    pa_operation_unref(operation);
    return SUCCESS;
}

int32_t PaRendererStreamImpl::Stop()
{
    AUDIO_INFO_LOG("Enter PaRendererStreamImpl::Stop");
    state_ = STOPPING;
    PaLockGuard lock(mainloop_);

    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }

    pa_operation *operation = pa_stream_cork(paStream_, 1, PaRendererStreamImpl::PAStreamAsyncStopSuccessCb,
        reinterpret_cast<void *>(this));
    CHECK_AND_RETURN_RET_LOG(operation != nullptr, ERR_OPERATION_FAILED, "pa_stream_cork operation is null");
    pa_operation_unref(operation);
    return SUCCESS;
}

int32_t PaRendererStreamImpl::Release()
{
    std::shared_ptr<IStatusCallback> statusCallback = statusCallback_.lock();
    if (statusCallback != nullptr) {
        statusCallback->OnStatusUpdate(OPERATION_RELEASED);
    }
    state_ = RELEASED;
    PaLockGuard lock(mainloop_);
    if (paStream_) {
        pa_stream_set_state_callback(paStream_, nullptr, nullptr);
        pa_stream_set_write_callback(paStream_, nullptr, nullptr);
        pa_stream_set_latency_update_callback(paStream_, nullptr, nullptr);
        pa_stream_set_underflow_callback(paStream_, nullptr, nullptr);
        pa_stream_set_moved_callback(paStream_, nullptr, nullptr);
        pa_stream_set_started_callback(paStream_, nullptr, nullptr);

        pa_stream_disconnect(paStream_);
        pa_stream_unref(paStream_);
        paStream_ = nullptr;
    }
    DumpFileUtil::CloseDumpFile(&dumpFile_);
    return SUCCESS;
}

int32_t PaRendererStreamImpl::GetStreamFramesWritten(uint64_t &framesWritten)
{
    CHECK_AND_RETURN_RET_LOG(byteSizePerFrame_ != 0, ERR_ILLEGAL_STATE, "Error frame size");
    framesWritten = totalBytesWritten_ / byteSizePerFrame_;
    return SUCCESS;
}

int32_t PaRendererStreamImpl::GetCurrentTimeStamp(uint64_t &timeStamp)
{
    PaLockGuard lock(mainloop_);
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }

    pa_operation *operation = pa_stream_update_timing_info(paStream_, NULL, NULL);
    if (operation != nullptr) {
        while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
            pa_threaded_mainloop_wait(mainloop_);
        }
        pa_operation_unref(operation);
    } else {
        AUDIO_ERR_LOG("pa_stream_update_timing_info failed");
    }

    const pa_timing_info *info = pa_stream_get_timing_info(paStream_);
    if (info == nullptr) {
        AUDIO_ERR_LOG("pa_stream_get_timing_info failed");
        return ERR_OPERATION_FAILED;
    }

    const pa_sample_spec *sampleSpec = pa_stream_get_sample_spec(paStream_);
    timeStamp = pa_bytes_to_usec(info->write_index, sampleSpec);
    return SUCCESS;
}

int32_t PaRendererStreamImpl::GetLatency(uint64_t &latency)
{
    PaLockGuard lock(mainloop_);
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }
    pa_usec_t paLatency {0};
    pa_usec_t cacheLatency {0};
    int32_t negative {0};

    // Get PA latency
    while (true) {
        pa_operation *operation = pa_stream_update_timing_info(paStream_, NULL, NULL);
        if (operation != nullptr) {
            pa_operation_unref(operation);
        } else {
            AUDIO_ERR_LOG("pa_stream_update_timing_info failed");
        }
        if (pa_stream_get_latency(paStream_, &paLatency, &negative) >= 0) {
            if (negative) {
                latency = 0;
                return ERR_OPERATION_FAILED;
            }
            break;
        }
        AUDIO_INFO_LOG("waiting for audio latency information");
        pa_threaded_mainloop_wait(mainloop_);
    }
    lock.Unlock();
    cacheLatency = 0;
    // In plan: Total latency will be sum of audio write cache latency plus PA latency
    uint64_t fwLatency = paLatency + cacheLatency;
    uint64_t sinkLatency = sinkLatencyInMsec_ * PA_USEC_PER_MSEC;
    if (fwLatency > sinkLatency) {
        latency = fwLatency - sinkLatency;
    } else {
        latency = fwLatency;
    }
    AUDIO_DEBUG_LOG("total latency: %{public}" PRIu64 ", pa latency: %{public}"
        PRIu64 ", cache latency: %{public}" PRIu64, latency, paLatency, cacheLatency);

    return SUCCESS;
}

int32_t PaRendererStreamImpl::SetRate(int32_t rate)
{
    AUDIO_INFO_LOG("SetRate in");
    PaLockGuard lock(mainloop_);
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }
    uint32_t currentRate = processConfig_.streamInfo.samplingRate;
    switch (rate) {
        case RENDER_RATE_NORMAL:
            break;
        case RENDER_RATE_DOUBLE:
            currentRate *= DOUBLE_VALUE;
            break;
        case RENDER_RATE_HALF:
            currentRate /= DOUBLE_VALUE;
            break;
        default:
            return ERR_INVALID_PARAM;
    }
    renderRate_ = rate;

    pa_operation *operation = pa_stream_update_sample_rate(paStream_, currentRate, nullptr, nullptr);
    if (operation != nullptr) {
        pa_operation_unref(operation);
    } else {
        AUDIO_ERR_LOG("SetRate: operation is nullptr");
    }
    return SUCCESS;
}

int32_t PaRendererStreamImpl::SetLowPowerVolume(float powerVolume)
{
    AUDIO_INFO_LOG("SetLowPowerVolume: %{public}f", powerVolume);
    PaLockGuard lock(mainloop_);
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }

    /* Validate and return INVALID_PARAMS error */
    if ((powerVolume < MIN_STREAM_VOLUME_LEVEL) || (powerVolume > MAX_STREAM_VOLUME_LEVEL)) {
        AUDIO_ERR_LOG("Invalid Power Volume Set!");
        return -1;
    }

    powerVolumeFactor_ = powerVolume;
    pa_proplist *propList = pa_proplist_new();
    if (propList == nullptr) {
        AUDIO_ERR_LOG("pa_proplist_new failed");
        return ERR_OPERATION_FAILED;
    }

    pa_proplist_sets(propList, "stream.powerVolumeFactor", std::to_string(powerVolumeFactor_).c_str());
    pa_operation *updatePropOperation = pa_stream_proplist_update(paStream_, PA_UPDATE_REPLACE, propList,
        nullptr, nullptr);
    pa_proplist_free(propList);
    pa_operation_unref(updatePropOperation);

    // In plan: Call reset volume

    return SUCCESS;
}

int32_t PaRendererStreamImpl::GetLowPowerVolume(float &powerVolume)
{
    powerVolume = powerVolumeFactor_;
    return SUCCESS;
}

int32_t PaRendererStreamImpl::SetAudioEffectMode(int32_t effectMode)
{
    AUDIO_INFO_LOG("SetAudioEffectMode: %{public}d", effectMode);
    PaLockGuard lock(mainloop_);
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }

    effectMode_ = effectMode;
    const std::string effectModeName = GetEffectModeName(effectMode_);

    pa_proplist *propList = pa_proplist_new();
    if (propList == nullptr) {
        AUDIO_ERR_LOG("pa_proplist_new failed");
        return ERR_OPERATION_FAILED;
    }

    pa_proplist_sets(propList, "scene.mode", effectModeName.c_str());
    pa_operation *updatePropOperation = pa_stream_proplist_update(paStream_, PA_UPDATE_REPLACE, propList,
        nullptr, nullptr);
    pa_proplist_free(propList);
    pa_operation_unref(updatePropOperation);

    return SUCCESS;
}

const std::string PaRendererStreamImpl::GetEffectModeName(int32_t effectMode)
{
    std::string name;
    switch (effectMode) {
        case 0: // AudioEffectMode::EFFECT_NONE
            name = "EFFECT_NONE";
            break;
        default:
            name = "EFFECT_DEFAULT";
    }

    const std::string modeName = name;
    return modeName;
}

int32_t PaRendererStreamImpl::GetAudioEffectMode(int32_t &effectMode)
{
    effectMode = effectMode_;
    return SUCCESS;
}

int32_t PaRendererStreamImpl::SetPrivacyType(int32_t privacyType)
{
    AUDIO_DEBUG_LOG("SetInnerCapturerState: %{public}d", privacyType);
    privacyType_ = privacyType;
    return SUCCESS;
}

int32_t PaRendererStreamImpl::GetPrivacyType(int32_t &privacyType)
{
    privacyType_ = privacyType;
    return SUCCESS;
}


void PaRendererStreamImpl::RegisterStatusCallback(const std::weak_ptr<IStatusCallback> &callback)
{
    AUDIO_DEBUG_LOG("RegisterStatusCallback in");
    statusCallback_ = callback;
}

void PaRendererStreamImpl::RegisterWriteCallback(const std::weak_ptr<IWriteCallback> &callback)
{
    AUDIO_DEBUG_LOG("RegisterWriteCallback in");
    writeCallback_ = callback;
}

BufferDesc PaRendererStreamImpl::DequeueBuffer(size_t length)
{
    BufferDesc bufferDesc;
    bufferDesc.bufLength = length;
    // DequeueBuffer is called in PAStreamWriteCb which is running in mainloop, so don't need lock mainloop.
    pa_stream_begin_write(paStream_, reinterpret_cast<void **>(&bufferDesc.buffer), &bufferDesc.bufLength);
    return bufferDesc;
}

int32_t PaRendererStreamImpl::EnqueueBuffer(const BufferDesc &bufferDesc)
{
    Trace trace("PaRendererStreamImpl::EnqueueBuffer " + std::to_string(bufferDesc.bufLength) + " totalBytesWritten" +
        std::to_string(totalBytesWritten_));
    int32_t error = 0;
    error = OffloadUpdatePolicyInWrite();
    CHECK_AND_RETURN_RET_LOG(error == SUCCESS, error, "OffloadUpdatePolicyInWrite failed");

    // EnqueueBuffer is called in mainloop in most cases and don't need lock.
    bool isInMainloop = pa_threaded_mainloop_in_thread(mainloop_) ? true : false;
    if (!isInMainloop) {
        pa_threaded_mainloop_lock(mainloop_);
    }

    if (paStream_ == nullptr) {
        AUDIO_ERR_LOG("paStream is nullptr");
        if (!isInMainloop) {
            pa_threaded_mainloop_unlock(mainloop_);
        }
        return ERR_ILLEGAL_STATE;
    }

    error = pa_stream_write(paStream_, static_cast<void*>(bufferDesc.buffer), bufferDesc.bufLength, nullptr,
        0LL, PA_SEEK_RELATIVE);
    if (error < 0) {
        AUDIO_ERR_LOG("Write stream failed");
        pa_stream_cancel_write(paStream_);
    }

    if (!isInMainloop) {
        pa_threaded_mainloop_unlock(mainloop_);
    }
    totalBytesWritten_ += bufferDesc.bufLength;
    return SUCCESS;
}

void PaRendererStreamImpl::PAStreamWriteCb(pa_stream *stream, size_t length, void *userdata)
{
    CHECK_AND_RETURN_LOG(userdata, "PAStreamWriteCb: userdata is null");

    auto streamImpl = static_cast<PaRendererStreamImpl *>(userdata);
    if (streamImpl->abortFlag_ != 0) {
        AUDIO_ERR_LOG("PAStreamWriteCb: Abort pa stream write callback");
        streamImpl->abortFlag_--;
        return ;
    }
    Trace trace("PaRendererStreamImpl::PAStreamWriteCb sink-input:" + std::to_string(streamImpl->sinkInputIndex_) +
        " length:" + std::to_string(length));
    std::shared_ptr<IWriteCallback> writeCallback = streamImpl->writeCallback_.lock();
    if (writeCallback != nullptr) {
        writeCallback->OnWriteData(length);
    } else {
        AUDIO_ERR_LOG("Write callback is nullptr");
    }
}

void PaRendererStreamImpl::PAStreamMovedCb(pa_stream *stream, void *userdata)
{
    CHECK_AND_RETURN_LOG(userdata, "PAStreamMovedCb: userdata is null");

    // get stream informations.
    uint32_t deviceIndex = pa_stream_get_device_index(stream); // pa_context_get_sink_info_by_index

    // Return 1 if the sink or source this stream is connected to has been suspended.
    // This will return 0 if not, and a negative value on error.
    int res = pa_stream_is_suspended(stream);
    AUDIO_DEBUG_LOG("PAstream moved to index:[%{public}d] suspended:[%{public}d]",
        deviceIndex, res);
}

void PaRendererStreamImpl::PAStreamUnderFlowCb(pa_stream *stream, void *userdata)
{
    Trace trace("PaRendererStreamImpl::PAStreamUnderFlowCb");
    CHECK_AND_RETURN_LOG(userdata, "PAStreamUnderFlowCb: userdata is null");

    PaRendererStreamImpl *streamImpl = static_cast<PaRendererStreamImpl *>(userdata);
    streamImpl->underFlowCount_++;
    std::shared_ptr<IStatusCallback> statusCallback = streamImpl->statusCallback_.lock();
    if (statusCallback != nullptr) {
        statusCallback->OnStatusUpdate(OPERATION_UNDERRUN);
    }
    AUDIO_WARNING_LOG("PaRendererStreamImpl underrun: %{public}d!", streamImpl->underFlowCount_);
}


void PaRendererStreamImpl::PAStreamSetStartedCb(pa_stream *stream, void *userdata)
{
    CHECK_AND_RETURN_LOG(userdata, "PAStreamSetStartedCb: userdata is null");
    AUDIO_WARNING_LOG("PAStreamSetStartedCb");
    Trace trace("PaRendererStreamImpl::PAStreamSetStartedCb");
}

void PaRendererStreamImpl::PAStreamStartSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    AUDIO_INFO_LOG("PAStreamStartSuccessCb in");
    CHECK_AND_RETURN_LOG(userdata, "PAStreamStartSuccessCb: userdata is null");

    PaRendererStreamImpl *streamImpl = static_cast<PaRendererStreamImpl *>(userdata);
    streamImpl->state_ = RUNNING;
    streamImpl->offloadTsLast_ = 0;
    std::shared_ptr<IStatusCallback> statusCallback = streamImpl->statusCallback_.lock();
    if (statusCallback != nullptr) {
        statusCallback->OnStatusUpdate(OPERATION_STARTED);
    }
    streamImpl->streamCmdStatus_ = success;
}

void PaRendererStreamImpl::PAStreamPauseSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    CHECK_AND_RETURN_LOG(userdata, "PAStreamPauseSuccessCb: userdata is null");

    PaRendererStreamImpl *streamImpl = static_cast<PaRendererStreamImpl *>(userdata);
    streamImpl->state_ = PAUSED;
    streamImpl->offloadTsLast_ = 0;
    streamImpl->ResetOffload();
    std::shared_ptr<IStatusCallback> statusCallback = streamImpl->statusCallback_.lock();
    if (statusCallback != nullptr) {
        statusCallback->OnStatusUpdate(OPERATION_PAUSED);
    }
    streamImpl->streamCmdStatus_ = success;
}

void PaRendererStreamImpl::PAStreamFlushSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    CHECK_AND_RETURN_LOG(userdata, "PAStreamFlushSuccessCb: userdata is null");

    PaRendererStreamImpl *streamImpl = static_cast<PaRendererStreamImpl *>(userdata);
    std::shared_ptr<IStatusCallback> statusCallback = streamImpl->statusCallback_.lock();
    if (statusCallback != nullptr) {
        statusCallback->OnStatusUpdate(OPERATION_FLUSHED);
    }
    streamImpl->streamFlushStatus_ = success;
}

void PaRendererStreamImpl::PAStreamDrainSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    CHECK_AND_RETURN_LOG(userdata, "PAStreamDrainSuccessCb: userdata is null");

    PaRendererStreamImpl *streamImpl = static_cast<PaRendererStreamImpl *>(userdata);
    bool tempBool = true;
    if (rendererStreamInstanceMap_.Find(streamImpl, tempBool) == false) {
        AUDIO_ERR_LOG("streamImpl is null");
        return;
    }
    std::lock_guard<std::mutex> lock(streamImpl->rendererStreamLock_);
    std::shared_ptr<IStatusCallback> statusCallback = streamImpl->statusCallback_.lock();
    if (statusCallback != nullptr) {
        statusCallback->OnStatusUpdate(OPERATION_DRAINED);
    }
    streamImpl->streamDrainStatus_ = success;
    streamImpl->isDrain_ = false;
}

void PaRendererStreamImpl::PAStreamDrainInStopCb(pa_stream *stream, int32_t success, void *userdata)
{
    CHECK_AND_RETURN_LOG(userdata, "PAStreamDrainInStopCb: userdata is null");

    PaRendererStreamImpl *streamImpl = static_cast<PaRendererStreamImpl *>(userdata);
    pa_operation *operation = pa_stream_cork(streamImpl->paStream_, 1,
        PaRendererStreamImpl::PAStreamAsyncStopSuccessCb, userdata);

    pa_operation_unref(operation);
    streamImpl->streamDrainStatus_ = success;
}

void PaRendererStreamImpl::PAStreamAsyncStopSuccessCb(pa_stream *stream, int32_t success, void *userdata)
{
    AUDIO_DEBUG_LOG("PAStreamAsyncStopSuccessCb in");
    CHECK_AND_RETURN_LOG(userdata, "PAStreamAsyncStopSuccessCb: userdata is null");

    PaRendererStreamImpl *streamImpl = static_cast<PaRendererStreamImpl *>(userdata);
    streamImpl->state_ = STOPPED;
    std::shared_ptr<IStatusCallback> statusCallback = streamImpl->statusCallback_.lock();

    if (statusCallback != nullptr) {
        statusCallback->OnStatusUpdate(OPERATION_STOPPED);
    }
}

int32_t PaRendererStreamImpl::GetMinimumBufferSize(size_t &minBufferSize) const
{
    minBufferSize = minBufferSize_;
    return SUCCESS;
}

void PaRendererStreamImpl::GetByteSizePerFrame(size_t &byteSizePerFrame) const
{
    byteSizePerFrame = byteSizePerFrame_;
}

void PaRendererStreamImpl::GetSpanSizePerFrame(size_t &spanSizeInFrame) const
{
    spanSizeInFrame = spanSizeInFrame_;
}

void PaRendererStreamImpl::SetStreamIndex(uint32_t index)
{
    AUDIO_INFO_LOG("Using index/sessionId %{public}d", index);
    streamIndex_ = index;
}

uint32_t PaRendererStreamImpl::GetStreamIndex()
{
    return streamIndex_;
}

const std::string PaRendererStreamImpl::GetEffectSceneName(AudioStreamType audioType)
{
    std::string name;
    switch (audioType) {
        case STREAM_MUSIC:
            name = "SCENE_MUSIC";
            break;
        case STREAM_GAME:
            name = "SCENE_GAME";
            break;
        case STREAM_MOVIE:
            name = "SCENE_MOVIE";
            break;
        case STREAM_SPEECH:
        case STREAM_VOICE_CALL:
        case STREAM_VOICE_ASSISTANT:
            name = "SCENE_SPEECH";
            break;
        case STREAM_RING:
        case STREAM_ALARM:
        case STREAM_NOTIFICATION:
        case STREAM_SYSTEM:
        case STREAM_DTMF:
        case STREAM_SYSTEM_ENFORCED:
            name = "SCENE_RING";
            break;
        default:
            name = "SCENE_OTHERS";
    }

    const std::string sceneName = name;
    return sceneName;
}

void PaRendererStreamImpl::AbortCallback(int32_t abortTimes)
{
    abortFlag_ += abortTimes;
}

// offload

size_t PaRendererStreamImpl::GetWritableSize()
{
    PaLockGuard lock(mainloop_);
    if (paStream_ == nullptr) {
        return 0;
    }
    return pa_stream_writable_size(paStream_);
}

int32_t PaRendererStreamImpl::OffloadSetVolume(float volume)
{
    if (!offloadEnable_) {
        return ERR_OPERATION_FAILED;
    }
    IAudioRendererSink *audioRendererSinkInstance = IAudioRendererSink::GetInstance("offload", "");

    if (audioRendererSinkInstance == nullptr) {
        AUDIO_ERR_LOG("Renderer is null.");
        return ERROR;
    }
    return audioRendererSinkInstance->SetVolume(volume, 0);
}

int32_t PaRendererStreamImpl::OffloadGetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec)
{
    auto *audioRendererSinkInstance = static_cast<IOffloadAudioRendererSink*> (IAudioRendererSink::GetInstance(
        "offload", ""));

    CHECK_AND_RETURN_RET_LOG(audioRendererSinkInstance != nullptr, ERROR, "Renderer is null.");
    return audioRendererSinkInstance->GetPresentationPosition(frames, timeSec, timeNanoSec);
}

int32_t PaRendererStreamImpl::OffloadSetBufferSize(uint32_t sizeMs)
{
    auto *audioRendererSinkInstance = static_cast<IOffloadAudioRendererSink*> (IAudioRendererSink::GetInstance(
        "offload", ""));

    CHECK_AND_RETURN_RET_LOG(audioRendererSinkInstance != nullptr, ERROR, "Renderer is null.");
    return audioRendererSinkInstance->SetBufferSize(sizeMs);
}

int32_t PaRendererStreamImpl::GetOffloadApproximatelyCacheTime(uint64_t &timeStamp, uint64_t &paWriteIndex,
    uint64_t &cacheTimeDsp, uint64_t &cacheTimePa)
{
    if (!offloadEnable_) {
        return ERR_OPERATION_FAILED;
    }
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        return ERR_ILLEGAL_STATE;
    }
    PaLockGuard lock(mainloop_);

    pa_operation *operation = pa_stream_update_timing_info(paStream_, NULL, NULL);
    if (operation != nullptr) {
        pa_operation_unref(operation);
    } else {
        AUDIO_ERR_LOG("pa_stream_update_timing_info failed");
    }

    const pa_timing_info *info = pa_stream_get_timing_info(paStream_);
    if (info == nullptr) {
        AUDIO_WARNING_LOG("pa_stream_get_timing_info failed");
        return SUCCESS;
    }

    const pa_sample_spec *sampleSpec = pa_stream_get_sample_spec(paStream_);
    uint64_t readIndex = pa_bytes_to_usec(info->read_index, sampleSpec);
    uint64_t writeIndex = pa_bytes_to_usec(info->write_index, sampleSpec);
    timeStamp = info->timestamp.tv_sec * AUDIO_US_PER_SECOND + info->timestamp.tv_usec;
    lock.Unlock();

    int64_t cacheTimeInPulse = writeIndex > readIndex ? writeIndex - readIndex : 0;
    cacheTimePa = static_cast<uint64_t>(cacheTimeInPulse);
    paWriteIndex = writeIndex;

    bool first = offloadTsLast_ == 0;
    offloadTsLast_ = readIndex;

    uint64_t frames;
    int64_t timeSec;
    int64_t timeNanoSec;
    OffloadGetPresentationPosition(frames, timeSec, timeNanoSec);
    int64_t timeDelta = static_cast<int64_t>(timeStamp) -
                        static_cast<int64_t>(timeSec * AUDIO_US_PER_SECOND + timeNanoSec / AUDIO_NS_PER_US);
    int64_t framesInt = static_cast<int64_t>(frames) + timeDelta;
    framesInt = framesInt > 0 ? framesInt : 0;
    int64_t readIndexInt = static_cast<int64_t>(readIndex);
    if (framesInt + offloadTsOffset_ < readIndexInt - static_cast<int64_t>(
        (OFFLOAD_HDI_CACHE2 + MAX_LENGTH_OFFLOAD + OFFLOAD_BUFFER) * AUDIO_US_PER_MS) ||
        framesInt + offloadTsOffset_ > readIndexInt || first) {
        offloadTsOffset_ = readIndexInt - framesInt;
    }
    cacheTimeDsp = static_cast<uint64_t>(readIndexInt - (framesInt + offloadTsOffset_));
    return SUCCESS;
}

int32_t PaRendererStreamImpl::OffloadUpdatePolicyInWrite()
{
    int error = 0;
    if ((lastOffloadUpdateFinishTime_ != 0) &&
        (std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) > lastOffloadUpdateFinishTime_)) {
        AUDIO_INFO_LOG("PaWriteStream switching curTime %{public}" PRIu64 ", switchTime %{public}" PRIu64,
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), lastOffloadUpdateFinishTime_);
        error = OffloadUpdatePolicy(offloadNextStateTargetPolicy_, true);
    }
    return error;
}

void PaRendererStreamImpl::SyncOffloadMode()
{
    std::shared_ptr<IStatusCallback> statusCallback = statusCallback_.lock();
    if (statusCallback != nullptr) {
        if (offloadEnable_) {
            statusCallback->OnStatusUpdate(OPERATION_SET_OFFLOAD_ENABLE);
        } else {
            statusCallback->OnStatusUpdate(OPERATION_UNSET_OFFLOAD_ENABLE);
        }
    }
}

void PaRendererStreamImpl::ResetOffload()
{
    offloadEnable_ = false;
    SyncOffloadMode();
    offloadTsOffset_ = 0;
    offloadTsLast_ = 0;
    OffloadUpdatePolicy(OFFLOAD_DEFAULT, true);
}

int32_t PaRendererStreamImpl::OffloadUpdatePolicy(AudioOffloadType statePolicy, bool force)
{
    if (CheckReturnIfStreamInvalid(paStream_, ERR_ILLEGAL_STATE) < 0) {
        AUDIO_ERR_LOG("Set offload mode: invalid stream state, quit SetStreamOffloadMode due err");
        return ERR_ILLEGAL_STATE;
    }
    // if possible turn on the buffer immediately(long buffer -> short buffer), turn it at once.
    if (statePolicy < offloadStatePolicy_ || offloadStatePolicy_ == OFFLOAD_DEFAULT || force) {
        AUDIO_DEBUG_LOG("Update statePolicy immediately: %{public}d -> %{public}d, force(%d)",
            offloadStatePolicy_, statePolicy, force);
        lastOffloadUpdateFinishTime_ = 0;
        PaLockGuard lock(mainloop_);
        pa_proplist *propList = pa_proplist_new();
        CHECK_AND_RETURN_RET_LOG(propList != nullptr, ERR_OPERATION_FAILED, "pa_proplist_new failed");
        if (offloadEnable_) {
            pa_proplist_sets(propList, "stream.offload.enable", "1");
        } else {
            pa_proplist_sets(propList, "stream.offload.enable", "0");
        }
        pa_proplist_sets(propList, "stream.offload.statePolicy", std::to_string(statePolicy).c_str());

        pa_operation *updatePropOperation =
            pa_stream_proplist_update(paStream_, PA_UPDATE_REPLACE, propList, nullptr, nullptr);
        if (updatePropOperation == nullptr) {
            AUDIO_ERR_LOG("pa_stream_proplist_update failed!");
            pa_proplist_free(propList);
            return ERR_OPERATION_FAILED;
        }
        pa_proplist_free(propList);
        pa_operation_unref(updatePropOperation);

        if (!(statePolicy == OFFLOAD_DEFAULT &&
              (offloadStatePolicy_ == OFFLOAD_DEFAULT || offloadStatePolicy_ == OFFLOAD_ACTIVE_FOREGROUND))) {
            const uint32_t bufLenMs = statePolicy > 1 ? OFFLOAD_HDI_CACHE2 : OFFLOAD_HDI_CACHE1;
            OffloadSetBufferSize(bufLenMs);
        }

        offloadStatePolicy_ = statePolicy;
        offloadNextStateTargetPolicy_ = statePolicy; // Fix here if sometimes can't cut into state 3
    } else {
        // Otherwise, hdi_sink.c's times detects the stateTarget change and switches later
        // this time is checked the PaWriteStream to check if the switch has been made
        AUDIO_DEBUG_LOG("Update statePolicy in 3 seconds: %{public}d -> %{public}d", offloadStatePolicy_, statePolicy);
        lastOffloadUpdateFinishTime_ = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now() + std::chrono::seconds(3)); // add 3s latency to change offload state
        offloadNextStateTargetPolicy_ = statePolicy;
    }

    return SUCCESS;
}

int32_t PaRendererStreamImpl::SetOffloadMode(int32_t state, bool isAppBack)
{
#ifdef FEATURE_POWER_MANAGER
    AudioOffloadType statePolicy = OFFLOAD_DEFAULT;
    auto powerState = static_cast<PowerMgr::PowerState>(state);
    if ((powerState != PowerMgr::PowerState::AWAKE) && (powerState != PowerMgr::PowerState::FREEZE)) {
        statePolicy = OFFLOAD_INACTIVE_BACKGROUND;
    } else {
        statePolicy = OFFLOAD_ACTIVE_FOREGROUND;
    }

    if (statePolicy == OFFLOAD_DEFAULT) {
        AUDIO_ERR_LOG("impossible INPUT branch error");
        return ERR_OPERATION_FAILED;
    }

    AUDIO_INFO_LOG("calling set stream offloadMode PowerState: %{public}d, isAppBack: %{public}d", state, isAppBack);

    if (offloadNextStateTargetPolicy_ == statePolicy) {
        return SUCCESS;
    }

    if ((offloadStatePolicy_ == offloadNextStateTargetPolicy_) && (offloadStatePolicy_ == statePolicy)) {
        return SUCCESS;
    }

    offloadEnable_ = true;
    SyncOffloadMode();
    if (OffloadUpdatePolicy(statePolicy, false) != SUCCESS) {
        return ERR_OPERATION_FAILED;
    }
    if (statePolicy == OFFLOAD_ACTIVE_FOREGROUND) {
        pa_threaded_mainloop_signal(mainloop_, 0);
    }
#else
    AUDIO_INFO_LOG("SetStreamOffloadMode not available, FEATURE_POWER_MANAGER no define");
#endif
    return SUCCESS;
}

int32_t PaRendererStreamImpl::UnsetOffloadMode()
{
    offloadEnable_ = false;
    SyncOffloadMode();
    return OffloadUpdatePolicy(OFFLOAD_DEFAULT, true);
}
// offload end
} // namespace AudioStandard
} // namespace OHOS
