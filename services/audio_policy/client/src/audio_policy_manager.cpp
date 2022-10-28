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

#include "audio_errors.h"
#include "audio_policy_proxy.h"
#include "audio_server_death_recipient.h"
#include "iservice_registry.h"
#include "audio_log.h"
#include "system_ability_definition.h"
#include "audio_policy_manager.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;

static sptr<IAudioPolicy> g_sProxy = nullptr;
bool AudioPolicyManager::serverConnected = false;

void AudioPolicyManager::Init()
{
    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    CHECK_AND_RETURN_LOG(samgr != nullptr, "AudioPolicyManager::init failed");

    sptr<IRemoteObject> object = samgr->GetSystemAbility(AUDIO_POLICY_SERVICE_ID);
    CHECK_AND_RETURN_LOG(object != nullptr, "AudioPolicyManager::object is NULL.");

    g_sProxy = iface_cast<IAudioPolicy>(object);
    CHECK_AND_RETURN_LOG(g_sProxy != nullptr, "AudioPolicyManager::init g_sProxy is NULL.");

    serverConnected = true;
    AUDIO_DEBUG_LOG("AudioPolicyManager::init g_sProxy is assigned.");

    RegisterAudioPolicyServerDeathRecipient();
}

void AudioPolicyManager::RegisterAudioPolicyServerDeathRecipient()
{
    AUDIO_INFO_LOG("Register audio policy server death recipient");
    pid_t pid = 0;
    sptr<AudioServerDeathRecipient> deathRecipient_ = new(std::nothrow) AudioServerDeathRecipient(pid);
    if (deathRecipient_ != nullptr) {
        auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        CHECK_AND_RETURN_LOG(samgr != nullptr, "Failed to obtain samgr");

        sptr<IRemoteObject> object = samgr->GetSystemAbility(OHOS::AUDIO_POLICY_SERVICE_ID);
        CHECK_AND_RETURN_LOG(object != nullptr, "Policy service unavailable");

        deathRecipient_->SetNotifyCb(std::bind(&AudioPolicyManager::AudioPolicyServerDied, this,
                                               std::placeholders::_1));
        bool result = object->AddDeathRecipient(deathRecipient_);
        if (!result) {
            AUDIO_ERR_LOG("failed to add deathRecipient");
        }
    }
}

void AudioPolicyManager::AudioPolicyServerDied(pid_t pid)
{
    AUDIO_INFO_LOG("Audio policy server died: reestablish connection");
    serverConnected = false;
}

int32_t AudioPolicyManager::SetStreamVolume(AudioStreamType streamType, float volume)
{
    return g_sProxy->SetStreamVolume(streamType, volume);
}

int32_t AudioPolicyManager::SetRingerMode(AudioRingerMode ringMode)
{
    return g_sProxy->SetRingerMode(ringMode);
}

AudioRingerMode AudioPolicyManager::GetRingerMode()
{
    return g_sProxy->GetRingerMode();
}

int32_t AudioPolicyManager::SetAudioScene(AudioScene scene)
{
    return g_sProxy->SetAudioScene(scene);
}

int32_t AudioPolicyManager::SetMicrophoneMute(bool isMute)
{
    return g_sProxy->SetMicrophoneMute(isMute);
}

int32_t AudioPolicyManager::SetMicrophoneMuteAudioConfig(bool isMute)
{
    return g_sProxy->SetMicrophoneMuteAudioConfig(isMute);
}

bool AudioPolicyManager::IsMicrophoneMute()
{
    return g_sProxy->IsMicrophoneMute();
}

AudioScene AudioPolicyManager::GetAudioScene()
{
    return g_sProxy->GetAudioScene();
}

float AudioPolicyManager::GetStreamVolume(AudioStreamType streamType)
{
    return g_sProxy->GetStreamVolume(streamType);
}

int32_t AudioPolicyManager::SetStreamMute(AudioStreamType streamType, bool mute)
{
    return g_sProxy->SetStreamMute(streamType, mute);
}

bool AudioPolicyManager::GetStreamMute(AudioStreamType streamType)
{
    return g_sProxy->GetStreamMute(streamType);
}

int32_t AudioPolicyManager::SetLowPowerVolume(int32_t streamId, float volume)
{
    return g_sProxy->SetLowPowerVolume(streamId, volume);
}

float AudioPolicyManager::GetLowPowerVolume(int32_t streamId)
{
    return g_sProxy->GetLowPowerVolume(streamId);
}

float AudioPolicyManager::GetSingleStreamVolume(int32_t streamId)
{
    return g_sProxy->GetSingleStreamVolume(streamId);
}

bool AudioPolicyManager::IsStreamActive(AudioStreamType streamType)
{
    return g_sProxy->IsStreamActive(streamType);
}

int32_t AudioPolicyManager::SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors)
{
    return g_sProxy->SelectOutputDevice(audioRendererFilter, audioDeviceDescriptors);
}

std::string AudioPolicyManager::GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType)
{
    return g_sProxy->GetSelectedDeviceInfo(uid, pid, streamType);
}

int32_t AudioPolicyManager::SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors)
{
    return g_sProxy->SelectInputDevice(audioCapturerFilter, audioDeviceDescriptors);
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyManager::GetDevices(DeviceFlag deviceFlag)
{
    return g_sProxy->GetDevices(deviceFlag);
}

std::vector<int32_t> AudioPolicyManager::GetSupportedTones()
{
    return g_sProxy->GetSupportedTones();
}

std::shared_ptr<ToneInfo> AudioPolicyManager::GetToneConfig(int32_t ltonetype)
{
    AUDIO_DEBUG_LOG("AudioPolicyManager GetToneConfig,");

    return g_sProxy->GetToneConfig(ltonetype);
}

int32_t AudioPolicyManager::SetDeviceActive(InternalDeviceType deviceType, bool active)
{
    return g_sProxy->SetDeviceActive(deviceType, active);
}

bool AudioPolicyManager::IsDeviceActive(InternalDeviceType deviceType)
{
    return g_sProxy->IsDeviceActive(deviceType);
}

DeviceType AudioPolicyManager::GetActiveOutputDevice()
{
    return g_sProxy->GetActiveOutputDevice();
}

DeviceType AudioPolicyManager::GetActiveInputDevice()
{
    return g_sProxy->GetActiveInputDevice();
}

int32_t AudioPolicyManager::SetRingerModeCallback(const int32_t clientId,
                                                  const std::shared_ptr<AudioRingerModeCallback> &callback)
{
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    ringerModelistenerStub_ = new(std::nothrow) AudioRingerModeUpdateListenerStub();
    if (ringerModelistenerStub_ == nullptr || g_sProxy == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: object null");
        return ERROR;
    }
    ringerModelistenerStub_->SetCallback(callback);

    sptr<IRemoteObject> object = ringerModelistenerStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: listenerStub->AsObject is nullptr..");
        return ERROR;
    }

    return g_sProxy->SetRingerModeCallback(clientId, object);
}

int32_t AudioPolicyManager::UnsetRingerModeCallback(const int32_t clientId)
{
    return g_sProxy->UnsetRingerModeCallback(clientId);
}

int32_t AudioPolicyManager::SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag,
    const std::shared_ptr<AudioManagerDeviceChangeCallback> &callback)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    auto deviceChangeCbStub = new(std::nothrow) AudioPolicyManagerListenerStub();
    if (deviceChangeCbStub == nullptr || g_sProxy == nullptr) {
        AUDIO_ERR_LOG("SetDeviceChangeCallback: object null");
        return ERROR;
    }

    deviceChangeCbStub->SetDeviceChangeCallback(callback);

    sptr<IRemoteObject> object = deviceChangeCbStub->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: listenerStub->AsObject is nullptr..");
        delete deviceChangeCbStub;
        return ERROR;
    }

    return g_sProxy->SetDeviceChangeCallback(clientId, flag, object);
}

int32_t AudioPolicyManager::UnsetDeviceChangeCallback(const int32_t clientId)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    return g_sProxy->UnsetDeviceChangeCallback(clientId);
}

int32_t AudioPolicyManager::SetMicStateChangeCallback(const int32_t clientId,
    const std::shared_ptr<AudioManagerMicStateChangeCallback> &callback)
{
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    auto micStateChangeCbStub = new(std::nothrow) AudioRoutingManagerListenerStub();
    if (micStateChangeCbStub == nullptr || g_sProxy == nullptr) {
        AUDIO_ERR_LOG("SetMicStateChangeCallback: object null");
        return ERROR;
    }

    micStateChangeCbStub->SetMicStateChangeCallback(callback);

    sptr<IRemoteObject> object = micStateChangeCbStub->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: listenerStub->AsObject is nullptr..");
        return ERROR;
    }
    return g_sProxy->SetMicStateChangeCallback(clientId, object);
}

int32_t AudioPolicyManager::SetAudioInterruptCallback(const uint32_t sessionID,
                                                      const std::shared_ptr<AudioInterruptCallback> &callback)
{
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    // Need to lock member variable listenerStub_ as SetAudioInterruptCallback
    // can be called from different threads in multi renderer usage
    std::unique_lock<std::mutex> lock(listenerStubMutex_);
    listenerStub_ = new(std::nothrow) AudioPolicyManagerListenerStub();
    if (listenerStub_ == nullptr || g_sProxy == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: object null");
        return ERROR;
    }
    listenerStub_->SetInterruptCallback(callback);

    sptr<IRemoteObject> object = listenerStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: listenerStub->AsObject is nullptr..");
        return ERROR;
    }
    lock.unlock(); // unlock once it is converted into IRemoteObject

    return g_sProxy->SetAudioInterruptCallback(sessionID, object);
}

int32_t AudioPolicyManager::UnsetAudioInterruptCallback(const uint32_t sessionID)
{
    return g_sProxy->UnsetAudioInterruptCallback(sessionID);
}

int32_t AudioPolicyManager::ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt)
{
    return g_sProxy->ActivateAudioInterrupt(audioInterrupt);
}

int32_t AudioPolicyManager::DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt)
{
    return g_sProxy->DeactivateAudioInterrupt(audioInterrupt);
}

int32_t AudioPolicyManager::SetAudioManagerInterruptCallback(const uint32_t clientID,
    const std::shared_ptr<AudioInterruptCallback> &callback)
{
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    std::unique_lock<std::mutex> lock(listenerStubMutex_);
    sptr<AudioPolicyManagerListenerStub> interruptListenerStub = new(std::nothrow) AudioPolicyManagerListenerStub();
    if (interruptListenerStub == nullptr || g_sProxy == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: object null");
        return ERROR;
    }
    interruptListenerStub->SetInterruptCallback(callback);

    sptr<IRemoteObject> object = interruptListenerStub->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: onInterruptListenerStub->AsObject is nullptr..");
        return ERROR;
    }
    lock.unlock();

    return g_sProxy->SetAudioManagerInterruptCallback(clientID, object);
}

int32_t AudioPolicyManager::UnsetAudioManagerInterruptCallback(const uint32_t clientID)
{
    return g_sProxy->UnsetAudioManagerInterruptCallback(clientID);
}

int32_t AudioPolicyManager::RequestAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt)
{
    return g_sProxy->RequestAudioFocus(clientID, audioInterrupt);
}

int32_t AudioPolicyManager::AbandonAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt)
{
    return g_sProxy->AbandonAudioFocus(clientID, audioInterrupt);
}

AudioStreamType AudioPolicyManager::GetStreamInFocus()
{
    return g_sProxy->GetStreamInFocus();
}

int32_t AudioPolicyManager::GetSessionInfoInFocus(AudioInterrupt &audioInterrupt)
{
    return g_sProxy->GetSessionInfoInFocus(audioInterrupt);
}

int32_t AudioPolicyManager::SetVolumeKeyEventCallback(const int32_t clientPid,
                                                      const std::shared_ptr<VolumeKeyEventCallback> &callback)
{
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioSystemManager volume back is nullptr");
        return ERR_INVALID_PARAM;
    }

    std::lock_guard<std::mutex> lock(volumeCallbackMutex_);
    volumeKeyEventListenerStub_ = new(std::nothrow) AudioVolumeKeyEventCallbackStub();
    if (volumeKeyEventListenerStub_ == nullptr || g_sProxy == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: object null");
        return ERROR;
    }
    volumeKeyEventListenerStub_->SetOnVolumeKeyEventCallback(callback);

    sptr<IRemoteObject> object = volumeKeyEventListenerStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: volumeKeyEventListenerStub_->AsObject is nullptr..");
        return ERROR;
    }
    return g_sProxy->SetVolumeKeyEventCallback(clientPid, object);
}

int32_t AudioPolicyManager::UnsetVolumeKeyEventCallback(const int32_t clientPid)
{
    return g_sProxy->UnsetVolumeKeyEventCallback(clientPid);
}

int32_t AudioPolicyManager::RegisterAudioRendererEventListener(const int32_t clientUID,
    const std::shared_ptr<AudioRendererStateChangeCallback> &callback)
{
    AUDIO_INFO_LOG("AudioPolicyManager::RegisterAudioRendererEventListener");
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: RendererEvent Listener callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    std::unique_lock<std::mutex> lock(stateChangelistenerStubMutex_);
    rendererStateChangelistenerStub_ = new(std::nothrow) AudioRendererStateChangeListenerStub();
    if (rendererStateChangelistenerStub_ == nullptr || g_sProxy == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: object null");
        return ERROR;
    }

    rendererStateChangelistenerStub_->SetCallback(callback);

    sptr<IRemoteObject> object = rendererStateChangelistenerStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager:RenderStateChangeListener IPC object creation failed");
        return ERROR;
    }
    lock.unlock();

    return g_sProxy->RegisterAudioRendererEventListener(clientUID, object);
}

int32_t AudioPolicyManager::UnregisterAudioRendererEventListener(const int32_t clientUID)
{
    AUDIO_INFO_LOG("AudioPolicyManager::UnregisterAudioRendererEventListener");
    return g_sProxy->UnregisterAudioRendererEventListener(clientUID);
}

int32_t AudioPolicyManager::RegisterAudioCapturerEventListener(const int32_t clientUID,
    const std::shared_ptr<AudioCapturerStateChangeCallback> &callback)
{
    AUDIO_INFO_LOG("AudioPolicyManager::RegisterAudioCapturerEventListener");
    if (callback == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: Capturer Event Listener callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    std::unique_lock<std::mutex> lock(stateChangelistenerStubMutex_);
    capturerStateChangelistenerStub_ = new(std::nothrow) AudioCapturerStateChangeListenerStub();
    if (capturerStateChangelistenerStub_ == nullptr || g_sProxy == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager: object null");
        return ERROR;
    }

    capturerStateChangelistenerStub_->SetCallback(callback);

    sptr<IRemoteObject> object = capturerStateChangelistenerStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("AudioPolicyManager:CapturerStateChangeListener IPC object creation failed");
        return ERROR;
    }
    lock.unlock();

    return g_sProxy->RegisterAudioCapturerEventListener(clientUID, object);
}

int32_t AudioPolicyManager::UnregisterAudioCapturerEventListener(const int32_t clientUID)
{
    AUDIO_INFO_LOG("AudioPolicyManager::UnregisterAudioCapturerEventListener");
    return g_sProxy->UnregisterAudioCapturerEventListener(clientUID);
}

int32_t AudioPolicyManager::RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
    const std::shared_ptr<AudioClientTracker> &clientTrackerObj)
{
    AUDIO_INFO_LOG("AudioPolicyManager::RegisterTracker");

    std::unique_lock<std::mutex> lock(clientTrackerStubMutex_);
    clientTrackerCbStub_ = new(std::nothrow) AudioClientTrackerCallbackStub();
    if (clientTrackerCbStub_ == nullptr || g_sProxy == nullptr) {
        AUDIO_ERR_LOG("clientTrackerCbStub: memory allocation failed");
        return ERROR;
    }

    clientTrackerCbStub_->SetClientTrackerCallback(clientTrackerObj);

    sptr<IRemoteObject> object = clientTrackerCbStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("clientTrackerCbStub: IPC object creation failed");
        return ERROR;
    }
    lock.unlock();

    return g_sProxy->RegisterTracker(mode, streamChangeInfo, object);
}

int32_t AudioPolicyManager::UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo)
{
    AUDIO_INFO_LOG("AudioPolicyManager::UpdateTracker");
    return g_sProxy->UpdateTracker(mode, streamChangeInfo);
}

bool AudioPolicyManager::VerifyClientPermission(const std::string &permissionName,
    uint32_t appTokenId, int32_t appUid, bool privacyFlag, AudioPermissionState state)
{
    return g_sProxy->VerifyClientPermission(permissionName, appTokenId, appUid, privacyFlag, state);
}

bool AudioPolicyManager::getUsingPemissionFromPrivacy(const std::string &permissionName, uint32_t appTokenId,
    AudioPermissionState state)
{
    return g_sProxy->getUsingPemissionFromPrivacy(permissionName, appTokenId, state);
}

int32_t AudioPolicyManager::ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType)
{
    return g_sProxy->ReconfigureAudioChannel(count, deviceType);
}

int32_t AudioPolicyManager::GetAudioLatencyFromXml()
{
    return g_sProxy->GetAudioLatencyFromXml();
}

uint32_t AudioPolicyManager::GetSinkLatencyFromXml()
{
    return g_sProxy->GetSinkLatencyFromXml();
}

int32_t AudioPolicyManager::GetCurrentRendererChangeInfos(
    vector<unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos)
{
    AUDIO_DEBUG_LOG("AudioPolicyManager::GetCurrentRendererChangeInfos");

    return g_sProxy->GetCurrentRendererChangeInfos(audioRendererChangeInfos);
}

int32_t AudioPolicyManager::GetCurrentCapturerChangeInfos(
    vector<unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    AUDIO_DEBUG_LOG("AudioPolicyManager::GetCurrentCapturerChangeInfos");

    return g_sProxy->GetCurrentCapturerChangeInfos(audioCapturerChangeInfos);
}

int32_t AudioPolicyManager::UpdateStreamState(const int32_t clientUid,
    StreamSetState streamSetState, AudioStreamType audioStreamType)
{
    AUDIO_DEBUG_LOG("AudioPolicyManager::UpdateStreamState");
    return  g_sProxy->UpdateStreamState(clientUid, streamSetState, audioStreamType);
}

std::vector<sptr<VolumeGroupInfo>> AudioPolicyManager::GetVolumeGroupInfos()
{
    return g_sProxy->GetVolumeGroupInfos();
}

bool AudioPolicyManager::IsAudioRendererLowLatencySupported(const AudioStreamInfo &audioStreamInfo)
{
    AUDIO_DEBUG_LOG("AudioPolicyManager::IsAudioRendererLowLatencySupported");

    return g_sProxy->IsAudioRendererLowLatencySupported(audioStreamInfo);
}
} // namespace AudioStandard
} // namespace OHOS
