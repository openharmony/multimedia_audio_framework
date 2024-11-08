/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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
#define LOG_TAG "AudioPolicyServerHandler"
#endif

#include "audio_policy_server_handler.h"
#include "audio_policy_service.h"
#include "audio_utils.h"

namespace OHOS {
namespace AudioStandard {
namespace  {
    constexpr int32_t MAX_DELAY_TIME = 4 * 1000;
}
AudioPolicyServerHandler::AudioPolicyServerHandler() : AppExecFwk::EventHandler(
    AppExecFwk::EventRunner::Create("OS_APAsyncRunner"))
{
    AUDIO_DEBUG_LOG("ctor");
}

AudioPolicyServerHandler::~AudioPolicyServerHandler()
{
    AUDIO_WARNING_LOG("dtor should not happen");
};

void AudioPolicyServerHandler::Init(std::shared_ptr<IAudioInterruptEventDispatcher> dispatcher)
{
    interruptEventDispatcher_ = dispatcher;
}

void AudioPolicyServerHandler::AddAudioPolicyClientProxyMap(int32_t clientPid, const sptr<IAudioPolicyClient>& cb)
{
    std::lock_guard<std::mutex> lock(runnerMutex_);
    auto [it, res] = audioPolicyClientProxyAPSCbsMap_.try_emplace(clientPid, cb);
    if (!res) {
        if (cb == it->second) {
            AUDIO_WARNING_LOG("Duplicate registration");
        } else {
            AUDIO_ERR_LOG("client registers multiple callbacks, the callback may be lost.");
        }
    }
    AUDIO_INFO_LOG("group data num [%{public}zu] pid [%{public}d]",
        audioPolicyClientProxyAPSCbsMap_.size(), clientPid);
}

void AudioPolicyServerHandler::RemoveAudioPolicyClientProxyMap(pid_t clientPid)
{
    std::lock_guard<std::mutex> lock(runnerMutex_);
    audioPolicyClientProxyAPSCbsMap_.erase(clientPid);
    clientCallbacksMap_.erase(clientPid);
    AUDIO_INFO_LOG("RemoveAudioPolicyClientProxyMap, group data num [%{public}zu]",
        audioPolicyClientProxyAPSCbsMap_.size());
}

void AudioPolicyServerHandler::AddExternInterruptCbsMap(int32_t clientId,
    const std::shared_ptr<AudioInterruptCallback> &callback)
{
    std::lock_guard<std::mutex> lock(runnerMutex_);
    amInterruptCbsMap_[clientId] = callback;
    AUDIO_INFO_LOG("AddExternInterruptCbsMap, group data num [%{public}zu]",
        amInterruptCbsMap_.size());
}

int32_t AudioPolicyServerHandler::RemoveExternInterruptCbsMap(int32_t clientId)
{
    std::lock_guard<std::mutex> lock(runnerMutex_);
    if (amInterruptCbsMap_.erase(clientId) == 0) {
        AUDIO_ERR_LOG("RemoveExternInterruptCbsMap client %{public}d not present", clientId);
        return ERR_INVALID_OPERATION;
    }
    return SUCCESS;
}

void AudioPolicyServerHandler::AddAvailableDeviceChangeMap(int32_t clientId, const AudioDeviceUsage usage,
    const sptr<IStandardAudioPolicyManagerListener> &callback)
{
    std::lock_guard<std::mutex> lock(runnerMutex_);
    availableDeviceChangeCbsMap_[{clientId, usage}] = callback;
    AUDIO_INFO_LOG("AddAvailableDeviceChangeMap, group data num [%{public}zu]", availableDeviceChangeCbsMap_.size());
}

void AudioPolicyServerHandler::RemoveAvailableDeviceChangeMap(const int32_t clientId, AudioDeviceUsage usage)
{
    std::lock_guard<std::mutex> lock(runnerMutex_);
    if (availableDeviceChangeCbsMap_.erase({clientId, usage}) == 0) {
        AUDIO_INFO_LOG("client not present in %{public}s", __func__);
    }
    // for routing manager napi remove all device change callback
    if (usage == AudioDeviceUsage::D_ALL_DEVICES) {
        for (auto it = availableDeviceChangeCbsMap_.begin(); it != availableDeviceChangeCbsMap_.end();) {
            if ((*it).first.first == clientId) {
                it = availableDeviceChangeCbsMap_.erase(it);
            } else {
                it++;
            }
        }
    }
    AUDIO_INFO_LOG("RemoveAvailableDeviceChangeMap, group data num [%{public}zu]",
        availableDeviceChangeCbsMap_.size());
}

void AudioPolicyServerHandler::AddDistributedRoutingRoleChangeCbsMap(int32_t clientId,
    const sptr<IStandardAudioRoutingManagerListener> &callback)
{
    std::lock_guard<std::mutex> lock(runnerMutex_);
    if (callback != nullptr) {
        distributedRoutingRoleChangeCbsMap_[clientId] = callback;
    }
    AUDIO_DEBUG_LOG("SetDistributedRoutingRoleCallback: distributedRoutingRoleChangeCbsMap_ size: %{public}zu",
        distributedRoutingRoleChangeCbsMap_.size());
}

int32_t AudioPolicyServerHandler::RemoveDistributedRoutingRoleChangeCbsMap(int32_t clientId)
{
    std::lock_guard<std::mutex> lock(runnerMutex_);
    if (distributedRoutingRoleChangeCbsMap_.erase(clientId) == 0) {
        AUDIO_ERR_LOG("RemoveDistributedRoutingRoleChangeCbsMap clientPid %{public}d not present", clientId);
        return ERR_INVALID_OPERATION;
    }

    AUDIO_DEBUG_LOG("UnsetDistributedRoutingRoleCallback: distributedRoutingRoleChangeCbsMap_ size: %{public}zu",
        distributedRoutingRoleChangeCbsMap_.size());
    return SUCCESS;
}

void AudioPolicyServerHandler::AddConcurrencyEventDispatcher(std::shared_ptr<IAudioConcurrencyEventDispatcher>
    dispatcher)
{
    concurrencyEventDispatcher_ = dispatcher;
}

bool AudioPolicyServerHandler::SendDeviceChangedCallback(const std::vector<sptr<AudioDeviceDescriptor>> &desc,
    bool isConnected)
{
    Trace trace("AudioPolicyServerHandler::SendDeviceChangedCallback");
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->deviceChangeAction.type = isConnected ? DeviceChangeType::CONNECT : DeviceChangeType::DISCONNECT;
    eventContextObj->deviceChangeAction.deviceDescriptors = desc;

    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::AUDIO_DEVICE_CHANGE, eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendDeviceChangedCallback event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendMicrophoneBlockedCallback(const std::vector<sptr<AudioDeviceDescriptor>> &desc,
    DeviceBlockStatus status)
{
    Trace trace("AudioPolicyServerHandler::SendMicrophoneBlockedCallback");
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->microphoneBlockedInfo.status = status;
    eventContextObj->microphoneBlockedInfo.deviceDescriptors = desc;

    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::MICROPHONE_BLOCKED, eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendMicrophoneBlockedCallback event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendAvailableDeviceChange(const std::vector<sptr<AudioDeviceDescriptor>> &desc,
    bool isConnected)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->deviceChangeAction.type = isConnected ? DeviceChangeType::CONNECT : DeviceChangeType::DISCONNECT;
    eventContextObj->deviceChangeAction.deviceDescriptors = desc;

    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::AVAILABLE_AUDIO_DEVICE_CHANGE,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendAvailableDeviceChange event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendVolumeKeyEventCallback(const VolumeEvent &volumeEvent)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->volumeEvent = volumeEvent;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::VOLUME_KEY_EVENT, eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendVolumeKeyEventCallback event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendAudioSessionDeactiveCallback(
    const std::pair<int32_t, AudioSessionDeactiveEvent> &sessionDeactivePair)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->sessionDeactivePair = sessionDeactivePair;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::AUDIO_SESSION_DEACTIVE_EVENT,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendAudioSessionDeactiveCallback event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendAudioFocusInfoChangeCallback(int32_t callbackCategory,
    const AudioInterrupt &audioInterrupt, const std::list<std::pair<AudioInterrupt, AudioFocuState>> &focusInfoList)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->audioInterrupt = audioInterrupt;
    eventContextObj->focusInfoList = focusInfoList;
    bool ret = false;

    lock_guard<mutex> runnerlock(runnerMutex_);
    if (callbackCategory == FocusCallbackCategory::REQUEST_CALLBACK_CATEGORY) {
        ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::REQUEST_CATEGORY_EVENT, eventContextObj));
        CHECK_AND_RETURN_RET_LOG(ret, ret, "Send REQUEST_CATEGORY_EVENT event failed");
    } else if (callbackCategory == FocusCallbackCategory::ABANDON_CALLBACK_CATEGORY) {
        ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::ABANDON_CATEGORY_EVENT, eventContextObj));
        CHECK_AND_RETURN_RET_LOG(ret, ret, "Send ABANDON_CATEGORY_EVENT event failed");
    }
    ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::FOCUS_INFOCHANGE, eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send FOCUS_INFOCHANGE event failed");

    return ret;
}

bool AudioPolicyServerHandler::SendRingerModeUpdatedCallback(const AudioRingerMode &ringMode)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->ringMode = ringMode;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::RINGER_MODEUPDATE_EVENT, eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send RINGER_MODEUPDATE_EVENT event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendMicStateUpdatedCallback(const MicStateChangeEvent &micStateChangeEvent)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->micStateChangeEvent = micStateChangeEvent;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::MIC_STATE_CHANGE_EVENT, eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send MIC_STATE_CHANGE_EVENT event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendMicStateWithClientIdCallback(const MicStateChangeEvent &micStateChangeEvent,
    int32_t clientId)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->micStateChangeEvent = micStateChangeEvent;
    eventContextObj->clientId = clientId;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::MIC_STATE_CHANGE_EVENT_WITH_CLIENTID,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send MIC_STATE_CHANGE_EVENT_WITH_CLIENTID event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendInterruptEventInternalCallback(const InterruptEventInternal &interruptEvent)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->interruptEvent = interruptEvent;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::INTERRUPT_EVENT, eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send INTERRUPT_EVENT event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendInterruptEventWithSessionIdCallback(const InterruptEventInternal &interruptEvent,
    const uint32_t &sessionId)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->interruptEvent = interruptEvent;
    eventContextObj->sessionId = sessionId;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::INTERRUPT_EVENT_WITH_SESSIONID,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send INTERRUPT_EVENT_WITH_SESSIONID event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendInterruptEventWithClientIdCallback(const InterruptEventInternal &interruptEvent,
    const int32_t &clientId)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->interruptEvent = interruptEvent;
    eventContextObj->clientId = clientId;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::INTERRUPT_EVENT_WITH_CLIENTID,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send INTERRUPT_EVENT_WITH_CLIENTID event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendPreferredOutputDeviceUpdated()
{
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::PREFERRED_OUTPUT_DEVICE_UPDATED));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendPreferredOutputDeviceUpdated event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendPreferredInputDeviceUpdated()
{
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::PREFERRED_INPUT_DEVICE_UPDATED));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendPreferredInputDeviceUpdated event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendDistributedRoutingRoleChange(const sptr<AudioDeviceDescriptor> descriptor,
    const CastType &type)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->descriptor = descriptor;
    eventContextObj->type = type;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::DISTRIBUTED_ROUTING_ROLE_CHANGE,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendDistributedRoutingRoleChange event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendRendererInfoEvent(
    const std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos)
{
    std::vector<std::unique_ptr<AudioRendererChangeInfo>> rendererChangeInfos;
    for (const auto &changeInfo : audioRendererChangeInfos) {
        rendererChangeInfos.push_back(std::make_unique<AudioRendererChangeInfo>(*changeInfo));
    }

    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->audioRendererChangeInfos = move(rendererChangeInfos);

    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::RENDERER_INFO_EVENT,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendRendererInfoEvent event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendCapturerInfoEvent(
    const std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> capturerChangeInfos;
    for (const auto &changeInfo : audioCapturerChangeInfos) {
        capturerChangeInfos.push_back(std::make_unique<AudioCapturerChangeInfo>(*changeInfo));
    }

    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->audioCapturerChangeInfos = move(capturerChangeInfos);

    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::CAPTURER_INFO_EVENT,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendRendererInfoEvent event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendRendererDeviceChangeEvent(const int32_t clientPid, const uint32_t sessionId,
    const DeviceInfo &outputDeviceInfo, const AudioStreamDeviceChangeReasonExt reason)
{
    std::shared_ptr<RendererDeviceChangeEvent> eventContextObj = std::make_shared<RendererDeviceChangeEvent>(
        clientPid, sessionId, outputDeviceInfo, reason);
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");

    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::RENDERER_DEVICE_CHANGE_EVENT,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendRendererDeviceChangeEvent event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendCapturerCreateEvent(AudioCapturerInfo capturerInfo,
    AudioStreamInfo streamInfo, uint64_t sessionId, bool isSync, int32_t &error)
{
    auto eventContextObj = std::make_shared<CapturerCreateEvent>(capturerInfo, streamInfo, sessionId, SUCCESS);
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");

    bool ret;
    if (isSync) {
        ret = SendSyncEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::ON_CAPTURER_CREATE,
            eventContextObj));
        error = eventContextObj->error_;
    } else {
        ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::ON_CAPTURER_CREATE,
            eventContextObj));
        error = SUCCESS;
    }
    CHECK_AND_RETURN_RET_LOG(ret, ret, "failed");
    return ret;
}

bool AudioPolicyServerHandler::SendCapturerRemovedEvent(uint64_t sessionId, bool isSync)
{
    auto eventContextObj = std::make_shared<uint64_t>(sessionId);
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");

    bool ret;
    if (isSync) {
        ret = SendSyncEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::ON_CAPTURER_REMOVED,
            eventContextObj));
    } else {
        ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::ON_CAPTURER_REMOVED,
            eventContextObj));
    }
    CHECK_AND_RETURN_RET_LOG(ret, ret, "failed");
    return ret;
}

bool AudioPolicyServerHandler::SendWakeupCloseEvent(bool isSync)
{
    bool ret;
    if (isSync) {
        ret = SendSyncEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::ON_WAKEUP_CLOSE));
    } else {
        ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::ON_WAKEUP_CLOSE));
    }
    CHECK_AND_RETURN_RET_LOG(ret, ret, "failed");
    return ret;
}

bool AudioPolicyServerHandler::SendRecreateRendererStreamEvent(int32_t clientId, uint32_t sessionID, int32_t streamFlag,
    const AudioStreamDeviceChangeReasonExt reason)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->clientId = clientId;
    eventContextObj->sessionId = sessionID;
    eventContextObj->streamFlag = streamFlag;
    eventContextObj->reason_ = reason;
    return SendSyncEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::RECREATE_RENDERER_STREAM_EVENT,
        eventContextObj));
}

bool AudioPolicyServerHandler::SendRecreateCapturerStreamEvent(int32_t clientId, uint32_t sessionID, int32_t streamFlag,
    const AudioStreamDeviceChangeReasonExt reason)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->clientId = clientId;
    eventContextObj->sessionId = sessionID;
    eventContextObj->streamFlag = streamFlag;
    eventContextObj->reason_ = reason;
    return SendSyncEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::RECREATE_CAPTURER_STREAM_EVENT,
        eventContextObj));
}

bool AudioPolicyServerHandler::SendHeadTrackingDeviceChangeEvent(
    const std::unordered_map<std::string, bool> &changeInfo)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->headTrackingDeviceChangeInfo = changeInfo;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::HEAD_TRACKING_DEVICE_CHANGE,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send HEAD_TRACKING_DEVICE_CHANGE event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendSpatializatonEnabledChangeEvent(const bool &enabled)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->spatializationEnabled = enabled;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::SPATIALIZATION_ENABLED_CHANGE,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send SPATIALIZATION_ENABLED_CHANGE event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendSpatializatonEnabledChangeForAnyDeviceEvent(const sptr<AudioDeviceDescriptor>
    &selectedAudioDevice, const bool &enabled)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->spatializationEnabled = enabled;
    eventContextObj->descriptor = selectedAudioDevice;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::SPATIALIZATION_ENABLED_CHANGE_FOR_ANY_DEVICE,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send SPATIALIZATION_ENABLED_CHANGE event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendHeadTrackingEnabledChangeEvent(const bool &enabled)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->headTrackingEnabled = enabled;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::HEAD_TRACKING_ENABLED_CHANGE,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send HEAD_TRACKING_ENABLED_CHANGE event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendKvDataUpdate(const bool &isFirstBoot)
{
    auto eventContextObj = std::make_shared<bool>(isFirstBoot);
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = true;
    if (isFirstBoot) {
        ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::DATABASE_UPDATE, eventContextObj),
            MAX_DELAY_TIME);
    } else {
        ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::DATABASE_UPDATE, eventContextObj));
    }
    CHECK_AND_RETURN_RET_LOG(ret, ret, "SendKvDataUpdate event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendHeadTrackingEnabledChangeForAnyDeviceEvent(const sptr<AudioDeviceDescriptor>
    &selectedAudioDevice,
    const bool &enabled)
{
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->headTrackingEnabled = enabled;
    eventContextObj->descriptor = selectedAudioDevice;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::HEAD_TRACKING_ENABLED_CHANGE_FOR_ANY_DEVICE,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send HEAD_TRACKING_ENABLED_CHANGE event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendPipeStreamCleanEvent(AudioPipeType pipeType)
{
    auto eventContextObj = std::make_shared<int32_t>(pipeType);
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::PIPE_STREAM_CLEAN_EVENT,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send PIPE_STREAM_CLEAN_EVENT event failed");
    return ret;
}

bool AudioPolicyServerHandler::SendConcurrencyEventWithSessionIDCallback(const uint32_t sessionID)
{
    AUDIO_INFO_LOG("session %{public}u send concurrency event", sessionID);
    std::shared_ptr<EventContextObj> eventContextObj = std::make_shared<EventContextObj>();
    CHECK_AND_RETURN_RET_LOG(eventContextObj != nullptr, false, "EventContextObj get nullptr");
    eventContextObj->sessionId = sessionID;
    lock_guard<mutex> runnerlock(runnerMutex_);
    bool ret = SendEvent(AppExecFwk::InnerEvent::Get(EventAudioServerCmd::CONCURRENCY_EVENT_WITH_SESSIONID,
        eventContextObj));
    CHECK_AND_RETURN_RET_LOG(ret, ret, "Send CONCURRENCY_EVENT_WITH_SESSIONID event failed");
    return ret;
}

void AudioPolicyServerHandler::HandleDeviceChangedCallback(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        if (it->second && eventContextObj->deviceChangeAction.deviceDescriptors.size() > 0) {
            DeviceChangeAction deviceChangeAction = eventContextObj->deviceChangeAction;
            if (!(it->second->hasBTPermission_)) {
                AudioPolicyService::GetAudioPolicyService().
                    UpdateDescWhenNoBTPermission(deviceChangeAction.deviceDescriptors);
            }
            if (clientCallbacksMap_.count(it->first) > 0 &&
                clientCallbacksMap_[it->first].count(CALLBACK_SET_DEVICE_CHANGE) > 0 &&
                clientCallbacksMap_[it->first][CALLBACK_SET_DEVICE_CHANGE]) {
                it->second->OnDeviceChange(deviceChangeAction);
            }
        }
    }
}

void AudioPolicyServerHandler::HandleMicrophoneBlockedCallback(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);

    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        if (it->second && eventContextObj->micPhoneBlockedInfo.deviceDescriptors.size() > 0) {
            MicPhoneBlockedInfo micPhoneBlockedInfo = eventContextObj->micPhoneBlockedInfo;
            it->second->OnMicrophoneBlocked(micPhoneBlockedInfo);
        }
    }
}

void AudioPolicyServerHandler::HandleMicrophoneBlockedCallback(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);

    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        if (it->second && eventContextObj->microphoneBlockedInfo.deviceDescriptors.size() > 0) {
            MicrophoneBlockedInfo microphoneBlockedInfo = eventContextObj->microphoneBlockedInfo;
            it->second->OnMicrophoneBlocked(microphoneBlockedInfo);
        }
    }
}

void AudioPolicyServerHandler::HandleMicrophoneBlockedCallback(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);

    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        if (it->second && eventContextObj->microphoneBlockedInfo.deviceDescriptors.size() > 0) {
            MicrophoneBlockedInfo microphoneBlockedInfo = eventContextObj->microphoneBlockedInfo;
            it->second->OnMicrophoneBlocked(microphoneBlockedInfo);
        }
    }
}

void AudioPolicyServerHandler::HandleAvailableDeviceChange(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = availableDeviceChangeCbsMap_.begin(); it != availableDeviceChangeCbsMap_.end(); ++it) {
        AudioDeviceUsage usage = it->first.second;
        eventContextObj->deviceChangeAction.deviceDescriptors = AudioPolicyService::GetAudioPolicyService().
            DeviceFilterByUsageInner(it->first.second, eventContextObj->deviceChangeAction.deviceDescriptors);
        if (it->second && eventContextObj->deviceChangeAction.deviceDescriptors.size() > 0) {
            if (!(it->second->hasBTPermission_)) {
                AudioPolicyService::GetAudioPolicyService().
                    UpdateDescWhenNoBTPermission(eventContextObj->deviceChangeAction.deviceDescriptors);
            }
            it->second->OnAvailableDeviceChange(usage, eventContextObj->deviceChangeAction);
        }
    }
}

void AudioPolicyServerHandler::HandleVolumeKeyEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        sptr<IAudioPolicyClient> volumeChangeCb = it->second;
        if (volumeChangeCb == nullptr) {
            AUDIO_ERR_LOG("volumeChangeCb: nullptr for client : %{public}d", it->first);
            continue;
        }
        AUDIO_PRERELEASE_LOGI("Trigger volumeChangeCb clientPid : %{public}d, volumeType : %{public}d," \
            " volume : %{public}d, updateUi : %{public}d ", it->first,
            static_cast<int32_t>(eventContextObj->volumeEvent.volumeType), eventContextObj->volumeEvent.volume,
            static_cast<int32_t>(eventContextObj->volumeEvent.updateUi));
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_SET_VOLUME_KEY_EVENT) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_SET_VOLUME_KEY_EVENT]) {
            volumeChangeCb->OnVolumeKeyEvent(eventContextObj->volumeEvent);
        }
    }
}

void AudioPolicyServerHandler::HandleAudioSessionDeactiveCallback(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    int32_t clientPid = eventContextObj->sessionDeactivePair.first;
    auto iterator = audioPolicyClientProxyAPSCbsMap_.find(clientPid);
    if (iterator == audioPolicyClientProxyAPSCbsMap_.end()) {
        AUDIO_ERR_LOG("AudioSessionDeactiveCallback: no client callback for client pid %{public}d", clientPid);
        return;
    }
    if (clientCallbacksMap_.count(iterator->first) > 0 &&
        clientCallbacksMap_[iterator->first].count(CALLBACK_AUDIO_SESSION) > 0 &&
        clientCallbacksMap_[iterator->first][CALLBACK_AUDIO_SESSION]) {
        // the client has registered audio session callback.
        sptr<IAudioPolicyClient> audioSessionCb = iterator->second;
        if (audioSessionCb == nullptr) {
            AUDIO_ERR_LOG("AudioSessionDeactiveCallback: nullptr for client pid %{public}d", clientPid);
            return;
        }
        AUDIO_INFO_LOG("Trigger AudioSessionDeactiveCallback for client pid : %{public}d", clientPid);
        audioSessionCb->OnAudioSessionDeactive(eventContextObj->sessionDeactivePair.second);
    } else {
        AUDIO_ERR_LOG("AudioSessionDeactiveCallback: no registered callback for pid %{public}d", clientPid);
    }
}

void AudioPolicyServerHandler::HandleRequestCateGoryEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");

    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_FOCUS_INFO_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_FOCUS_INFO_CHANGE]) {
            it->second->OnAudioFocusRequested(eventContextObj->audioInterrupt);
        }
    }
}

void AudioPolicyServerHandler::HandleAbandonCateGoryEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_FOCUS_INFO_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_FOCUS_INFO_CHANGE]) {
            it->second->OnAudioFocusAbandoned(eventContextObj->audioInterrupt);
        }
    }
}

void AudioPolicyServerHandler::HandleFocusInfoChangeEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    AUDIO_INFO_LOG("HandleFocusInfoChangeEvent focusInfoList :%{public}zu", eventContextObj->focusInfoList.size());
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_FOCUS_INFO_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_FOCUS_INFO_CHANGE]) {
            it->second->OnAudioFocusInfoChange(eventContextObj->focusInfoList);
        }
    }
}

void AudioPolicyServerHandler::HandleRingerModeUpdatedEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        sptr<IAudioPolicyClient> ringerModeListenerCb = it->second;
        if (ringerModeListenerCb == nullptr) {
            AUDIO_ERR_LOG("ringerModeListenerCb nullptr for client %{public}d", it->first);
            continue;
        }

        AUDIO_DEBUG_LOG("ringerModeListenerCb client %{public}d", it->first);
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_SET_RINGER_MODE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_SET_RINGER_MODE]) {
            ringerModeListenerCb->OnRingerModeUpdated(eventContextObj->ringMode);
        }
    }
}

void AudioPolicyServerHandler::HandleMicStateUpdatedEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        sptr<IAudioPolicyClient> micStateChangeListenerCb = it->second;
        if (micStateChangeListenerCb == nullptr) {
            AUDIO_ERR_LOG("callback is nullptr for client %{public}d", it->first);
            continue;
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_SET_MIC_STATE_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_SET_MIC_STATE_CHANGE]) {
            micStateChangeListenerCb->OnMicStateUpdated(eventContextObj->micStateChangeEvent);
        }
    }
}

void AudioPolicyServerHandler::HandleMicStateUpdatedEventWithClientId(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    int32_t clientId = eventContextObj->clientId;
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        if (it->first != clientId) {
            AUDIO_DEBUG_LOG("This client %{public}d is not need to trigger the callback ", it->first);
            continue;
        }
        sptr<IAudioPolicyClient> micStateChangeListenerCb = it->second;
        if (micStateChangeListenerCb == nullptr) {
            AUDIO_ERR_LOG("callback is nullptr for client %{public}d", it->first);
            continue;
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_SET_MIC_STATE_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_SET_MIC_STATE_CHANGE]) {
            micStateChangeListenerCb->OnMicStateUpdated(eventContextObj->micStateChangeEvent);
        }
    }
}

void AudioPolicyServerHandler::HandleInterruptEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");

    std::unique_lock<std::mutex> lock(runnerMutex_);
    std::shared_ptr<IAudioInterruptEventDispatcher> dispatcher = interruptEventDispatcher_.lock();
    lock.unlock();
    if (dispatcher != nullptr) {
        dispatcher->DispatchInterruptEventWithSessionId(0, eventContextObj->interruptEvent);
    }
}

void AudioPolicyServerHandler::HandleInterruptEventWithSessionId(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");

    std::unique_lock<std::mutex> lock(runnerMutex_);
    std::shared_ptr<IAudioInterruptEventDispatcher> dispatcher = interruptEventDispatcher_.lock();
    lock.unlock();
    if (dispatcher != nullptr) {
        dispatcher->DispatchInterruptEventWithSessionId(eventContextObj->sessionId,
            eventContextObj->interruptEvent);
    }
}

void AudioPolicyServerHandler::HandleInterruptEventWithClientId(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");

    std::lock_guard<std::mutex> lock(runnerMutex_);
    std::shared_ptr<AudioInterruptCallback> policyListenerCb = amInterruptCbsMap_[eventContextObj->clientId];
    CHECK_AND_RETURN_LOG(policyListenerCb != nullptr, "policyListenerCb get nullptr");
    policyListenerCb->OnInterrupt(eventContextObj->interruptEvent);
}

void AudioPolicyServerHandler::HandlePreferredOutputDeviceUpdated()
{
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        AudioRendererInfo rendererInfo;
        auto deviceDescs = AudioPolicyService::GetAudioPolicyService().
            GetPreferredOutputDeviceDescInner(rendererInfo);
        if (!(it->second->hasBTPermission_)) {
            AudioPolicyService::GetAudioPolicyService().UpdateDescWhenNoBTPermission(deviceDescs);
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_PREFERRED_OUTPUT_DEVICE_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_PREFERRED_OUTPUT_DEVICE_CHANGE]) {
            it->second->OnPreferredOutputDeviceUpdated(deviceDescs);
        }
    }
}

void AudioPolicyServerHandler::HandlePreferredInputDeviceUpdated()
{
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        AudioCapturerInfo captureInfo;
        auto deviceDescs = AudioPolicyService::GetAudioPolicyService().GetPreferredInputDeviceDescInner(captureInfo);
        if (!(it->second->hasBTPermission_)) {
            AudioPolicyService::GetAudioPolicyService().UpdateDescWhenNoBTPermission(deviceDescs);
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_PREFERRED_INPUT_DEVICE_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_PREFERRED_INPUT_DEVICE_CHANGE]) {
            it->second->OnPreferredInputDeviceUpdated(deviceDescs);
        }
    }
}

void AudioPolicyServerHandler::HandleDistributedRoutingRoleChangeEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = distributedRoutingRoleChangeCbsMap_.begin(); it != distributedRoutingRoleChangeCbsMap_.end(); it++) {
        it->second->OnDistributedRoutingRoleChange(eventContextObj->descriptor, eventContextObj->type);
    }
}

void AudioPolicyServerHandler::HandleRendererInfoEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    Trace trace("AudioPolicyServerHandler::HandleRendererInfoEvent");
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        Trace traceFor("for pid:" + std::to_string(it->first));
        sptr<IAudioPolicyClient> rendererStateChangeCb = it->second;
        if (rendererStateChangeCb == nullptr) {
            AUDIO_ERR_LOG("rendererStateChangeCb : nullptr for client : %{public}d", it->first);
            continue;
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_RENDERER_STATE_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_RENDERER_STATE_CHANGE]) {
                Trace traceCallback("rendererStateChangeCb->OnRendererStateChange");
            rendererStateChangeCb->OnRendererStateChange(eventContextObj->audioRendererChangeInfos);
            ResetRingerModeMute(eventContextObj->audioRendererChangeInfos);
        }
    }
}

void AudioPolicyServerHandler::ResetRingerModeMute(const std::vector<std::unique_ptr<AudioRendererChangeInfo>>
    &audioRendererChangeInfos)
{
    for (const std::unique_ptr<AudioRendererChangeInfo> &rendererChangeInfo: audioRendererChangeInfos) {
        if (!rendererChangeInfo) {
            AUDIO_ERR_LOG("Renderer change info null, something wrong!!");
            continue;
        }
        StreamUsage streamUsage = rendererChangeInfo->rendererInfo.streamUsage;
        RendererState rendererState = rendererChangeInfo->rendererState;
        if (Util::IsRingerOrAlarmerStreamUsage(streamUsage) && (rendererState == RENDERER_PAUSED ||
            rendererState == RENDERER_STOPPED || rendererState == RENDERER_RELEASED)) {
            AUDIO_INFO_LOG("reset ringer mode mute, stream usage:%{public}d, renderer state:%{public}d",
                streamUsage, rendererState);
            AudioPolicyService::GetAudioPolicyService().ResetRingerModeMute();
        }
    }
}

void AudioPolicyServerHandler::HandleCapturerInfoEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        sptr<IAudioPolicyClient> capturerStateChangeCb = it->second;
        if (capturerStateChangeCb == nullptr) {
            AUDIO_ERR_LOG("capturerStateChangeCb : nullptr for client : %{public}d", it->first);
            continue;
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_CAPTURER_STATE_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_CAPTURER_STATE_CHANGE]) {
            capturerStateChangeCb->OnCapturerStateChange(eventContextObj->audioCapturerChangeInfos);
        }
    }
}

void AudioPolicyServerHandler::HandleRendererDeviceChangeEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<RendererDeviceChangeEvent> eventContextObj = event->GetSharedObject<RendererDeviceChangeEvent>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    const auto &[pid, sessionId, outputDeviceInfo, reason] = *eventContextObj;
    Trace trace("AudioPolicyServerHandler::HandleRendererDeviceChangeEvent pid:" + std::to_string(pid));
    std::lock_guard<std::mutex> lock(runnerMutex_);
    if (audioPolicyClientProxyAPSCbsMap_.count(pid) == 0) {
        return;
    }
    sptr<IAudioPolicyClient> capturerStateChangeCb = audioPolicyClientProxyAPSCbsMap_.at(pid);
    if (capturerStateChangeCb == nullptr) {
        AUDIO_ERR_LOG("capturerStateChangeCb : nullptr for client : %{public}" PRId32 "", pid);
        return;
    }
    Trace traceCallback("capturerStateChangeCb->OnRendererDeviceChange sessionId:" + std::to_string(sessionId));
    if (clientCallbacksMap_.count(pid) > 0 &&
        clientCallbacksMap_[pid].count(CALLBACK_DEVICE_CHANGE_WITH_INFO) > 0 &&
        clientCallbacksMap_[pid][CALLBACK_DEVICE_CHANGE_WITH_INFO]) {
        capturerStateChangeCb->OnRendererDeviceChange(sessionId, outputDeviceInfo, reason);
    }
}

void AudioPolicyServerHandler::HandleCapturerCreateEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<CapturerCreateEvent> eventContextObj = event->GetSharedObject<CapturerCreateEvent>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");

    uint64_t sessionId = eventContextObj->sessionId_;
    SessionInfo sessionInfo{eventContextObj->capturerInfo_.sourceType, eventContextObj->streamInfo_.samplingRate,
        eventContextObj->streamInfo_.channels};

    eventContextObj->error_ = AudioPolicyService::GetAudioPolicyService().OnCapturerSessionAdded(sessionId,
        sessionInfo, eventContextObj->streamInfo_);
}

void AudioPolicyServerHandler::HandleCapturerRemovedEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<uint64_t> eventContextObj = event->GetSharedObject<uint64_t>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");

    uint64_t sessionId = *eventContextObj;

    AudioPolicyService::GetAudioPolicyService().OnCapturerSessionRemoved(sessionId);
}

void AudioPolicyServerHandler::HandleWakeupCloseEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    AudioPolicyService::GetAudioPolicyService().CloseWakeUpAudioCapturer();
}

void AudioPolicyServerHandler::HandleSendRecreateRendererStreamEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    if (audioPolicyClientProxyAPSCbsMap_.count(eventContextObj->clientId) == 0) {
        AUDIO_ERR_LOG("No client id %{public}d", eventContextObj->clientId);
        return;
    }
    sptr<IAudioPolicyClient> rendererCb = audioPolicyClientProxyAPSCbsMap_.at(eventContextObj->clientId);
    CHECK_AND_RETURN_LOG(rendererCb != nullptr, "Callback for id %{public}d is null", eventContextObj->clientId);

    if (clientCallbacksMap_.count(eventContextObj->clientId) > 0 &&
        clientCallbacksMap_[eventContextObj->clientId].count(CALLBACK_DEVICE_CHANGE_WITH_INFO) > 0 &&
        clientCallbacksMap_[eventContextObj->clientId][CALLBACK_DEVICE_CHANGE_WITH_INFO]) {
        rendererCb->OnRecreateRendererStreamEvent(eventContextObj->sessionId, eventContextObj->streamFlag,
            eventContextObj->reason_);
    }
}

void AudioPolicyServerHandler::HandleSendRecreateCapturerStreamEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    if (audioPolicyClientProxyAPSCbsMap_.count(eventContextObj->clientId) == 0) {
        AUDIO_ERR_LOG("No client id %{public}d", eventContextObj->clientId);
        return;
    }
    sptr<IAudioPolicyClient> capturerCb = audioPolicyClientProxyAPSCbsMap_.at(eventContextObj->clientId);
    CHECK_AND_RETURN_LOG(capturerCb != nullptr, "Callback for id %{public}d is null", eventContextObj->clientId);

    if (clientCallbacksMap_.count(eventContextObj->clientId) > 0 &&
        clientCallbacksMap_[eventContextObj->clientId].count(CALLBACK_DEVICE_CHANGE_WITH_INFO) > 0 &&
        clientCallbacksMap_[eventContextObj->clientId][CALLBACK_DEVICE_CHANGE_WITH_INFO]) {
        capturerCb->OnRecreateCapturerStreamEvent(eventContextObj->sessionId, eventContextObj->streamFlag,
            eventContextObj->reason_);
    }
}

void AudioPolicyServerHandler::HandleHeadTrackingDeviceChangeEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        sptr<IAudioPolicyClient> headTrackingDeviceChangeCb = it->second;
        if (headTrackingDeviceChangeCb == nullptr) {
            AUDIO_ERR_LOG("headTrackingDeviceChangeCb : nullptr for client : %{public}d", it->first);
            continue;
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_HEAD_TRACKING_DATA_REQUESTED_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_HEAD_TRACKING_DATA_REQUESTED_CHANGE]) {
            headTrackingDeviceChangeCb->OnHeadTrackingDeviceChange(eventContextObj->headTrackingDeviceChangeInfo);
        }
    }
}

void AudioPolicyServerHandler::HandleSpatializatonEnabledChangeEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        sptr<IAudioPolicyClient> spatializationEnabledChangeCb = it->second;
        if (spatializationEnabledChangeCb == nullptr) {
            AUDIO_ERR_LOG("spatializationEnabledChangeCb : nullptr for client : %{public}d", it->first);
            continue;
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_SPATIALIZATION_ENABLED_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_SPATIALIZATION_ENABLED_CHANGE]) {
            spatializationEnabledChangeCb->OnSpatializationEnabledChange(eventContextObj->spatializationEnabled);
        }
    }
}

void AudioPolicyServerHandler::HandleSpatializatonEnabledChangeForAnyDeviceEvent(
    const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        sptr<IAudioPolicyClient> spatializationEnabledChangeCb = it->second;
        if (spatializationEnabledChangeCb == nullptr) {
            AUDIO_ERR_LOG("spatializationEnabledChangeCb : nullptr for client : %{public}d", it->first);
            continue;
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_SPATIALIZATION_ENABLED_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_SPATIALIZATION_ENABLED_CHANGE]) {
            spatializationEnabledChangeCb->OnSpatializationEnabledChangeForAnyDevice(eventContextObj->descriptor,
                eventContextObj->spatializationEnabled);
        }
    }
}

void AudioPolicyServerHandler::HandleHeadTrackingEnabledChangeEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        sptr<IAudioPolicyClient> headTrackingEnabledChangeCb = it->second;
        if (headTrackingEnabledChangeCb == nullptr) {
            AUDIO_ERR_LOG("headTrackingEnabledChangeCb : nullptr for client : %{public}d", it->first);
            continue;
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_HEAD_TRACKING_ENABLED_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_HEAD_TRACKING_ENABLED_CHANGE]) {
            headTrackingEnabledChangeCb->OnHeadTrackingEnabledChange(eventContextObj->headTrackingEnabled);
        }
    }
}

void AudioPolicyServerHandler::HandleUpdateKvDataEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<bool> eventContextObj = event->GetSharedObject<bool>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    bool isFristBoot = *eventContextObj;
    AudioPolicyManagerFactory::GetAudioPolicyManager().HandleKvData(isFristBoot);
}

void AudioPolicyServerHandler::HandleHeadTrackingEnabledChangeForAnyDeviceEvent(
    const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    std::lock_guard<std::mutex> lock(runnerMutex_);
    for (auto it = audioPolicyClientProxyAPSCbsMap_.begin(); it != audioPolicyClientProxyAPSCbsMap_.end(); ++it) {
        sptr<IAudioPolicyClient> headTrackingEnabledChangeCb = it->second;
        if (headTrackingEnabledChangeCb == nullptr) {
            AUDIO_ERR_LOG("headTrackingEnabledChangeCb : nullptr for client : %{public}d", it->first);
            continue;
        }
        if (clientCallbacksMap_.count(it->first) > 0 &&
            clientCallbacksMap_[it->first].count(CALLBACK_HEAD_TRACKING_ENABLED_CHANGE) > 0 &&
            clientCallbacksMap_[it->first][CALLBACK_HEAD_TRACKING_ENABLED_CHANGE]) {
            headTrackingEnabledChangeCb->OnHeadTrackingEnabledChangeForAnyDevice(eventContextObj->descriptor,
                eventContextObj->headTrackingEnabled);
        }
    }
}

void AudioPolicyServerHandler::HandlePipeStreamCleanEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<int32_t> eventContextObj = event->GetSharedObject<int32_t>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");
    AudioPipeType pipeType = static_cast<AudioPipeType>(*eventContextObj);
    AudioPolicyService::GetAudioPolicyService().DynamicUnloadModule(pipeType);
}

void AudioPolicyServerHandler::HandleConcurrencyEventWithSessionID(const AppExecFwk::InnerEvent::Pointer &event)
{
    std::shared_ptr<EventContextObj> eventContextObj = event->GetSharedObject<EventContextObj>();
    CHECK_AND_RETURN_LOG(eventContextObj != nullptr, "EventContextObj get nullptr");

    std::unique_lock<std::mutex> lock(runnerMutex_);
    std::shared_ptr<IAudioConcurrencyEventDispatcher> dispatcher = concurrencyEventDispatcher_.lock();
    lock.unlock();
    if (dispatcher != nullptr) {
        dispatcher->DispatchConcurrencyEventWithSessionId(eventContextObj->sessionId);
    }
}

void AudioPolicyServerHandler::HandleServiceEvent(const uint32_t &eventId,
    const AppExecFwk::InnerEvent::Pointer &event)
{
    HandleOtherServiceEvent(eventId, event);
    switch (eventId) {
        case EventAudioServerCmd::AUDIO_DEVICE_CHANGE:
            HandleDeviceChangedCallback(event);
            break;
        case EventAudioServerCmd::PREFERRED_OUTPUT_DEVICE_UPDATED:
            HandlePreferredOutputDeviceUpdated();
            break;
        case EventAudioServerCmd::PREFERRED_INPUT_DEVICE_UPDATED:
            HandlePreferredInputDeviceUpdated();
            break;
        case EventAudioServerCmd::AVAILABLE_AUDIO_DEVICE_CHANGE:
            HandleAvailableDeviceChange(event);
            break;
        case EventAudioServerCmd::RENDERER_INFO_EVENT:
            HandleRendererInfoEvent(event);
            break;
        case EventAudioServerCmd::CAPTURER_INFO_EVENT:
            HandleCapturerInfoEvent(event);
            break;
        case EventAudioServerCmd::RENDERER_DEVICE_CHANGE_EVENT:
            HandleRendererDeviceChangeEvent(event);
            break;
        case EventAudioServerCmd::ON_CAPTURER_CREATE:
            HandleCapturerCreateEvent(event);
            break;
        case EventAudioServerCmd::ON_CAPTURER_REMOVED:
            HandleCapturerRemovedEvent(event);
            break;
        case EventAudioServerCmd::ON_WAKEUP_CLOSE:
            HandleWakeupCloseEvent(event);
            break;
        case EventAudioServerCmd::RECREATE_RENDERER_STREAM_EVENT:
            HandleSendRecreateRendererStreamEvent(event);
            break;
        case EventAudioServerCmd::RECREATE_CAPTURER_STREAM_EVENT:
            HandleSendRecreateCapturerStreamEvent(event);
            break;
        case EventAudioServerCmd::DATABASE_UPDATE:
            HandleUpdateKvDataEvent(event);
            break;
        case EventAudioServerCmd::PIPE_STREAM_CLEAN_EVENT:
            HandlePipeStreamCleanEvent(event);
            break;
        default:
            break;
    }
}

void AudioPolicyServerHandler::HandleOtherServiceEvent(const uint32_t &eventId,
    const AppExecFwk::InnerEvent::Pointer &event)
{
    switch (eventId) {
        case EventAudioServerCmd::CONCURRENCY_EVENT_WITH_SESSIONID:
            HandleConcurrencyEventWithSessionID(event);
            break;
        case EventAudioServerCmd::SPATIALIZATION_ENABLED_CHANGE_FOR_ANY_DEVICE:
            HandleSpatializatonEnabledChangeForAnyDeviceEvent(event);
            break;
        case EventAudioServerCmd::HEAD_TRACKING_ENABLED_CHANGE_FOR_ANY_DEVICE:
            HandleHeadTrackingEnabledChangeForAnyDeviceEvent(event);
            break;
        case EventAudioServerCmd::AUDIO_SESSION_DEACTIVE_EVENT:
            HandleAudioSessionDeactiveCallback(event);
            break;
        case EventAudioServerCmd::MICROPHONE_BLOCKED:
            HandleMicrophoneBlockedCallback(event);
            break;
        default:
            break;
    }
}

void AudioPolicyServerHandler::ProcessEvent(const AppExecFwk::InnerEvent::Pointer &event)
{
    uint32_t eventId = event->GetInnerEventId();
    HandleServiceEvent(eventId, event);
    switch (eventId) {
        case EventAudioServerCmd::VOLUME_KEY_EVENT:
            HandleVolumeKeyEvent(event);
            break;
        case EventAudioServerCmd::REQUEST_CATEGORY_EVENT:
            HandleRequestCateGoryEvent(event);
            break;
        case EventAudioServerCmd::ABANDON_CATEGORY_EVENT:
            HandleAbandonCateGoryEvent(event);
            break;
        case EventAudioServerCmd::FOCUS_INFOCHANGE:
            HandleFocusInfoChangeEvent(event);
            break;
        case EventAudioServerCmd::RINGER_MODEUPDATE_EVENT:
            HandleRingerModeUpdatedEvent(event);
            break;
        case EventAudioServerCmd::MIC_STATE_CHANGE_EVENT:
            HandleMicStateUpdatedEvent(event);
            break;
        case EventAudioServerCmd::MIC_STATE_CHANGE_EVENT_WITH_CLIENTID:
            HandleMicStateUpdatedEventWithClientId(event);
            break;
        case EventAudioServerCmd::INTERRUPT_EVENT:
            HandleInterruptEvent(event);
            break;
        case EventAudioServerCmd::INTERRUPT_EVENT_WITH_SESSIONID:
            HandleInterruptEventWithSessionId(event);
            break;
        case EventAudioServerCmd::INTERRUPT_EVENT_WITH_CLIENTID:
            HandleInterruptEventWithClientId(event);
            break;
        case EventAudioServerCmd::DISTRIBUTED_ROUTING_ROLE_CHANGE:
            HandleDistributedRoutingRoleChangeEvent(event);
            break;
        case EventAudioServerCmd::HEAD_TRACKING_DEVICE_CHANGE:
            HandleHeadTrackingDeviceChangeEvent(event);
            break;
        case EventAudioServerCmd::SPATIALIZATION_ENABLED_CHANGE:
            HandleSpatializatonEnabledChangeEvent(event);
            break;
        case EventAudioServerCmd::HEAD_TRACKING_ENABLED_CHANGE:
            HandleHeadTrackingEnabledChangeEvent(event);
            break;
        default:
            break;
    }
}

int32_t AudioPolicyServerHandler::SetClientCallbacksEnable(const CallbackChange &callbackchange, const bool &enable)
{
    if (callbackchange <= CALLBACK_UNKNOWN || callbackchange >= CALLBACK_MAX) {
        AUDIO_ERR_LOG("Illegal parameter");
        return AUDIO_ERR;
    }

    int32_t clientId = IPCSkeleton::GetCallingPid();
    lock_guard<mutex> runnerlock(runnerMutex_);
    clientCallbacksMap_[clientId][callbackchange] = enable;
    string str = (enable ? "true" : "false");
    AUDIO_INFO_LOG("Set clientId:%{public}d, callbacks:%{public}d, enable:%{public}s",
        clientId, callbackchange, str.c_str());
    return AUDIO_OK;
}
} // namespace AudioStandard
} // namespace OHOS
