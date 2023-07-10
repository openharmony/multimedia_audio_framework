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

#ifndef ST_PULSEAUDIO_AUDIO_SERVICE_ADAPTER_IMPL_H
#define ST_PULSEAUDIO_AUDIO_SERVICE_ADAPTER_IMPL_H

#include "pulse_audio_service_adapter_impl.h"

#include <sstream>
#include <unistd.h>

#include "audio_errors.h"
#include "audio_log.h"
#include "hisysevent.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
static unique_ptr<AudioServiceAdapterCallback> g_audioServiceAdapterCallback;
std::unordered_map<uint32_t, uint32_t> PulseAudioServiceAdapterImpl::sinkIndexSessionIDMap;

AudioServiceAdapter::~AudioServiceAdapter() = default;
PulseAudioServiceAdapterImpl::~PulseAudioServiceAdapterImpl() = default;

unique_ptr<AudioServiceAdapter> AudioServiceAdapter::CreateAudioAdapter(unique_ptr<AudioServiceAdapterCallback> cb)
{
    if (!cb) {
        AUDIO_ERR_LOG("AudioServiceAdapter::CreateAudioAdapter cb is nullptr!");
        return nullptr;
    }
    return make_unique<PulseAudioServiceAdapterImpl>(cb);
}

PulseAudioServiceAdapterImpl::PulseAudioServiceAdapterImpl(unique_ptr<AudioServiceAdapterCallback> &cb)
{
    g_audioServiceAdapterCallback = move(cb);
}

bool PulseAudioServiceAdapterImpl::Connect()
{
    mMainLoop = pa_threaded_mainloop_new();
    if (!mMainLoop) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] MainLoop creation failed");
        return false;
    }

    if (pa_threaded_mainloop_start(mMainLoop) < 0) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Failed to start mainloop");
        pa_threaded_mainloop_free(mMainLoop);
        return false;
    }

    pa_threaded_mainloop_lock(mMainLoop);

    while (true) {
        pa_context_state_t state;

        if (mContext != nullptr) {
            state = pa_context_get_state(mContext);
            if (state == PA_CONTEXT_READY) {
                break;
            }
            // if pulseaudio is ready, retry connect to pulseaudio. before retry wait for sometime. reduce sleep later
            usleep(PA_CONNECT_RETRY_SLEEP_IN_MICRO_SECONDS);
        }

        bool result = ConnectToPulseAudio();
        if (!result || !PA_CONTEXT_IS_GOOD(pa_context_get_state(mContext))) {
            continue;
        }

        AUDIO_DEBUG_LOG("[PulseAudioServiceAdapterImpl] pa context not ready... wait");

        // Wait for the context to be ready
        pa_threaded_mainloop_wait(mMainLoop);
    }

    pa_threaded_mainloop_unlock(mMainLoop);

    return true;
}

bool PulseAudioServiceAdapterImpl::ConnectToPulseAudio()
{
    if (mContext != nullptr) {
        AUDIO_INFO_LOG("[PulseAudioServiceAdapterImpl] context is not null, disconnect first!");
        pa_context_disconnect(mContext);
        pa_context_set_state_callback(mContext, nullptr, nullptr);
        pa_context_set_subscribe_callback(mContext, nullptr, nullptr);
        pa_context_unref(mContext);
    }

    pa_proplist *proplist = pa_proplist_new();
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, "PulseAudio Service");
    pa_proplist_sets(proplist, PA_PROP_APPLICATION_ID, "com.ohos.pulseaudio.service");
    mContext = pa_context_new_with_proplist(pa_threaded_mainloop_get_api(mMainLoop), nullptr, proplist);
    pa_proplist_free(proplist);

    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] creating pa context failed");
        return false;
    }

    pa_context_set_state_callback(mContext,  PulseAudioServiceAdapterImpl::PaContextStateCb, this);
    if (pa_context_connect(mContext, nullptr, PA_CONTEXT_NOFAIL, nullptr) < 0) {
        if (pa_context_errno(mContext) == PA_ERR_INVALID) {
            AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa context connect failed: %{public}s",
                pa_strerror(pa_context_errno(mContext)));
            goto Fail;
        }
    }

    return true;

Fail:
    /* Make sure we don't get any further callbacks */
    pa_context_set_state_callback(mContext, nullptr, nullptr);
    pa_context_set_subscribe_callback(mContext, nullptr, nullptr);
    pa_context_unref(mContext);
    return false;
}

uint32_t PulseAudioServiceAdapterImpl::OpenAudioPort(string audioPortName, string moduleArgs)
{
    lock_guard<mutex> lock(mMutex);

    unique_ptr<UserData> userData = make_unique<UserData>();
    userData->thiz = this;

    pa_threaded_mainloop_lock(mMainLoop);
    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[OpenAudioPort] mContext is nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERROR;
    }

    pa_operation *operation = pa_context_load_module(mContext, audioPortName.c_str(), moduleArgs.c_str(),
        PaModuleLoadCb, reinterpret_cast<void*>(userData.get()));
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa_context_load_module returned nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return PA_INVALID_INDEX;
    }

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mMainLoop);
    }

    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    if (userData->idx == PA_INVALID_INDEX) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] OpenAudioPort returned invalid index");
        return PA_INVALID_INDEX;
    }

    return userData->idx;
}

int32_t PulseAudioServiceAdapterImpl::CloseAudioPort(int32_t audioHandleIndex)
{
    lock_guard<mutex> lock(mMutex);

    pa_threaded_mainloop_lock(mMainLoop);
    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[CloseAudioPort] mContext is nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERROR;
    }

    pa_operation *operation = pa_context_unload_module(mContext, audioHandleIndex, nullptr, nullptr);
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa_context_unload_module returned nullptr!");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERROR;
    }

    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);
    return SUCCESS;
}

int32_t PulseAudioServiceAdapterImpl::SuspendAudioDevice(string &audioPortName, bool isSuspend)
{
    AUDIO_INFO_LOG("SuspendAudioDevice: [%{public}s] : [%{public}d]", audioPortName.c_str(), isSuspend);
    pa_threaded_mainloop_lock(mMainLoop);
    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[SuspendAudioDevice] mContext is nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERROR;
    }

    auto suspendFlag = isSuspend ? 1 : 0;
    pa_operation *operation = pa_context_suspend_sink_by_name(mContext, audioPortName.c_str(), suspendFlag,
        nullptr, nullptr);
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa_context_suspend_sink_by_name failed!");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERR_OPERATION_FAILED;
    }

    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    return SUCCESS;
}

int32_t PulseAudioServiceAdapterImpl::SetDefaultSink(string name)
{
    pa_threaded_mainloop_lock(mMainLoop);
    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[SetDefaultSink] mContext is nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERROR;
    }

    pa_operation *operation = pa_context_set_default_sink(mContext, name.c_str(), nullptr, nullptr);
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa_context_set_default_sink failed!");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERR_OPERATION_FAILED;
    }
    isSetDefaultSink_ = true;
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    return SUCCESS;
}

int32_t PulseAudioServiceAdapterImpl::SetDefaultSource(string name)
{
    pa_threaded_mainloop_lock(mMainLoop);
    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[SetDefaultSource] mContext is nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERROR;
    }

    pa_operation *operation = pa_context_set_default_source(mContext, name.c_str(), nullptr, nullptr);
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa_context_set_default_source failed!");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERR_OPERATION_FAILED;
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    return SUCCESS;
}

void PulseAudioServiceAdapterImpl::PaGetSinksCb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
    UserData *userData = reinterpret_cast<UserData *>(userdata);
    PulseAudioServiceAdapterImpl *thiz = userData->thiz;
    if (eol < 0) {
        AUDIO_ERR_LOG("[PaGetSinksCb] Failed to get sink information: %{public}s",
            pa_strerror(pa_context_errno(c)));
        return;
    }

    if (eol) {
        pa_threaded_mainloop_signal(thiz->mMainLoop, 0);
        return;
    }

    if (i->proplist == nullptr) {
        AUDIO_ERR_LOG("[PaGetSinksCb] Invalid Proplist for sink (%{public}d).", i->index);
        return;
    }

    const char *adapterCStr = pa_proplist_gets(i->proplist, PA_PROP_DEVICE_STRING);
    AUDIO_INFO_LOG("[PaGetSinksCb] sink[%{public}d] device[%{public}s] name[%{public}s]", i->index, adapterCStr,
        i->name);
    std::string sinkDeviceName(adapterCStr);
    std::string sinkName(i->name);
    SinkInfo sinkInfo = {};
    sinkInfo.sinkId = i->index;
    sinkInfo.sinkName = sinkName;
    sinkInfo.adapterName = sinkDeviceName;
    userData->sinkInfos.push_back(sinkInfo);
}

std::vector<SinkInfo> PulseAudioServiceAdapterImpl::GetAllSinks()
{
    AUDIO_INFO_LOG("GetAllSinks enter.");
    lock_guard<mutex> lock(mMutex);
    unique_ptr<UserData> userData = make_unique<UserData>();
    userData->thiz = this;
    userData->sinkInfos = {};

    if (mContext == nullptr) {
        AUDIO_ERR_LOG("GetAllSinks mContext is nullptr");
        return userData->sinkInfos;
    }

    pa_threaded_mainloop_lock(mMainLoop);

    pa_operation *operation = pa_context_get_sink_info_list(mContext,
        PulseAudioServiceAdapterImpl::PaGetSinksCb, reinterpret_cast<void*>(userData.get()));
    if (operation == nullptr) {
        AUDIO_ERR_LOG("GetAllSinks pa_context_get_sink_info_list returned nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return userData->sinkInfos;
    }

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mMainLoop);
    }

    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    AUDIO_INFO_LOG("GetAllSinks end, get [%{public}zu] sinks.", userData->sinkInfos.size());
    return userData->sinkInfos;
}

std::vector<uint32_t> PulseAudioServiceAdapterImpl::GetTargetSinks(std::string adapterName)
{
    std::vector<SinkInfo> sinkInfos = GetAllSinks();
    std::vector<uint32_t> targetSinkIds = {};
    for (size_t i = 0; i < sinkInfos.size(); i++) {
        if (sinkInfos[i].adapterName == adapterName) {
            targetSinkIds.push_back(sinkInfos[i].sinkId);
        }
    }
    return targetSinkIds;
}

int32_t PulseAudioServiceAdapterImpl::SetLocalDefaultSink(std::string name)
{
    std::vector<SinkInput> allSinkInputs = GetAllSinkInputs();

    std::string remoteDevice = "remote";
    std::vector<uint32_t> remoteSinks = GetTargetSinks(remoteDevice);

    // filter sink-inputs which are not connected with remote sinks.
    for (auto sinkInput : allSinkInputs) {
        uint32_t sink = sinkInput.deviceSinkId;
        if (std::find(remoteSinks.begin(), remoteSinks.end(), sink) != remoteSinks.end()) {
            AUDIO_INFO_LOG("[SetLocalDefaultSink] sink-input[%{public}d] connects with remote device[%{public}d]",
                sinkInput.paStreamId, sinkInput.deviceSinkId);
            continue;
        } else {
            uint32_t invalidSinkId = PA_INVALID_INDEX;
            MoveSinkInputByIndexOrName(sinkInput.paStreamId, invalidSinkId, name);
        }
    }

    return SUCCESS;
}

int32_t PulseAudioServiceAdapterImpl::MoveSinkInputByIndexOrName(uint32_t sinkInputId, uint32_t sinkIndex,
    std::string sinkName)
{
    lock_guard<mutex> lock(mMutex);

    unique_ptr<UserData> userData = make_unique<UserData>();
    userData->thiz = this;

    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[MoveSinkInputByIndexOrName] SetVolume mContext is nullptr");
        return ERROR;
    }
    pa_threaded_mainloop_lock(mMainLoop);
    pa_operation *operation = nullptr;
    if (sinkName.empty()) {
        operation = pa_context_move_sink_input_by_index(mContext, sinkInputId, sinkIndex,
            PulseAudioServiceAdapterImpl::PaMoveSinkInputCb, reinterpret_cast<void *>(userData.get()));
    } else {
        operation = pa_context_move_sink_input_by_name(mContext, sinkInputId, sinkName.c_str(),
            PulseAudioServiceAdapterImpl::PaMoveSinkInputCb, reinterpret_cast<void *>(userData.get()));
    }

    if (operation == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa_context_get_sink_input_info_list nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERROR;
    }
    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mMainLoop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    int result = userData->moveResult;
    AUDIO_DEBUG_LOG("move result:[%{public}d]", result);

    return SUCCESS;
}

int32_t PulseAudioServiceAdapterImpl::MoveSourceOutputByIndexOrName(uint32_t sourceOutputId, uint32_t sourceIndex,
    std::string sourceName)
{
    lock_guard<mutex> lock(mMutex);

    unique_ptr<UserData> userData = make_unique<UserData>();
    userData->thiz = this;

    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[MoveSourceOutputByIndexOrName] SetVolume mContext is nullptr");
        return ERROR;
    }
    pa_threaded_mainloop_lock(mMainLoop);
    pa_operation *operation = nullptr;
    if (sourceName.empty()) {
        operation = pa_context_move_source_output_by_index(mContext, sourceOutputId, sourceIndex,
            PulseAudioServiceAdapterImpl::PaMoveSourceOutputCb, reinterpret_cast<void *>(userData.get()));
    } else {
        operation = pa_context_move_source_output_by_name(mContext, sourceOutputId, sourceName.c_str(),
            PulseAudioServiceAdapterImpl::PaMoveSourceOutputCb, reinterpret_cast<void *>(userData.get()));
    }

    if (operation == nullptr) {
        AUDIO_ERR_LOG("[MoveSourceOutputByIndexOrName] pa_context_get_sink_input_info_list nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERROR;
    }
    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mMainLoop);
    }
    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    int result = userData->moveResult;
    AUDIO_DEBUG_LOG("move result:[%{public}d]", result);

    return SUCCESS;
}

int32_t PulseAudioServiceAdapterImpl::SetVolume(AudioStreamType streamType, float volume)
{
    lock_guard<mutex> lock(mMutex);

    unique_ptr<UserData> userData = make_unique<UserData>();
    if (userData == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] SetVolume UserData memory alloc failed");
        return ERROR;
    }

    userData->thiz = this;
    userData->volume = volume;
    userData->streamType = streamType;

    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] SetVolume mContext is nullptr");
        return ERROR;
    }
    pa_threaded_mainloop_lock(mMainLoop);
    pa_operation *operation = pa_context_get_sink_input_info_list(mContext,
        PulseAudioServiceAdapterImpl::PaGetSinkInputInfoVolumeCb, reinterpret_cast<void*>(userData.get()));
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa_context_get_sink_input_info_list nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERROR;
    }
    userData.release();

    pa_threaded_mainloop_accept(mMainLoop);

    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    return SUCCESS;
}

int32_t PulseAudioServiceAdapterImpl::SetSourceOutputMute(int32_t uid, bool setMute)
{
    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[SetSourceOutputMute] mContext is nullptr");
        return ERROR;
    }
    vector<SourceOutput> sourOutputs = GetAllSourceOutputs();
    lock_guard<mutex> lock(mMutex);
    int32_t streamSet = 0;
    for (uint32_t i = 0; i < sourOutputs.size(); i ++) {
        if (sourOutputs[i].uid == uid) {
            pa_operation_unref(pa_context_set_source_output_mute(mContext, sourOutputs[i].paStreamId, (setMute ? 1 : 0),
                nullptr, nullptr));
            AUDIO_INFO_LOG("[SetSourceOutputMute] set source output Mute : %{public}s for stream :uid %{public}d",
                (setMute ? "true" : "false"), sourOutputs[i].uid);
            streamSet++;
        }
    }
    AUDIO_INFO_LOG("[SetSourceOutputMute] set %{public}d %{public}s", streamSet, (setMute ? "mute" : "unmuted"));
    return streamSet;
}


int32_t PulseAudioServiceAdapterImpl::SetMute(AudioStreamType streamType, bool mute)
{
    lock_guard<mutex> lock(mMutex);

    unique_ptr<UserData> userData = make_unique<UserData>();
    userData->thiz = this;
    userData->mute = mute;
    userData->streamType = streamType;

    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] SetMute mContext is nullptr");
        return ERROR;
    }
    pa_threaded_mainloop_lock(mMainLoop);

    pa_operation *operation = pa_context_get_sink_input_info_list(mContext,
        PulseAudioServiceAdapterImpl::PaGetSinkInputInfoMuteCb, reinterpret_cast<void*>(userData.get()));
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa_context_get_sink_input_info_list returned nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return ERROR;
    }

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mMainLoop);
    }

    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    return SUCCESS;
}

bool PulseAudioServiceAdapterImpl::IsMute(AudioStreamType streamType)
{
    lock_guard<mutex> lock(mMutex);
    if (!isSetDefaultSink_) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] IsMute not SetDefaultSink first");
        return false;
    }
    unique_ptr<UserData> userData = make_unique<UserData>();
    userData->thiz = this;
    userData->streamType = streamType;
    userData->mute = false;

    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] IsMute mContext is nullptr");
        return false;
    }

    pa_threaded_mainloop_lock(mMainLoop);

    pa_operation *operation = pa_context_get_sink_input_info_list(mContext,
        PulseAudioServiceAdapterImpl::PaGetSinkInputInfoMuteStatusCb, reinterpret_cast<void*>(userData.get()));
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa_context_get_sink_input_info_list returned nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return false;
    }

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mMainLoop);
    }

    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    return (userData->mute) ? true : false;
}

bool PulseAudioServiceAdapterImpl::IsStreamActive(AudioStreamType streamType)
{
    lock_guard<mutex> lock(mMutex);
    if (!isSetDefaultSink_) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] IsStreamActive not SetDefaultSink first");
        return false;
    }
    unique_ptr<UserData> userData = make_unique<UserData>();
    userData->thiz = this;
    userData->streamType = streamType;
    userData->isCorked = true;

    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] IsStreamActive mContext is nullptr");
        return false;
    }

    pa_threaded_mainloop_lock(mMainLoop);

    pa_operation *operation = pa_context_get_sink_input_info_list(mContext,
        PulseAudioServiceAdapterImpl::PaGetSinkInputInfoCorkStatusCb, reinterpret_cast<void*>(userData.get()));
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[IsStreamActive] pa_context_get_sink_input_info_list returned nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return false;
    }

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mMainLoop);
    }

    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    AUDIO_INFO_LOG("[IsStreamActive] cork for stream %s : %d",
        GetNameByStreamType(streamType).c_str(), userData->isCorked);

    return (userData->isCorked) ? false : true;
}

vector<SinkInput> PulseAudioServiceAdapterImpl::GetAllSinkInputs()
{
    AUDIO_INFO_LOG("GetAllSinkInputs enter");
    unique_ptr<UserData> userData = make_unique<UserData>();
    userData->thiz = this;
    userData->sinkInfos = GetAllSinks();

    lock_guard<mutex> lock(mMutex);
    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] GetAllSinkInputs mContext is nullptr");
        return userData->sinkInputList;
    }

    pa_threaded_mainloop_lock(mMainLoop);

    pa_operation *operation = pa_context_get_sink_input_info_list(mContext,
        PulseAudioServiceAdapterImpl::PaGetAllSinkInputsCb, reinterpret_cast<void*>(userData.get()));
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[GetAllSinkInputs] pa_context_get_sink_input_info_list returned nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return userData->sinkInputList;
    }

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mMainLoop);
    }

    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    AUDIO_INFO_LOG("GetAllSinkInputs get:[%{public}zu]", userData->sinkInputList.size());
    return userData->sinkInputList;
}

vector<SourceOutput> PulseAudioServiceAdapterImpl::GetAllSourceOutputs()
{
    lock_guard<mutex> lock(mMutex);

    unique_ptr<UserData> userData = make_unique<UserData>();
    userData->thiz = this;

    if (mContext == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] GetAllSourceOutputs mContext is nullptr");
        return userData->sourceOutputList;
    }

    pa_threaded_mainloop_lock(mMainLoop);

    pa_operation *operation = pa_context_get_source_output_info_list(mContext,
        PulseAudioServiceAdapterImpl::PaGetAllSourceOutputsCb, reinterpret_cast<void*>(userData.get()));
    if (operation == nullptr) {
        AUDIO_ERR_LOG("[GetAllSourceOutputs] pa_context_get_source_output_info_list returned nullptr");
        pa_threaded_mainloop_unlock(mMainLoop);
        return userData->sourceOutputList;
    }

    while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mMainLoop);
    }

    pa_operation_unref(operation);
    pa_threaded_mainloop_unlock(mMainLoop);

    return userData->sourceOutputList;
}

void PulseAudioServiceAdapterImpl::Disconnect()
{
    if (mContext != nullptr) {
        AUDIO_INFO_LOG("[PulseAudioServiceAdapterImpl] disconnect context!");
        pa_context_disconnect(mContext);
        /* Make sure we don't get any further callbacks */
        pa_context_set_state_callback(mContext, nullptr, nullptr);
        pa_context_set_subscribe_callback(mContext, nullptr, nullptr);
        pa_context_unref(mContext);
    }

    if (mMainLoop != nullptr) {
        pa_threaded_mainloop_stop(mMainLoop);
        pa_threaded_mainloop_free(mMainLoop);
    }
}

string PulseAudioServiceAdapterImpl::GetNameByStreamType(AudioStreamType streamType)
{
    switch (streamType) {
        case STREAM_MUSIC:
            return "music";
        case STREAM_RING:
            return "ring";
        case STREAM_SYSTEM:
            return "system";
        case STREAM_NOTIFICATION:
            return "notification";
        case STREAM_ALARM:
            return "alarm";
        case STREAM_DTMF:
            return "dtmf";
        case STREAM_VOICE_CALL:
            return "voice_call";
        case STREAM_VOICE_ASSISTANT:
            return "voice_assistant";
        default:
            return "";
    }
}

AudioStreamType PulseAudioServiceAdapterImpl::GetIdByStreamType(string streamType)
{
    AudioStreamType stream;

    if (!streamType.compare(string("music"))) {
        stream = STREAM_MUSIC;
    } else if (!streamType.compare(string("ring"))) {
        stream = STREAM_RING;
    } else if (!streamType.compare(string("system"))) {
        stream = STREAM_SYSTEM;
    } else if (!streamType.compare(string("notification"))) {
        stream = STREAM_NOTIFICATION;
    } else if (!streamType.compare(string("alarm"))) {
        stream = STREAM_ALARM;
    } else if (!streamType.compare(string("voice_call"))) {
        stream = STREAM_VOICE_CALL;
    }  else if (!streamType.compare(string("voice_assistant"))) {
        stream = STREAM_VOICE_ASSISTANT;
    } else {
        stream = STREAM_MUSIC;
    }

    return stream;
}

void PulseAudioServiceAdapterImpl::PaGetSinkInputInfoMuteStatusCb(pa_context *c, const pa_sink_input_info *i, int eol,
    void *userdata)
{
    UserData *userData = reinterpret_cast<UserData*>(userdata);
    PulseAudioServiceAdapterImpl *thiz = userData->thiz;

    if (eol < 0) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Failed to get sink input information: %{public}s",
            pa_strerror(pa_context_errno(c)));
        return;
    }

    if (eol) {
        pa_threaded_mainloop_signal(thiz->mMainLoop, 0);
        return;
    }

    if (i->proplist == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Invalid Proplist for sink input (%{public}d).", i->index);
        return;
    }

    const char *streamtype = pa_proplist_gets(i->proplist, "stream.type");
    if (streamtype == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Invalid StreamType.");
        return;
    }

    string streamType(streamtype);
    if (!streamType.compare(thiz->GetNameByStreamType(userData->streamType))) {
        userData->mute = i->mute;
        AUDIO_INFO_LOG("[PulseAudioServiceAdapterImpl] Mute : %{public}d for stream : %{public}s",
            userData->mute, i->name);
    }

    return;
}

void PulseAudioServiceAdapterImpl::PaGetSinkInputInfoMuteCb(pa_context *c, const pa_sink_input_info *i,
    int eol, void *userdata)
{
    UserData *userData = reinterpret_cast<UserData*>(userdata);
    PulseAudioServiceAdapterImpl *thiz = userData->thiz;

    if (eol < 0) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Failed to get sink input information: %{public}s",
            pa_strerror(pa_context_errno(c)));
        return;
    }

    if (eol) {
        pa_threaded_mainloop_signal(thiz->mMainLoop, 0);
        return;
    }

    if (i->proplist == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Invalid Proplist for sink input (%{public}d).", i->index);
        return;
    }

    const char *streamtype = pa_proplist_gets(i->proplist, "stream.type");
    if (streamtype == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Invalid StreamType.");
        return;
    }

    string streamType(streamtype);
    if (!streamType.compare(thiz->GetNameByStreamType(userData->streamType))) {
        pa_operation_unref(pa_context_set_sink_input_mute(c, i->index, (userData->mute) ? 1 : 0, nullptr, nullptr));
        AUDIO_INFO_LOG("[PulseAudioServiceAdapterImpl] Applied Mute : %{public}d for stream : %{public}s",
            userData->mute, i->name);
    }

    return;
}

void PulseAudioServiceAdapterImpl::PaMoveSinkInputCb(pa_context *c, int success, void *userdata)
{
    UserData *userData = reinterpret_cast<UserData *>(userdata);

    AUDIO_INFO_LOG("[PaMoveSinkInputCb] result[%{public}d]", success);
    userData->moveResult = success;

    pa_threaded_mainloop_signal(userData->thiz->mMainLoop, 0);

    return;
}

void PulseAudioServiceAdapterImpl::PaMoveSourceOutputCb(pa_context *c, int success, void *userdata)
{
    UserData *userData = reinterpret_cast<UserData *>(userdata);

    AUDIO_INFO_LOG("[PaMoveSourceOutputCb] result[%{public}d]", success);
    userData->moveResult = success;

    pa_threaded_mainloop_signal(userData->thiz->mMainLoop, 0);

    return;
}

void PulseAudioServiceAdapterImpl::PaContextStateCb(pa_context *c, void *userdata)
{
    PulseAudioServiceAdapterImpl *thiz = reinterpret_cast<PulseAudioServiceAdapterImpl*>(userdata);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY: {
            pa_context_set_subscribe_callback(c, PulseAudioServiceAdapterImpl::PaSubscribeCb, thiz);

            pa_operation *operation = pa_context_subscribe(c, (pa_subscription_mask_t)
                (PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE |
                PA_SUBSCRIPTION_MASK_SINK_INPUT | PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT |
                PA_SUBSCRIPTION_MASK_CARD), nullptr, nullptr);
            if (operation == nullptr) {
                pa_threaded_mainloop_signal(thiz->mMainLoop, 0);
                return;
            }
            pa_operation_unref(operation);
            pa_threaded_mainloop_signal(thiz->mMainLoop, 0);
            break;
        }

        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(thiz->mMainLoop, 0);
            return;

        case PA_CONTEXT_TERMINATED:
        default:
            return;
    }
}

void PulseAudioServiceAdapterImpl::PaModuleLoadCb(pa_context *c, uint32_t idx, void *userdata)
{
    UserData *userData = reinterpret_cast<UserData*>(userdata);
    if (idx == PA_INVALID_INDEX) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Failure: %{public}s", pa_strerror(pa_context_errno(c)));
        userData->idx = PA_INVALID_INDEX;
    } else {
        userData->idx = idx;
    }
    pa_threaded_mainloop_signal(userData->thiz->mMainLoop, 0);

    return;
}

void PulseAudioServiceAdapterImpl::PaGetSinkInputInfoVolumeCb(pa_context *c, const pa_sink_input_info *i, int eol,
    void *userdata)
{
    UserData *userData = reinterpret_cast<UserData*>(userdata);
    PulseAudioServiceAdapterImpl *thiz = userData->thiz;

    AUDIO_INFO_LOG("[PulseAudioServiceAdapterImpl] GetSinkInputInfoVolumeCb");
    if (eol < 0) {
        delete userData;
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Failed to get sink input information: %{public}s",
            pa_strerror(pa_context_errno(c)));
        return;
    }

    if (eol) {
        pa_threaded_mainloop_signal(thiz->mMainLoop, 1);
        delete userData;
        return;
    }

    if (i->proplist == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Invalid Proplist for sink input (%{public}d).", i->index);
        return;
    }

    const char *streamtype = pa_proplist_gets(i->proplist, "stream.type");
    const char *streamVolume = pa_proplist_gets(i->proplist, "stream.volumeFactor");
    const char *streamPowerVolume = pa_proplist_gets(i->proplist, "stream.powerVolumeFactor");
    const char *sessionCStr = pa_proplist_gets(i->proplist, "stream.sessionID");
    if ((streamtype == nullptr) || (streamVolume == nullptr) || (streamPowerVolume == nullptr) ||
        (sessionCStr == nullptr)) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Invalid Stream parameter info.");
        return;
    }

    std::stringstream sessionStr;
    uint32_t sessionID;
    sessionStr << sessionCStr;
    sessionStr >> sessionID;
    AUDIO_INFO_LOG("PulseAudioServiceAdapterImpl: PaGetSinkInputInfoVolumeCb sessionID %{public}u", sessionID);

    sinkIndexSessionIDMap[i->index] = sessionID;

    string streamType(streamtype);
    float volumeFactor = atof(streamVolume);
    float powerVolumeFactor = atof(streamPowerVolume);
    AudioStreamType streamID = thiz->GetIdByStreamType(streamType);
    float volumeCb = g_audioServiceAdapterCallback->OnGetVolumeCb(streamtype);
    float vol = volumeCb * volumeFactor * powerVolumeFactor;

    pa_cvolume cv = i->volume;
    uint32_t volume = pa_sw_volume_from_linear(vol);
    pa_cvolume_set(&cv, i->channel_map.channels, volume);
    pa_operation_unref(pa_context_set_sink_input_volume(c, i->index, &cv, nullptr, nullptr));

    if (streamID == userData->streamType) {
        if (i->mute) {
            pa_operation_unref(pa_context_set_sink_input_mute(c, i->index, 0, nullptr, nullptr));
        }
    }
    AUDIO_INFO_LOG("[PulseAudioServiceAdapterImpl]volume : %{public}f for stream : %{public}s, volumeInt%{public}d",
        vol, i->name, volume);
    HiviewDFX::HiSysEvent::Write("AUDIO", "AUDIO_VOLUME_CHANGE", HiviewDFX::HiSysEvent::EventType::BEHAVIOR,
        "ISOUTPUT", 1, "STREAMID", sessionID, "STREAMTYPE", streamID, "VOLUME", vol);
}

void PulseAudioServiceAdapterImpl::PaGetSinkInputInfoCorkStatusCb(pa_context *c, const pa_sink_input_info *i, int eol,
    void *userdata)
{
    UserData *userData = reinterpret_cast<UserData*>(userdata);
    PulseAudioServiceAdapterImpl *thiz = userData->thiz;

    if (eol < 0) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Failed to get sink input information: %{public}s",
            pa_strerror(pa_context_errno(c)));
        return;
    }

    if (eol) {
        pa_threaded_mainloop_signal(thiz->mMainLoop, 0);
        return;
    }

    if (i->proplist == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Invalid Proplist for sink input (%{public}d).", i->index);
        return;
    }

    const char *streamtype = pa_proplist_gets(i->proplist, "stream.type");
    if (streamtype == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Invalid StreamType.");
        return;
    }

    string streamType(streamtype);
    if (!streamType.compare(thiz->GetNameByStreamType(userData->streamType))) {
        userData->isCorked = i->corked;
        AUDIO_INFO_LOG("[PulseAudioServiceAdapterImpl] corked : %{public}d for stream : %{public}s",
            userData->isCorked, i->name);
    }
}

template <typename T>
inline void CastValue(T &a, const char *raw)
{
    if (raw == nullptr) {
        return;
    }
    std::stringstream valueStr;
    valueStr << raw;
    valueStr >> a;
}

void PulseAudioServiceAdapterImpl::PaGetAllSinkInputsCb(pa_context *c, const pa_sink_input_info *i, int eol,
    void *userdata)
{
    AUDIO_INFO_LOG("[PaGetAllSinkInputsCb] in eol[%{public}d]", eol);
    UserData *userData = reinterpret_cast<UserData *>(userdata);
    PulseAudioServiceAdapterImpl *thiz = userData->thiz;

    if (eol < 0) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Failed to get sink input information: %{public}s",
            pa_strerror(pa_context_errno(c)));
        return;
    }

    if (eol) {
        pa_threaded_mainloop_signal(thiz->mMainLoop, 0);
        return;
    }

    if (i->proplist == nullptr) {
        AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] Invalid Proplist for sink input (%{public}d).", i->index);
        return;
    }

    AudioStreamType audioStreamType = STREAM_DEFAULT;
    const char *streamType = pa_proplist_gets(i->proplist, "stream.type");
    if (streamType != nullptr) {
        audioStreamType = thiz->GetIdByStreamType(streamType);
    }

    SinkInput sinkInput = {};
    sinkInput.streamType = audioStreamType;

    sinkInput.deviceSinkId = i->sink;
    for (auto sinkInfo : userData->sinkInfos) {
        if (sinkInput.deviceSinkId == sinkInfo.sinkId) {
            sinkInput.sinkName = sinkInfo.sinkName;
            break;
        }
    }
    sinkInput.paStreamId = i->index;
    CastValue<int32_t>(sinkInput.streamId, pa_proplist_gets(i->proplist, "stream.sessionID"));
    CastValue<int32_t>(sinkInput.uid, pa_proplist_gets(i->proplist, "stream.client.uid"));
    CastValue<int32_t>(sinkInput.pid, pa_proplist_gets(i->proplist, "stream.client.pid"));
    CastValue<uint64_t>(sinkInput.startTime, pa_proplist_gets(i->proplist, "stream.startTime"));

    userData->sinkInputList.push_back(sinkInput);
}

void PulseAudioServiceAdapterImpl::PaGetAllSourceOutputsCb(pa_context *c, const pa_source_output_info *i, int eol,
    void *userdata)
{
    AUDIO_INFO_LOG("[PaGetAllSourceOutputsCb] in eol[%{public}d]", eol);
    UserData *userData = reinterpret_cast<UserData *>(userdata);
    PulseAudioServiceAdapterImpl *thiz = userData->thiz;

    if (eol < 0) {
        AUDIO_ERR_LOG("[PaGetAllSourceOutputsCb] Failed to get source output information: %{public}s",
            pa_strerror(pa_context_errno(c)));
        return;
    }

    if (eol) {
        pa_threaded_mainloop_signal(thiz->mMainLoop, 0);
        return;
    }

    if (i->proplist == nullptr) {
        AUDIO_ERR_LOG("[PaGetAllSourceOutputsCb] Invalid Proplist for source output (%{public}d).", i->index);
        return;
    }

    uint32_t sessionID = 0;
    const char *sessionCStr = pa_proplist_gets(i->proplist, "stream.sessionID");
    if (sessionCStr != nullptr) {
        std::stringstream sessionStr;
        sessionStr << sessionCStr;
        sessionStr >> sessionID;
    }

    AudioStreamType audioStreamType = STREAM_DEFAULT;
    const char *streamType = pa_proplist_gets(i->proplist, "stream.type");
    if (streamType != nullptr) {
        audioStreamType = thiz->GetIdByStreamType(streamType);
    }

    SourceOutput sourceOutput = {};
    sourceOutput.streamId = sessionID;
    sourceOutput.streamType = audioStreamType;

    sourceOutput.paStreamId = i->index;
    sourceOutput.deviceSourceId = i->source;
    CastValue<int32_t>(sourceOutput.uid, pa_proplist_gets(i->proplist, "stream.client.uid"));
    CastValue<int32_t>(sourceOutput.pid, pa_proplist_gets(i->proplist, "stream.client.pid"));
    CastValue<uint64_t>(sourceOutput.startTime, pa_proplist_gets(i->proplist, "stream.startTime"));
    userData->sourceOutputList.push_back(sourceOutput);
}

void PulseAudioServiceAdapterImpl::PaSubscribeCb(pa_context *c, pa_subscription_event_type_t t, uint32_t idx,
    void *userdata)
{
    unique_ptr<UserData> userData = make_unique<UserData>();
    PulseAudioServiceAdapterImpl *thiz = reinterpret_cast<PulseAudioServiceAdapterImpl*>(userdata);
    userData->thiz = thiz;
    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
        case PA_SUBSCRIPTION_EVENT_SINK:
            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE:
            break;

        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
                pa_threaded_mainloop_lock(thiz->mMainLoop);
                pa_operation *operation = pa_context_get_sink_input_info(c, idx,
                    PulseAudioServiceAdapterImpl::PaGetSinkInputInfoVolumeCb, reinterpret_cast<void*>(userData.get()));
                if (operation == nullptr) {
                    AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] pa_context_get_sink_input_info_list nullptr");
                    pa_threaded_mainloop_unlock(thiz->mMainLoop);
                    return;
                }
                userData.release();
                pa_threaded_mainloop_accept(thiz->mMainLoop);
                pa_operation_unref(operation);
                pa_threaded_mainloop_unlock(thiz->mMainLoop);
            } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
                uint32_t sessionID = sinkIndexSessionIDMap[idx];
                AUDIO_ERR_LOG("[PulseAudioServiceAdapterImpl] sessionID: %{public}d  removed", sessionID);
                g_audioServiceAdapterCallback->OnSessionRemoved(sessionID);
            }
            break;

        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            break;

        default:
            break;
    }
}
} // namespace AudioStandard
} // namespace OHOS

#endif // ST_PULSEAUDIO_AUDIO_SERVICE_ADAPTER_IMPL_H
