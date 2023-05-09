/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include "audio_stream_collector.h"

#include "audio_capturer_state_change_listener_proxy.h"
#include "audio_errors.h"
#include "audio_renderer_state_change_listener_proxy.h"
#include "audio_client_tracker_callback_proxy.h"

#include "i_standard_renderer_state_change_listener.h"
#include "i_standard_capturer_state_change_listener.h"
#include "i_standard_client_tracker.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;

const map<pair<ContentType, StreamUsage>, AudioStreamType> AudioStreamCollector::streamTypeMap_ =
    AudioStreamCollector::CreateStreamMap();

map<pair<ContentType, StreamUsage>, AudioStreamType> AudioStreamCollector::CreateStreamMap()
{
    map<pair<ContentType, StreamUsage>, AudioStreamType> streamMap;

    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_UNKNOWN)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_MEDIA)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_UNKNOWN, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_UNKNOWN)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_MEDIA)] = STREAM_VOICE_ASSISTANT;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_VOICE_CALL;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_VOICE_ASSISTANT;
    streamMap[make_pair(CONTENT_TYPE_SPEECH, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_UNKNOWN)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_MEDIA)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_VOICE_ASSISTANT;
    streamMap[make_pair(CONTENT_TYPE_MUSIC, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_RING;

    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_UNKNOWN)] = STREAM_MEDIA;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_MEDIA)] = STREAM_MEDIA;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_MOVIE, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_UNKNOWN)] = STREAM_NOTIFICATION;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_MEDIA)] = STREAM_NOTIFICATION;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_SONIFICATION, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_MUSIC;

    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_UNKNOWN)] = STREAM_RING;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_MEDIA)] = STREAM_RING;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_VOICE_COMMUNICATION)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_VOICE_ASSISTANT)] = STREAM_MUSIC;
    streamMap[make_pair(CONTENT_TYPE_RINGTONE, STREAM_USAGE_NOTIFICATION_RINGTONE)] = STREAM_RING;

    return streamMap;
}

AudioStreamCollector::AudioStreamCollector() : mDispatcherService
    (AudioStreamEventDispatcher::GetAudioStreamEventDispatcher())
{
    AUDIO_INFO_LOG("AudioStreamCollector::AudioStreamCollector()");
}

AudioStreamCollector::~AudioStreamCollector()
{
    AUDIO_INFO_LOG("AudioStreamCollector::~AudioStreamCollector()");
}

int32_t AudioStreamCollector::RegisterAudioRendererEventListener(int32_t clientUID, const sptr<IRemoteObject> &object,
    bool hasBTPermission)
{
    AUDIO_INFO_LOG("AudioStreamCollector: RegisterAudioRendererEventListener client id %{public}d done", clientUID);

    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_INVALID_PARAM,
        "AudioStreamCollector:set renderer state change event listener object is nullptr");

    sptr<IStandardRendererStateChangeListener> listener = iface_cast<IStandardRendererStateChangeListener>(object);
    CHECK_AND_RETURN_RET_LOG(listener != nullptr, ERR_INVALID_PARAM,
        "AudioStreamCollector: renderer listener obj cast failed");

    std::shared_ptr<AudioRendererStateChangeCallback> callback =
         std::make_shared<AudioRendererStateChangeListenerCallback>(listener, hasBTPermission);
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM, "AudioStreamCollector: failed to  create cb obj");

    mDispatcherService.addRendererListener(clientUID, callback);
    return SUCCESS;
}

int32_t AudioStreamCollector::UnregisterAudioRendererEventListener(int32_t clientUID)
{
    AUDIO_INFO_LOG("AudioStreamCollector::UnregisterAudioRendererEventListener()");
    mDispatcherService.removeRendererListener(clientUID);
    return SUCCESS;
}

int32_t AudioStreamCollector::RegisterAudioCapturerEventListener(int32_t clientUID, const sptr<IRemoteObject> &object,
    bool hasBTPermission)
{
    AUDIO_INFO_LOG("AudioStreamCollector: RegisterAudioCapturerEventListener for client id %{public}d done", clientUID);

    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_INVALID_PARAM,
        "AudioStreamCollector:set capturer event listener object is nullptr");

    sptr<IStandardCapturerStateChangeListener> listener = iface_cast<IStandardCapturerStateChangeListener>(object);
    CHECK_AND_RETURN_RET_LOG(listener != nullptr, ERR_INVALID_PARAM, "AudioStreamCollector: capturer obj cast failed");

    std::shared_ptr<AudioCapturerStateChangeCallback> callback =
        std::make_shared<AudioCapturerStateChangeListenerCallback>(listener, hasBTPermission);
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM,
        "AudioStreamCollector: failed to create capturer cb obj");

    mDispatcherService.addCapturerListener(clientUID, callback);
    return SUCCESS;
}

int32_t AudioStreamCollector::UnregisterAudioCapturerEventListener(int32_t clientUID)
{
    AUDIO_INFO_LOG("AudioStreamCollector: UnregisterAudioCapturerEventListener client id %{public}d done", clientUID);
    mDispatcherService.removeCapturerListener(clientUID);
    return SUCCESS;
}

int32_t AudioStreamCollector::AddRendererStream(AudioStreamChangeInfo &streamChangeInfo)
{
    AUDIO_INFO_LOG("AudioStreamCollector: AddRendererStream playback client id %{public}d session %{public}d",
        streamChangeInfo.audioRendererChangeInfo.clientUID, streamChangeInfo.audioRendererChangeInfo.sessionId);

    rendererStatequeue_.insert({{streamChangeInfo.audioRendererChangeInfo.clientUID,
        streamChangeInfo.audioRendererChangeInfo.sessionId},
        streamChangeInfo.audioRendererChangeInfo.rendererState});

    unique_ptr<AudioRendererChangeInfo> rendererChangeInfo = make_unique<AudioRendererChangeInfo>();
    if (!rendererChangeInfo) {
        AUDIO_ERR_LOG("AudioStreamCollector::AddRendererStream Memory Allocation Failed");
        return ERR_MEMORY_ALLOC_FAILED;
    }
    rendererChangeInfo->clientUID = streamChangeInfo.audioRendererChangeInfo.clientUID;
    rendererChangeInfo->sessionId = streamChangeInfo.audioRendererChangeInfo.sessionId;
    rendererChangeInfo->rendererState = streamChangeInfo.audioRendererChangeInfo.rendererState;
    rendererChangeInfo->rendererInfo = streamChangeInfo.audioRendererChangeInfo.rendererInfo;
    rendererChangeInfo->outputDeviceInfo = streamChangeInfo.audioRendererChangeInfo.outputDeviceInfo;
    audioRendererChangeInfos_.push_back(move(rendererChangeInfo));

    AUDIO_DEBUG_LOG("AudioStreamCollector: audioRendererChangeInfos_: Added for client %{public}d session %{public}d",
        streamChangeInfo.audioRendererChangeInfo.clientUID, streamChangeInfo.audioRendererChangeInfo.sessionId);

    mDispatcherService.SendRendererInfoEventToDispatcher(AudioMode::AUDIO_MODE_PLAYBACK, audioRendererChangeInfos_);
    return SUCCESS;
}

int32_t AudioStreamCollector::AddCapturerStream(AudioStreamChangeInfo &streamChangeInfo)
{
    AUDIO_INFO_LOG("AudioStreamCollector: AddCapturerStream recording client id %{public}d session %{public}d",
        streamChangeInfo.audioCapturerChangeInfo.clientUID, streamChangeInfo.audioCapturerChangeInfo.sessionId);

    capturerStatequeue_.insert({{streamChangeInfo.audioCapturerChangeInfo.clientUID,
        streamChangeInfo.audioCapturerChangeInfo.sessionId},
        streamChangeInfo.audioCapturerChangeInfo.capturerState});

    unique_ptr<AudioCapturerChangeInfo> capturerChangeInfo = make_unique<AudioCapturerChangeInfo>();
    if (!capturerChangeInfo) {
        AUDIO_ERR_LOG("AudioStreamCollector::AddCapturerStream Memory Allocation Failed");
        return ERR_MEMORY_ALLOC_FAILED;
    }
    capturerChangeInfo->clientUID = streamChangeInfo.audioCapturerChangeInfo.clientUID;
    capturerChangeInfo->sessionId = streamChangeInfo.audioCapturerChangeInfo.sessionId;
    capturerChangeInfo->capturerState = streamChangeInfo.audioCapturerChangeInfo.capturerState;
    capturerChangeInfo->capturerInfo = streamChangeInfo.audioCapturerChangeInfo.capturerInfo;
    capturerChangeInfo->inputDeviceInfo = streamChangeInfo.audioCapturerChangeInfo.inputDeviceInfo;
    audioCapturerChangeInfos_.push_back(move(capturerChangeInfo));

    AUDIO_DEBUG_LOG("AudioStreamCollector: audioCapturerChangeInfos_: Added for client %{public}d session %{public}d",
        streamChangeInfo.audioCapturerChangeInfo.clientUID, streamChangeInfo.audioCapturerChangeInfo.sessionId);

    mDispatcherService.SendCapturerInfoEventToDispatcher(AudioMode::AUDIO_MODE_RECORD, audioCapturerChangeInfos_);
    return SUCCESS;
}

int32_t AudioStreamCollector::RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
    const sptr<IRemoteObject> &object)
{
    AUDIO_INFO_LOG("AudioStreamCollector: RegisterTracker mode %{public}d", mode);

    int32_t clientID;
    std::lock_guard<std::mutex> lock(streamsInfoMutex_);
    if (mode == AUDIO_MODE_PLAYBACK) {
        AddRendererStream(streamChangeInfo);
        clientID = streamChangeInfo.audioRendererChangeInfo.sessionId;
    } else {
        // mode = AUDIO_MODE_RECORD
        AddCapturerStream(streamChangeInfo);
        clientID = streamChangeInfo.audioCapturerChangeInfo.sessionId;
    }

    sptr<IStandardClientTracker> listener = iface_cast<IStandardClientTracker>(object);
    CHECK_AND_RETURN_RET_LOG(listener != nullptr,
        ERR_INVALID_PARAM, "AudioStreamCollector: client tracker obj cast failed");
    std::shared_ptr<AudioClientTracker> callback = std::make_shared<ClientTrackerCallbackListener>(listener);
    CHECK_AND_RETURN_RET_LOG(callback != nullptr,
        ERR_INVALID_PARAM, "AudioStreamCollector: failed to create tracker cb obj");
    clientTracker_[clientID] = callback;

    return SUCCESS;
}

int32_t AudioStreamCollector::UpdateRendererStream(AudioStreamChangeInfo &streamChangeInfo)
{
    AUDIO_INFO_LOG("AudioStreamCollector: UpdateRendererStream client %{public}d state %{public}d session %{public}d",
        streamChangeInfo.audioRendererChangeInfo.clientUID, streamChangeInfo.audioRendererChangeInfo.rendererState,
        streamChangeInfo.audioRendererChangeInfo.sessionId);

    if (rendererStatequeue_.find(make_pair(streamChangeInfo.audioRendererChangeInfo.clientUID,
        streamChangeInfo.audioRendererChangeInfo.sessionId)) != rendererStatequeue_.end()) {
        if (streamChangeInfo.audioRendererChangeInfo.rendererState ==
            rendererStatequeue_[make_pair(streamChangeInfo.audioRendererChangeInfo.clientUID,
                streamChangeInfo.audioRendererChangeInfo.sessionId)]) {
            // Renderer state not changed
            return SUCCESS;
        }
    } else {
        AUDIO_INFO_LOG("UpdateRendererStream client %{public}d not found in rendererStatequeue_",
            streamChangeInfo.audioRendererChangeInfo.clientUID);
    }

    // Update the renderer info in audioRendererChangeInfos_
    for (auto it = audioRendererChangeInfos_.begin(); it != audioRendererChangeInfos_.end(); it++) {
        AudioRendererChangeInfo audioRendererChangeInfo = **it;
        if (audioRendererChangeInfo.clientUID == streamChangeInfo.audioRendererChangeInfo.clientUID &&
            audioRendererChangeInfo.sessionId == streamChangeInfo.audioRendererChangeInfo.sessionId) {
            rendererStatequeue_[make_pair(audioRendererChangeInfo.clientUID, audioRendererChangeInfo.sessionId)] =
                streamChangeInfo.audioRendererChangeInfo.rendererState;
            AUDIO_DEBUG_LOG("AudioStreamCollector: UpdateRendererStream: update client %{public}d session %{public}d",
                audioRendererChangeInfo.clientUID, audioRendererChangeInfo.sessionId);

            unique_ptr<AudioRendererChangeInfo> RendererChangeInfo = make_unique<AudioRendererChangeInfo>();
            CHECK_AND_RETURN_RET_LOG(RendererChangeInfo != nullptr,
                ERR_MEMORY_ALLOC_FAILED, "AudioStreamCollector::RendererChangeInfo Memory Allocation Failed");
            RendererChangeInfo->clientUID = streamChangeInfo.audioRendererChangeInfo.clientUID;
            RendererChangeInfo->sessionId = streamChangeInfo.audioRendererChangeInfo.sessionId;
            RendererChangeInfo->rendererState = streamChangeInfo.audioRendererChangeInfo.rendererState;
            RendererChangeInfo->rendererInfo = streamChangeInfo.audioRendererChangeInfo.rendererInfo;
            RendererChangeInfo->outputDeviceInfo = streamChangeInfo.audioRendererChangeInfo.outputDeviceInfo;
            *it = move(RendererChangeInfo);
            AUDIO_DEBUG_LOG("AudioStreamCollector: Playback details updated, to be dispatched");

            mDispatcherService.SendRendererInfoEventToDispatcher(AudioMode::AUDIO_MODE_PLAYBACK,
                audioRendererChangeInfos_);

            if (streamChangeInfo.audioRendererChangeInfo.rendererState == RENDERER_RELEASED) {
                audioRendererChangeInfos_.erase(it);
                AUDIO_DEBUG_LOG("AudioStreamCollector: Session removed for client %{public}d session %{public}d",
                    streamChangeInfo.audioRendererChangeInfo.clientUID,
                    streamChangeInfo.audioRendererChangeInfo.sessionId);
                rendererStatequeue_.erase(make_pair(audioRendererChangeInfo.clientUID,
                    audioRendererChangeInfo.sessionId));
                clientTracker_.erase(audioRendererChangeInfo.sessionId);
            }
            return SUCCESS;
        }
    }
    AUDIO_INFO_LOG("UpdateRendererStream: Not found clientUid:%{public}d sessionId:%{public}d",
        streamChangeInfo.audioRendererChangeInfo.clientUID, streamChangeInfo.audioRendererChangeInfo.clientUID);
    return SUCCESS;
}

int32_t AudioStreamCollector::UpdateCapturerStream(AudioStreamChangeInfo &streamChangeInfo)
{
    AUDIO_INFO_LOG("AudioStreamCollector: UpdateCapturerStream client %{public}d state %{public}d session %{public}d",
        streamChangeInfo.audioCapturerChangeInfo.clientUID, streamChangeInfo.audioCapturerChangeInfo.capturerState,
        streamChangeInfo.audioCapturerChangeInfo.sessionId);

    if (capturerStatequeue_.find(make_pair(streamChangeInfo.audioCapturerChangeInfo.clientUID,
        streamChangeInfo.audioCapturerChangeInfo.sessionId)) != capturerStatequeue_.end()) {
        if (streamChangeInfo.audioCapturerChangeInfo.capturerState ==
            capturerStatequeue_[make_pair(streamChangeInfo.audioCapturerChangeInfo.clientUID,
                streamChangeInfo.audioCapturerChangeInfo.sessionId)]) {
            // Capturer state not changed
            return SUCCESS;
        }
    } else {
        AUDIO_INFO_LOG("AudioStreamCollector: UpdateCapturerStream client %{public}d not found in capturerStatequeue_",
            streamChangeInfo.audioCapturerChangeInfo.clientUID);
    }

    // Update the capturer info in audioCapturerChangeInfos_
    for (auto it = audioCapturerChangeInfos_.begin(); it != audioCapturerChangeInfos_.end(); it++) {
        AudioCapturerChangeInfo audioCapturerChangeInfo = **it;
        if (audioCapturerChangeInfo.clientUID == streamChangeInfo.audioCapturerChangeInfo.clientUID &&
            audioCapturerChangeInfo.sessionId == streamChangeInfo.audioCapturerChangeInfo.sessionId) {
            capturerStatequeue_[make_pair(audioCapturerChangeInfo.clientUID, audioCapturerChangeInfo.sessionId)] =
                streamChangeInfo.audioCapturerChangeInfo.capturerState;

            AUDIO_DEBUG_LOG("AudioStreamCollector: Session is updated for client %{public}d session %{public}d",
                streamChangeInfo.audioCapturerChangeInfo.clientUID,
                streamChangeInfo.audioCapturerChangeInfo.sessionId);

            unique_ptr<AudioCapturerChangeInfo> CapturerChangeInfo = make_unique<AudioCapturerChangeInfo>();
            CHECK_AND_RETURN_RET_LOG(CapturerChangeInfo != nullptr,
                ERR_MEMORY_ALLOC_FAILED, "AudioStreamCollector::CapturerChangeInfo Memory Allocation Failed");
            CapturerChangeInfo->clientUID = streamChangeInfo.audioCapturerChangeInfo.clientUID;
            CapturerChangeInfo->sessionId = streamChangeInfo.audioCapturerChangeInfo.sessionId;
            CapturerChangeInfo->capturerState = streamChangeInfo.audioCapturerChangeInfo.capturerState;
            CapturerChangeInfo->capturerInfo = streamChangeInfo.audioCapturerChangeInfo.capturerInfo;
            CapturerChangeInfo->inputDeviceInfo = streamChangeInfo.audioCapturerChangeInfo.inputDeviceInfo;
            *it = move(CapturerChangeInfo);

            mDispatcherService.SendCapturerInfoEventToDispatcher(AudioMode::AUDIO_MODE_RECORD,
                audioCapturerChangeInfos_);
            if (streamChangeInfo.audioCapturerChangeInfo.capturerState ==  CAPTURER_RELEASED) {
                audioCapturerChangeInfos_.erase(it);
                AUDIO_DEBUG_LOG("UpdateCapturerStream::Session is removed for client %{public}d session %{public}d",
                    streamChangeInfo.audioCapturerChangeInfo.clientUID,
                    streamChangeInfo.audioCapturerChangeInfo.sessionId);
                capturerStatequeue_.erase(make_pair(audioCapturerChangeInfo.clientUID,
                    audioCapturerChangeInfo.sessionId));
                clientTracker_.erase(audioCapturerChangeInfo.sessionId);
            }
        return SUCCESS;
        }
    }
    AUDIO_INFO_LOG("AudioStreamCollector:UpdateCapturerStream: clientUI not in audioCapturerChangeInfos_::%{public}d",
        streamChangeInfo.audioCapturerChangeInfo.clientUID);
    return SUCCESS;
}

int32_t AudioStreamCollector::UpdateRendererDeviceInfo(DeviceInfo &outputDeviceInfo)
{
    bool deviceInfoUpdated = false;

    for (auto it = audioRendererChangeInfos_.begin(); it != audioRendererChangeInfos_.end(); it++) {
        if ((*it)->outputDeviceInfo.deviceType != outputDeviceInfo.deviceType) {
            AUDIO_DEBUG_LOG("UpdateRendererDeviceInfo: old device: %{public}d new device: %{public}d",
                (*it)->outputDeviceInfo.deviceType, outputDeviceInfo.deviceType);
            (*it)->outputDeviceInfo = outputDeviceInfo;
            deviceInfoUpdated = true;
        }
    }

    if (deviceInfoUpdated) {
        mDispatcherService.SendRendererInfoEventToDispatcher(AudioMode::AUDIO_MODE_PLAYBACK,
            audioRendererChangeInfos_);
    }

    return SUCCESS;
}

int32_t AudioStreamCollector::UpdateCapturerDeviceInfo(DeviceInfo &inputDeviceInfo)
{
    bool deviceInfoUpdated = false;

    for (auto it = audioCapturerChangeInfos_.begin(); it != audioCapturerChangeInfos_.end(); it++) {
        if ((*it)->inputDeviceInfo.deviceType != inputDeviceInfo.deviceType) {
            AUDIO_DEBUG_LOG("UpdateCapturerDeviceInfo: old device: %{public}d new device: %{public}d",
                (*it)->inputDeviceInfo.deviceType, inputDeviceInfo.deviceType);
            (*it)->inputDeviceInfo = inputDeviceInfo;
            deviceInfoUpdated = true;
        }
    }

    if (deviceInfoUpdated) {
        mDispatcherService.SendRendererInfoEventToDispatcher(AudioMode::AUDIO_MODE_PLAYBACK,
            audioRendererChangeInfos_);
    }

    return SUCCESS;
}

int32_t AudioStreamCollector::UpdateTracker(const AudioMode &mode, DeviceInfo &deviceInfo)
{
    std::lock_guard<std::mutex> lock(streamsInfoMutex_);
    if (mode == AUDIO_MODE_PLAYBACK) {
        UpdateRendererDeviceInfo(deviceInfo);
    } else {
        UpdateCapturerDeviceInfo(deviceInfo);
    }

    return SUCCESS;
}

int32_t AudioStreamCollector::UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo)
{
    std::lock_guard<std::mutex> lock(streamsInfoMutex_);
    // update the stream change info
    if (mode == AUDIO_MODE_PLAYBACK) {
        UpdateRendererStream(streamChangeInfo);
    } else {
    // mode = AUDIO_MODE_RECORD
        UpdateCapturerStream(streamChangeInfo);
    }
    return SUCCESS;
}

int32_t AudioStreamCollector::GetCurrentRendererChangeInfos(
    vector<unique_ptr<AudioRendererChangeInfo>> &rendererChangeInfos)
{
    std::lock_guard<std::mutex> lock(streamsInfoMutex_);
    for (const auto &changeInfo : audioRendererChangeInfos_) {
        rendererChangeInfos.push_back(make_unique<AudioRendererChangeInfo>(*changeInfo));
    }
    AUDIO_DEBUG_LOG("AudioStreamCollector::GetCurrentRendererChangeInfos returned");

    return SUCCESS;
}

int32_t AudioStreamCollector::GetCurrentCapturerChangeInfos(
    vector<unique_ptr<AudioCapturerChangeInfo>> &capturerChangeInfos)
{
    AUDIO_DEBUG_LOG("AudioStreamCollector::GetCurrentCapturerChangeInfos");
    std::lock_guard<std::mutex> lock(streamsInfoMutex_);
    for (const auto &changeInfo : audioCapturerChangeInfos_) {
        capturerChangeInfos.push_back(make_unique<AudioCapturerChangeInfo>(*changeInfo));
    AUDIO_DEBUG_LOG("AudioStreamCollector::GetCurrentCapturerChangeInfos returned");
    }

    return SUCCESS;
}

void AudioStreamCollector::RegisteredTrackerClientDied(int32_t uid)
{
    AUDIO_INFO_LOG("TrackerClientDied:client:%{public}d Died", uid);

    // Send the release state event notification for all streams of died client to registered app
    int32_t sessionID = -1;
    std::lock_guard<std::mutex> lock(streamsInfoMutex_);
    vector<std::unique_ptr<AudioRendererChangeInfo>>::iterator audioRendererBegin = audioRendererChangeInfos_.begin();
    while (audioRendererBegin != audioRendererChangeInfos_.end()) {
        const auto &audioRendererChangeInfo = *audioRendererBegin;
        if (audioRendererChangeInfo == nullptr || audioRendererChangeInfo->clientUID != uid) {
            audioRendererBegin++;
            continue;
        }
        sessionID = audioRendererChangeInfo->sessionId;
        audioRendererChangeInfo->rendererState = RENDERER_RELEASED;
        mDispatcherService.SendRendererInfoEventToDispatcher(AudioMode::AUDIO_MODE_PLAYBACK,
            audioRendererChangeInfos_);
        rendererStatequeue_.erase(make_pair(audioRendererChangeInfo->clientUID,
            audioRendererChangeInfo->sessionId));
        vector<std::unique_ptr<AudioRendererChangeInfo>>::iterator temp = audioRendererBegin;
        audioRendererBegin = audioRendererChangeInfos_.erase(temp);
        if ((sessionID != -1) && clientTracker_.erase(sessionID)) {
            AUDIO_DEBUG_LOG("AudioStreamCollector::TrackerClientDied:client %{public}d cleared ", sessionID);
        }
    }

    sessionID = -1;
    vector<std::unique_ptr<AudioCapturerChangeInfo>>::iterator audioCapturerBegin = audioCapturerChangeInfos_.begin();
    while (audioCapturerBegin != audioCapturerChangeInfos_.end()) {
        const auto &audioCapturerChangeInfo = *audioCapturerBegin;
        if (audioCapturerChangeInfo == nullptr || audioCapturerChangeInfo->clientUID != uid) {
            audioCapturerBegin++;
            continue;
        }
        sessionID = audioCapturerChangeInfo->sessionId;
        audioCapturerChangeInfo->capturerState = CAPTURER_RELEASED;
        mDispatcherService.SendCapturerInfoEventToDispatcher(AudioMode::AUDIO_MODE_RECORD,
            audioCapturerChangeInfos_);
        capturerStatequeue_.erase(make_pair(audioCapturerChangeInfo->clientUID,
            audioCapturerChangeInfo->sessionId));
        vector<std::unique_ptr<AudioCapturerChangeInfo>>::iterator temp = audioCapturerBegin;
        audioCapturerBegin = audioCapturerChangeInfos_.erase(temp);
        if ((sessionID != -1) && clientTracker_.erase(sessionID)) {
            AUDIO_DEBUG_LOG("AudioStreamCollector::TrackerClientDied:client %{public}d cleared", sessionID);
        }
    }
}

void AudioStreamCollector::RegisteredStreamListenerClientDied(int32_t uid)
{
    AUDIO_INFO_LOG("AudioStreamCollector::StreamListenerClientDied:client %{public}d", uid);
    mDispatcherService.removeRendererListener(uid);
    mDispatcherService.removeCapturerListener(uid);
}

bool AudioStreamCollector::GetAndCompareStreamType(AudioStreamType requiredType, AudioRendererInfo rendererInfo)
{
    AudioStreamType defaultStreamType = STREAM_MUSIC;
    auto pos = streamTypeMap_.find(make_pair(rendererInfo.contentType, rendererInfo.streamUsage));
    if (pos != streamTypeMap_.end()) {
        defaultStreamType = pos->second;
    }
    return defaultStreamType == requiredType;
}

int32_t AudioStreamCollector::UpdateStreamState(int32_t clientUid,
    StreamSetStateEventInternal &streamSetStateEventInternal)
{
    std::lock_guard<std::mutex> lock(streamsInfoMutex_);
    for (const auto &changeInfo : audioRendererChangeInfos_) {
        if (changeInfo->clientUID == clientUid &&
            GetAndCompareStreamType(streamSetStateEventInternal.audioStreamType, changeInfo->rendererInfo)) {
            AUDIO_INFO_LOG("AudioStreamCollector:UpdateStreamState Found matching uid and type");
            std::shared_ptr<AudioClientTracker> callback = clientTracker_[changeInfo->sessionId];
            if (callback == nullptr) {
                AUDIO_ERR_LOG("AudioStreamCollector:UpdateStreamState callback failed sId:%{public}d",
                    changeInfo->sessionId);
                continue;
            }
            if (streamSetStateEventInternal.streamSetState == StreamSetState::STREAM_PAUSE) {
                callback->PausedStreamImpl(streamSetStateEventInternal);
            } else if (streamSetStateEventInternal.streamSetState == StreamSetState::STREAM_RESUME) {
                callback->ResumeStreamImpl(streamSetStateEventInternal);
            }
        }
    }

    return SUCCESS;
}

int32_t AudioStreamCollector::SetLowPowerVolume(int32_t streamId, float volume)
{
    std::lock_guard<std::mutex> lock(streamsInfoMutex_);
    CHECK_AND_RETURN_RET_LOG(!(clientTracker_.count(streamId) == 0),
        ERR_INVALID_PARAM, "AudioStreamCollector:SetLowPowerVolume streamId invalid.");
    std::shared_ptr<AudioClientTracker> callback = clientTracker_[streamId];
    CHECK_AND_RETURN_RET_LOG(callback != nullptr,
        ERR_INVALID_PARAM, "AudioStreamCollector:SetLowPowerVolume callback failed");
    callback->SetLowPowerVolumeImpl(volume);
    return SUCCESS;
}

float AudioStreamCollector::GetLowPowerVolume(int32_t streamId)
{
    std::lock_guard<std::mutex> lock(streamsInfoMutex_);
    CHECK_AND_RETURN_RET_LOG(!(clientTracker_.count(streamId) == 0),
        ERR_INVALID_PARAM, "AudioStreamCollector:GetLowPowerVolume streamId invalid.");
    float volume;
    std::shared_ptr<AudioClientTracker> callback = clientTracker_[streamId];
    CHECK_AND_RETURN_RET_LOG(callback != nullptr,
        ERR_INVALID_PARAM, "AudioStreamCollector:GetLowPowerVolume callback failed");
    callback->GetLowPowerVolumeImpl(volume);
    return volume;
}

float AudioStreamCollector::GetSingleStreamVolume(int32_t streamId)
{
    std::lock_guard<std::mutex> lock(streamsInfoMutex_);
    CHECK_AND_RETURN_RET_LOG(!(clientTracker_.count(streamId) == 0),
        ERR_INVALID_PARAM, "AudioStreamCollector:GetSingleStreamVolume streamId invalid.");
    float volume;
    std::shared_ptr<AudioClientTracker> callback = clientTracker_[streamId];
    CHECK_AND_RETURN_RET_LOG(callback != nullptr,
        ERR_INVALID_PARAM, "AudioStreamCollector:GetSingleStreamVolume callback failed");
    callback->GetSingleStreamVolumeImpl(volume);
    return volume;
}
} // namespace AudioStandard
} // namespace OHOS
