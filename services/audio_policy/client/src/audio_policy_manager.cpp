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

#include "audio_policy_manager.h"
#include "audio_errors.h"
#include "audio_policy_proxy.h"
#include "audio_server_death_recipient.h"
#include "iservice_registry.h"
#include "audio_log.h"
#include "system_ability_definition.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;

static sptr<IAudioPolicy> g_apProxy = nullptr;
mutex g_apProxyMutex;

const sptr<IAudioPolicy> AudioPolicyManager::GetAudioPolicyManagerProxy()
{
    AUDIO_DEBUG_LOG("GetAudioPolicyManagerProxy Start to get audio manager service proxy.");
    lock_guard<mutex> lock(g_apProxyMutex);

    if (g_apProxy == nullptr) {
        auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        if (samgr == nullptr) {
            AUDIO_ERR_LOG("GetAudioPolicyManagerProxy samgr init failed.");
            return nullptr;
        }

        sptr<IRemoteObject> object = samgr->GetSystemAbility(AUDIO_POLICY_SERVICE_ID);
        if (object == nullptr) {
            AUDIO_ERR_LOG("GetAudioPolicyManagerProxy Object is NULL.");
            return nullptr;
        }

        g_apProxy = iface_cast<IAudioPolicy>(object);
        if (g_apProxy == nullptr) {
            AUDIO_ERR_LOG("GetAudioPolicyManagerProxy Init g_apProxy is NULL.");
            return nullptr;
        }

        AUDIO_DEBUG_LOG("GetAudioPolicyManagerProxy Init g_apProxy is assigned.");
        RegisterAudioPolicyServerDeathRecipient();
    }

    const sptr<IAudioPolicy> gsp = g_apProxy;
    return gsp;
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
    lock_guard<mutex> lock(g_apProxyMutex);
    AUDIO_INFO_LOG("Audio policy server died: reestablish connection");
    g_apProxy = nullptr;
}

int32_t AudioPolicyManager::SetStreamVolume(AudioStreamType streamType, float volume)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetStreamVolume: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->SetStreamVolume(streamType, volume);
}

int32_t AudioPolicyManager::SetRingerMode(AudioRingerMode ringMode)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetRingerMode: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->SetRingerMode(ringMode);
}

AudioRingerMode AudioPolicyManager::GetRingerMode()
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetRingerMode: audio policy manager proxy is NULL.");
        return RINGER_MODE_NORMAL;
    }
    return gsp->GetRingerMode();
}

int32_t AudioPolicyManager::SetAudioScene(AudioScene scene)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetAudioScene: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->SetAudioScene(scene);
}

int32_t AudioPolicyManager::SetMicrophoneMute(bool isMute)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetMicrophoneMute: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->SetMicrophoneMute(isMute);
}

int32_t AudioPolicyManager::SetMicrophoneMuteAudioConfig(bool isMute)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetMicrophoneMuteAudioConfig: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->SetMicrophoneMuteAudioConfig(isMute);
}

bool AudioPolicyManager::IsMicrophoneMute(API_VERSION api_v)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("IsMicrophoneMute: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->IsMicrophoneMute(api_v);
}

AudioScene AudioPolicyManager::GetAudioScene()
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetAudioScene: audio policy manager proxy is NULL.");
        return AUDIO_SCENE_DEFAULT;
    }
    return gsp->GetAudioScene();
}

float AudioPolicyManager::GetStreamVolume(AudioStreamType streamType)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetStreamVolume: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->GetStreamVolume(streamType);
}

int32_t AudioPolicyManager::SetStreamMute(AudioStreamType streamType, bool mute)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetStreamMute: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->SetStreamMute(streamType, mute);
}

bool AudioPolicyManager::GetStreamMute(AudioStreamType streamType)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetStreamMute: audio policy manager proxy is NULL.");
        return false;
    }
    return gsp->GetStreamMute(streamType);
}

int32_t AudioPolicyManager::SetLowPowerVolume(int32_t streamId, float volume)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetLowPowerVolume: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->SetLowPowerVolume(streamId, volume);
}

float AudioPolicyManager::GetLowPowerVolume(int32_t streamId)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetLowPowerVolume: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->GetLowPowerVolume(streamId);
}

float AudioPolicyManager::GetSingleStreamVolume(int32_t streamId)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetSingleStreamVolume: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->GetSingleStreamVolume(streamId);
}

bool AudioPolicyManager::IsStreamActive(AudioStreamType streamType)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("IsStreamActive: audio policy manager proxy is NULL.");
        return false;
    }
    return gsp->IsStreamActive(streamType);
}

int32_t AudioPolicyManager::SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SelectOutputDevice: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->SelectOutputDevice(audioRendererFilter, audioDeviceDescriptors);
}

std::string AudioPolicyManager::GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetSelectedDeviceInfo: audio policy manager proxy is NULL.");
        return "";
    }
    return gsp->GetSelectedDeviceInfo(uid, pid, streamType);
}

int32_t AudioPolicyManager::SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SelectInputDevice: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->SelectInputDevice(audioCapturerFilter, audioDeviceDescriptors);
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyManager::GetDevices(DeviceFlag deviceFlag)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetDevices: audio policy manager proxy is NULL.");
        std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;
        return deviceInfo;
    }
    return gsp->GetDevices(deviceFlag);
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyManager::GetActiveOutputDeviceDescriptors()
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetActiveOutputDeviceDescriptors: audio policy manager proxy is NULL.");
        std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;
        return deviceInfo;
    }
    return gsp->GetActiveOutputDeviceDescriptors();
}

std::vector<int32_t> AudioPolicyManager::GetSupportedTones()
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetSupportedTones: audio policy manager proxy is NULL.");
        std::vector<int> lSupportedToneList = {};
        return lSupportedToneList;
    }
    return gsp->GetSupportedTones();
}

std::shared_ptr<ToneInfo> AudioPolicyManager::GetToneConfig(int32_t ltonetype)
{
    AUDIO_DEBUG_LOG("AudioPolicyManager GetToneConfig,");

    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetToneConfig: audio policy manager proxy is NULL.");
        return nullptr;
    }
    return gsp->GetToneConfig(ltonetype);
}

int32_t AudioPolicyManager::SetDeviceActive(InternalDeviceType deviceType, bool active)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetDeviceActive: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->SetDeviceActive(deviceType, active);
}

bool AudioPolicyManager::IsDeviceActive(InternalDeviceType deviceType)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("IsDeviceActive: audio policy manager proxy is NULL.");
        return false;
    }
    return gsp->IsDeviceActive(deviceType);
}

DeviceType AudioPolicyManager::GetActiveOutputDevice()
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetActiveOutputDevice: audio policy manager proxy is NULL.");
        return DEVICE_TYPE_INVALID;
    }
    return gsp->GetActiveOutputDevice();
}

DeviceType AudioPolicyManager::GetActiveInputDevice()
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetActiveInputDevice: audio policy manager proxy is NULL.");
        return DEVICE_TYPE_INVALID;
    }
    return gsp->GetActiveInputDevice();
}

int32_t AudioPolicyManager::SetRingerModeCallback(const int32_t clientId,
                                                  const std::shared_ptr<AudioRingerModeCallback> &callback)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetRingerModeCallback: audio policy manager proxy is NULL.");
        return ERR_INVALID_PARAM;
    }

    if (callback == nullptr) {
        AUDIO_ERR_LOG("SetRingerModeCallback: callback is nullptr");
        return ERR_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> lockSet(ringerModelistenerStubMutex_);
    ringerModelistenerStub_ = new(std::nothrow) AudioRingerModeUpdateListenerStub();
    if (ringerModelistenerStub_ == nullptr) {
        AUDIO_ERR_LOG("SetRingerModeCallback: object null");
        return ERROR;
    }
    ringerModelistenerStub_->SetCallback(callback);

    sptr<IRemoteObject> object = ringerModelistenerStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("SetRingerModeCallback: listenerStub->AsObject is nullptr..");
        return ERROR;
    }

    return gsp->SetRingerModeCallback(clientId, object);
}

int32_t AudioPolicyManager::UnsetRingerModeCallback(const int32_t clientId)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("UnsetRingerModeCallback: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->UnsetRingerModeCallback(clientId);
}

int32_t AudioPolicyManager::SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag,
    const std::shared_ptr<AudioManagerDeviceChangeCallback> &callback)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetDeviceChangeCallback: audio policy manager proxy is NULL.");
        return -1;
    }
    AUDIO_INFO_LOG("Entered %{public}s", __func__);
    if (callback == nullptr) {
        AUDIO_ERR_LOG("SetDeviceChangeCallback: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    auto deviceChangeCbStub = new(std::nothrow) AudioPolicyManagerListenerStub();
    if (deviceChangeCbStub == nullptr) {
        AUDIO_ERR_LOG("SetDeviceChangeCallback: object null");
        return ERROR;
    }

    deviceChangeCbStub->SetDeviceChangeCallback(callback);

    sptr<IRemoteObject> object = deviceChangeCbStub->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("SetDeviceChangeCallback: listenerStub->AsObject is nullptr..");
        delete deviceChangeCbStub;
        return ERROR;
    }

    return gsp->SetDeviceChangeCallback(clientId, flag, object);
}

int32_t AudioPolicyManager::UnsetDeviceChangeCallback(const int32_t clientId)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("UnsetDeviceChangeCallback: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->UnsetDeviceChangeCallback(clientId);
}

int32_t AudioPolicyManager::SetMicStateChangeCallback(const int32_t clientId,
    const std::shared_ptr<AudioManagerMicStateChangeCallback> &callback)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetMicStateChangeCallback: audio policy manager proxy is NULL.");
        return ERROR;
    }
    if (callback == nullptr) {
        AUDIO_ERR_LOG("SetMicStateChangeCallback: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    auto micStateChangeCbStub = new(std::nothrow) AudioRoutingManagerListenerStub();
    if (micStateChangeCbStub == nullptr) {
        AUDIO_ERR_LOG("SetMicStateChangeCallback: object null");
        return ERROR;
    }

    micStateChangeCbStub->SetMicStateChangeCallback(callback);

    sptr<IRemoteObject> object = micStateChangeCbStub->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("SetMicStateChangeCallback: listenerStub->AsObject is nullptr..");
        return ERROR;
    }
    return gsp->SetMicStateChangeCallback(clientId, object);
}

int32_t AudioPolicyManager::SetAudioInterruptCallback(const uint32_t sessionID,
                                                      const std::shared_ptr<AudioInterruptCallback> &callback)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetAudioInterruptCallback: audio policy manager proxy is NULL.");
        return ERROR;
    }
    if (callback == nullptr) {
        AUDIO_ERR_LOG("SetAudioInterruptCallback: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    // Need to lock member variable listenerStub_ as SetAudioInterruptCallback
    // can be called from different threads in multi renderer usage
    std::unique_lock<std::mutex> lock(listenerStubMutex_);
    listenerStub_ = new(std::nothrow) AudioPolicyManagerListenerStub();
    if (listenerStub_ == nullptr) {
        AUDIO_ERR_LOG("SetAudioInterruptCallback: object null");
        return ERROR;
    }
    listenerStub_->SetInterruptCallback(callback);

    sptr<IRemoteObject> object = listenerStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("SetAudioInterruptCallback: listenerStub->AsObject is nullptr..");
        return ERROR;
    }
    lock.unlock(); // unlock once it is converted into IRemoteObject

    return gsp->SetAudioInterruptCallback(sessionID, object);
}

int32_t AudioPolicyManager::UnsetAudioInterruptCallback(const uint32_t sessionID)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("UnsetAudioInterruptCallback: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->UnsetAudioInterruptCallback(sessionID);
}

int32_t AudioPolicyManager::ActivateAudioInterrupt(const AudioInterrupt &audioInterrupt)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("ActivateAudioInterrupt: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->ActivateAudioInterrupt(audioInterrupt);
}

int32_t AudioPolicyManager::DeactivateAudioInterrupt(const AudioInterrupt &audioInterrupt)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("DeactivateAudioInterrupt: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->DeactivateAudioInterrupt(audioInterrupt);
}

int32_t AudioPolicyManager::SetAudioManagerInterruptCallback(const uint32_t clientID,
    const std::shared_ptr<AudioInterruptCallback> &callback)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetAudioManagerInterruptCallback: audio policy manager proxy is NULL.");
        return ERROR;
    }
    if (callback == nullptr) {
        AUDIO_ERR_LOG("SetAudioManagerInterruptCallback: callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    std::unique_lock<std::mutex> lock(listenerStubMutex_);
    sptr<AudioPolicyManagerListenerStub> interruptListenerStub = new(std::nothrow) AudioPolicyManagerListenerStub();
    if (interruptListenerStub == nullptr) {
        AUDIO_ERR_LOG("SetAudioManagerInterruptCallback: object null");
        return ERROR;
    }
    interruptListenerStub->SetInterruptCallback(callback);

    sptr<IRemoteObject> object = interruptListenerStub->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("SetAudioManagerInterruptCallback: onInterruptListenerStub->AsObject is nullptr.");
        return ERROR;
    }
    lock.unlock();

    return gsp->SetAudioManagerInterruptCallback(clientID, object);
}

int32_t AudioPolicyManager::UnsetAudioManagerInterruptCallback(const uint32_t clientID)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("UnsetAudioManagerInterruptCallback: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->UnsetAudioManagerInterruptCallback(clientID);
}

int32_t AudioPolicyManager::RequestAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("RequestAudioFocus: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->RequestAudioFocus(clientID, audioInterrupt);
}

int32_t AudioPolicyManager::AbandonAudioFocus(const uint32_t clientID, const AudioInterrupt &audioInterrupt)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("AbandonAudioFocus: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->AbandonAudioFocus(clientID, audioInterrupt);
}

AudioStreamType AudioPolicyManager::GetStreamInFocus()
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetStreamInFocus: audio policy manager proxy is NULL.");
        return STREAM_DEFAULT;
    }
    return gsp->GetStreamInFocus();
}

int32_t AudioPolicyManager::GetSessionInfoInFocus(AudioInterrupt &audioInterrupt)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetSessionInfoInFocus: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->GetSessionInfoInFocus(audioInterrupt);
}

int32_t AudioPolicyManager::SetVolumeKeyEventCallback(const int32_t clientPid,
                                                      const std::shared_ptr<VolumeKeyEventCallback> &callback)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetVolumeKeyEventCallback: audio policy manager proxy is NULL.");
        return ERROR;
    }

    if (callback == nullptr) {
        AUDIO_ERR_LOG("SetVolumeKeyEventCallback volume back is nullptr");
        return ERR_INVALID_PARAM;
    }

    std::lock_guard<std::mutex> lock(volumeCallbackMutex_);
    volumeKeyEventListenerStub_ = new(std::nothrow) AudioVolumeKeyEventCallbackStub();
    if (volumeKeyEventListenerStub_ == nullptr) {
        AUDIO_ERR_LOG("SetVolumeKeyEventCallback: object null");
        return ERROR;
    }
    volumeKeyEventListenerStub_->SetOnVolumeKeyEventCallback(callback);

    sptr<IRemoteObject> object = volumeKeyEventListenerStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("SetVolumeKeyEventCallback: volumeKeyEventListenerStub_->AsObject is nullptr.");
        return ERROR;
    }
    return gsp->SetVolumeKeyEventCallback(clientPid, object);
}

int32_t AudioPolicyManager::UnsetVolumeKeyEventCallback(const int32_t clientPid)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("UnsetVolumeKeyEventCallback: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->UnsetVolumeKeyEventCallback(clientPid);
}

int32_t AudioPolicyManager::RegisterAudioRendererEventListener(const int32_t clientUID,
    const std::shared_ptr<AudioRendererStateChangeCallback> &callback)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioRendererEventListener: audio policy manager proxy is NULL.");
        return ERROR;
    }

    AUDIO_INFO_LOG("AudioPolicyManager::RegisterAudioRendererEventListener");
    if (callback == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioRendererEventListener: RendererEvent Listener callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    std::unique_lock<std::mutex> lock(stateChangelistenerStubMutex_);
    rendererStateChangelistenerStub_ = new(std::nothrow) AudioRendererStateChangeListenerStub();
    if (rendererStateChangelistenerStub_ == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioRendererEventListener: object null");
        return ERROR;
    }

    rendererStateChangelistenerStub_->SetCallback(callback);

    sptr<IRemoteObject> object = rendererStateChangelistenerStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioRendererEventListener:RenderStateChangeListener IPC object creation failed");
        return ERROR;
    }
    lock.unlock();

    return gsp->RegisterAudioRendererEventListener(clientUID, object);
}

int32_t AudioPolicyManager::UnregisterAudioRendererEventListener(const int32_t clientUID)
{
    AUDIO_INFO_LOG("AudioPolicyManager::UnregisterAudioRendererEventListener");
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("UnregisterAudioRendererEventListener: audio policy manager proxy is NULL.");
        return ERROR;
    }
    return gsp->UnregisterAudioRendererEventListener(clientUID);
}

int32_t AudioPolicyManager::RegisterAudioCapturerEventListener(const int32_t clientUID,
    const std::shared_ptr<AudioCapturerStateChangeCallback> &callback)
{
    AUDIO_INFO_LOG("AudioPolicyManager::RegisterAudioCapturerEventListener");
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioCapturerEventListener: audio policy manager proxy is NULL.");
        return ERROR;
    }

    if (callback == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioCapturerEventListener: Capturer Event Listener callback is nullptr");
        return ERR_INVALID_PARAM;
    }

    std::unique_lock<std::mutex> lock(stateChangelistenerStubMutex_);
    capturerStateChangelistenerStub_ = new(std::nothrow) AudioCapturerStateChangeListenerStub();
    if (capturerStateChangelistenerStub_ == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioCapturerEventListener: object null");
        return ERROR;
    }

    capturerStateChangelistenerStub_->SetCallback(callback);

    sptr<IRemoteObject> object = capturerStateChangelistenerStub_->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("RegisterAudioCapturerEventListener: CapturerStateChangeListener IPC object creation failed");
        return ERROR;
    }
    lock.unlock();

    return gsp->RegisterAudioCapturerEventListener(clientUID, object);
}

int32_t AudioPolicyManager::UnregisterAudioCapturerEventListener(const int32_t clientUID)
{
    AUDIO_INFO_LOG("AudioPolicyManager::UnregisterAudioCapturerEventListener");
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("UnregisterAudioCapturerEventListener: audio policy manager proxy is NULL.");
        return ERROR;
    }
    return gsp->UnregisterAudioCapturerEventListener(clientUID);
}

int32_t AudioPolicyManager::RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
    const std::shared_ptr<AudioClientTracker> &clientTrackerObj)
{
    AUDIO_INFO_LOG("AudioPolicyManager::RegisterTracker");
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("RegisterTracker: audio policy manager proxy is NULL.");
        return ERROR;
    }

    std::unique_lock<std::mutex> lock(clientTrackerStubMutex_);
    clientTrackerCbStub_ = new(std::nothrow) AudioClientTrackerCallbackStub();
    if (clientTrackerCbStub_ == nullptr) {
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

    return gsp->RegisterTracker(mode, streamChangeInfo, object);
}

int32_t AudioPolicyManager::UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo)
{
    AUDIO_INFO_LOG("AudioPolicyManager::UpdateTracker");
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("UpdateTracker: audio policy manager proxy is NULL.");
        return ERROR;
    }
    return gsp->UpdateTracker(mode, streamChangeInfo);
}

bool AudioPolicyManager::VerifyClientPermission(const std::string &permissionName,
    uint32_t appTokenId, int32_t appUid, bool privacyFlag, AudioPermissionState state)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("VerifyClientPermission: audio policy manager proxy is NULL.");
        return false;
    }
    return gsp->VerifyClientPermission(permissionName, appTokenId, appUid, privacyFlag, state);
}

bool AudioPolicyManager::getUsingPemissionFromPrivacy(const std::string &permissionName, uint32_t appTokenId,
    AudioPermissionState state)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("getUsingPemissionFromPrivacy: audio policy manager proxy is NULL.");
        return false;
    }
    return gsp->getUsingPemissionFromPrivacy(permissionName, appTokenId, state);
}

int32_t AudioPolicyManager::ReconfigureAudioChannel(const uint32_t &count, DeviceType deviceType)
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("ReconfigureAudioChannel: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->ReconfigureAudioChannel(count, deviceType);
}

int32_t AudioPolicyManager::GetAudioLatencyFromXml()
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetAudioLatencyFromXml: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->GetAudioLatencyFromXml();
}

uint32_t AudioPolicyManager::GetSinkLatencyFromXml()
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetSinkLatencyFromXml: audio policy manager proxy is NULL.");
        return 0;
    }
    return gsp->GetSinkLatencyFromXml();
}

int32_t AudioPolicyManager::GetCurrentRendererChangeInfos(
    vector<unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos)
{
    AUDIO_DEBUG_LOG("AudioPolicyManager::GetCurrentRendererChangeInfos");
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetCurrentRendererChangeInfos: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->GetCurrentRendererChangeInfos(audioRendererChangeInfos);
}

int32_t AudioPolicyManager::GetCurrentCapturerChangeInfos(
    vector<unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    AUDIO_DEBUG_LOG("AudioPolicyManager::GetCurrentCapturerChangeInfos");
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetCurrentCapturerChangeInfos: audio policy manager proxy is NULL.");
        return ERROR;
    }
    return gsp->GetCurrentCapturerChangeInfos(audioCapturerChangeInfos);
}

int32_t AudioPolicyManager::UpdateStreamState(const int32_t clientUid,
    StreamSetState streamSetState, AudioStreamType audioStreamType)
{
    AUDIO_DEBUG_LOG("AudioPolicyManager::UpdateStreamState");
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("UpdateStreamState: audio policy manager proxy is NULL.");
        return ERROR;
    }
    return  gsp->UpdateStreamState(clientUid, streamSetState, audioStreamType);
}

std::vector<sptr<VolumeGroupInfo>> AudioPolicyManager::GetVolumeGroupInfos()
{
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("GetVolumeGroupInfos failed, g_apProxy is nullptr.");
        std::vector<sptr<VolumeGroupInfo>> volumeGroupInfos = {};
        return volumeGroupInfos;
    }
    return gsp->GetVolumeGroupInfos();
}

bool AudioPolicyManager::IsAudioRendererLowLatencySupported(const AudioStreamInfo &audioStreamInfo)
{
    AUDIO_DEBUG_LOG("AudioPolicyManager::IsAudioRendererLowLatencySupported");
    const sptr<IAudioPolicy> gsp = GetAudioPolicyManagerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("IsAudioRendererLowLatencySupported: audio policy manager proxy is NULL.");
        return -1;
    }
    return gsp->IsAudioRendererLowLatencySupported(audioStreamInfo);
}
} // namespace AudioStandard
} // namespace OHOS
