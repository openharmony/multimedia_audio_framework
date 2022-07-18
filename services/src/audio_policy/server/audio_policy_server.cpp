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

#include <csignal>
#include <memory>
#include <vector>

#include "audio_errors.h"
#include "audio_policy_manager_listener_proxy.h"
#include "audio_ringermode_update_listener_proxy.h"
#include "audio_volume_key_event_callback_proxy.h"
#include "i_standard_audio_policy_manager_listener.h"

#include "input_manager.h"
#include "key_event.h"
#include "key_option.h"

#include "accesstoken_kit.h"
#include "audio_log.h"
#include "ipc_skeleton.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"

#include "audio_service_dump.h"
#include "audio_policy_server.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
constexpr float DUCK_FACTOR = 0.2f; // 20%
REGISTER_SYSTEM_ABILITY_BY_ID(AudioPolicyServer, AUDIO_POLICY_SERVICE_ID, true)

AudioPolicyServer::AudioPolicyServer(int32_t systemAbilityId, bool runOnCreate)
    : SystemAbility(systemAbilityId, runOnCreate),
      mPolicyService(AudioPolicyService::GetAudioPolicyService())
{
    if (mPolicyService.SetAudioSessionCallback(this)) {
        AUDIO_DEBUG_LOG("AudioPolicyServer: SetAudioSessionCallback failed");
    }

    interruptPriorityMap_[STREAM_VOICE_CALL] = THIRD_PRIORITY;
    interruptPriorityMap_[STREAM_RING] = SECOND_PRIORITY;
    interruptPriorityMap_[STREAM_MUSIC] = FIRST_PRIORITY;

    clientOnFocus_ = 0;
    focussedAudioInterruptInfo_ = nullptr;
}

void AudioPolicyServer::OnDump()
{
    return;
}

void AudioPolicyServer::OnStart()
{
    bool res = Publish(this);
    if (res) {
        AUDIO_DEBUG_LOG("AudioPolicyService OnStart res=%d", res);
    }
    AddSystemAbilityListener(DISTRIBUTED_KV_DATA_SERVICE_ABILITY_ID);
    AddSystemAbilityListener(MULTIMODAL_INPUT_SERVICE_ID);
    AddSystemAbilityListener(AUDIO_DISTRIBUTED_SERVICE_ID);

    mPolicyService.Init();
    RegisterAudioServerDeathRecipient();
    return;
}

void AudioPolicyServer::OnStop()
{
    mPolicyService.Deinit();
    return;
}

void AudioPolicyServer::OnAddSystemAbility(int32_t systemAbilityId, const std::string& deviceId)
{
    AUDIO_DEBUG_LOG("AudioPolicyServer::OnAddSystemAbility systemAbilityId:%{public}d", systemAbilityId);
    switch (systemAbilityId) {
        case MULTIMODAL_INPUT_SERVICE_ID:
            AUDIO_DEBUG_LOG("AudioPolicyServer::OnAddSystemAbility SubscribeKeyEvents");
            SubscribeKeyEvents();
            break;
        case DISTRIBUTED_KV_DATA_SERVICE_ABILITY_ID:
            AUDIO_DEBUG_LOG("AudioPolicyServer::OnAddSystemAbility InitKVStore");
            InitKVStore();
            break;
        case AUDIO_DISTRIBUTED_SERVICE_ID:
            AUDIO_DEBUG_LOG("AudioPolicyServer::OnAddSystemAbility ConnectServiceAdapter");
            ConnectServiceAdapter();
            break;
        default:
            AUDIO_DEBUG_LOG("AudioPolicyServer::OnAddSystemAbility unhandled sysabilityId:%{public}d", systemAbilityId);
            break;
    }
}

void AudioPolicyServer::OnRemoveSystemAbility(int32_t systemAbilityId, const std::string& deviceId)
{
    AUDIO_DEBUG_LOG("AudioPolicyServer::OnRemoveSystemAbility systemAbilityId:%{public}d removed", systemAbilityId);
}

void AudioPolicyServer::RegisterAudioServerDeathRecipient()
{
    AUDIO_INFO_LOG("Register audio server death recipient");
    pid_t pid = IPCSkeleton::GetCallingPid();
    sptr<AudioServerDeathRecipient> deathRecipient_ = new(std::nothrow) AudioServerDeathRecipient(pid);
    if (deathRecipient_ != nullptr) {
        auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        CHECK_AND_RETURN_LOG(samgr != nullptr, "Failed to obtain system ability manager");
        sptr<IRemoteObject> object = samgr->GetSystemAbility(OHOS::AUDIO_DISTRIBUTED_SERVICE_ID);
        CHECK_AND_RETURN_LOG(object != nullptr, "Audio service unavailable");

        deathRecipient_->SetNotifyCb(std::bind(&AudioPolicyServer::AudioServerDied, this, std::placeholders::_1));
        bool result = object->AddDeathRecipient(deathRecipient_);
        if (!result) {
            AUDIO_ERR_LOG("failed to add deathRecipient");
        }
    }
}

void AudioPolicyServer::AudioServerDied(pid_t pid)
{
    AUDIO_INFO_LOG("Audio server died: restart policy server pid %{public}d", pid);
    kill(pid, SIGKILL);
}

void AudioPolicyServer::SubscribeKeyEvents()
{
    MMI::InputManager *im = MMI::InputManager::GetInstance();
    CHECK_AND_RETURN_LOG(im != nullptr, "Failed to obtain INPUT manager");

    std::set<int32_t> preKeys;
    std::shared_ptr<OHOS::MMI::KeyOption> keyOption_down = std::make_shared<OHOS::MMI::KeyOption>();
    CHECK_AND_RETURN_LOG(keyOption_down != nullptr, "Invalid key option");
    keyOption_down->SetPreKeys(preKeys);
    keyOption_down->SetFinalKey(OHOS::MMI::KeyEvent::KEYCODE_VOLUME_DOWN);
    keyOption_down->SetFinalKeyDown(true);
    keyOption_down->SetFinalKeyDownDuration(VOLUME_KEY_DURATION);
    im->SubscribeKeyEvent(keyOption_down, [=](std::shared_ptr<MMI::KeyEvent> keyEventCallBack) {
        std::lock_guard<std::mutex> lock(volumeKeyEventMutex_);
        AudioStreamType streamInFocus = GetStreamInFocus();
        if (streamInFocus == AudioStreamType::STREAM_DEFAULT) {
            streamInFocus = AudioStreamType::STREAM_MUSIC;
        }
        float currentVolume = GetStreamVolume(streamInFocus);
        int32_t volumeLevelInInt = ConvertVolumeToInt(currentVolume);
        if (volumeLevelInInt <= MIN_VOLUME_LEVEL) {
            for (auto it = volumeChangeCbsMap_.begin(); it != volumeChangeCbsMap_.end(); ++it) {
                std::shared_ptr<VolumeKeyEventCallback> volumeChangeCb = it->second;
                if (volumeChangeCb == nullptr) {
                    AUDIO_ERR_LOG("volumeChangeCb: nullptr for client : %{public}d", it->first);
                    continue;
                }

                AUDIO_DEBUG_LOG("AudioPolicyServer:: trigger volumeChangeCb clientPid : %{public}d", it->first);
                volumeChangeCb->OnVolumeKeyEvent(streamInFocus, MIN_VOLUME_LEVEL, true);
            }
            return;
        }
        SetStreamVolume(streamInFocus, MapVolumeToHDI(volumeLevelInInt - 1), true);
    });
    std::shared_ptr<OHOS::MMI::KeyOption> keyOption_up = std::make_shared<OHOS::MMI::KeyOption>();
    keyOption_up->SetPreKeys(preKeys);
    keyOption_up->SetFinalKey(OHOS::MMI::KeyEvent::KEYCODE_VOLUME_UP);
    keyOption_up->SetFinalKeyDown(true);
    keyOption_up->SetFinalKeyDownDuration(0);
    im->SubscribeKeyEvent(keyOption_up, [=](std::shared_ptr<MMI::KeyEvent> keyEventCallBack) {
        std::lock_guard<std::mutex> lock(volumeKeyEventMutex_);
        AudioStreamType streamInFocus = GetStreamInFocus();
        if (streamInFocus == AudioStreamType::STREAM_DEFAULT) {
            streamInFocus = AudioStreamType::STREAM_MUSIC;
        }
        float currentVolume = GetStreamVolume(streamInFocus);
        int32_t volumeLevelInInt = ConvertVolumeToInt(currentVolume);
        if (volumeLevelInInt >= MAX_VOLUME_LEVEL) {
            for (auto it = volumeChangeCbsMap_.begin(); it != volumeChangeCbsMap_.end(); ++it) {
                std::shared_ptr<VolumeKeyEventCallback> volumeChangeCb = it->second;
                if (volumeChangeCb == nullptr) {
                    AUDIO_ERR_LOG("volumeChangeCb: nullptr for client : %{public}d", it->first);
                    continue;
                }

                AUDIO_DEBUG_LOG("AudioPolicyServer:: trigger volumeChangeCb clientPid : %{public}d", it->first);
                volumeChangeCb->OnVolumeKeyEvent(streamInFocus, MAX_VOLUME_LEVEL, true);
            }
            return;
        }
        SetStreamVolume(streamInFocus, MapVolumeToHDI(volumeLevelInInt + 1), true);
    });
}

void AudioPolicyServer::InitKVStore()
{
    mPolicyService.InitKVStore();
}

void AudioPolicyServer::ConnectServiceAdapter()
{
    if (!mPolicyService.ConnectServiceAdapter()) {
        AUDIO_ERR_LOG("AudioPolicyServer::ConnectServiceAdapter Error in connecting to audio service adapter");
        return;
    }
}

int32_t AudioPolicyServer::SetStreamVolume(AudioStreamType streamType, float volume)
{
    return SetStreamVolume(streamType, volume, false);
}

float AudioPolicyServer::GetStreamVolume(AudioStreamType streamType)
{
    if (GetStreamMute(streamType)) {
        return MIN_VOLUME_LEVEL;
    }
    return mPolicyService.GetStreamVolume(streamType);
}

int32_t AudioPolicyServer::SetStreamMute(AudioStreamType streamType, bool mute)
{
    if (streamType == AudioStreamType::STREAM_RING) {
        if (!VerifyClientPermission(ACCESS_NOTIFICATION_POLICY_PERMISSION)) {
            AUDIO_ERR_LOG("SetStreamMute permission denied for stream type : %{public}d", streamType);
            return ERR_PERMISSION_DENIED;
        }
    }

    int result = mPolicyService.SetStreamMute(streamType, mute);
    for (auto it = volumeChangeCbsMap_.begin(); it != volumeChangeCbsMap_.end(); ++it) {
        std::shared_ptr<VolumeKeyEventCallback> volumeChangeCb = it->second;
        if (volumeChangeCb == nullptr) {
            AUDIO_ERR_LOG("volumeChangeCb: nullptr for client : %{public}d", it->first);
            continue;
        }
        AUDIO_DEBUG_LOG("AudioPolicyServer::SetStreamMute trigger volumeChangeCb clientPid : %{public}d", it->first);
        volumeChangeCb->OnVolumeKeyEvent(streamType, ConvertVolumeToInt(GetStreamVolume(streamType)), false);
    }

    return result;
}

int32_t AudioPolicyServer::SetStreamVolume(AudioStreamType streamType, float volume, bool isUpdateUi)
{
    if (streamType == AudioStreamType::STREAM_RING && !isUpdateUi) {
        float currentRingVolume = GetStreamVolume(AudioStreamType::STREAM_RING);
        if ((currentRingVolume > 0.0f && volume == 0.0f) || (currentRingVolume == 0.0f && volume > 0.0f)) {
            if (!VerifyClientPermission(ACCESS_NOTIFICATION_POLICY_PERMISSION)) {
                AUDIO_ERR_LOG("Access policy permission denied for volume type : %{public}d", streamType);
                return ERR_PERMISSION_DENIED;
            }
        }
    }

    int ret = mPolicyService.SetStreamVolume(streamType, volume);
    for (auto it = volumeChangeCbsMap_.begin(); it != volumeChangeCbsMap_.end(); ++it) {
        std::shared_ptr<VolumeKeyEventCallback> volumeChangeCb = it->second;
        if (volumeChangeCb == nullptr) {
            AUDIO_ERR_LOG("volumeChangeCb: nullptr for client : %{public}d", it->first);
            continue;
        }

        AUDIO_DEBUG_LOG("AudioPolicyServer::SetStreamVolume trigger volumeChangeCb clientPid : %{public}d", it->first);
        volumeChangeCb->OnVolumeKeyEvent(streamType, ConvertVolumeToInt(GetStreamVolume(streamType)), isUpdateUi);
    }

    return ret;
}

bool AudioPolicyServer::GetStreamMute(AudioStreamType streamType)
{
    if (streamType == AudioStreamType::STREAM_RING) {
        if (!VerifyClientPermission(ACCESS_NOTIFICATION_POLICY_PERMISSION)) {
            AUDIO_ERR_LOG("GetStreamMute permission denied for stream type : %{public}d", streamType);
            return false;
        }
    }

    return mPolicyService.GetStreamMute(streamType);
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyServer::GetDevices(DeviceFlag deviceFlag)
{
    std::vector<sptr<AudioDeviceDescriptor>> deviceDescs = mPolicyService.GetDevices(deviceFlag);
    bool hasBTPermission = VerifyClientPermission(USE_BLUETOOTH_PERMISSION);
    if (!hasBTPermission) {
        for (sptr<AudioDeviceDescriptor> desc : deviceDescs) {
            if ((desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP)
                || (desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_SCO)) {
                desc->deviceName_ = "";
                desc->macAddress_ = "";
            }
        }
    }

    return deviceDescs;
}

bool AudioPolicyServer::IsStreamActive(AudioStreamType streamType)
{
    return mPolicyService.IsStreamActive(streamType);
}

int32_t AudioPolicyServer::SetDeviceActive(InternalDeviceType deviceType, bool active)
{
    return mPolicyService.SetDeviceActive(deviceType, active);
}

bool AudioPolicyServer::IsDeviceActive(InternalDeviceType deviceType)
{
    return mPolicyService.IsDeviceActive(deviceType);
}

InternalDeviceType AudioPolicyServer::GetActiveOutputDevice()
{
    return mPolicyService.GetActiveOutputDevice();
}

InternalDeviceType AudioPolicyServer::GetActiveInputDevice()
{
    return mPolicyService.GetActiveInputDevice();
}

int32_t AudioPolicyServer::SetRingerMode(AudioRingerMode ringMode)
{
    bool isPermissionRequired = false;

    if (ringMode == AudioRingerMode::RINGER_MODE_SILENT) {
        isPermissionRequired = true;
    } else {
        AudioRingerMode currentRingerMode = GetRingerMode();
        if (currentRingerMode == AudioRingerMode::RINGER_MODE_SILENT) {
            isPermissionRequired = true;
        }
    }

    if (isPermissionRequired) {
        if (!VerifyClientPermission(ACCESS_NOTIFICATION_POLICY_PERMISSION)) {
            AUDIO_ERR_LOG("Access policy permission denied for ringerMode : %{public}d", ringMode);
            return ERR_PERMISSION_DENIED;
        }
    }

    int32_t ret = mPolicyService.SetRingerMode(ringMode);
    if (ret == SUCCESS) {
        for (auto it = ringerModeListenerCbsMap_.begin(); it != ringerModeListenerCbsMap_.end(); ++it) {
            std::shared_ptr<AudioRingerModeCallback> ringerModeListenerCb = it->second;
            if (ringerModeListenerCb == nullptr) {
                AUDIO_ERR_LOG("ringerModeListenerCbsMap_: nullptr for client : %{public}d", it->first);
                continue;
            }

            AUDIO_DEBUG_LOG("ringerModeListenerCbsMap_ :client =  %{public}d", it->first);
            ringerModeListenerCb->OnRingerModeUpdated(ringMode);
        }
    }

    return ret;
}

AudioRingerMode AudioPolicyServer::GetRingerMode()
{
    return mPolicyService.GetRingerMode();
}

int32_t AudioPolicyServer::SetAudioScene(AudioScene audioScene)
{
    return mPolicyService.SetAudioScene(audioScene);
}

AudioScene AudioPolicyServer::GetAudioScene()
{
    return mPolicyService.GetAudioScene();
}

int32_t AudioPolicyServer::SetRingerModeCallback(const int32_t clientId, const sptr<IRemoteObject> &object)
{
    std::lock_guard<std::mutex> lock(ringerModeMutex_);

    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer:set listener object is nullptr");

    sptr<IStandardRingerModeUpdateListener> listener = iface_cast<IStandardRingerModeUpdateListener>(object);
    CHECK_AND_RETURN_RET_LOG(listener != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer: listener obj cast failed");

    std::shared_ptr<AudioRingerModeCallback> callback = std::make_shared<AudioRingerModeListenerCallback>(listener);
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer: failed to  create cb obj");

    ringerModeListenerCbsMap_[clientId] = callback;

    return SUCCESS;
}

int32_t AudioPolicyServer::UnsetRingerModeCallback(const int32_t clientId)
{
    std::lock_guard<std::mutex> lock(ringerModeMutex_);

    if (ringerModeListenerCbsMap_.find(clientId) != ringerModeListenerCbsMap_.end()) {
        ringerModeListenerCbsMap_.erase(clientId);
        AUDIO_ERR_LOG("AudioPolicyServer: UnsetRingerModeCallback for client %{public}d done", clientId);
        return SUCCESS;
    } else {
        AUDIO_ERR_LOG("AudioPolicyServer: Cb does not exit for client %{public}d cannot unregister", clientId);
        return ERR_INVALID_OPERATION;
    }
}

int32_t AudioPolicyServer::SetDeviceChangeCallback(const int32_t clientId, const sptr<IRemoteObject> &object)
{
    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer:set listener object is nullptr");

    return mPolicyService.SetDeviceChangeCallback(clientId, object);
}

int32_t AudioPolicyServer::UnsetDeviceChangeCallback(const int32_t clientId)
{
    return mPolicyService.UnsetDeviceChangeCallback(clientId);
}

int32_t AudioPolicyServer::SetAudioInterruptCallback(const uint32_t sessionID, const sptr<IRemoteObject> &object)
{
    std::lock_guard<std::mutex> lock(interruptMutex_);

    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer:set listener object is nullptr");

    sptr<IStandardAudioPolicyManagerListener> listener = iface_cast<IStandardAudioPolicyManagerListener>(object);
    CHECK_AND_RETURN_RET_LOG(listener != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer: listener obj cast failed");

    std::shared_ptr<AudioInterruptCallback> callback = std::make_shared<AudioPolicyManagerListenerCallback>(listener);
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer: failed to  create cb obj");

    policyListenerCbsMap_[sessionID] = callback;
    AUDIO_DEBUG_LOG("AudioPolicyServer: SetAudioInterruptCallback for sessionID %{public}d done", sessionID);

    return SUCCESS;
}

int32_t AudioPolicyServer::UnsetAudioInterruptCallback(const uint32_t sessionID)
{
    std::lock_guard<std::mutex> lock(interruptMutex_);

    if (policyListenerCbsMap_.erase(sessionID)) {
        AUDIO_DEBUG_LOG("AudioPolicyServer:UnsetAudioInterruptCallback for sessionID %{public}d done", sessionID);
    } else {
        AUDIO_DEBUG_LOG("AudioPolicyServer:UnsetAudioInterruptCallback sessionID %{public}d not present/unset already",
                        sessionID);
    }

    return SUCCESS;
}

int32_t AudioPolicyServer::SetAudioManagerInterruptCallback(const uint32_t clientID,
                                                            const sptr<IRemoteObject> &object)
{
    std::lock_guard<std::mutex> lock(interruptMutex_);

    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer:set listener object is nullptr");

    sptr<IStandardAudioPolicyManagerListener> listener = iface_cast<IStandardAudioPolicyManagerListener>(object);
    CHECK_AND_RETURN_RET_LOG(listener != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer: listener obj cast failed");

    std::shared_ptr<AudioInterruptCallback> callback = std::make_shared<AudioPolicyManagerListenerCallback>(listener);
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer: failed to  create cb obj");

    audioManagerListenerCbsMap_[clientID] = callback;
    AUDIO_INFO_LOG("AudioPolicyServer: SetAudioManagerInterruptCallback for client id %{public}d done", clientID);

    return SUCCESS;
}

int32_t AudioPolicyServer::UnsetAudioManagerInterruptCallback(const uint32_t clientID)
{
    if (audioManagerListenerCbsMap_.erase(clientID)) {
        AUDIO_INFO_LOG("AudioPolicyServer:UnsetAudioManagerInterruptCallback for client %{public}d done", clientID);
    } else {
        AUDIO_DEBUG_LOG("AudioPolicyServer:UnsetAudioManagerInterruptCallback client %{public}d not present",
                        clientID);
    }

    return SUCCESS;
}

int32_t AudioPolicyServer::RequestAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt)
{
    AUDIO_DEBUG_LOG("AudioPolicyServer: RequestAudioFocus in");
    if (clientOnFocus_ == clientID) {
        AUDIO_DEBUG_LOG("AudioPolicyServer: client already has focus");
        NotifyFocusGranted(clientID, audioInterrupt);
        return SUCCESS;
    }

    if (focussedAudioInterruptInfo_ != nullptr) {
        AUDIO_DEBUG_LOG("AudioPolicyServer: Existing stream: %{public}d, incoming stream: %{public}d",
                        focussedAudioInterruptInfo_->streamType, audioInterrupt.streamType);
        NotifyFocusAbandoned(clientOnFocus_, *focussedAudioInterruptInfo_);
        AbandonAudioFocus(clientOnFocus_, *focussedAudioInterruptInfo_);
    }

    AUDIO_DEBUG_LOG("AudioPolicyServer: Grant audio focus");
    NotifyFocusGranted(clientID, audioInterrupt);

    return SUCCESS;
}

int32_t AudioPolicyServer::AbandonAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt)
{
    AUDIO_INFO_LOG("AudioPolicyServer: AbandonAudioFocus in");

    if (clientID == clientOnFocus_) {
        AUDIO_DEBUG_LOG("AudioPolicyServer: remove app focus");
        focussedAudioInterruptInfo_.reset();
        focussedAudioInterruptInfo_ = nullptr;
        clientOnFocus_ = 0;
    }

    return SUCCESS;
}

void AudioPolicyServer::NotifyFocusGranted(const uint32_t clientID, const AudioInterrupt &audioInterrupt)
{
    AUDIO_INFO_LOG("AudioPolicyServer: Notify focus granted in: %{public}d", clientID);
    std::shared_ptr<AudioInterruptCallback> interruptCb = nullptr;
    interruptCb = audioManagerListenerCbsMap_[clientID];
    if (interruptCb) {
        InterruptEventInternal interruptEvent = {};
        interruptEvent.eventType = INTERRUPT_TYPE_END;
        interruptEvent.forceType = INTERRUPT_SHARE;
        interruptEvent.hintType = INTERRUPT_HINT_NONE;
        interruptEvent.duckVolume = 0;

        AUDIO_DEBUG_LOG("AudioPolicyServer: callback focus granted");
        interruptCb->OnInterrupt(interruptEvent);

        unique_ptr<AudioInterrupt> tempAudioInterruptInfo = make_unique<AudioInterrupt>();
        tempAudioInterruptInfo->streamUsage = audioInterrupt.streamUsage;
        tempAudioInterruptInfo->contentType = audioInterrupt.contentType;
        tempAudioInterruptInfo->streamType = audioInterrupt.streamType;
        tempAudioInterruptInfo->pauseWhenDucked = audioInterrupt.pauseWhenDucked;
        focussedAudioInterruptInfo_ = move(tempAudioInterruptInfo);
        clientOnFocus_ = clientID;
    }
}

int32_t AudioPolicyServer::NotifyFocusAbandoned(const uint32_t clientID, const AudioInterrupt &audioInterrupt)
{
    AUDIO_INFO_LOG("AudioPolicyServer: Notify focus abandoned in: %{public}d", clientID);
    std::shared_ptr<AudioInterruptCallback> interruptCb = nullptr;
    interruptCb = audioManagerListenerCbsMap_[clientID];
    if (!interruptCb) {
        AUDIO_INFO_LOG("AudioPolicyServer: Notify failed, callback not present");
        return ERR_INVALID_PARAM;
    }

    InterruptEventInternal interruptEvent = {};
    interruptEvent.eventType = INTERRUPT_TYPE_BEGIN;
    interruptEvent.forceType = INTERRUPT_SHARE;
    interruptEvent.hintType = INTERRUPT_HINT_STOP;
    interruptEvent.duckVolume = 0;
    AUDIO_DEBUG_LOG("AudioPolicyServer: callback focus abandoned");
    interruptCb->OnInterrupt(interruptEvent);

    return SUCCESS;
}

void AudioPolicyServer::PrintOwnersLists()
{
    AUDIO_DEBUG_LOG("AudioPolicyServer: Printing active list");
    for (auto it = curActiveOwnersList_.begin(); it != curActiveOwnersList_.end(); ++it) {
        AUDIO_DEBUG_LOG("AudioPolicyServer: curActiveOwnersList_: streamType: %{public}d", it->streamType);
        AUDIO_DEBUG_LOG("AudioPolicyServer: curActiveOwnersList_: sessionID: %{public}u", it->sessionID);
    }

    AUDIO_DEBUG_LOG("AudioPolicyServer: Printing pending list");
    for (auto it = pendingOwnersList_.begin(); it != pendingOwnersList_.end(); ++it) {
        AUDIO_DEBUG_LOG("AudioPolicyServer: pendingOwnersList_: streamType: %{public}d", it->streamType);
        AUDIO_DEBUG_LOG("AudioPolicyServer: pendingOwnersList_: sessionID: %{public}u", it->sessionID);
    }
}

bool AudioPolicyServer::ProcessPendingInterrupt(std::list<AudioInterrupt>::iterator &iterPending,
                                                const AudioInterrupt &incoming)
{
    bool iterPendingErased = false;
    AudioStreamType pendingStreamType = iterPending->streamType;
    AudioStreamType incomingStreamType = incoming.streamType;

    auto focusTable = mPolicyService.GetAudioFocusTable();
    AudioFocusEntry focusEntry = focusTable[pendingStreamType][incomingStreamType];
    float duckVol = 0.2f;
    InterruptEventInternal interruptEvent {INTERRUPT_TYPE_BEGIN, focusEntry.forceType, focusEntry.hintType, duckVol};

    uint32_t pendingSessionID = iterPending->sessionID;
    std::shared_ptr<AudioInterruptCallback> policyListenerCb = nullptr;

    if (focusEntry.actionOn == CURRENT && focusEntry.forceType == INTERRUPT_FORCE) {
        policyListenerCb = policyListenerCbsMap_[pendingSessionID];

        if (focusEntry.hintType == INTERRUPT_HINT_STOP) {
            iterPending = pendingOwnersList_.erase(iterPending);
            iterPendingErased = true;
        }

        if (policyListenerCb == nullptr) {
            AUDIO_WARNING_LOG("AudioPolicyServer: policyListenerCb is null so ignoring to apply focus policy");
            return iterPendingErased;
        }
        policyListenerCb->OnInterrupt(interruptEvent);
    }

    return iterPendingErased;
}

bool AudioPolicyServer::ProcessCurActiveInterrupt(std::list<AudioInterrupt>::iterator &iterActive,
                                                  const AudioInterrupt &incoming)
{
    bool iterActiveErased = false;
    AudioStreamType activeStreamType = iterActive->streamType;
    AudioStreamType incomingStreamType = incoming.streamType;

    auto focusTable = mPolicyService.GetAudioFocusTable();
    AudioFocusEntry focusEntry = focusTable[activeStreamType][incomingStreamType];
    InterruptEventInternal interruptEvent {INTERRUPT_TYPE_BEGIN, focusEntry.forceType, focusEntry.hintType, 0.2f};

    uint32_t activeSessionID = iterActive->sessionID;
    uint32_t incomingSessionID = incoming.sessionID;
    std::shared_ptr<AudioInterruptCallback> policyListenerCb = nullptr;
    if (focusEntry.actionOn == CURRENT) {
        policyListenerCb = policyListenerCbsMap_[activeSessionID];
    } else {
        policyListenerCb = policyListenerCbsMap_[incomingSessionID];
    }

    // focusEntry.forceType == INTERRUPT_SHARE
    if (focusEntry.forceType != INTERRUPT_FORCE) {
        if (policyListenerCb == nullptr) {
            AUDIO_WARNING_LOG("AudioPolicyServer: policyListenerCb is null so ignoring to apply focus policy");
            return iterActiveErased;
        }
        policyListenerCb->OnInterrupt(interruptEvent);
        return iterActiveErased;
    }

    // focusEntry.forceType == INTERRUPT_FORCE
    AUDIO_INFO_LOG("AudioPolicyServer: Action is taken on: %{public}d", focusEntry.actionOn);
    float volume = 0.0f;
    if (focusEntry.actionOn == CURRENT) {
        switch (focusEntry.hintType) {
            case INTERRUPT_HINT_STOP:
                iterActive = curActiveOwnersList_.erase(iterActive);
                iterActiveErased = true;
                break;
            case INTERRUPT_HINT_PAUSE:
                pendingOwnersList_.emplace_front(*iterActive);
                iterActive = curActiveOwnersList_.erase(iterActive);
                iterActiveErased = true;
                break;
            case INTERRUPT_HINT_DUCK:
                volume = GetStreamVolume(incomingStreamType);
                interruptEvent.duckVolume = DUCK_FACTOR * volume;
                break;
            default:
                break;
        }
    } else { // INCOMING
        if (focusEntry.hintType == INTERRUPT_HINT_DUCK) {
            AUDIO_INFO_LOG("AudioPolicyServer: force duck get GetStreamVolume(activeStreamType)");
            volume = GetStreamVolume(activeStreamType);
            interruptEvent.duckVolume = DUCK_FACTOR * volume;
        }
    }

    if (policyListenerCb == nullptr) {
        AUDIO_WARNING_LOG("AudioPolicyServer: policyListenerCb is null so ignoring to apply focus policy");
        return iterActiveErased;
    }
    policyListenerCb->OnInterrupt(interruptEvent);

    return iterActiveErased;
}

int32_t AudioPolicyServer::ProcessFocusEntry(const AudioInterrupt &incomingInterrupt)
{
    // Function: First Process pendingList and remove session that loses focus indefinitely
    for (auto iterPending = pendingOwnersList_.begin(); iterPending != pendingOwnersList_.end();) {
        bool IsIterPendingErased = ProcessPendingInterrupt(iterPending, incomingInterrupt);
        if (!IsIterPendingErased) {
            AUDIO_INFO_LOG("AudioPolicyServer: iterPending not erased while processing ++increment it");
            ++iterPending;
        }
    }

    auto focusTable = mPolicyService.GetAudioFocusTable();
    // Function: Process Focus entry
    for (auto iterActive = curActiveOwnersList_.begin(); iterActive != curActiveOwnersList_.end();) {
        AudioStreamType activeStreamType = iterActive->streamType;
        AudioStreamType incomingStreamType = incomingInterrupt.streamType;
        AudioFocusEntry focusEntry = focusTable[activeStreamType][incomingStreamType];
        if (focusEntry.isReject) {
            AUDIO_INFO_LOG("AudioPolicyServer: focusEntry.isReject : ActivateAudioInterrupt request rejected");
            return ERR_FOCUS_DENIED;
        }
        bool IsIterActiveErased = ProcessCurActiveInterrupt(iterActive, incomingInterrupt);
        if (!IsIterActiveErased) {
            AUDIO_INFO_LOG("AudioPolicyServer: iterActive not erased while processing ++increment it");
            ++iterActive;
        }
    }

    return SUCCESS;
}

void AudioPolicyServer::AddToCurActiveList(const AudioInterrupt &audioInterrupt)
{
    if (curActiveOwnersList_.empty()) {
        curActiveOwnersList_.emplace_front(audioInterrupt);
        return;
    }

    auto itCurActive = curActiveOwnersList_.begin();

    for (; itCurActive != curActiveOwnersList_.end(); ++itCurActive) {
        AudioStreamType existingPriorityStreamType = itCurActive->streamType;
        if (interruptPriorityMap_[existingPriorityStreamType] > interruptPriorityMap_[audioInterrupt.streamType]) {
            continue;
        } else {
            curActiveOwnersList_.emplace(itCurActive, audioInterrupt);
            return;
        }
    }

    if (itCurActive == curActiveOwnersList_.end()) {
        curActiveOwnersList_.emplace_back(audioInterrupt);
    }
}

int32_t AudioPolicyServer::ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt)
{
    std::lock_guard<std::mutex> lock(interruptMutex_);

    AUDIO_DEBUG_LOG("AudioPolicyServer: ActivateAudioInterrupt");
    AUDIO_DEBUG_LOG("AudioPolicyServer: audioInterrupt.streamType: %{public}d", audioInterrupt.streamType);
    AUDIO_DEBUG_LOG("AudioPolicyServer: audioInterrupt.sessionID: %{public}u", audioInterrupt.sessionID);

    if (!mPolicyService.IsAudioInterruptEnabled()) {
        AUDIO_DEBUG_LOG("AudioPolicyServer: interrupt is not enabled. No need to ActivateAudioInterrupt");
        AddToCurActiveList(audioInterrupt);
        return SUCCESS;
    }

    AUDIO_DEBUG_LOG("AudioPolicyServer: ActivateAudioInterrupt start: print active and pending lists");
    PrintOwnersLists();

    // Check if the session is present in pending list, remove and treat it as a new request
    uint32_t incomingSessionID = audioInterrupt.sessionID;
    if (!pendingOwnersList_.empty()) {
        AUDIO_DEBUG_LOG("If it is present in pending list, remove and treat is as new request");
        pendingOwnersList_.remove_if([&incomingSessionID](AudioInterrupt &interrupt) {
            return interrupt.sessionID == incomingSessionID;
        });
    }

    // If active owners list is empty, directly activate interrupt
    if (curActiveOwnersList_.empty()) {
        curActiveOwnersList_.emplace_front(audioInterrupt);
        AUDIO_DEBUG_LOG("AudioPolicyServer: ActivateAudioInterrupt end: print active and pending lists");
        PrintOwnersLists();
        return SUCCESS;
    }

    // If the session is already in active list, return
    for (auto it = curActiveOwnersList_.begin(); it != curActiveOwnersList_.end(); ++it) {
        if (it->sessionID == audioInterrupt.sessionID) {
            AUDIO_DEBUG_LOG("AudioPolicyServer: sessionID %{public}d is already active", audioInterrupt.sessionID);
            AUDIO_DEBUG_LOG("AudioPolicyServer: ActivateAudioInterrupt end: print active and pending lists");
            PrintOwnersLists();
            return SUCCESS;
        }
    }

    // Process ProcessFocusEntryTable for current active and pending lists
    int32_t ret = ProcessFocusEntry(audioInterrupt);
    if (ret) {
        AUDIO_ERR_LOG("AudioPolicyServer:  ActivateAudioInterrupt request rejected");
        AUDIO_DEBUG_LOG("AudioPolicyServer: ActivateAudioInterrupt end: print active and pending lists");
        PrintOwnersLists();
        return ERR_FOCUS_DENIED;
    }

    // Activate request processed and accepted. Add the entry to active list
    AddToCurActiveList(audioInterrupt);

    AUDIO_DEBUG_LOG("AudioPolicyServer: ActivateAudioInterrupt end: print active and pending lists");
    PrintOwnersLists();
    return SUCCESS;
}

void AudioPolicyServer::UnduckCurActiveList(const AudioInterrupt &exitInterrupt)
{
    std::shared_ptr<AudioInterruptCallback> policyListenerCb = nullptr;
    AudioStreamType exitStreamType = exitInterrupt.streamType;
    InterruptEventInternal forcedUnducking {INTERRUPT_TYPE_END, INTERRUPT_FORCE, INTERRUPT_HINT_UNDUCK, 0.2f};

    for (auto it = curActiveOwnersList_.begin(); it != curActiveOwnersList_.end(); ++it) {
        AudioStreamType activeStreamType = it->streamType;
        uint32_t activeSessionID = it->sessionID;
        if (interruptPriorityMap_[activeStreamType] > interruptPriorityMap_[exitStreamType]) {
                continue;
        }
        policyListenerCb = policyListenerCbsMap_[activeSessionID];
        if (policyListenerCb == nullptr) {
            AUDIO_WARNING_LOG("AudioPolicyServer: Cb sessionID: %{public}d null. ignoring to Unduck", activeSessionID);
            return;
        }
        policyListenerCb->OnInterrupt(forcedUnducking);
    }
}

void AudioPolicyServer::ResumeUnduckPendingList(const AudioInterrupt &exitInterrupt)
{
    std::shared_ptr<AudioInterruptCallback> policyListenerCb = nullptr;
    AudioStreamType exitStreamType = exitInterrupt.streamType;
    InterruptEventInternal forcedUnducking {INTERRUPT_TYPE_END, INTERRUPT_FORCE, INTERRUPT_HINT_UNDUCK, 0.2f};
    InterruptEventInternal resumeForcePaused {INTERRUPT_TYPE_END, INTERRUPT_FORCE, INTERRUPT_HINT_RESUME, 0.2f};

    for (auto it = pendingOwnersList_.begin(); it != pendingOwnersList_.end();) {
        AudioStreamType pendingStreamType = it->streamType;
        uint32_t pendingSessionID = it->sessionID;
        if (interruptPriorityMap_[pendingStreamType] > interruptPriorityMap_[exitStreamType]) {
            ++it;
            continue;
        }
        it = pendingOwnersList_.erase(it);
        policyListenerCb = policyListenerCbsMap_[pendingSessionID];
        if (policyListenerCb == nullptr) {
            AUDIO_WARNING_LOG("AudioPolicyServer: Cb sessionID: %{public}d null. ignoring resume", pendingSessionID);
            return;
        }
        policyListenerCb->OnInterrupt(forcedUnducking);
        policyListenerCb->OnInterrupt(resumeForcePaused);
    }
}

int32_t AudioPolicyServer::DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt)
{
    std::lock_guard<std::mutex> lock(interruptMutex_);

    if (!mPolicyService.IsAudioInterruptEnabled()) {
        AUDIO_DEBUG_LOG("AudioPolicyServer: interrupt is not enabled. No need to DeactivateAudioInterrupt");
        uint32_t exitSessionID = audioInterrupt.sessionID;
        curActiveOwnersList_.remove_if([&exitSessionID](AudioInterrupt &interrupt) {
            return interrupt.sessionID == exitSessionID;
        });
        return SUCCESS;
    }

    AUDIO_DEBUG_LOG("AudioPolicyServer: DeactivateAudioInterrupt");
    AUDIO_DEBUG_LOG("AudioPolicyServer: audioInterrupt.streamType: %{public}d", audioInterrupt.streamType);
    AUDIO_DEBUG_LOG("AudioPolicyServer: audioInterrupt.sessionID: %{public}u", audioInterrupt.sessionID);

    AUDIO_DEBUG_LOG("AudioPolicyServer: DeactivateAudioInterrupt start: print active and pending lists");
    PrintOwnersLists();

    // Check and remove, its entry from pending first
    uint32_t exitSessionID = audioInterrupt.sessionID;
    pendingOwnersList_.remove_if([&exitSessionID](AudioInterrupt &interrupt) {
        return interrupt.sessionID == exitSessionID;
    });

    bool isInterruptActive = false;
    InterruptEventInternal forcedUnducking {INTERRUPT_TYPE_END, INTERRUPT_FORCE, INTERRUPT_HINT_UNDUCK, 0.2f};
    std::shared_ptr<AudioInterruptCallback> policyListenerCb = nullptr;
    for (auto it = curActiveOwnersList_.begin(); it != curActiveOwnersList_.end();) {
        if (it->sessionID == audioInterrupt.sessionID) {
            policyListenerCb = policyListenerCbsMap_[it->sessionID];
            if (policyListenerCb != nullptr) {
                policyListenerCb->OnInterrupt(forcedUnducking); // Unducks self, if ducked before
            }
            it = curActiveOwnersList_.erase(it);
            isInterruptActive = true;
        } else {
            ++it;
        }
    }

    // If it was not present in both the lists or present only in pending list,
    // No need to take any action on other sessions, just return.
    if (!isInterruptActive) {
        AUDIO_DEBUG_LOG("AudioPolicyServer: Session : %{public}d is not currently active. return success",
                        audioInterrupt.sessionID);
        AUDIO_DEBUG_LOG("AudioPolicyServer: DeactivateAudioInterrupt start: print active and pending lists");
        PrintOwnersLists();
        return SUCCESS;
    }

    if (curActiveOwnersList_.empty() && pendingOwnersList_.empty()) {
        AUDIO_DEBUG_LOG("AudioPolicyServer: No other session active or pending. Deactivate complete, return success");
        AUDIO_DEBUG_LOG("AudioPolicyServer: DeactivateAudioInterrupt start: print active and pending lists");
        PrintOwnersLists();
        return SUCCESS;
    }

    // unduck if the session was forced ducked
    UnduckCurActiveList(audioInterrupt);

    // resume and unduck if the session was forced paused
    ResumeUnduckPendingList(audioInterrupt);

    AUDIO_DEBUG_LOG("AudioPolicyServer: DeactivateAudioInterrupt end: print active and pending lists");
    PrintOwnersLists();
    return SUCCESS;
}

void AudioPolicyServer::OnSessionRemoved(const uint32_t sessionID)
{
    uint32_t removedSessionID = sessionID;

    auto isSessionPresent = [&removedSessionID] (const AudioInterrupt &interrupt) {
        return interrupt.sessionID == removedSessionID;
    };

    AUDIO_DEBUG_LOG("AudioPolicyServer::OnSessionRemoved");
    std::unique_lock<std::mutex> lock(interruptMutex_);

    auto iterActive = std::find_if(curActiveOwnersList_.begin(), curActiveOwnersList_.end(), isSessionPresent);
    if (iterActive != curActiveOwnersList_.end()) {
        AudioInterrupt removedInterrupt = *iterActive;
        lock.unlock();
        AUDIO_DEBUG_LOG("Removed SessionID: %{public}u is present in active list", removedSessionID);

        (void)DeactivateAudioInterrupt(removedInterrupt);
        (void)UnsetAudioInterruptCallback(removedSessionID);
        return;
    }
    AUDIO_DEBUG_LOG("Removed SessionID: %{public}u is not present in active list", removedSessionID);

    auto iterPending = std::find_if(pendingOwnersList_.begin(), pendingOwnersList_.end(), isSessionPresent);
    if (iterPending != pendingOwnersList_.end()) {
        AudioInterrupt removedInterrupt = *iterPending;
        lock.unlock();
        AUDIO_DEBUG_LOG("Removed SessionID: %{public}u is present in pending list", removedSessionID);

        (void)DeactivateAudioInterrupt(removedInterrupt);
        (void)UnsetAudioInterruptCallback(removedSessionID);
        return;
    }
    AUDIO_DEBUG_LOG("Removed SessionID: %{public}u not present in pending list either", removedSessionID);

    // Though it is not present in the owners list, check and clear its entry from callback map
    lock.unlock();
    (void)UnsetAudioInterruptCallback(removedSessionID);
}

AudioStreamType AudioPolicyServer::GetStreamInFocus()
{
    AudioStreamType streamInFocus = STREAM_DEFAULT;
    if (!curActiveOwnersList_.empty()) {
        streamInFocus = curActiveOwnersList_.front().streamType;
    }

    return streamInFocus;
}

int32_t AudioPolicyServer::GetSessionInfoInFocus(AudioInterrupt &audioInterrupt)
{
    uint32_t invalidSessionID = static_cast<uint32_t>(-1);
    audioInterrupt = {STREAM_USAGE_UNKNOWN, CONTENT_TYPE_UNKNOWN, STREAM_DEFAULT, invalidSessionID};

    if (!curActiveOwnersList_.empty()) {
        audioInterrupt = curActiveOwnersList_.front();
    }

    return SUCCESS;
}

int32_t AudioPolicyServer::SetVolumeKeyEventCallback(const int32_t clientPid, const sptr<IRemoteObject> &object)
{
    std::lock_guard<std::mutex> lock(volumeKeyEventMutex_);
    AUDIO_DEBUG_LOG("AudioPolicyServer::SetVolumeKeyEventCallback");
    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_INVALID_PARAM,
                             "AudioPolicyServer::SetVolumeKeyEventCallback listener object is nullptr");

    sptr<IAudioVolumeKeyEventCallback> listener = iface_cast<IAudioVolumeKeyEventCallback>(object);
    CHECK_AND_RETURN_RET_LOG(listener != nullptr, ERR_INVALID_PARAM,
                             "AudioPolicyServer::SetVolumeKeyEventCallback listener obj cast failed");

    std::shared_ptr<VolumeKeyEventCallback> callback = std::make_shared<VolumeKeyEventCallbackListner>(listener);
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM,
                             "AudioPolicyServer::SetVolumeKeyEventCallback failed to create cb obj");

    volumeChangeCbsMap_[clientPid] = callback;
    return SUCCESS;
}

int32_t AudioPolicyServer::UnsetVolumeKeyEventCallback(const int32_t clientPid)
{
    std::lock_guard<std::mutex> lock(volumeKeyEventMutex_);

    if (volumeChangeCbsMap_.find(clientPid) != volumeChangeCbsMap_.end()) {
        volumeChangeCbsMap_.erase(clientPid);
        AUDIO_ERR_LOG("AudioPolicyServer::UnsetVolumeKeyEventCallback for clientPid %{public}d done", clientPid);
    } else {
        AUDIO_DEBUG_LOG("AudioPolicyServer::UnsetVolumeKeyEventCallback clientPid %{public}d not present/unset already",
                        clientPid);
    }

    return SUCCESS;
}

bool AudioPolicyServer::VerifyClientPermission(const std::string &permissionName, uint32_t appTokenId, int32_t appUid)
{
    auto callerUid = IPCSkeleton::GetCallingUid();
    AUDIO_DEBUG_LOG("[%{public}s] [tid:%{public}d] [uid:%{public}d]", permissionName.c_str(), appTokenId, callerUid);

    // Root users should be whitelisted
    if ((callerUid == ROOT_UID) || ((callerUid == MEDIA_SERVICE_UID) && (appUid == ROOT_UID))) {
        AUDIO_DEBUG_LOG("Root user. Permission GRANTED!!!");
        return true;
    }

    Security::AccessToken::AccessTokenID clientTokenId = 0;
    // Special handling required for media service
    if (callerUid == MEDIA_SERVICE_UID) {
        if (appTokenId == 0) {
            AUDIO_ERR_LOG("Invalid token received. Permission rejected");
            return false;
        }
        clientTokenId = appTokenId;
    } else {
        clientTokenId = IPCSkeleton::GetCallingTokenID();
    }

    int res = Security::AccessToken::AccessTokenKit::VerifyAccessToken(clientTokenId, permissionName);
    if (res != Security::AccessToken::PermissionState::PERMISSION_GRANTED) {
        AUDIO_ERR_LOG("Permission denied [tid:%{public}d]", clientTokenId);
        return false;
    }

    return true;
}

int32_t AudioPolicyServer::ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType)
{
    // Only root users should have access to this api
    if (ROOT_UID != IPCSkeleton::GetCallingUid()) {
        AUDIO_INFO_LOG("Unautorized user. Cannot modify channel");
        return ERR_PERMISSION_DENIED;
    }

    return mPolicyService.ReconfigureAudioChannel(count, deviceType);
}

float AudioPolicyServer::MapVolumeToHDI(int32_t volume)
{
    float value = (float)volume / MAX_VOLUME_LEVEL;
    float roundValue = (int)(value * CONST_FACTOR);

    return (float)roundValue / CONST_FACTOR;
}

int32_t AudioPolicyServer::ConvertVolumeToInt(float volume)
{
    float value = (float)volume * MAX_VOLUME_LEVEL;
    return nearbyint(value);
}

void AudioPolicyServer::GetPolicyData(PolicyData &policyData)
{
    policyData.ringerMode = GetRingerMode();
    policyData.callStatus = GetAudioScene();

    // Get stream volumes
    for (int stream = AudioStreamType::STREAM_VOICE_CALL; stream <= AudioStreamType::STREAM_TTS; stream++) {
        AudioStreamType streamType = (AudioStreamType)stream;

        if (AudioServiceDump::IsStreamSupported(streamType)) {
            int32_t volume = ConvertVolumeToInt(GetStreamVolume(streamType));
            policyData.streamVolumes.insert({ streamType, volume });
        }
    }

    // Get Audio Focus Information
    AudioInterrupt audioInterrupt;
    if (GetSessionInfoInFocus(audioInterrupt) == SUCCESS) {
        policyData.audioFocusInfo = audioInterrupt;
    }

    // Get Input & Output Devices

    DeviceFlag deviceFlag = DeviceFlag::INPUT_DEVICES_FLAG;
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors = GetDevices(deviceFlag);

    for (auto it = audioDeviceDescriptors.begin(); it != audioDeviceDescriptors.end(); it++) {
        AudioDeviceDescriptor audioDeviceDescriptor = **it;
        DevicesInfo deviceInfo;
        deviceInfo.deviceType = audioDeviceDescriptor.deviceType_;
        deviceInfo.deviceRole = audioDeviceDescriptor.deviceRole_;
        policyData.inputDevices.push_back(deviceInfo);
    }

    deviceFlag = DeviceFlag::OUTPUT_DEVICES_FLAG;
    audioDeviceDescriptors = GetDevices(deviceFlag);

    for (auto it = audioDeviceDescriptors.begin(); it != audioDeviceDescriptors.end(); it++) {
        AudioDeviceDescriptor audioDeviceDescriptor = **it;
        DevicesInfo deviceInfo;
        deviceInfo.deviceType = audioDeviceDescriptor.deviceType_;
        deviceInfo.deviceRole = audioDeviceDescriptor.deviceRole_;
        policyData.outputDevices.push_back(deviceInfo);
    }
}

int32_t AudioPolicyServer::Dump(int32_t fd, const std::vector<std::u16string> &args)
{
    AUDIO_DEBUG_LOG("AudioPolicyServer: Dump Process Invoked");

    std::string dumpString;
    PolicyData policyData;
    AudioServiceDump dumpObj;

    if (dumpObj.Initialize() != AUDIO_DUMP_SUCCESS) {
        AUDIO_ERR_LOG("AudioPolicyServer:: Audio Service Dump Not initialised\n");
        return AUDIO_DUMP_INIT_ERR;
    }

    GetPolicyData(policyData);
    dumpObj.AudioDataDump(policyData, dumpString);

    return write(fd, dumpString.c_str(), dumpString.size());
}

int32_t AudioPolicyServer::GetAudioLatencyFromXml()
{
    return mPolicyService.GetAudioLatencyFromXml();
}

uint32_t AudioPolicyServer::GetSinkLatencyFromXml()
{
    return mPolicyService.GetSinkLatencyFromXml();
}

int32_t AudioPolicyServer::RegisterAudioRendererEventListener(int32_t clientUID, const sptr<IRemoteObject> &object)
{
    RegisterClientDeathRecipient(object, LISTENER_CLIENT);
    uint32_t clientTokenId = IPCSkeleton::GetCallingTokenID();
    bool hasBTPermission = VerifyClientPermission(USE_BLUETOOTH_PERMISSION, clientTokenId);
    return mPolicyService.RegisterAudioRendererEventListener(clientUID, object, hasBTPermission);
}

int32_t AudioPolicyServer::UnregisterAudioRendererEventListener(int32_t clientUID)
{
    return mPolicyService.UnregisterAudioRendererEventListener(clientUID);
}

int32_t AudioPolicyServer::RegisterAudioCapturerEventListener(int32_t clientUID, const sptr<IRemoteObject> &object)
{
    RegisterClientDeathRecipient(object, LISTENER_CLIENT);
    uint32_t clientTokenId = IPCSkeleton::GetCallingTokenID();
    bool hasBTPermission = VerifyClientPermission(USE_BLUETOOTH_PERMISSION, clientTokenId);
    return mPolicyService.RegisterAudioCapturerEventListener(clientUID, object, hasBTPermission);
}

int32_t AudioPolicyServer::UnregisterAudioCapturerEventListener(int32_t clientUID)
{
    return mPolicyService.UnregisterAudioCapturerEventListener(clientUID);
}

int32_t AudioPolicyServer::RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
    const sptr<IRemoteObject> &object)
{
    // update the clientUID
    auto callerUid =  IPCSkeleton::GetCallingUid();
    AUDIO_INFO_LOG("  AudioPolicyServer::RegisterTracker : [caller uid:%{public}d]==", callerUid);
    if (callerUid != MEDIA_SERVICE_UID) {
        if (mode == AUDIO_MODE_PLAYBACK) {
            streamChangeInfo.audioRendererChangeInfo.clientUID = callerUid;
            AUDIO_DEBUG_LOG("Non media service caller, use the uid retrieved. ClientUID:%{public}d]",
                streamChangeInfo.audioRendererChangeInfo.clientUID);
        } else {
            streamChangeInfo.audioCapturerChangeInfo.clientUID = callerUid;
            AUDIO_DEBUG_LOG("Non media service caller, use the uid retrieved. ClientUID:%{public}d]",
                streamChangeInfo.audioCapturerChangeInfo.clientUID);
        }
    }
    RegisterClientDeathRecipient(object, TRACKER_CLIENT);
    return mPolicyService.RegisterTracker(mode, streamChangeInfo, object);
}

int32_t AudioPolicyServer::UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo)
{
    // update the clientUID
    auto callerUid =  IPCSkeleton::GetCallingUid();
    AUDIO_INFO_LOG("  AudioPolicyServer::UpdateTracker : [caller uid:%{public}d]==", callerUid);
    if (callerUid != MEDIA_SERVICE_UID) {
        if (mode == AUDIO_MODE_PLAYBACK) {
            streamChangeInfo.audioRendererChangeInfo.clientUID = callerUid;
            AUDIO_DEBUG_LOG("Non media service caller, use the uid retrieved. ClientUID:%{public}d]",
                streamChangeInfo.audioRendererChangeInfo.clientUID);
        } else {
            streamChangeInfo.audioCapturerChangeInfo.clientUID = callerUid;
            AUDIO_DEBUG_LOG("Non media service caller, use the uid retrieved. ClientUID:%{public}d]",
                streamChangeInfo.audioCapturerChangeInfo.clientUID);
        }
    }
    return mPolicyService.UpdateTracker(mode, streamChangeInfo);
}

int32_t AudioPolicyServer::GetCurrentRendererChangeInfos(
    vector<unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos)
{
    bool hasBTPermission = VerifyClientPermission(USE_BLUETOOTH_PERMISSION);
    AUDIO_DEBUG_LOG("GetCurrentRendererChangeInfos: BT use permission: %{public}d", hasBTPermission);
    return mPolicyService.GetCurrentRendererChangeInfos(audioRendererChangeInfos, hasBTPermission);
}

int32_t AudioPolicyServer::GetCurrentCapturerChangeInfos(
    vector<unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    bool hasBTPermission = VerifyClientPermission(USE_BLUETOOTH_PERMISSION);
    AUDIO_DEBUG_LOG("GetCurrentCapturerChangeInfos: BT use permission: %{public}d", hasBTPermission);
    return mPolicyService.GetCurrentCapturerChangeInfos(audioCapturerChangeInfos, hasBTPermission);
}

void AudioPolicyServer::RegisterClientDeathRecipient(const sptr<IRemoteObject> &object, DeathRecipientId id)
{
    AUDIO_INFO_LOG("Register clients death recipient!!");
    CHECK_AND_RETURN_LOG(object != nullptr, "Client proxy obj NULL!!");

    pid_t uid = 0;
    if (id == TRACKER_CLIENT) {
        // Deliberately casting UID to pid_t
        uid = static_cast<pid_t>(IPCSkeleton::GetCallingUid());
    } else {
        uid = IPCSkeleton::GetCallingPid();
    }
    sptr<AudioServerDeathRecipient> deathRecipient_ = new(std::nothrow) AudioServerDeathRecipient(uid);
    if (deathRecipient_ != nullptr) {
        if (id == TRACKER_CLIENT) {
            deathRecipient_->SetNotifyCb(std::bind(&AudioPolicyServer::RegisteredTrackerClientDied,
                this, std::placeholders::_1));
        } else {
            AUDIO_INFO_LOG("RegisteredStreamListenerClientDied register!!");
            deathRecipient_->SetNotifyCb(std::bind(&AudioPolicyServer::RegisteredStreamListenerClientDied,
                this, std::placeholders::_1));
        }
        bool result = object->AddDeathRecipient(deathRecipient_);
        if (!result) {
            AUDIO_ERR_LOG("failed to add deathRecipient");
        }
    }
}

void AudioPolicyServer::RegisteredTrackerClientDied(pid_t pid)
{
    AUDIO_INFO_LOG("RegisteredTrackerClient died: remove entry, uid %{public}d", pid);
    mPolicyService.RegisteredTrackerClientDied(pid);
}

void AudioPolicyServer::RegisteredStreamListenerClientDied(pid_t pid)
{
    AUDIO_INFO_LOG("RegisteredStreamListenerClient died: remove entry, uid %{public}d", pid);
    mPolicyService.RegisteredStreamListenerClientDied(pid);
}
} // namespace AudioStandard
} // namespace OHOS
