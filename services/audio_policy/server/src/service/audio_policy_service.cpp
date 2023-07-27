/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "audio_policy_service.h"

#include "ipc_skeleton.h"
#include "hisysevent.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#include "parameter.h"

#include "audio_errors.h"
#include "audio_log.h"
#include "audio_utils.h"
#include "audio_focus_parser.h"
#include "audio_manager_listener_stub.h"
#include "datashare_helper.h"
#include "datashare_predicates.h"
#include "datashare_result_set.h"
#include "data_share_observer_callback.h"
#include "device_manager.h"
#include "device_init_callback.h"
#include "device_manager_impl.h"
#include "uri.h"

#ifdef BLUETOOTH_ENABLE
#include "audio_server_death_recipient.h"
#include "audio_bluetooth_manager.h"
#endif

namespace OHOS {
namespace AudioStandard {
using namespace std;

static const std::string INNER_CAPTURER_SINK_NAME = "InnerCapturer";
static const std::string RECEIVER_SINK_NAME = "Receiver";
static const std::string SINK_NAME_FOR_CAPTURE_SUFFIX = "_CAP";
static const std::string MONITOR_SOURCE_SUFFIX = ".monitor";

static const std::string SETTINGS_DATA_BASE_URI =
    "datashare:///com.ohos.settingsdata/entry/settingsdata/SETTINGSDATA?Proxy=true";
static const std::string SETTINGS_DATA_FIELD_KEYWORD = "KEYWORD";
static const std::string SETTINGS_DATA_FIELD_VALUE = "VALUE";
static const std::string PREDICATES_STRING = "settings.general.device_name";
const uint32_t PCM_8_BIT = 8;
const uint32_t PCM_16_BIT = 16;
const uint32_t PCM_24_BIT = 24;
const uint32_t PCM_32_BIT = 32;
const uint32_t BT_BUFFER_ADJUSTMENT_FACTOR = 50;
const std::string AUDIO_SERVICE_PKG = "audio_manager_service";
const uint32_t PRIORITY_LIST_OFFSET_POSTION = 1;
std::shared_ptr<DataShare::DataShareHelper> g_dataShareHelper = nullptr;
static sptr<IStandardAudioService> g_adProxy = nullptr;
#ifdef BLUETOOTH_ENABLE
static sptr<IStandardAudioService> g_btProxy = nullptr;
#endif
static int32_t startDeviceId = 1;
mutex g_adProxyMutex;
mutex g_dataShareHelperMutex;
#ifdef BLUETOOTH_ENABLE
mutex g_btProxyMutex;
#endif

AudioPolicyService::~AudioPolicyService()
{
    AUDIO_ERR_LOG("~AudioPolicyService()");
    Deinit();
}

bool AudioPolicyService::Init(void)
{
    AUDIO_INFO_LOG("AudioPolicyService init");
    serviceFlag_.reset();
    audioPolicyManager_.Init();
    audioEffectManager_.EffectManagerInit();

    if (!configParser_.LoadConfiguration()) {
        AUDIO_ERR_LOG("Audio Config Load Configuration failed");
        return false;
    }
    if (!configParser_.Parse()) {
        AUDIO_ERR_LOG("Audio Config Parse failed");
        return false;
    }

#ifdef FEATURE_DTMF_TONE
    std::unique_ptr<AudioToneParser> audioToneParser = make_unique<AudioToneParser>();
    CHECK_AND_RETURN_RET_LOG(audioToneParser != nullptr, false, "Failed to create AudioToneParser");
    std::string AUDIO_TONE_CONFIG_FILE = "system/etc/audio/audio_tone_dtmf_config.xml";

    if (audioToneParser->LoadConfig(toneDescriptorMap)) {
        AUDIO_ERR_LOG("Audio Tone Load Configuration failed");
        return false;
    }
#endif

    std::unique_ptr<AudioFocusParser> audioFocusParser = make_unique<AudioFocusParser>();
    CHECK_AND_RETURN_RET_LOG(audioFocusParser != nullptr, false, "Failed to create AudioFocusParser");
    std::string AUDIO_FOCUS_CONFIG_FILE = "system/etc/audio/audio_interrupt_policy_config.xml";

    if (audioFocusParser->LoadConfig(focusMap_)) {
        AUDIO_ERR_LOG("Failed to load audio interrupt configuration!");
        return false;
    }
    AUDIO_INFO_LOG("Audio interrupt configuration has been loaded. FocusMap.size: %{public}zu", focusMap_.size());

    if (deviceStatusListener_->RegisterDeviceStatusListener()) {
        AUDIO_ERR_LOG("[Policy Service] Register for device status events failed");
        return false;
    }

    RegisterRemoteDevStatusCallback();

    // Get device type from const.product.devicetype when starting.
    char devicesType[100] = {0}; // 100 for system parameter usage
    (void)GetParameter("const.product.devicetype", " ", devicesType, sizeof(devicesType));
    localDevicesType_ = devicesType;

    if (policyVolumeMap_ == nullptr) {
        size_t mapSize = IPolicyProvider::GetVolumeVectorSize() * sizeof(Volume);
        AUDIO_INFO_LOG("InitSharedVolume create shared volume map with size %{public}zu", mapSize);
        policyVolumeMap_ = AudioSharedMemory::CreateFormLocal(mapSize, "PolicyVolumeMap");
        CHECK_AND_RETURN_RET_LOG(policyVolumeMap_ != nullptr && policyVolumeMap_->GetBase() != nullptr,
            false, "Get shared memory failed!");
        volumeVector_ = reinterpret_cast<Volume *>(policyVolumeMap_->GetBase());
    }
    return true;
}

const sptr<IStandardAudioService> AudioPolicyService::GetAudioServerProxy()
{
    AUDIO_DEBUG_LOG("[Policy Service] Start get audio policy service proxy.");
    lock_guard<mutex> lock(g_adProxyMutex);

    if (g_adProxy == nullptr) {
        auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        if (samgr == nullptr) {
            AUDIO_ERR_LOG("[Policy Service] Get samgr failed.");
            return nullptr;
        }

        sptr<IRemoteObject> object = samgr->GetSystemAbility(AUDIO_DISTRIBUTED_SERVICE_ID);
        if (object == nullptr) {
            AUDIO_ERR_LOG("[Policy Service] audio service remote object is NULL.");
            return nullptr;
        }

        g_adProxy = iface_cast<IStandardAudioService>(object);
        if (g_adProxy == nullptr) {
            AUDIO_ERR_LOG("[Policy Service] init g_adProxy is NULL.");
            return nullptr;
        }
    }
    const sptr<IStandardAudioService> gsp = g_adProxy;
    return gsp;
}

void AudioPolicyService::InitKVStore()
{
    audioPolicyManager_.InitKVStore();
}

bool AudioPolicyService::ConnectServiceAdapter()
{
    if (!audioPolicyManager_.ConnectServiceAdapter()) {
        AUDIO_ERR_LOG("AudioPolicyService::ConnectServiceAdapter  Error in connecting to audio service adapter");
        return false;
    }

    OnServiceConnected(AudioServiceIndex::AUDIO_SERVICE_INDEX);

    return true;
}

void AudioPolicyService::Deinit(void)
{
    AUDIO_ERR_LOG("Policy service died. closing active ports");
    std::for_each(IOHandles_.begin(), IOHandles_.end(), [&](std::pair<std::string, AudioIOHandle> handle) {
        audioPolicyManager_.CloseAudioPort(handle.second);
    });

    IOHandles_.clear();
#ifdef ACCESSIBILITY_ENABLE
    accessibilityConfigListener_->UnsubscribeObserver();
#endif
    deviceStatusListener_->UnRegisterDeviceStatusListener();

    if (isBtListenerRegistered) {
        UnregisterBluetoothListener();
    }
    volumeVector_ = nullptr;
    policyVolumeMap_ = nullptr;

    return;
}

int32_t AudioPolicyService::SetAudioSessionCallback(AudioSessionCallback *callback)
{
    return audioPolicyManager_.SetAudioSessionCallback(callback);
}

int32_t AudioPolicyService::GetMaxVolumeLevel(AudioVolumeType volumeType) const
{
    if (volumeType == STREAM_ALL) {
        volumeType = STREAM_MUSIC;
    }
    return audioPolicyManager_.GetMaxVolumeLevel(volumeType);
}

int32_t AudioPolicyService::GetMinVolumeLevel(AudioVolumeType volumeType) const
{
    if (volumeType == STREAM_ALL) {
        volumeType = STREAM_MUSIC;
    }
    return audioPolicyManager_.GetMinVolumeLevel(volumeType);
}

int32_t AudioPolicyService::SetSystemVolumeLevel(AudioStreamType streamType, int32_t volumeLevel, bool isFromVolumeKey)
{
    int32_t result = audioPolicyManager_.SetSystemVolumeLevel(streamType, volumeLevel, isFromVolumeKey);
    if (result == SUCCESS && streamType == STREAM_VOICE_CALL) {
        SetVoiceCallVolume(volumeLevel);
    }
    // todo
    Volume vol = {false, 1.0f, 0};
    vol.volumeFloat = GetSystemVolumeInDb(streamType, volumeLevel, currentActiveDevice_);
    SetSharedVolume(streamType, currentActiveDevice_, vol);
    return result;
}

void AudioPolicyService::SetVoiceCallVolume(int32_t volumeLevel)
{
    // set voice volume by the interface from hdi.
    if (volumeLevel == 0) {
        AUDIO_ERR_LOG("SetVoiceVolume: volume of voice_call cannot be set to 0");
        return;
    }
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetVoiceVolume: gsp null");
        return;
    }
    float volumeDb = static_cast<float>(volumeLevel) /
        static_cast<float>(audioPolicyManager_.GetMaxVolumeLevel(STREAM_VOICE_CALL));
    gsp->SetVoiceVolume(volumeDb);
    AUDIO_INFO_LOG("SetVoiceVolume: %{public}f", volumeDb);
}

void AudioPolicyService::SetVolumeForSwitchDevice(DeviceType deviceType)
{
    // Load volume from KvStore and set volume for each stream type
    audioPolicyManager_.SetVolumeForSwitchDevice(deviceType);

    // The volume of voice_call needs to be adjusted separately
    SetVoiceCallVolume(GetSystemVolumeLevel(STREAM_VOICE_CALL));
}


int32_t AudioPolicyService::GetSystemVolumeLevel(AudioStreamType streamType, bool isFromVolumeKey) const
{
    return audioPolicyManager_.GetSystemVolumeLevel(streamType, isFromVolumeKey);
}

float AudioPolicyService::GetSystemVolumeDb(AudioStreamType streamType) const
{
    return audioPolicyManager_.GetSystemVolumeDb(streamType);
}

int32_t AudioPolicyService::SetLowPowerVolume(int32_t streamId, float volume) const
{
    return streamCollector_.SetLowPowerVolume(streamId, volume);
}

float AudioPolicyService::GetLowPowerVolume(int32_t streamId) const
{
    return streamCollector_.GetLowPowerVolume(streamId);
}

float AudioPolicyService::GetSingleStreamVolume(int32_t streamId) const
{
    return streamCollector_.GetSingleStreamVolume(streamId);
}

int32_t AudioPolicyService::SetStreamMute(AudioStreamType streamType, bool mute) const
{
    return audioPolicyManager_.SetStreamMute(streamType, mute);
}

int32_t AudioPolicyService::SetSourceOutputStreamMute(int32_t uid, bool setMute) const
{
    return audioPolicyManager_.SetSourceOutputStreamMute(uid, setMute);
}


bool AudioPolicyService::GetStreamMute(AudioStreamType streamType) const
{
    return audioPolicyManager_.GetStreamMute(streamType);
}

inline std::string PrintSinkInput(SinkInput sinkInput)
{
    std::stringstream value;
    value << "streamId:[" << sinkInput.streamId << "] ";
    value << "streamType:[" << sinkInput.streamType << "] ";
    value << "uid:[" << sinkInput.uid << "] ";
    value << "pid:[" << sinkInput.pid << "] ";
    value << "statusMark:[" << sinkInput.statusMark << "] ";
    value << "sinkName:[" << sinkInput.sinkName << "] ";
    value << "startTime:[" << sinkInput.startTime << "]";
    return value.str();
}

inline std::string GetRemoteModuleName(std::string networkId, DeviceRole role)
{
    return networkId + (role == DeviceRole::OUTPUT_DEVICE ? "_out" : "_in");
}

std::string AudioPolicyService::GetSelectedDeviceInfo(int32_t uid, int32_t pid, AudioStreamType streamType)
{
    (void)streamType;

    std::lock_guard<std::mutex> lock(routerMapMutex_);
    if (!routerMap_.count(uid)) {
        AUDIO_INFO_LOG("GetSelectedDeviceInfo no such uid[%{public}d]", uid);
        return "";
    }
    std::string selectedDevice = "";
    if (routerMap_[uid].second == pid) {
        selectedDevice = routerMap_[uid].first;
    } else if (routerMap_[uid].second == G_UNKNOWN_PID) {
        routerMap_[uid].second = pid;
        selectedDevice = routerMap_[uid].first;
    } else {
        AUDIO_INFO_LOG("GetSelectedDeviceInfo: uid[%{public}d] changed pid, get local as defalut", uid);
        routerMap_.erase(uid);
        selectedDevice = LOCAL_NETWORK_ID;
    }

    if (LOCAL_NETWORK_ID == selectedDevice) {
        AUDIO_INFO_LOG("GetSelectedDeviceInfo: uid[%{public}d]-->local.", uid);
        return "";
    }
    // check if connected.
    bool isConnected = false;
    for (auto device : connectedDevices_) {
        if (GetRemoteModuleName(device->networkId_, device->deviceRole_) == selectedDevice) {
            isConnected = true;
            break;
        }
    }

    if (isConnected) {
        AUDIO_INFO_LOG("GetSelectedDeviceInfo result[%{public}s]", selectedDevice.c_str());
        return selectedDevice;
    } else {
        routerMap_.erase(uid);
        AUDIO_INFO_LOG("GetSelectedDeviceInfo device already disconnected.");
        return "";
    }
}

void AudioPolicyService::NotifyRemoteRenderState(std::string networkId, std::string condition, std::string value)
{
    AUDIO_INFO_LOG("NotifyRemoteRenderState device<%{public}s> condition:%{public}s value:%{public}s",
        networkId.c_str(), condition.c_str(), value.c_str());

    vector<SinkInput> sinkInputs = audioPolicyManager_.GetAllSinkInputs();
    vector<SinkInput> targetSinkInputs = {};
    for (auto sinkInput : sinkInputs) {
        if (sinkInput.sinkName == networkId) {
            targetSinkInputs.push_back(sinkInput);
        }
    }
    AUDIO_INFO_LOG("NotifyRemoteRenderState move [%{public}zu] of all [%{public}zu]sink-inputs to local.",
        targetSinkInputs.size(), sinkInputs.size());
    sptr<AudioDeviceDescriptor> localDevice = new(std::nothrow) AudioDeviceDescriptor();
    if (localDevice == nullptr) {
        AUDIO_ERR_LOG("Device error: null device.");
        return;
    }
    localDevice->networkId_ = LOCAL_NETWORK_ID;
    localDevice->deviceRole_ = DeviceRole::OUTPUT_DEVICE;
    localDevice->deviceType_ = DeviceType::DEVICE_TYPE_SPEAKER;

    int32_t ret = MoveToLocalOutputDevice(targetSinkInputs, localDevice);
    CHECK_AND_RETURN_LOG((ret == SUCCESS), "MoveToLocalOutputDevice failed!");

    // Suspend device, notify audio stream manager that device has been changed.
    ret = audioPolicyManager_.SuspendAudioDevice(networkId, true);
    CHECK_AND_RETURN_LOG((ret == SUCCESS), "SuspendAudioDevice failed!");

    std::vector<sptr<AudioDeviceDescriptor>> desc = {};
    desc.push_back(localDevice);
    UpdateTrackerDeviceChange(desc);
    OnPreferOutputDeviceUpdated(currentActiveDevice_, LOCAL_NETWORK_ID);
    AUDIO_INFO_LOG("NotifyRemoteRenderState success");
}

bool AudioPolicyService::IsDeviceConnected(sptr<AudioDeviceDescriptor> &audioDeviceDescriptors) const
{
    size_t connectedDevicesNum = connectedDevices_.size();
    for (size_t i = 0; i < connectedDevicesNum; i++) {
        if (connectedDevices_[i] != nullptr) {
            if (connectedDevices_[i]->deviceRole_ == audioDeviceDescriptors->deviceRole_
                && connectedDevices_[i]->deviceType_ == audioDeviceDescriptors->deviceType_
                && connectedDevices_[i]->interruptGroupId_ == audioDeviceDescriptors->interruptGroupId_
                && connectedDevices_[i]->volumeGroupId_ == audioDeviceDescriptors->volumeGroupId_
                && connectedDevices_[i]->networkId_ == audioDeviceDescriptors->networkId_) {
                return true;
            }
        }
    }
    return false;
}

int32_t AudioPolicyService::DeviceParamsCheck(DeviceRole targetRole,
    std::vector<sptr<AudioDeviceDescriptor>> &audioDeviceDescriptors) const
{
    size_t targetSize = audioDeviceDescriptors.size();
    if (targetSize != 1) {
        AUDIO_ERR_LOG("Device error: size[%{public}zu]", targetSize);
        return ERR_INVALID_OPERATION;
    }

    bool isDeviceTypeCorrect = false;
    if (targetRole == DeviceRole::OUTPUT_DEVICE) {
        isDeviceTypeCorrect = IsOutputDevice(audioDeviceDescriptors[0]->deviceType_) &&
            IsDeviceConnected(audioDeviceDescriptors[0]);
    } else if (targetRole == DeviceRole::INPUT_DEVICE) {
        isDeviceTypeCorrect = IsInputDevice(audioDeviceDescriptors[0]->deviceType_) &&
            IsDeviceConnected(audioDeviceDescriptors[0]);
    }

    if (audioDeviceDescriptors[0]->deviceRole_ != targetRole || !isDeviceTypeCorrect) {
        AUDIO_ERR_LOG("Device error: size[%{public}zu] deviceRole[%{public}d]", targetSize,
            static_cast<int32_t>(audioDeviceDescriptors[0]->deviceRole_));
        return ERR_INVALID_OPERATION;
    }
    return SUCCESS;
}

int32_t AudioPolicyService::SelectOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors)
{
    AUDIO_INFO_LOG("SelectOutputDevice start for uid[%{public}d]", audioRendererFilter->uid);
    // check size == 1 && output device
    int32_t res = DeviceParamsCheck(DeviceRole::OUTPUT_DEVICE, audioDeviceDescriptors);
    CHECK_AND_RETURN_RET_LOG(res == SUCCESS, res, "DeviceParamsCheck no success");

    std::string networkId = audioDeviceDescriptors[0]->networkId_;
    DeviceType deviceType = audioDeviceDescriptors[0]->deviceType_;

    // switch between local devices
    if (!isCurrentRemoteRenderer && networkId == LOCAL_NETWORK_ID && currentActiveDevice_ != deviceType) {
        if (deviceType == DeviceType::DEVICE_TYPE_DEFAULT) {
            deviceType = FetchHighPriorityDevice(true);
        }
        return SelectNewDevice(DeviceRole::OUTPUT_DEVICE, deviceType);
    }

    int32_t targetUid = audioRendererFilter->uid;
    AudioStreamType targetStreamType = audioRendererFilter->streamType;
    // move all sink-input.
    bool moveAll = false;
    if (targetUid == -1) {
        AUDIO_INFO_LOG("Move all sink inputs.");
        moveAll = true;
        std::lock_guard<std::mutex> lock(routerMapMutex_);
        routerMap_.clear();
    }

    // find sink-input id with audioRendererFilter
    std::vector<SinkInput> targetSinkInputs = {};
    vector<SinkInput> sinkInputs = audioPolicyManager_.GetAllSinkInputs();

    for (size_t i = 0; i < sinkInputs.size(); i++) {
        if (sinkInputs[i].uid == dAudioClientUid) {
            AUDIO_INFO_LOG("Find sink-input with daudio[%{public}d]", sinkInputs[i].pid);
            continue;
        }
        if (sinkInputs[i].streamType == STREAM_DEFAULT) {
            AUDIO_INFO_LOG("Sink-input[%{public}zu] of effect sink, don't move", i);
            continue;
        }
        AUDIO_DEBUG_LOG("sinkinput[%{public}zu]:%{public}s", i, PrintSinkInput(sinkInputs[i]).c_str());
        if (moveAll || (targetUid == sinkInputs[i].uid && targetStreamType == sinkInputs[i].streamType)) {
            targetSinkInputs.push_back(sinkInputs[i]);
        }
    }

    // move target uid, but no stream played yet, record the routing info for first start.
    if (!moveAll && targetSinkInputs.size() == 0) {
        return RememberRoutingInfo(audioRendererFilter, audioDeviceDescriptors[0]);
    }

    int32_t ret = SUCCESS;
    ret = (networkId == LOCAL_NETWORK_ID) ? MoveToLocalOutputDevice(targetSinkInputs, audioDeviceDescriptors[0]):
                                            MoveToRemoteOutputDevice(targetSinkInputs, audioDeviceDescriptors[0]);
    UpdateTrackerDeviceChange(audioDeviceDescriptors);
    OnPreferOutputDeviceUpdated(currentActiveDevice_, networkId);
    AUDIO_INFO_LOG("SelectOutputDevice result[%{public}d], [%{public}zu] moved.", ret, targetSinkInputs.size());
    return ret;
}


int32_t AudioPolicyService::RememberRoutingInfo(sptr<AudioRendererFilter> audioRendererFilter,
    sptr<AudioDeviceDescriptor> deviceDescriptor)
{
    AUDIO_INFO_LOG("RememberRoutingInfo for uid[%{public}d] device[%{public}s]", audioRendererFilter->uid,
        deviceDescriptor->networkId_.c_str());
    if (deviceDescriptor->networkId_ == LOCAL_NETWORK_ID) {
        std::lock_guard<std::mutex> lock(routerMapMutex_);
        routerMap_[audioRendererFilter->uid] = std::pair(LOCAL_NETWORK_ID, G_UNKNOWN_PID);
        return SUCCESS;
    }
    // remote device.
    std::string networkId = deviceDescriptor->networkId_;
    DeviceRole deviceRole = deviceDescriptor->deviceRole_;

    std::string moduleName = GetRemoteModuleName(networkId, deviceRole);
    if (!IOHandles_.count(moduleName)) {
        AUDIO_ERR_LOG("Device error: no such device:%{public}s", networkId.c_str());
        return ERR_INVALID_PARAM;
    }
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    CHECK_AND_RETURN_RET_LOG(gsp != nullptr, ERR_OPERATION_FAILED, "Service proxy unavailable");
    int32_t ret = gsp->CheckRemoteDeviceState(networkId, deviceRole, true);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ERR_OPERATION_FAILED, "remote device state is invalid!");

    std::lock_guard<std::mutex> lock(routerMapMutex_);
    routerMap_[audioRendererFilter->uid] = std::pair(moduleName, G_UNKNOWN_PID);
    return SUCCESS;
}

int32_t AudioPolicyService::MoveToLocalOutputDevice(std::vector<SinkInput> sinkInputIds,
    sptr<AudioDeviceDescriptor> localDeviceDescriptor)
{
    AUDIO_INFO_LOG("MoveToLocalOutputDevice for [%{public}zu] sink-inputs", sinkInputIds.size());
    // check
    if (LOCAL_NETWORK_ID != localDeviceDescriptor->networkId_) {
        AUDIO_ERR_LOG("MoveToLocalOutputDevice failed: not a local device.");
        return ERR_INVALID_OPERATION;
    }

    DeviceType localDeviceType = localDeviceDescriptor->deviceType_;
    if (localDeviceType != currentActiveDevice_) {
        AUDIO_WARNING_LOG("MoveToLocalOutputDevice: device[%{public}d] not active, use device[%{public}d] instead.",
            static_cast<int32_t>(localDeviceType), static_cast<int32_t>(currentActiveDevice_));
    }

    // start move.
    uint32_t sinkId = -1; // invalid sink id, use sink name instead.
    std::string sinkName = GetPortName(currentActiveDevice_);
    for (size_t i = 0; i < sinkInputIds.size(); i++) {
        if (audioPolicyManager_.MoveSinkInputByIndexOrName(sinkInputIds[i].paStreamId, sinkId, sinkName) != SUCCESS) {
            AUDIO_ERR_LOG("move [%{public}d] to local failed", sinkInputIds[i].streamId);
            return ERROR;
        }
        std::lock_guard<std::mutex> lock(routerMapMutex_);
        routerMap_[sinkInputIds[i].uid] = std::pair(LOCAL_NETWORK_ID, sinkInputIds[i].pid);
    }

    isCurrentRemoteRenderer = false;
    return SUCCESS;
}

int32_t AudioPolicyService::OpenRemoteAudioDevice(std::string networkId, DeviceRole deviceRole, DeviceType deviceType,
    sptr<AudioDeviceDescriptor> remoteDeviceDescriptor)
{
    // open the test device. We should open it when device is online.
    std::string moduleName = GetRemoteModuleName(networkId, deviceRole);
    AudioModuleInfo remoteDeviceInfo = ConstructRemoteAudioModuleInfo(networkId, deviceRole, deviceType);
    AudioIOHandle remoteIOIdx = audioPolicyManager_.OpenAudioPort(remoteDeviceInfo);
    AUDIO_DEBUG_LOG("OpenAudioPort remoteIOIdx %{public}d", remoteIOIdx);
    CHECK_AND_RETURN_RET_LOG(remoteIOIdx != OPEN_PORT_FAILURE, ERR_INVALID_HANDLE, "OpenAudioPort failed %{public}d",
        remoteIOIdx);
    IOHandles_[moduleName] = remoteIOIdx;

    // If device already in list, remove it else do not modify the list.
    auto isPresent = [&deviceType, &networkId] (const sptr<AudioDeviceDescriptor> &descriptor) {
        return descriptor->deviceType_ == deviceType && descriptor->networkId_ == networkId;
    };
    connectedDevices_.erase(std::remove_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent),
        connectedDevices_.end());
    UpdateDisplayName(remoteDeviceDescriptor);
    connectedDevices_.insert(connectedDevices_.begin(), remoteDeviceDescriptor);
    return SUCCESS;
}

int32_t AudioPolicyService::MoveToRemoteOutputDevice(std::vector<SinkInput> sinkInputIds,
    sptr<AudioDeviceDescriptor> remoteDeviceDescriptor)
{
    AUDIO_INFO_LOG("MoveToRemoteOutputDevice for [%{public}zu] sink-inputs", sinkInputIds.size());

    std::string networkId = remoteDeviceDescriptor->networkId_;
    DeviceRole deviceRole = remoteDeviceDescriptor->deviceRole_;
    DeviceType deviceType = remoteDeviceDescriptor->deviceType_;

    if (networkId == LOCAL_NETWORK_ID) { // check: networkid
        AUDIO_ERR_LOG("MoveToRemoteOutputDevice failed: not a remote device.");
        return ERR_INVALID_OPERATION;
    }

    uint32_t sinkId = -1; // invalid sink id, use sink name instead.
    std::string moduleName = GetRemoteModuleName(networkId, deviceRole);
    if (IOHandles_.count(moduleName)) {
        IOHandles_[moduleName]; // mIOHandle is module id, not equal to sink id.
    } else {
        AUDIO_ERR_LOG("no such device.");
        if (!isOpenRemoteDevice) {
            return ERR_INVALID_PARAM;
        } else {
            return OpenRemoteAudioDevice(networkId, deviceRole, deviceType, remoteDeviceDescriptor);
        }
    }

    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    CHECK_AND_RETURN_RET_LOG(gsp != nullptr, ERR_OPERATION_FAILED, "Service proxy unavailable");
    CHECK_AND_RETURN_RET_LOG((gsp->CheckRemoteDeviceState(networkId, deviceRole, true) == SUCCESS),
        ERR_OPERATION_FAILED, "remote device state is invalid!");

    // start move.
    for (size_t i = 0; i < sinkInputIds.size(); i++) {
        if (audioPolicyManager_.MoveSinkInputByIndexOrName(sinkInputIds[i].paStreamId,
            sinkId, moduleName) != SUCCESS) {
            AUDIO_ERR_LOG("move [%{public}d] failed", sinkInputIds[i].streamId);
            return ERROR;
        }
        std::lock_guard<std::mutex> lock(routerMapMutex_);
        routerMap_[sinkInputIds[i].uid] = std::pair(moduleName, sinkInputIds[i].pid);
    }

    if (deviceType != DeviceType::DEVICE_TYPE_DEFAULT) {
        AUDIO_WARNING_LOG("Not defult type[%{public}d] on device:[%{public}s]", deviceType, networkId.c_str());
    }
    isCurrentRemoteRenderer = true;
    return SUCCESS;
}

inline std::string PrintSourceOutput(SourceOutput sourceOutput)
{
    std::stringstream value;
    value << "streamId:[" << sourceOutput.streamId << "] ";
    value << "streamType:[" << sourceOutput.streamType << "] ";
    value << "uid:[" << sourceOutput.uid << "] ";
    value << "pid:[" << sourceOutput.pid << "] ";
    value << "statusMark:[" << sourceOutput.statusMark << "] ";
    value << "deviceSourceId:[" << sourceOutput.deviceSourceId << "] ";
    value << "startTime:[" << sourceOutput.startTime << "]";
    return value.str();
}

int32_t AudioPolicyService::SelectInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors)
{
    AUDIO_INFO_LOG("Select input device start for uid[%{public}d]", audioCapturerFilter->uid);
    // check size == 1 && input device
    int32_t res = DeviceParamsCheck(DeviceRole::INPUT_DEVICE, audioDeviceDescriptors);
    if (res != SUCCESS) {
        return res;
    }

    std::string networkId = audioDeviceDescriptors[0]->networkId_;
    DeviceType deviceType = audioDeviceDescriptors[0]->deviceType_;

    // switch between local devices
    if (LOCAL_NETWORK_ID == networkId && activeInputDevice_ != deviceType) {
        if (deviceType == DeviceType::DEVICE_TYPE_DEFAULT) {
            deviceType = FetchHighPriorityDevice(false);
        }
        return SelectNewDevice(DeviceRole::INPUT_DEVICE, deviceType);
    }

    if (!remoteCapturerSwitch_) {
        AUDIO_DEBUG_LOG("remote capturer capbility is not open now.");
        return SUCCESS;
    }
    int32_t targetUid = audioCapturerFilter->uid;
    // move all source-output.
    bool moveAll = false;
    if (targetUid == -1) {
        AUDIO_DEBUG_LOG("Move all source outputs.");
        moveAll = true;
    }

    // find source-output id with audioCapturerFilter
    std::vector<uint32_t> targetSourceOutputIds = {};
    vector<SourceOutput> sourceOutputs = audioPolicyManager_.GetAllSourceOutputs();
    for (size_t i = 0; i < sourceOutputs.size();i++) {
        AUDIO_DEBUG_LOG("SourceOutput[%{public}zu]:%{public}s", i, PrintSourceOutput(sourceOutputs[i]).c_str());
        if (moveAll || (targetUid == sourceOutputs[i].uid)) {
            targetSourceOutputIds.push_back(sourceOutputs[i].paStreamId);
        }
    }

    int32_t ret = SUCCESS;
    if (LOCAL_NETWORK_ID == networkId) {
        ret = MoveToLocalInputDevice(targetSourceOutputIds, audioDeviceDescriptors[0]);
    } else {
        ret = MoveToRemoteInputDevice(targetSourceOutputIds, audioDeviceDescriptors[0]);
    }

    AUDIO_INFO_LOG("SelectInputDevice result[%{public}d]", ret);
    return ret;
}

int32_t AudioPolicyService::MoveToLocalInputDevice(std::vector<uint32_t> sourceOutputIds,
    sptr<AudioDeviceDescriptor> localDeviceDescriptor)
{
    AUDIO_INFO_LOG("MoveToLocalInputDevice start");
    // check
    if (LOCAL_NETWORK_ID != localDeviceDescriptor->networkId_) {
        AUDIO_ERR_LOG("MoveToLocalInputDevice failed: not a local device.");
        return ERR_INVALID_OPERATION;
    }

    DeviceType localDeviceType = localDeviceDescriptor->deviceType_;
    if (localDeviceType != activeInputDevice_) {
        AUDIO_WARNING_LOG("MoveToLocalInputDevice: device[%{public}d] not active, use device[%{public}d] instead.",
            static_cast<int32_t>(localDeviceType), static_cast<int32_t>(activeInputDevice_));
    }

    // start move.
    uint32_t sourceId = -1; // invalid source id, use source name instead.
    std::string sourceName = GetPortName(activeInputDevice_);
    for (size_t i = 0; i < sourceOutputIds.size(); i++) {
        if (audioPolicyManager_.MoveSourceOutputByIndexOrName(sourceOutputIds[i], sourceId, sourceName) != SUCCESS) {
            AUDIO_DEBUG_LOG("move [%{public}d] to local failed", sourceOutputIds[i]);
            return ERROR;
        }
    }

    return SUCCESS;
}

int32_t AudioPolicyService::MoveToRemoteInputDevice(std::vector<uint32_t> sourceOutputIds,
    sptr<AudioDeviceDescriptor> remoteDeviceDescriptor)
{
    AUDIO_INFO_LOG("MoveToRemoteInputDevice start");

    std::string networkId = remoteDeviceDescriptor->networkId_;
    DeviceRole deviceRole = remoteDeviceDescriptor->deviceRole_;
    DeviceType deviceType = remoteDeviceDescriptor->deviceType_;

    if (networkId == LOCAL_NETWORK_ID) { // check: networkid
        AUDIO_ERR_LOG("MoveToRemoteInputDevice failed: not a remote device.");
        return ERR_INVALID_OPERATION;
    }

    uint32_t sourceId = -1; // invalid sink id, use sink name instead.
    std::string moduleName = GetRemoteModuleName(networkId, deviceRole);
    if (IOHandles_.count(moduleName)) {
        IOHandles_[moduleName]; // mIOHandle is module id, not equal to sink id.
    } else {
        AUDIO_ERR_LOG("no such device.");
        if (!isOpenRemoteDevice) {
            return ERR_INVALID_PARAM;
        } else {
            return OpenRemoteAudioDevice(networkId, deviceRole, deviceType, remoteDeviceDescriptor);
        }
    }

    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    CHECK_AND_RETURN_RET_LOG(gsp != nullptr, ERR_OPERATION_FAILED, "Service proxy unavailable");
    CHECK_AND_RETURN_RET_LOG((gsp->CheckRemoteDeviceState(networkId, deviceRole, true) == SUCCESS),
        ERR_OPERATION_FAILED, "remote device state is invalid!");

    // start move.
    for (size_t i = 0; i < sourceOutputIds.size(); i++) {
        if (audioPolicyManager_.MoveSourceOutputByIndexOrName(sourceOutputIds[i], sourceId, moduleName) != SUCCESS) {
            AUDIO_DEBUG_LOG("move [%{public}d] failed", sourceOutputIds[i]);
            return ERROR;
        }
    }

    if (deviceType != DeviceType::DEVICE_TYPE_DEFAULT) {
        AUDIO_DEBUG_LOG("Not defult type[%{public}d] on device:[%{public}s]", deviceType, networkId.c_str());
    }
    return SUCCESS;
}

bool AudioPolicyService::IsStreamActive(AudioStreamType streamType) const
{
    if (streamType == STREAM_VOICE_CALL && audioScene_ == AUDIO_SCENE_PHONE_CALL) {
        return true;
    }
    return audioPolicyManager_.IsStreamActive(streamType);
}

std::string AudioPolicyService::GetPortName(InternalDeviceType deviceType)
{
    std::string portName = PORT_NONE;
    switch (deviceType) {
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
            portName = BLUETOOTH_SPEAKER;
            break;
        case InternalDeviceType::DEVICE_TYPE_EARPIECE:
        case InternalDeviceType::DEVICE_TYPE_SPEAKER:
        case InternalDeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_WIRED_HEADPHONES:
        case InternalDeviceType::DEVICE_TYPE_USB_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
            portName = PRIMARY_SPEAKER;
            break;
        case InternalDeviceType::DEVICE_TYPE_MIC:
            portName = PRIMARY_MIC;
            break;
        case InternalDeviceType::DEVICE_TYPE_WAKEUP:
            portName = PRIMARY_WAKEUP;
            break;
        case InternalDeviceType::DEVICE_TYPE_FILE_SINK:
            portName = FILE_SINK;
            break;
        case InternalDeviceType::DEVICE_TYPE_FILE_SOURCE:
            portName = FILE_SOURCE;
            break;
        default:
            portName = PORT_NONE;
            break;
    }

    AUDIO_INFO_LOG("port name is %{public}s", portName.c_str());
    return portName;
}

// private method
AudioModuleInfo AudioPolicyService::ConstructRemoteAudioModuleInfo(std::string networkId, DeviceRole deviceRole,
    DeviceType deviceType)
{
    AudioModuleInfo audioModuleInfo = {};
    if (deviceRole == DeviceRole::OUTPUT_DEVICE) {
        audioModuleInfo.lib = "libmodule-hdi-sink.z.so";
        audioModuleInfo.format = "s16le"; // 16bit little endian
        audioModuleInfo.fixedLatency = "1"; // here we need to set latency fixed for a fixed buffer size.
    } else if (deviceRole == DeviceRole::INPUT_DEVICE) {
        audioModuleInfo.lib = "libmodule-hdi-source.z.so";
        audioModuleInfo.format = "s16le"; // we assume it is bigger endian
    } else {
        AUDIO_ERR_LOG("Invalid flag provided %{public}d", static_cast<int32_t>(deviceType));
    }

    // used as "sink_name" in hdi_sink.c, hope we could use name to find target sink.
    audioModuleInfo.name = GetRemoteModuleName(networkId, deviceRole);
    audioModuleInfo.networkId = networkId;

    std::stringstream typeValue;
    typeValue << static_cast<int32_t>(deviceType);
    audioModuleInfo.deviceType = typeValue.str();

    audioModuleInfo.adapterName = "remote";
    audioModuleInfo.className = "remote"; // used in renderer_sink_adapter.c
    audioModuleInfo.fileName = "remote_dump_file";

    audioModuleInfo.channels = "2";
    audioModuleInfo.rate = "48000";
    audioModuleInfo.bufferSize = "4096";

    return audioModuleInfo;
}

// private method
AudioModuleInfo AudioPolicyService::ConstructWakeUpAudioModuleInfo(int32_t sourceType)
{
    AudioModuleInfo audioModuleInfo = {};
    audioModuleInfo.lib = "libmodule-hdi-source.z.so";
    audioModuleInfo.format = "s16le";
    audioModuleInfo.name = "Built_in_wakeup";
    audioModuleInfo.networkId = "LocalDevice";

    audioModuleInfo.adapterName = "primary";
    audioModuleInfo.className = "primary";
    audioModuleInfo.fileName = "";

    audioModuleInfo.channels = "1";
    audioModuleInfo.rate = "16000";
    audioModuleInfo.bufferSize = "1280";
    audioModuleInfo.OpenMicSpeaker = "1";

    std::stringstream sourceType_;
    sourceType_ << static_cast<int32_t>(sourceType);
    audioModuleInfo.sourceType = sourceType_.str();

    return audioModuleInfo;
}

void AudioPolicyService::OnPreferOutputDeviceUpdated(DeviceType deviceType, std::string networkId)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    for (auto it = preferOutputDeviceCbsMap_.begin(); it != preferOutputDeviceCbsMap_.end(); ++it) {
        AudioRendererInfo rendererInfo;
        auto deviceDescs = GetPreferOutputDeviceDescriptors(rendererInfo);
        if (!(it->second->hasBTPermission_)) {
            UpdateDescWhenNoBTPermission(deviceDescs);
        }
        it->second->OnPreferOutputDeviceUpdated(deviceDescs);
    }
    UpdateEffectDefaultSink(deviceType);
}

bool AudioPolicyService::SetWakeUpAudioCapturer(InternalAudioCapturerOptions options)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);
    auto sourceType = options.capturerInfo.sourceType;
    AudioModuleInfo moduleInfo = ConstructWakeUpAudioModuleInfo(sourceType);
    AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
    CHECK_AND_RETURN_RET_LOG(ioHandle != OPEN_PORT_FAILURE, ERR_OPERATION_FAILED,
        "OpenAudioPort failed %{public}d", ioHandle);
    
    {
        std::lock_guard<std::mutex> lck(ioHandlesMutex_);
        IOHandles_[moduleInfo.name] = ioHandle;
    }

    auto devType = GetDeviceType(moduleInfo.name);
    int32_t result = ERROR;
    result = audioPolicyManager_.SetDeviceActive(ioHandle, devType, moduleInfo.name, true);
    if (result != SUCCESS) {
        AUDIO_ERR_LOG("SetWakeUpAudioCapturer failed!");
        return false;
    }
    AUDIO_INFO_LOG("SetWakeUpAudioCapturer Active Success!");
    return true;
}

bool AudioPolicyService::CloseWakeUpAudioCapturer()
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);
    AudioIOHandle ioHandle;
    {
        std::lock_guard<std::mutex> lck(ioHandlesMutex_);
        auto ioHandleIter = IOHandles_.find(PRIMARY_WAKEUP);
        if (ioHandleIter == IOHandles_.end()) {
            AUDIO_ERR_LOG("CloseWakeUpAudioCapturer failed");
            return false;
        } else {
            ioHandle = ioHandleIter->second;
            IOHandles_.erase(ioHandleIter);
        }
    }
    audioPolicyManager_.CloseAudioPort(ioHandle);
    return true;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyService::GetDevices(DeviceFlag deviceFlag)
{
    AUDIO_DEBUG_LOG("Entered %{public}s", __func__);
    std::vector<sptr<AudioDeviceDescriptor>> deviceList = {};

    if (deviceFlag < DeviceFlag::OUTPUT_DEVICES_FLAG || deviceFlag > DeviceFlag::ALL_L_D_DEVICES_FLAG) {
        AUDIO_ERR_LOG("Invalid flag provided %{public}d", deviceFlag);
        return deviceList;
    }

    if (deviceFlag == DeviceFlag::ALL_L_D_DEVICES_FLAG) {
        return connectedDevices_;
    }

    for (const auto& device : connectedDevices_) {
        if (device == nullptr) {
            continue;
        }
        bool filterAllLocal = deviceFlag == DeviceFlag::ALL_DEVICES_FLAG && device->networkId_ == LOCAL_NETWORK_ID;
        bool filterLocalOutput = deviceFlag == DeviceFlag::OUTPUT_DEVICES_FLAG
            && device->networkId_ == LOCAL_NETWORK_ID
            && device->deviceRole_ == DeviceRole::OUTPUT_DEVICE;
        bool filterLocalInput = deviceFlag == DeviceFlag::INPUT_DEVICES_FLAG
            && device->networkId_ == LOCAL_NETWORK_ID
            && device->deviceRole_ == DeviceRole::INPUT_DEVICE;

        bool filterAllRemote = deviceFlag == DeviceFlag::ALL_DISTRIBUTED_DEVICES_FLAG
            && device->networkId_ != LOCAL_NETWORK_ID;
        bool filterRemoteOutput = deviceFlag == DeviceFlag::DISTRIBUTED_OUTPUT_DEVICES_FLAG
            && device->networkId_ != LOCAL_NETWORK_ID
            && device->deviceRole_ == DeviceRole::OUTPUT_DEVICE;
        bool filterRemoteInput = deviceFlag == DeviceFlag::DISTRIBUTED_INPUT_DEVICES_FLAG
            && device->networkId_ != LOCAL_NETWORK_ID
            && device->deviceRole_ == DeviceRole::INPUT_DEVICE;

        if (filterAllLocal || filterLocalOutput || filterLocalInput || filterAllRemote || filterRemoteOutput
            || filterRemoteInput) {
            sptr<AudioDeviceDescriptor> devDesc = new(std::nothrow) AudioDeviceDescriptor(*device);
            deviceList.push_back(devDesc);
        }
    }

    AUDIO_DEBUG_LOG("GetDevices list size = [%{public}zu]", deviceList.size());
    return deviceList;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyService::GetPreferOutputDeviceDescriptors(
    AudioRendererInfo &rendererInfo, std::string networkId)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    std::vector<sptr<AudioDeviceDescriptor>> deviceList = {};
    for (const auto& device : connectedDevices_) {
        if (device == nullptr) {
            continue;
        }
        bool filterLocalOutput = ((currentActiveDevice_ == device->deviceType_)
            && (device->networkId_ == LOCAL_NETWORK_ID)
            && (device->deviceRole_ == DeviceRole::OUTPUT_DEVICE));
        if (!isCurrentRemoteRenderer && filterLocalOutput && networkId == LOCAL_NETWORK_ID) {
            sptr<AudioDeviceDescriptor> devDesc = new(std::nothrow) AudioDeviceDescriptor(*device);
            deviceList.push_back(devDesc);
        }

        bool filterRemoteOutput = ((device->networkId_ != networkId)
            && (device->deviceRole_ == DeviceRole::OUTPUT_DEVICE));
        if (isCurrentRemoteRenderer && filterRemoteOutput) {
            sptr<AudioDeviceDescriptor> devDesc = new(std::nothrow) AudioDeviceDescriptor(*device);
            deviceList.push_back(devDesc);
        }
    }

    return deviceList;
}

DeviceType AudioPolicyService::FetchHighPriorityDevice(bool isOutputDevice = true)
{
    AUDIO_DEBUG_LOG("Entered %{public}s", __func__);
    DeviceType priorityDevice = isOutputDevice ? DEVICE_TYPE_SPEAKER : DEVICE_TYPE_MIC;
    std::vector<DeviceType> priorityList = isOutputDevice ? outputPriorityList_ : inputPriorityList_;
    for (const auto &device : priorityList) {
        auto isPresent = [&device, this] (const sptr<AudioDeviceDescriptor> &desc) {
            CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid device descriptor");
            if ((audioScene_ == AUDIO_SCENE_PHONE_CALL || audioScene_ == AUDIO_SCENE_PHONE_CHAT) &&
                (desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP)) {
                return false;
            } else {
                return desc->deviceType_ == device;
            }
        };

        auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent);
        if (itr != connectedDevices_.end()) {
            priorityDevice = (*itr)->deviceType_;
            break;
        }
    }
    AUDIO_INFO_LOG("FetchHighPriorityDevice: priorityDevice: %{public}d, currentActiveDevice_: %{public}d",
        priorityDevice, currentActiveDevice_);
    return priorityDevice;
}

int32_t AudioPolicyService::SetMicrophoneMute(bool isMute)
{
    AUDIO_DEBUG_LOG("SetMicrophoneMute state[%{public}d]", isMute);
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    CHECK_AND_RETURN_RET_LOG(gsp != nullptr, ERR_OPERATION_FAILED, "Service proxy unavailable");
    return gsp->SetMicrophoneMute(isMute);
}

bool AudioPolicyService::IsMicrophoneMute()
{
    AUDIO_DEBUG_LOG("Enter IsMicrophoneMute");
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    CHECK_AND_RETURN_RET_LOG(gsp != nullptr, false, "Service proxy unavailable");
    return gsp->IsMicrophoneMute();
}

int32_t AudioPolicyService::SetSystemSoundUri(const std::string &key, const std::string &uri)
{
    return audioPolicyManager_.SetSystemSoundUri(key, uri);
}

std::string AudioPolicyService::GetSystemSoundUri(const std::string &key)
{
    return audioPolicyManager_.GetSystemSoundUri(key);
}

bool AudioPolicyService::IsSessionIdValid(int32_t callerUid, int32_t sessionId)
{
    AUDIO_INFO_LOG("IsSessionIdValid::callerUid: %{public}d, sessionId: %{public}d", callerUid, sessionId);

    constexpr int32_t mediaUid = 1013; // "uid" : "media"
    if (callerUid == mediaUid) {
        AUDIO_INFO_LOG("IsSessionIdValid::sessionId:%{public}d is an valid id from media", sessionId);
        return true;
    }

    return true;
}

void UpdateActiveDeviceRoute(InternalDeviceType deviceType)
{
    AUDIO_DEBUG_LOG("UpdateActiveDeviceRoute Device type[%{public}d]", deviceType);

    if (g_adProxy == nullptr) {
        return;
    }
    auto ret = SUCCESS;

    switch (deviceType) {
        case DEVICE_TYPE_BLUETOOTH_SCO:
        case DEVICE_TYPE_USB_HEADSET:
        case DEVICE_TYPE_WIRED_HEADSET: {
            ret = g_adProxy->UpdateActiveDeviceRoute(deviceType, DeviceFlag::ALL_DEVICES_FLAG);
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "Failed to update the route for %{public}d", deviceType);
            break;
        }
        case DEVICE_TYPE_WIRED_HEADPHONES:
        case DEVICE_TYPE_EARPIECE:
        case DEVICE_TYPE_SPEAKER: {
            ret = g_adProxy->UpdateActiveDeviceRoute(deviceType, DeviceFlag::OUTPUT_DEVICES_FLAG);
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "Failed to update the route for %{public}d", deviceType);
            break;
        }
        case DEVICE_TYPE_MIC: {
            ret = g_adProxy->UpdateActiveDeviceRoute(deviceType, DeviceFlag::INPUT_DEVICES_FLAG);
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "Failed to update the route for %{public}d", deviceType);
            break;
        }
        default:
            break;
    }
}

static string ConvertToHDIAudioFormat(AudioSampleFormat sampleFormat)
{
    switch (sampleFormat) {
        case SAMPLE_U8:
            return "u8";
        case SAMPLE_S16LE:
            return "s16le";
        case SAMPLE_S24LE:
            return "s24le";
        case SAMPLE_S32LE:
            return "s32le";
        default:
            return "";
    }
}

static uint32_t GetSampleFormatValue(AudioSampleFormat sampleFormat)
{
    switch (sampleFormat) {
        case SAMPLE_U8:
            return PCM_8_BIT;
        case SAMPLE_S16LE:
            return PCM_16_BIT;
        case SAMPLE_S24LE:
            return PCM_24_BIT;
        case SAMPLE_S32LE:
            return PCM_32_BIT;
        default:
            return PCM_16_BIT;
    }
}

int32_t AudioPolicyService::SelectNewDevice(DeviceRole deviceRole, DeviceType deviceType)
{
    int32_t result = SUCCESS;

    if (deviceRole == DeviceRole::OUTPUT_DEVICE) {
        std::string activePort = GetPortName(currentActiveDevice_);
        AUDIO_INFO_LOG("port %{public}s, active device %{public}d", activePort.c_str(), currentActiveDevice_);
        audioPolicyManager_.SuspendAudioDevice(activePort, true);
    }

    std::string portName = GetPortName(deviceType);
    CHECK_AND_RETURN_RET_LOG(portName != PORT_NONE, result, "Invalid port name %{public}s", portName.c_str());

    if (deviceRole == DeviceRole::OUTPUT_DEVICE) {
        int32_t muteDuration = 500000; // us
        std::thread switchThread(&AudioPolicyService::KeepPortMute, this, muteDuration, portName, deviceType);
        switchThread.detach(); // add another sleep before switch local can avoid pop in some case
    }

    result = audioPolicyManager_.SelectDevice(deviceRole, deviceType, portName);
    CHECK_AND_RETURN_RET_LOG(portName != PORT_NONE, result, "SetDeviceActive failed %{public}d", result);
    audioPolicyManager_.SuspendAudioDevice(portName, false);

    if (isUpdateRouteSupported_) {
        DeviceFlag deviceFlag = deviceRole == DeviceRole::OUTPUT_DEVICE ? OUTPUT_DEVICES_FLAG : INPUT_DEVICES_FLAG;
        const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
        CHECK_AND_RETURN_RET_LOG(gsp != nullptr, ERR_OPERATION_FAILED, "Service proxy unavailable");
        gsp->UpdateActiveDeviceRoute(deviceType, deviceFlag);
    }

    if (deviceRole == DeviceRole::OUTPUT_DEVICE) {
        SetVolumeForSwitchDevice(deviceType);
        currentActiveDevice_ = deviceType;
        OnPreferOutputDeviceUpdated(currentActiveDevice_, LOCAL_NETWORK_ID);
    } else {
        activeInputDevice_ = deviceType;
    }
    return SUCCESS;
}

int32_t AudioPolicyService::HandleA2dpDevice(DeviceType deviceType)
{
    Trace trace("AudioPolicyService::HandleA2dpDevice");
    std::string activePort = GetPortName(currentActiveDevice_);
    if (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        auto primaryModulesPos = deviceClassInfo_.find(ClassType::TYPE_A2DP);
        if (primaryModulesPos != deviceClassInfo_.end()) {
            auto moduleInfoList = primaryModulesPos->second;
            for (auto &moduleInfo : moduleInfoList) {
                if (IOHandles_.find(moduleInfo.name) == IOHandles_.end()) {
                    AUDIO_INFO_LOG("Load a2dp module [%{public}s]", moduleInfo.name.c_str());
                    AudioStreamInfo audioStreamInfo = {};
                    GetActiveDeviceStreamInfo(deviceType, audioStreamInfo);
                    uint32_t bufferSize = (audioStreamInfo.samplingRate * GetSampleFormatValue(audioStreamInfo.format) *
                        audioStreamInfo.channels) / (PCM_8_BIT * BT_BUFFER_ADJUSTMENT_FACTOR);
                    AUDIO_INFO_LOG("a2dp rate: %{public}d, format: %{public}d, channel: %{public}d",
                        audioStreamInfo.samplingRate, audioStreamInfo.format, audioStreamInfo.channels);
                    moduleInfo.channels = to_string(audioStreamInfo.channels);
                    moduleInfo.rate = to_string(audioStreamInfo.samplingRate);
                    moduleInfo.format = ConvertToHDIAudioFormat(audioStreamInfo.format);
                    moduleInfo.bufferSize = to_string(bufferSize);

                    AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
                    CHECK_AND_RETURN_RET_LOG(ioHandle != OPEN_PORT_FAILURE, ERR_OPERATION_FAILED,
                        "OpenAudioPort failed %{public}d", ioHandle);
                    IOHandles_[moduleInfo.name] = ioHandle;
                }

                AUDIO_INFO_LOG("port %{public}s, active device %{public}d", activePort.c_str(), currentActiveDevice_);
                audioPolicyManager_.SuspendAudioDevice(activePort, true);
            }
        }
    } else if (currentActiveDevice_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
        audioPolicyManager_.SuspendAudioDevice(activePort, true);
        int32_t muteDuration =  1000000; // us
        std::thread switchThread(&AudioPolicyService::KeepPortMute, this, muteDuration, PRIMARY_SPEAKER, deviceType);
        switchThread.detach();
        int32_t beforSwitchDelay = 300000;
        usleep(beforSwitchDelay);
    }

    if (isUpdateRouteSupported_) {
        UpdateActiveDeviceRoute(deviceType);
    }
    SetVolumeForSwitchDevice(deviceType);

    AudioIOHandle ioHandle = GetAudioIOHandle(deviceType);
    std::string portName = GetPortName(deviceType);
    CHECK_AND_RETURN_RET_LOG(portName != PORT_NONE, ERR_OPERATION_FAILED, "Invalid port %{public}s", portName.c_str());

    int32_t result = audioPolicyManager_.SetDeviceActive(ioHandle, deviceType, portName, true);
    CHECK_AND_RETURN_RET_LOG(portName != PORT_NONE, result, "SetDeviceActive failed %{public}d", result);
    audioPolicyManager_.SuspendAudioDevice(portName, false);

    UpdateInputDeviceInfo(deviceType);

    return SUCCESS;
}

int32_t AudioPolicyService::HandleFileDevice(DeviceType deviceType)
{
    AUDIO_INFO_LOG("HandleFileDevice");
    AudioIOHandle ioHandle = GetAudioIOHandle(deviceType);
    std::string portName = GetPortName(deviceType);
    CHECK_AND_RETURN_RET_LOG(portName != PORT_NONE, ERR_OPERATION_FAILED, "Invalid port %{public}s", portName.c_str());

    int32_t result = audioPolicyManager_.SetDeviceActive(ioHandle, deviceType, portName, true);
    CHECK_AND_RETURN_RET_LOG(portName != PORT_NONE, result, "SetDeviceActive failed %{public}d", result);
    audioPolicyManager_.SuspendAudioDevice(portName, false);

    if (isUpdateRouteSupported_) {
        UpdateActiveDeviceRoute(deviceType);
    }

    UpdateInputDeviceInfo(deviceType);
    return SUCCESS;
}

int32_t AudioPolicyService::ActivateNewDevice(DeviceType deviceType, bool isSceneActivation = false)
{
    AUDIO_INFO_LOG("Switch device: [%{public}d]-->[%{public}d]", currentActiveDevice_, deviceType);
    int32_t result = SUCCESS;

    if (currentActiveDevice_ == deviceType) {
        return result;
    }

    if (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP || currentActiveDevice_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
        result = HandleA2dpDevice(deviceType);
        return result;
    }

    if (deviceType == DEVICE_TYPE_FILE_SINK || currentActiveDevice_ == DEVICE_TYPE_FILE_SINK) {
        result = HandleFileDevice(deviceType);
        return result;
    }

    std::string portName = GetPortName(deviceType);
    CHECK_AND_RETURN_RET_LOG(portName != PORT_NONE, ERR_OPERATION_FAILED, "Invalid port %{public}s", portName.c_str());
    bool isVolumeSwitched = false;
    if (isUpdateRouteSupported_ && !isSceneActivation) {
        if (GetDeviceRole(deviceType) == OUTPUT_DEVICE) {
            int32_t muteDuration = 1200000; // us
            std::thread switchThread(&AudioPolicyService::KeepPortMute, this, muteDuration, portName, deviceType);
            switchThread.detach();
            int32_t beforSwitchDelay = 300000; // 300 ms
            usleep(beforSwitchDelay);
        }
        UpdateActiveDeviceRoute(deviceType);
        if (GetDeviceRole(deviceType) == OUTPUT_DEVICE) {
            SetVolumeForSwitchDevice(deviceType);
            isVolumeSwitched = true;
        }
    }

    if (GetDeviceRole(deviceType) == OUTPUT_DEVICE && !isVolumeSwitched) {
        SetVolumeForSwitchDevice(deviceType);
    }

    UpdateInputDeviceInfo(deviceType);

    return SUCCESS;
}

void AudioPolicyService::KeepPortMute(int32_t muteDuration, std::string portName, DeviceType deviceType)
{
    Trace trace("AudioPolicyService::KeepPortMute:" + portName);
    AUDIO_INFO_LOG("KeepPortMute %{public}d us for device type[%{public}d]", muteDuration, deviceType);
    audioPolicyManager_.SetSinkMute(portName, true);
    usleep(muteDuration);
    audioPolicyManager_.SetSinkMute(portName, false);
}

int32_t AudioPolicyService::ActivateNewDevice(std::string networkId, DeviceType deviceType, bool isRemote)
{
    if (isRemote) {
        AudioModuleInfo moduleInfo = ConstructRemoteAudioModuleInfo(networkId, GetDeviceRole(deviceType), deviceType);
        AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
        CHECK_AND_RETURN_RET_LOG(ioHandle != OPEN_PORT_FAILURE, ERR_OPERATION_FAILED,
            "OpenAudioPort failed %{public}d", ioHandle);
        std::string moduleName = GetRemoteModuleName(networkId, GetDeviceRole(deviceType));
        IOHandles_[moduleName] = ioHandle;
    }
    return SUCCESS;
}

int32_t AudioPolicyService::SetDeviceActive(InternalDeviceType deviceType, bool active)
{
    AUDIO_INFO_LOG("SetDeviceActive: Device type[%{public}d] flag[%{public}d]", deviceType, active);
    CHECK_AND_RETURN_RET_LOG(deviceType != DEVICE_TYPE_NONE, ERR_DEVICE_NOT_SUPPORTED, "Invalid device");

    int32_t result = SUCCESS;

    if (!active) {
        CHECK_AND_RETURN_RET_LOG(deviceType == currentActiveDevice_, SUCCESS, "This device is not active");
        deviceType = FetchHighPriorityDevice();
    }

    if (deviceType == currentActiveDevice_) {
        AUDIO_INFO_LOG("Device already activated %{public}d. No need to activate again", currentActiveDevice_);
        return SUCCESS;
    }

    // Activate new device if its already connected
    auto isPresent = [&deviceType] (const sptr<AudioDeviceDescriptor> &desc) {
        CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "SetDeviceActive::Invalid device descriptor");
        return ((deviceType == desc->deviceType_) || (deviceType == DEVICE_TYPE_FILE_SINK));
    };

    auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent);
    if (itr == connectedDevices_.end()) {
        AUDIO_ERR_LOG("Requested device not available %{public}d ", deviceType);
        return ERR_OPERATION_FAILED;
    }

    switch (deviceType) {
        case DEVICE_TYPE_EARPIECE:
            result = ActivateNewDevice(DEVICE_TYPE_EARPIECE);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "Earpiece activation err %{public}d", result);
            result = ActivateNewDevice(DEVICE_TYPE_MIC);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "Microphone activation err %{public}d", result);
            break;
        case DEVICE_TYPE_SPEAKER:
            result = ActivateNewDevice(DEVICE_TYPE_SPEAKER);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "Speaker activation err %{public}d", result);
            result = ActivateNewDevice(DEVICE_TYPE_MIC);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "Microphone activation err %{public}d", result);
            break;
        case DEVICE_TYPE_FILE_SINK:
            result = ActivateNewDevice(DEVICE_TYPE_FILE_SINK);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "FILE_SINK activation err %{public}d", result);
            result = ActivateNewDevice(DEVICE_TYPE_FILE_SOURCE);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "FILE_SOURCE activation err %{public}d", result);
            break;
        default:
            result = ActivateNewDevice(deviceType);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "Activation failed for %{public}d", deviceType);
            break;
    }

    currentActiveDevice_ = deviceType;
    OnPreferOutputDeviceUpdated(currentActiveDevice_, LOCAL_NETWORK_ID);
    return result;
}

bool AudioPolicyService::IsDeviceActive(InternalDeviceType deviceType) const
{
    AUDIO_INFO_LOG("Entered AudioPolicyService::%{public}s", __func__);
    return currentActiveDevice_ == deviceType;
}

DeviceType AudioPolicyService::GetActiveOutputDevice() const
{
    return currentActiveDevice_;
}

DeviceType AudioPolicyService::GetActiveInputDevice() const
{
    return activeInputDevice_;
}

int32_t AudioPolicyService::SetRingerMode(AudioRingerMode ringMode)
{
    return audioPolicyManager_.SetRingerMode(ringMode);
}

AudioRingerMode AudioPolicyService::GetRingerMode() const
{
    return audioPolicyManager_.GetRingerMode();
}

int32_t AudioPolicyService::SetAudioScene(AudioScene audioScene)
{
    AUDIO_INFO_LOG("SetAudioScene: %{public}d", audioScene);
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    CHECK_AND_RETURN_RET_LOG(gsp != nullptr, ERR_OPERATION_FAILED, "Service proxy unavailable");
    audioScene_ = audioScene;

    if (isUpdateRouteSupported_) {
        SetEarpieceState();
    }

    auto priorityDev = FetchHighPriorityDevice();
    AUDIO_INFO_LOG("Current active device: %{public}d. Priority device: %{public}d", currentActiveDevice_, priorityDev);

    int32_t result = ActivateNewDevice(priorityDev, true);
    CHECK_AND_RETURN_RET_LOG(result == SUCCESS, ERR_OPERATION_FAILED, "Device activation failed [%{public}d]", result);

    currentActiveDevice_ = priorityDev;
    AUDIO_INFO_LOG("Current active device updates: %{public}d", currentActiveDevice_);
    OnPreferOutputDeviceUpdated(currentActiveDevice_, LOCAL_NETWORK_ID);

    result = gsp->SetAudioScene(audioScene, priorityDev);
    CHECK_AND_RETURN_RET_LOG(result == SUCCESS, ERR_OPERATION_FAILED, "SetAudioScene failed [%{public}d]", result);

    return SUCCESS;
}

void AudioPolicyService::SetEarpieceState()
{
    if (audioScene_ == AUDIO_SCENE_PHONE_CALL) {
        AUDIO_INFO_LOG("SetEarpieceState: add earpiece device only for [phone], localDevicesType [%{public}s]",
            localDevicesType_.c_str());
        if (localDevicesType_.compare("phone") != 0) {
            return;
        }

        // add earpiece to connectedDevices_
        auto isPresent = [](const sptr<AudioDeviceDescriptor> &desc) {
            CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid device descriptor");
            return desc->deviceType_ == DEVICE_TYPE_EARPIECE;
        };
        auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent);
        if (itr == connectedDevices_.end()) {
            sptr<AudioDeviceDescriptor> audioDescriptor =
                new (std::nothrow) AudioDeviceDescriptor(DEVICE_TYPE_EARPIECE, OUTPUT_DEVICE);
            UpdateDisplayName(audioDescriptor);
            connectedDevices_.insert(connectedDevices_.begin(), audioDescriptor);
            AUDIO_INFO_LOG("SetAudioScene: Add earpiece to connectedDevices_");
        }

        // add earpiece to outputPriorityList_
        auto earpiecePos = find(outputPriorityList_.begin(), outputPriorityList_.end(), DEVICE_TYPE_EARPIECE);
        if (earpiecePos == outputPriorityList_.end()) {
            outputPriorityList_.insert(outputPriorityList_.end() - PRIORITY_LIST_OFFSET_POSTION,
                DEVICE_TYPE_EARPIECE);
            AUDIO_INFO_LOG("SetAudioScene: Add earpiece to outputPriorityList_");
        }
    } else {
        // remove earpiece from connectedDevices_
        auto isPresent = [] (const sptr<AudioDeviceDescriptor> &desc) {
            CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid device descriptor");
            return desc->deviceType_ == DEVICE_TYPE_EARPIECE;
        };
        auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent);
        if (itr != connectedDevices_.end()) {
            connectedDevices_.erase(itr);
            AUDIO_INFO_LOG("SetAudioScene: Remove earpiece from connectedDevices_");
        }
        // remove earpiece from outputPriorityList_
        auto earpiecePos = find(outputPriorityList_.begin(), outputPriorityList_.end(), DEVICE_TYPE_EARPIECE);
        if (earpiecePos != outputPriorityList_.end()) {
            outputPriorityList_.erase(earpiecePos);
            AUDIO_INFO_LOG("SetAudioScene: Remove earpiece from outputPriorityList_");
        }
    }
}

std::string AudioPolicyService::GetLocalDevicesType()
{
    return localDevicesType_;
}

AudioScene AudioPolicyService::GetAudioScene(bool hasSystemPermission) const
{
    AUDIO_INFO_LOG("GetAudioScene return value: %{public}d", audioScene_);
    if (!hasSystemPermission) {
        switch (audioScene_) {
            case AUDIO_SCENE_RINGING:
            case AUDIO_SCENE_PHONE_CALL:
                return AUDIO_SCENE_DEFAULT;
            default:
                break;
        }
    }
    return audioScene_;
}

bool AudioPolicyService::IsAudioInterruptEnabled() const
{
    return interruptEnabled_;
}

void AudioPolicyService::OnAudioInterruptEnable(bool enable)
{
    interruptEnabled_ = enable;
}

void AudioPolicyService::OnUpdateRouteSupport(bool isSupported)
{
    isUpdateRouteSupported_ = isSupported;
}

bool AudioPolicyService::GetActiveDeviceStreamInfo(DeviceType deviceType, AudioStreamInfo &streamInfo)
{
    if (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        auto streamInfoPos = connectedA2dpDeviceMap_.find(activeBTDevice_);
        if (streamInfoPos != connectedA2dpDeviceMap_.end()) {
            streamInfo.samplingRate = streamInfoPos->second.samplingRate;
            streamInfo.format = streamInfoPos->second.format;
            streamInfo.channels = streamInfoPos->second.channels;
            return true;
        }
    }

    return false;
}

bool AudioPolicyService::IsConfigurationUpdated(DeviceType deviceType, const AudioStreamInfo &streamInfo)
{
    if (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        AudioStreamInfo audioStreamInfo = {};
        if (GetActiveDeviceStreamInfo(deviceType, audioStreamInfo)) {
            AUDIO_DEBUG_LOG("Device configurations current rate: %{public}d, format: %{public}d, channel: %{public}d",
                audioStreamInfo.samplingRate, audioStreamInfo.format, audioStreamInfo.channels);
            AUDIO_DEBUG_LOG("Device configurations updated rate: %{public}d, format: %{public}d, channel: %{public}d",
                streamInfo.samplingRate, streamInfo.format, streamInfo.channels);
            if ((audioStreamInfo.samplingRate != streamInfo.samplingRate)
                || (audioStreamInfo.channels != streamInfo.channels)
                || (audioStreamInfo.format != streamInfo.format)) {
                return true;
            }
        }
    }

    return false;
}

void AudioPolicyService::UpdateConnectedDevicesWhenConnecting(const AudioDeviceDescriptor &deviceDescriptor,
    std::vector<sptr<AudioDeviceDescriptor>> &desc)
{
    sptr<AudioDeviceDescriptor> audioDescriptor = nullptr;

    if (IsOutputDevice(deviceDescriptor.deviceType_)) {
        AUDIO_INFO_LOG("Filling output device for %{public}d", deviceDescriptor.deviceType_);

        audioDescriptor = new(std::nothrow) AudioDeviceDescriptor(deviceDescriptor);
        audioDescriptor->deviceRole_ = OUTPUT_DEVICE;

        // Use speaker streaminfo for all output devices cap
        auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(),
        [](const sptr<AudioDeviceDescriptor> &devDesc) {
            CHECK_AND_RETURN_RET_LOG(devDesc != nullptr, false, "Invalid device descriptor");
            return (devDesc->deviceType_ == DEVICE_TYPE_SPEAKER);
        });
        if (itr != connectedDevices_.end()) {
            audioDescriptor->SetDeviceCapability((*itr)->audioStreamInfo_, 0);
        }

        desc.push_back(audioDescriptor);
        audioDescriptor->deviceId_ = startDeviceId++;
        UpdateDisplayName(audioDescriptor);
        connectedDevices_.insert(connectedDevices_.begin(), audioDescriptor);
    }
    if (IsInputDevice(deviceDescriptor.deviceType_)) {
        AUDIO_INFO_LOG("Filling input device for %{public}d", deviceDescriptor.deviceType_);

        audioDescriptor = new(std::nothrow) AudioDeviceDescriptor(deviceDescriptor);
        audioDescriptor->deviceRole_ = INPUT_DEVICE;

        // Use mic streaminfo for all input devices cap
        auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(),
            [](const sptr<AudioDeviceDescriptor> &devDesc) {
            CHECK_AND_RETURN_RET_LOG(devDesc != nullptr, false, "Invalid device descriptor");
            return (devDesc->deviceType_ == DEVICE_TYPE_MIC);
        });
        if (itr != connectedDevices_.end()) {
            audioDescriptor->SetDeviceCapability((*itr)->audioStreamInfo_, 0);
        }

        desc.push_back(audioDescriptor);
        audioDescriptor->deviceId_ = startDeviceId++;
        UpdateDisplayName(audioDescriptor);
        connectedDevices_.insert(connectedDevices_.begin(), audioDescriptor);
    }
}

void AudioPolicyService::UpdateConnectedDevicesWhenDisconnecting(const AudioDeviceDescriptor& deviceDescriptor,
    std::vector<sptr<AudioDeviceDescriptor>> &desc)
{
    AUDIO_INFO_LOG("[%{public}s], devType:[%{public}d]", __func__, deviceDescriptor.deviceType_);
    auto isPresent = [&deviceDescriptor](const sptr<AudioDeviceDescriptor>& descriptor) {
        return descriptor->deviceType_ == deviceDescriptor.deviceType_
            && descriptor->networkId_ == deviceDescriptor.networkId_;
    };
    // Remember the disconnected device descriptor and remove it
    for (auto it = connectedDevices_.begin(); it != connectedDevices_.end();) {
        it = find_if(it, connectedDevices_.end(), isPresent);
        if (it != connectedDevices_.end()) {
            desc.push_back(*it);
            it = connectedDevices_.erase(it);
        }
    }
}

void AudioPolicyService::OnPnpDeviceStatusUpdated(DeviceType devType, bool isConnected)
{
    CHECK_AND_RETURN_LOG(devType != DEVICE_TYPE_NONE, "devType is none type");
    if (!hasModulesLoaded) {
        AUDIO_WARNING_LOG("modules has not loaded");
        pnpDevice_ = devType;
        isPnpDeviceConnected = isConnected;
        return;
    }
    AudioStreamInfo streamInfo = {};
    if (g_adProxy == nullptr) {
        GetAudioServerProxy();
    }
    OnDeviceStatusUpdated(devType, isConnected, "", "", streamInfo);
}

void AudioPolicyService::UpdateLocalGroupInfo(bool isConnected, const std::string& macAddress,
    const std::string& deviceName, const AudioStreamInfo& streamInfo, AudioDeviceDescriptor& deviceDesc)
{
    deviceDesc.SetDeviceInfo(deviceName, macAddress);
    deviceDesc.SetDeviceCapability(streamInfo, 0);
    UpdateGroupInfo(VOLUME_TYPE, GROUP_NAME_DEFAULT, deviceDesc.volumeGroupId_, LOCAL_NETWORK_ID, isConnected,
        NO_REMOTE_ID);
    UpdateGroupInfo(INTERRUPT_TYPE, GROUP_NAME_DEFAULT, deviceDesc.interruptGroupId_, LOCAL_NETWORK_ID, isConnected,
        NO_REMOTE_ID);
    deviceDesc.networkId_ = LOCAL_NETWORK_ID;
}

int32_t AudioPolicyService::HandleLocalDeviceConnected(DeviceType devType, const std::string& macAddress,
    const std::string& deviceName, const AudioStreamInfo& streamInfo)
{
    AUDIO_INFO_LOG("[%{public}s], macAddress:[%{public}s]", __func__, macAddress.c_str());
    if (devType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        connectedA2dpDeviceMap_.insert(make_pair(macAddress, streamInfo));
        activeBTDevice_ = macAddress;
    }

    // new device found. If connected, add into active device list
    int32_t result = ActivateNewDevice(devType);
    CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "Failed to activate new device %{public}d", devType);
    currentActiveDevice_ = devType;

    return result;
}

int32_t AudioPolicyService::HandleLocalDeviceDisconnected(DeviceType devType, const std::string& macAddress)
{
    int32_t result = ERROR;
    if (currentActiveDevice_ == devType) {
        auto priorityDev = FetchHighPriorityDevice();
        AUDIO_INFO_LOG("Priority device is [%{public}d]", priorityDev);

        if (priorityDev == DEVICE_TYPE_SPEAKER) {
            result = ActivateNewDevice(DEVICE_TYPE_MIC);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "Failed to activate new device [%{public}d]",
                DEVICE_TYPE_MIC);
            result = ActivateNewDevice(DEVICE_TYPE_SPEAKER);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "Failed to activate new device [%{public}d]",
                DEVICE_TYPE_SPEAKER);
        } else {
            result = ActivateNewDevice(priorityDev);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "Failed to activate new device [%{public}d]",
                priorityDev);
        }

        currentActiveDevice_ = priorityDev;
        UpdateEffectDefaultSink(priorityDev);
    } else {
        // The disconnected device is not current acitve device. No need to change active device.
        AUDIO_INFO_LOG("Current acitve device: %{public}d. No need to change", currentActiveDevice_);
        result = SUCCESS;
    }

    if (devType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        connectedA2dpDeviceMap_.erase(macAddress);
        activeBTDevice_ = "";

        if (IOHandles_.find(BLUETOOTH_SPEAKER) != IOHandles_.end()) {
            audioPolicyManager_.CloseAudioPort(IOHandles_[BLUETOOTH_SPEAKER]);
            IOHandles_.erase(BLUETOOTH_SPEAKER);
        }
    }

    return result;
}

DeviceType AudioPolicyService::FindConnectedHeadset()
{
    DeviceType retType = DEVICE_TYPE_NONE;
    for (const auto& devDesc: connectedDevices_) {
        if ((devDesc->deviceType_ == DEVICE_TYPE_WIRED_HEADSET) ||
            (devDesc->deviceType_ == DEVICE_TYPE_WIRED_HEADPHONES) ||
            (devDesc->deviceType_ == DEVICE_TYPE_USB_HEADSET)) {
            retType = devDesc->deviceType_;
            break;
        }
    }
    return retType;
}

void AudioPolicyService::OnDeviceStatusUpdated(DeviceType devType, bool isConnected, const std::string& macAddress,
    const std::string& deviceName, const AudioStreamInfo& streamInfo)
{
    AUDIO_INFO_LOG("Device connection state updated | TYPE[%{public}d] STATUS[%{public}d]", devType, isConnected);

    // Special logic for extern cable, need refactor
    if (devType == DEVICE_TYPE_EXTERN_CABLE) {
        if (!isConnected) {
            AUDIO_INFO_LOG("Extern cable disconnected, do nothing");
            return;
        }
        DeviceType connectedHeadsetType = FindConnectedHeadset();
        if (connectedHeadsetType == DEVICE_TYPE_NONE) {
            AUDIO_INFO_LOG("Extern cable connect without headset connected before, do nothing");
            return;
        }
        devType = connectedHeadsetType;
        isConnected = false;
    }

    int32_t result = ERROR;
    AudioDeviceDescriptor deviceDesc(devType, GetDeviceRole(devType));
    UpdateLocalGroupInfo(isConnected, macAddress, deviceName, streamInfo, deviceDesc);

    // fill device change action for callback
    std::vector<sptr<AudioDeviceDescriptor>> deviceChangeDescriptor = {};
    auto isPresent = [&devType] (const sptr<AudioDeviceDescriptor> &descriptor) {
        return descriptor->deviceType_ == devType;
    };
    if (isConnected) {
        // If device already in list, remove it else do not modify the list
        connectedDevices_.erase(std::remove_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent),
            connectedDevices_.end());
        UpdateConnectedDevicesWhenConnecting(deviceDesc, deviceChangeDescriptor);

        if ((devType == DEVICE_TYPE_BLUETOOTH_SCO && GetAudioScene() == AUDIO_SCENE_DEFAULT) ||
            (devType == DEVICE_TYPE_BLUETOOTH_A2DP && GetAudioScene() == AUDIO_SCENE_PHONE_CALL)) {
            // For SCO or A2DP device, add to connected device and donot activate now
            AUDIO_INFO_LOG("BT SCO or A2DP device detected in non-call mode [%{public}d]", GetAudioScene());
            if (devType == DEVICE_TYPE_BLUETOOTH_A2DP) {
                connectedA2dpDeviceMap_.insert(make_pair(macAddress, streamInfo));
                activeBTDevice_ = macAddress;
            }
            TriggerDeviceChangedCallback(deviceChangeDescriptor, isConnected);
            UpdateTrackerDeviceChange(deviceChangeDescriptor);
            return;
        }
        result = HandleLocalDeviceConnected(devType, macAddress, deviceName, streamInfo);
        CHECK_AND_RETURN_LOG(result == SUCCESS, "Connect local device failed.");
    } else {
        UpdateConnectedDevicesWhenDisconnecting(deviceDesc, deviceChangeDescriptor);
        result = HandleLocalDeviceDisconnected(devType, macAddress);
        CHECK_AND_RETURN_LOG(result == SUCCESS, "Disconnect local device failed.");
    }

    OnPreferOutputDeviceUpdated(currentActiveDevice_, LOCAL_NETWORK_ID);
    TriggerDeviceChangedCallback(deviceChangeDescriptor, isConnected);
    UpdateTrackerDeviceChange(deviceChangeDescriptor);
}

#ifdef FEATURE_DTMF_TONE
std::vector<int32_t> AudioPolicyService::GetSupportedTones()
{
    std::vector<int> supportedToneList = {};
    for (auto i = toneDescriptorMap.begin(); i != toneDescriptorMap.end(); i++) {
        supportedToneList.push_back(i->first);
    }
    return supportedToneList;
}

std::shared_ptr<ToneInfo> AudioPolicyService::GetToneConfig(int32_t ltonetype)
{
    if (toneDescriptorMap.find(ltonetype) == toneDescriptorMap.end()) {
        return nullptr;
    }
    AUDIO_DEBUG_LOG("AudioPolicyService GetToneConfig %{public}d", ltonetype);
    return toneDescriptorMap[ltonetype];
}
#endif

void AudioPolicyService::OnDeviceConfigurationChanged(DeviceType deviceType, const std::string &macAddress,
    const std::string &deviceName, const AudioStreamInfo &streamInfo)
{
    AUDIO_DEBUG_LOG("OnDeviceConfigurationChanged in");
    if ((deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) && !macAddress.compare(activeBTDevice_)
        && IsDeviceActive(deviceType)) {
        if (!IsConfigurationUpdated(deviceType, streamInfo)) {
            AUDIO_DEBUG_LOG("Audio configuration same");
            return;
        }

        uint32_t bufferSize
            = (streamInfo.samplingRate * GetSampleFormatValue(streamInfo.format)
                * streamInfo.channels) / (PCM_8_BIT * BT_BUFFER_ADJUSTMENT_FACTOR);
        AUDIO_DEBUG_LOG("Updated buffer size: %{public}d", bufferSize);
        connectedA2dpDeviceMap_[macAddress] = streamInfo;

        auto a2dpModulesPos = deviceClassInfo_.find(ClassType::TYPE_A2DP);
        if (a2dpModulesPos != deviceClassInfo_.end()) {
            auto moduleInfoList = a2dpModulesPos->second;
            for (auto &moduleInfo : moduleInfoList) {
                if (IOHandles_.find(moduleInfo.name) != IOHandles_.end()) {
                    moduleInfo.channels = to_string(streamInfo.channels);
                    moduleInfo.rate = to_string(streamInfo.samplingRate);
                    moduleInfo.format = ConvertToHDIAudioFormat(streamInfo.format);
                    moduleInfo.bufferSize = to_string(bufferSize);

                    // First unload the existing bt sink
                    AUDIO_DEBUG_LOG("UnLoad existing a2dp module");
                    std::string currentActivePort = GetPortName(currentActiveDevice_);
                    AudioIOHandle activateDeviceIOHandle = IOHandles_[BLUETOOTH_SPEAKER];
                    audioPolicyManager_.SuspendAudioDevice(currentActivePort, true);
                    audioPolicyManager_.CloseAudioPort(activateDeviceIOHandle);

                    // Load bt sink module again with new configuration
                    AUDIO_DEBUG_LOG("Reload a2dp module [%{public}s]", moduleInfo.name.c_str());
                    AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
                    CHECK_AND_RETURN_LOG(ioHandle != OPEN_PORT_FAILURE, "OpenAudioPort failed %{public}d", ioHandle);
                    IOHandles_[moduleInfo.name] = ioHandle;
                    std::string portName = GetPortName(deviceType);
                    audioPolicyManager_.SetDeviceActive(ioHandle, deviceType, portName, true);
                    audioPolicyManager_.SuspendAudioDevice(portName, false);

                    auto isPresent = [&deviceType] (const sptr<AudioDeviceDescriptor> &descriptor) {
                        return descriptor->deviceType_ == deviceType;
                    };

                    sptr<AudioDeviceDescriptor> audioDescriptor
                        = new(std::nothrow) AudioDeviceDescriptor(deviceType, OUTPUT_DEVICE);
                    audioDescriptor->SetDeviceInfo(deviceName, macAddress);
                    audioDescriptor->SetDeviceCapability(streamInfo, 0);
                    std::replace_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent, audioDescriptor);
                    break;
                }
            }
        }
    }
}

void AudioPolicyService::RemoveDeviceInRouterMap(std::string networkId)
{
    std::lock_guard<std::mutex> lock(routerMapMutex_);
    std::unordered_map<int32_t, std::pair<std::string, int32_t>>::iterator it;
    for (it = routerMap_.begin();it != routerMap_.end();) {
        if (it->second.first == networkId) {
            routerMap_.erase(it++);
        } else {
            it++;
        }
    }
}

void AudioPolicyService::SetDisplayName(const std::string &deviceName, bool isLocalDevice)
{
    for (auto& deviceInfo : connectedDevices_) {
        if ((isLocalDevice && deviceInfo->networkId_ == LOCAL_NETWORK_ID) ||
            (!isLocalDevice && deviceInfo->networkId_ != LOCAL_NETWORK_ID)) {
            deviceInfo->displayName_ = deviceName;
        }
    }
}

void AudioPolicyService::RegisterRemoteDevStatusCallback()
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);
    std::shared_ptr<DistributedHardware::DmInitCallback> initCallback = std::make_shared<DeviceInitCallBack>();
    int32_t ret = DistributedHardware::DeviceManager::GetInstance().InitDeviceManager(AUDIO_SERVICE_PKG, initCallback);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("Init device manage failed");
        return;
    }
    std::shared_ptr<DistributedHardware::DeviceStatusCallback> callback = std::make_shared<DeviceStatusCallbackImpl>();
    DistributedHardware::DeviceManager::GetInstance().RegisterDevStatusCallback(AUDIO_SERVICE_PKG, "", callback);
}

bool AudioPolicyService::CreateDataShareHelperInstance()
{
    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    CHECK_AND_RETURN_RET_LOG(samgr != nullptr, false, "[Policy Service] Get samgr failed.");

    sptr<IRemoteObject> remoteObject = samgr->GetSystemAbility(AUDIO_POLICY_SERVICE_ID);
    CHECK_AND_RETURN_RET_LOG(remoteObject != nullptr, false, "[Policy Service] audio service remote object is NULL.");

    lock_guard<mutex> lock(g_dataShareHelperMutex);
    g_dataShareHelper = DataShare::DataShareHelper::Creator(remoteObject, SETTINGS_DATA_BASE_URI);
    CHECK_AND_RETURN_RET_LOG(g_dataShareHelper != nullptr, false, "CreateDataShareHelperInstance create fail.");
    return true;
}

int32_t AudioPolicyService::GetDeviceNameFromDataShareHelper(std::string &deviceName)
{
    lock_guard<mutex> lock(g_dataShareHelperMutex);
    CHECK_AND_RETURN_RET_LOG(g_dataShareHelper != nullptr, ERROR, "GetDeviceNameFromDataShareHelper NULL.");
    std::shared_ptr<Uri> uri = std::make_shared<Uri>(SETTINGS_DATA_BASE_URI);
    std::vector<std::string> columns;
    columns.emplace_back(SETTINGS_DATA_FIELD_VALUE);
    DataShare::DataSharePredicates predicates;
    predicates.EqualTo(SETTINGS_DATA_FIELD_KEYWORD, PREDICATES_STRING);

    auto resultSet = g_dataShareHelper->Query(*uri, predicates, columns);
    CHECK_AND_RETURN_RET_LOG(resultSet != nullptr, ERROR, "GetDeviceNameFromDataShareHelper query fail.");

    int32_t numRows = 0;
    resultSet->GetRowCount(numRows);

    if (numRows <= 0) {
        AUDIO_ERR_LOG("GetDeviceNameFromDataShareHelper row zero.");
        return ERROR;
    }
    int columnIndex;
    resultSet->GoToFirstRow();
    resultSet->GetColumnIndex(SETTINGS_DATA_FIELD_VALUE, columnIndex);
    resultSet->GetString(columnIndex, deviceName);
    AUDIO_INFO_LOG("GetDeviceNameFromDataShareHelper deviceName[%{public}s]", deviceName.c_str());
    return SUCCESS;
}

void AudioPolicyService::RegisterNameMonitorHelper()
{
    lock_guard<mutex> lock(g_dataShareHelperMutex);
    CHECK_AND_BREAK_LOG(g_dataShareHelper != g_dataShareHelper, "RegisterNameMonitorHelper g_dataShareHelper is NULL.");
    auto uri = std::make_shared<Uri>(SETTINGS_DATA_BASE_URI + "&key=" + PREDICATES_STRING);
    sptr<AAFwk::DataAbilityObserverStub> settingDataObserver = std::make_unique<DataShareObserverCallBack>().release();
    g_dataShareHelper->RegisterObserver(*uri, settingDataObserver);
}

void AudioPolicyService::UpdateDisplayName(sptr<AudioDeviceDescriptor> deviceDescriptor)
{
    if (deviceDescriptor->networkId_ == LOCAL_NETWORK_ID) {
        std::string devicesName = "";
        int32_t ret = GetDeviceNameFromDataShareHelper(devicesName);
        if (ret != SUCCESS) {
            AUDIO_ERR_LOG("Local UpdateDisplayName init device failed");
            return;
        }
        deviceDescriptor->displayName_ = devicesName;
    } else {
        std::shared_ptr<DistributedHardware::DmInitCallback> callback = std::make_shared<DeviceInitCallBack>();
        int32_t ret = DistributedHardware::DeviceManager::GetInstance().InitDeviceManager(AUDIO_SERVICE_PKG, callback);
        if (ret != SUCCESS) {
        AUDIO_ERR_LOG("UpdateDisplayName init device failed");
        return;
        }
        std::vector<DistributedHardware::DmDeviceInfo> deviceList;
        if (DistributedHardware::DeviceManager::GetInstance()
            .GetTrustedDeviceList(AUDIO_SERVICE_PKG, "", deviceList) == SUCCESS) {
            for (auto deviceInfo : deviceList) {
                std::string strNetworkId(deviceInfo.networkId);
                if (strNetworkId == deviceDescriptor->networkId_) {
                    AUDIO_INFO_LOG("UpdateDisplayName remote name [%{public}s]", deviceInfo.deviceName);
                    deviceDescriptor->displayName_ = deviceInfo.deviceName;
                    break;
                }
            }
        };
    }
}

void AudioPolicyService::OnDeviceStatusUpdated(DStatusInfo statusInfo)
{
    AUDIO_INFO_LOG("Device connection updated | HDI_PIN[%{public}d] CONNECT_STATUS[%{public}d] NETWORKID[%{public}s]",
        statusInfo.hdiPin, statusInfo.isConnected, statusInfo.networkId);
    DeviceType devType = GetDeviceTypeFromPin(statusInfo.hdiPin);
    const std::string networkId = statusInfo.networkId;
    AudioDeviceDescriptor deviceDesc(devType, GetDeviceRole(devType));
    deviceDesc.SetDeviceInfo(statusInfo.deviceName, statusInfo.macAddress);
    deviceDesc.SetDeviceCapability(statusInfo.streamInfo, 0);
    deviceDesc.networkId_ = networkId;
    UpdateGroupInfo(VOLUME_TYPE, GROUP_NAME_DEFAULT, deviceDesc.volumeGroupId_, networkId, statusInfo.isConnected,
        statusInfo.mappingVolumeId);
    UpdateGroupInfo(INTERRUPT_TYPE, GROUP_NAME_DEFAULT, deviceDesc.interruptGroupId_, networkId,
        statusInfo.isConnected, statusInfo.mappingInterruptId);

    std::vector<sptr<AudioDeviceDescriptor>> deviceChangeDescriptor = {};

    // new device found. If connected, add into active device list
    if (statusInfo.isConnected) {
        for (auto devDes : connectedDevices_) {
            if (devDes->deviceType_ == devType && devDes->networkId_ == networkId) {
                AUDIO_INFO_LOG("Device [%{public}s] Type [%{public}d] has connected already!",
                    networkId.c_str(), devType);
                return;
            }
        }

        int32_t ret = ActivateNewDevice(statusInfo.networkId, devType,
            statusInfo.connectType == ConnectType::CONNECT_TYPE_DISTRIBUTED);
        CHECK_AND_RETURN_LOG(ret == SUCCESS, "DEVICE online but open audio device failed.");
        UpdateConnectedDevicesWhenConnecting(deviceDesc, deviceChangeDescriptor);

        const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
        if (gsp != nullptr && statusInfo.connectType == ConnectType::CONNECT_TYPE_DISTRIBUTED) {
            gsp->NotifyDeviceInfo(networkId, true);
        }
    } else {
        UpdateConnectedDevicesWhenDisconnecting(deviceDesc, deviceChangeDescriptor);
        std::string moduleName = GetRemoteModuleName(networkId, GetDeviceRole(devType));
        if (IOHandles_.find(moduleName) != IOHandles_.end()) {
            audioPolicyManager_.CloseAudioPort(IOHandles_[moduleName]);
            IOHandles_.erase(moduleName);
        }
        RemoveDeviceInRouterMap(moduleName);
    }

    TriggerDeviceChangedCallback(deviceChangeDescriptor, statusInfo.isConnected);
    if (GetDeviceRole(devType) == DeviceRole::INPUT_DEVICE) {
        remoteCapturerSwitch_ = true;
    }
}

void AudioPolicyService::OnServiceConnected(AudioServiceIndex serviceIndex)
{
    AUDIO_INFO_LOG("[module_load]::OnServiceConnected for [%{public}d]", serviceIndex);
    CHECK_AND_RETURN_LOG(serviceIndex >= HDI_SERVICE_INDEX && serviceIndex <= AUDIO_SERVICE_INDEX, "invalid index");

    // If audio service or hdi service is not ready, donot load default modules
    lock_guard<mutex> lock(serviceFlagMutex_);
    serviceFlag_.set(serviceIndex, true);
    if (serviceFlag_.count() != MIN_SERVICE_COUNT) {
        AUDIO_INFO_LOG("[module_load]::hdi service or audio service not up. Cannot load default module now");
        return;
    }

    int32_t result = ERROR;
    AUDIO_INFO_LOG("[module_load]::HDI and AUDIO SERVICE is READY. Loading default modules");
    for (const auto &device : deviceClassInfo_) {
        if (device.first == ClassType::TYPE_PRIMARY || device.first == ClassType::TYPE_FILE_IO) {
            auto moduleInfoList = device.second;
            for (auto &moduleInfo : moduleInfoList) {
                AUDIO_INFO_LOG("[module_load]::Load module[%{public}s]", moduleInfo.name.c_str());
                moduleInfo.sinkLatency = sinkLatencyInMsec_ != 0 ? to_string(sinkLatencyInMsec_) : "";
                AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
                if (ioHandle == OPEN_PORT_FAILURE) {
                    AUDIO_INFO_LOG("[module_load]::Open port failed");
                    continue;
                }
                IOHandles_[moduleInfo.name] = ioHandle;
                auto devType = GetDeviceType(moduleInfo.name);
                if (devType == DEVICE_TYPE_SPEAKER || devType == DEVICE_TYPE_MIC) {
                    result = audioPolicyManager_.SetDeviceActive(ioHandle, devType, moduleInfo.name, true);
                    if (result != SUCCESS) {
                        AUDIO_ERR_LOG("[module_load]::Device failed %{public}d", devType);
                        continue;
                    }
                    AddAudioDevice(moduleInfo, devType);
                }
            }
        }
    }

    if (result == SUCCESS) {
        AUDIO_INFO_LOG("[module_load]::Setting speaker as active device on bootup");
        hasModulesLoaded = true;
        currentActiveDevice_ = DEVICE_TYPE_SPEAKER;
        activeInputDevice_ = DEVICE_TYPE_MIC;
        SetVolumeForSwitchDevice(currentActiveDevice_);
        OnPreferOutputDeviceUpdated(currentActiveDevice_, LOCAL_NETWORK_ID);
        OnPnpDeviceStatusUpdated(pnpDevice_, isPnpDeviceConnected);
        audioEffectManager_.SetMasterSinkAvailable();
        if (audioEffectManager_.CanLoadEffectSinks()) {
            LoadEffectSinks();
        }
    }
    RegisterBluetoothListener();
}

void AudioPolicyService::OnServiceDisconnected(AudioServiceIndex serviceIndex)
{
    AUDIO_ERR_LOG("OnServiceDisconnected for [%{public}d]", serviceIndex);
    CHECK_AND_RETURN_LOG(serviceIndex >= HDI_SERVICE_INDEX && serviceIndex <= AUDIO_SERVICE_INDEX, "invalid index");
    if (serviceIndex == HDI_SERVICE_INDEX) {
        AUDIO_ERR_LOG("Auto exit audio policy service for hdi service stopped!");
        _Exit(0);
    }
}

void AudioPolicyService::OnMonoAudioConfigChanged(bool audioMono)
{
    AUDIO_INFO_LOG("AudioPolicyService::OnMonoAudioConfigChanged: audioMono = %{public}s", audioMono? "true": "false");
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("Service proxy unavailable: g_adProxy null");
        return;
    }
    gsp->SetAudioMonoState(audioMono);
}

void AudioPolicyService::OnAudioBalanceChanged(float audioBalance)
{
    AUDIO_INFO_LOG("AudioPolicyService::OnAudioBalanceChanged: audioBalance = %{public}f", audioBalance);
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("Service proxy unavailable: g_adProxy null");
        return;
    }
    gsp->SetAudioBalanceValue(audioBalance);
}

void AudioPolicyService::UpdateEffectDefaultSink(DeviceType deviceType)
{
    if (effectActiveDevice_ == deviceType) {
        return;
    }
    effectActiveDevice_ = deviceType;
    switch (deviceType) {
        case DeviceType::DEVICE_TYPE_EARPIECE:
        case DeviceType::DEVICE_TYPE_SPEAKER:
        case DeviceType::DEVICE_TYPE_FILE_SINK:
        case DeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case DeviceType::DEVICE_TYPE_USB_HEADSET:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_SCO: {
            const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
            CHECK_AND_RETURN_LOG(gsp != nullptr, "Service proxy unavailable");
            std::string sinkName = GetPortName(deviceType);
            bool ret = gsp->SetOutputDeviceSink(deviceType, sinkName);
            CHECK_AND_RETURN_LOG(ret, "Failed to set output device sink");
            int res = audioPolicyManager_.UpdateSwapDeviceStatus();
            CHECK_AND_RETURN_LOG(res == SUCCESS, "Failed to update client swap device status");
            break;
        }
        default:
            break;
    }
}

void AudioPolicyService::LoadSinksForCapturer()
{
    AUDIO_INFO_LOG("LoadSinksForCapturer");
    LoadInnerCapturerSink();
    LoadReceiverSink();
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("LoadSinksForCapturer error for g_adProxy null");
        return;
    }
    bool ret = gsp->CreatePlaybackCapturerManager();
    CHECK_AND_RETURN_LOG(ret, "PlaybackCapturerManager create failed");
}

void AudioPolicyService::LoadInnerCapturerSink()
{
    AUDIO_INFO_LOG("LoadInnerCapturerSink");
    AudioModuleInfo moduleInfo = {};
    moduleInfo.lib = "libmodule-inner-capturer-sink.z.so";
    moduleInfo.name = INNER_CAPTURER_SINK_NAME;
    AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
    CHECK_AND_RETURN_LOG(ioHandle != OPEN_PORT_FAILURE,
        "OpenAudioPort failed %{public}d for InnerCapturer sink", ioHandle);
    IOHandles_[moduleInfo.name] = ioHandle;
}

void AudioPolicyService::LoadReceiverSink()
{
    AUDIO_INFO_LOG("LoadReceiverSink");
    AudioModuleInfo moduleInfo = {};
    moduleInfo.name = RECEIVER_SINK_NAME;
    moduleInfo.lib = "libmodule-receiver-sink.z.so";
    AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
    CHECK_AND_RETURN_LOG(ioHandle != OPEN_PORT_FAILURE, "OpenAudioPort failed %{public}d for Receiver sink", ioHandle);
    IOHandles_[moduleInfo.name] = ioHandle;
}

void AudioPolicyService::LoadLoopback()
{
    AudioIOHandle ioHandle;
    std::string moduleName;
    AUDIO_INFO_LOG("LoadLoopback");

    if (IOHandles_.count(INNER_CAPTURER_SINK_NAME) != 1u) {
        AUDIO_ERR_LOG("LoadLoopback failed for InnerCapturer not loaded");
        return;
    }

    LoopbackModuleInfo moduleInfo = {};
    moduleInfo.lib = "libmodule-loopback.z.so";
    moduleInfo.sink = INNER_CAPTURER_SINK_NAME;

    for (auto sceneType = AUDIO_SUPPORTED_SCENE_TYPES.begin(); sceneType != AUDIO_SUPPORTED_SCENE_TYPES.end();
        ++sceneType) {
        moduleInfo.source = sceneType->second + SINK_NAME_FOR_CAPTURE_SUFFIX + MONITOR_SOURCE_SUFFIX;
        ioHandle = audioPolicyManager_.LoadLoopback(moduleInfo);
        CHECK_AND_RETURN_LOG(ioHandle != OPEN_PORT_FAILURE, "LoadLoopback failed %{public}d", ioHandle);
        moduleName = moduleInfo.source + moduleInfo.sink;
        IOHandles_[moduleName] = ioHandle;
    }

    if (IOHandles_.count(RECEIVER_SINK_NAME) != 1u) {
        AUDIO_ERR_LOG("receiver sink not exist");
    } else {
        moduleInfo.source = RECEIVER_SINK_NAME + MONITOR_SOURCE_SUFFIX;
        ioHandle = audioPolicyManager_.LoadLoopback(moduleInfo);
        CHECK_AND_RETURN_LOG(ioHandle != OPEN_PORT_FAILURE, "LoadLoopback failed %{public}d", ioHandle);
        moduleName = moduleInfo.source + moduleInfo.sink;
        IOHandles_[moduleName] = ioHandle;
    }
}

void AudioPolicyService::UnloadLoopback()
{
    std::string module;
    AUDIO_INFO_LOG("UnloadLoopback");

    for (auto sceneType = AUDIO_SUPPORTED_SCENE_TYPES.begin(); sceneType != AUDIO_SUPPORTED_SCENE_TYPES.end();
        ++sceneType) {
        module = sceneType->second + SINK_NAME_FOR_CAPTURE_SUFFIX + MONITOR_SOURCE_SUFFIX + INNER_CAPTURER_SINK_NAME;
        if (IOHandles_.find(module) != IOHandles_.end()) {
            audioPolicyManager_.CloseAudioPort(IOHandles_[module]);
            IOHandles_.erase(module);
        }
    }

    module = RECEIVER_SINK_NAME + MONITOR_SOURCE_SUFFIX + INNER_CAPTURER_SINK_NAME;
    if (IOHandles_.find(module) != IOHandles_.end()) {
        audioPolicyManager_.CloseAudioPort(IOHandles_[module]);
        IOHandles_.erase(module);
    }
}

void AudioPolicyService::LoadEffectSinks()
{
    // Create sink for each effect
    AudioModuleInfo moduleInfo = {};
    moduleInfo.lib = "libmodule-cluster-sink.z.so";
    moduleInfo.name = "CLUSTER";
    AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
    CHECK_AND_RETURN_LOG(ioHandle != OPEN_PORT_FAILURE, "OpenAudioPort failed %{public}d", ioHandle);
    IOHandles_[moduleInfo.name] = ioHandle;

    moduleInfo.lib = "libmodule-effect-sink.z.so";
    char device[50] = {0};
    int ret = GetParameter("const.build.product", " ", device, sizeof(device));
    std::string deviceString(device);
    if (ret > 0 && deviceString.compare("default") == 0) {
        moduleInfo.rate = "44100";
    } else {
        moduleInfo.rate = "48000";
    }
    for (auto sceneType = AUDIO_SUPPORTED_SCENE_TYPES.begin(); sceneType != AUDIO_SUPPORTED_SCENE_TYPES.end();
        ++sceneType) {
        AUDIO_INFO_LOG("Initial sink for scene name %{public}s", sceneType->second.c_str());
        moduleInfo.name = sceneType->second;
        moduleInfo.sceneName.clear();
        ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
        CHECK_AND_RETURN_LOG(ioHandle != OPEN_PORT_FAILURE, "OpenAudioPort failed %{public}d", ioHandle);
        IOHandles_[moduleInfo.name] = ioHandle;

        moduleInfo.name += SINK_NAME_FOR_CAPTURE_SUFFIX;
        moduleInfo.sceneName = sceneType->second;
        AUDIO_INFO_LOG("Initial effect sink:%{public}s for capturer", moduleInfo.name.c_str());
        ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
        CHECK_AND_RETURN_LOG(ioHandle != OPEN_PORT_FAILURE, "OpenAudioPort failed %{public}d", ioHandle);
        IOHandles_[moduleInfo.name] = ioHandle;
    }
    LoadSinksForCapturer(); // repy on the success of loading effect sink
}

void AudioPolicyService::LoadEffectLibrary()
{
    // IPC -> audioservice load library
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("Service proxy unavailable: g_adProxy null");
        return;
    }
    OriginalEffectConfig oriEffectConfig = {};
    audioEffectManager_.GetOriginalEffectConfig(oriEffectConfig);
    vector<Effect> successLoadedEffects;
    bool loadSuccess = gsp->LoadAudioEffectLibraries(oriEffectConfig.libraries,
                                                     oriEffectConfig.effects,
                                                     successLoadedEffects);
    if (!loadSuccess) {
        AUDIO_ERR_LOG("Load audio effect failed, please check log");
    }

    audioEffectManager_.UpdateAvailableEffects(successLoadedEffects);
    audioEffectManager_.BuildAvailableAEConfig();

    // Initialize EffectChainManager in audio service through IPC
    SupportedEffectConfig supportedEffectConfig;
    audioEffectManager_.GetSupportedEffectConfig(supportedEffectConfig);
    std::unordered_map<std::string, std::string> sceneTypeToEffectChainNameMap;
    audioEffectManager_.ConstructSceneTypeToEffectChainNameMap(sceneTypeToEffectChainNameMap);
    bool ret = gsp->CreateEffectChainManager(supportedEffectConfig.effectChains, sceneTypeToEffectChainNameMap);
    CHECK_AND_RETURN_LOG(ret, "EffectChainManager create failed");

    audioEffectManager_.SetEffectChainManagerAvailable();
    if (audioEffectManager_.CanLoadEffectSinks()) {
        LoadEffectSinks();
    }
}

void AudioPolicyService::GetEffectManagerInfo(OriginalEffectConfig& oriEffectConfig,
                                              std::vector<Effect>& availableEffects)
{
    audioEffectManager_.GetOriginalEffectConfig(oriEffectConfig);
    audioEffectManager_.GetAvailableEffects(availableEffects);
}

void AudioPolicyService::AddAudioDevice(AudioModuleInfo& moduleInfo, InternalDeviceType devType)
{
    // add new device into active device list
    std::string volumeGroupName = GetGroupName(moduleInfo.name, VOLUME_TYPE);
    std::string interruptGroupName = GetGroupName(moduleInfo.name, INTERRUPT_TYPE);
    int32_t volumeGroupId = GROUP_ID_NONE;
    int32_t interruptGroupId = GROUP_ID_NONE;
    UpdateGroupInfo(GroupType::VOLUME_TYPE, volumeGroupName, volumeGroupId, LOCAL_NETWORK_ID, true,
        NO_REMOTE_ID);
    UpdateGroupInfo(GroupType::INTERRUPT_TYPE, interruptGroupName, interruptGroupId, LOCAL_NETWORK_ID,
        true, NO_REMOTE_ID);

    sptr<AudioDeviceDescriptor> audioDescriptor = new(std::nothrow) AudioDeviceDescriptor(devType,
        GetDeviceRole(moduleInfo.role), volumeGroupId, interruptGroupId, LOCAL_NETWORK_ID);
    if (!moduleInfo.rate.empty() && !moduleInfo.channels.empty()) {
        AudioStreamInfo streamInfo = {};
        streamInfo.samplingRate = static_cast<AudioSamplingRate>(stoi(moduleInfo.rate));
        streamInfo.channels = static_cast<AudioChannel>(stoi(moduleInfo.channels));
        audioDescriptor->SetDeviceCapability(streamInfo, 0);
    }

    audioDescriptor->deviceId_ = startDeviceId++;
    UpdateDisplayName(audioDescriptor);
    connectedDevices_.insert(connectedDevices_.begin(), audioDescriptor);
}

// Parser callbacks
void AudioPolicyService::OnXmlParsingCompleted(const std::unordered_map<ClassType, std::list<AudioModuleInfo>> &xmlData)
{
    AUDIO_INFO_LOG("AudioPolicyService::%{public}s, device class num [%{public}zu]", __func__, xmlData.size());
    if (xmlData.empty()) {
        AUDIO_ERR_LOG("failed to parse xml file. Received data is empty");
        return;
    }

    deviceClassInfo_ = xmlData;
}

void AudioPolicyService::OnVolumeGroupParsed(std::unordered_map<std::string, std::string>& volumeGroupData)
{
    AUDIO_INFO_LOG("AudioPolicyService::%{public}s, group data num [%{public}zu]", __func__, volumeGroupData.size());
    if (volumeGroupData.empty()) {
        AUDIO_ERR_LOG("failed to parse xml file. Received data is empty");
        return;
    }

    volumeGroupData_ = volumeGroupData;
}

void AudioPolicyService::OnInterruptGroupParsed(std::unordered_map<std::string, std::string>& interruptGroupData)
{
    AUDIO_INFO_LOG("AudioPolicyService::%{public}s, group data num [%{public}zu]", __func__, interruptGroupData.size());
    if (interruptGroupData.empty()) {
        AUDIO_ERR_LOG("failed to parse xml file. Received data is empty");
        return;
    }

    interruptGroupData_ = interruptGroupData;
}

int32_t AudioPolicyService::SetDeviceChangeCallback(const int32_t clientId, const DeviceFlag flag,
    const sptr<IRemoteObject> &object, bool hasBTPermission)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    sptr<IStandardAudioPolicyManagerListener> callback = iface_cast<IStandardAudioPolicyManagerListener>(object);

    if (callback != nullptr) {
        callback->hasBTPermission_ = hasBTPermission;
        deviceChangeCbsMap_[{clientId, flag}] = callback;
    }
    AUDIO_INFO_LOG("SetDeviceChangeCallback:: deviceChangeCbsMap_ size: %{public}zu", deviceChangeCbsMap_.size());
    return SUCCESS;
}

int32_t AudioPolicyService::UnsetDeviceChangeCallback(const int32_t clientId, DeviceFlag flag)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    if (deviceChangeCbsMap_.erase({clientId, flag}) == 0) {
        AUDIO_INFO_LOG("client not present in %{public}s", __func__);
    }
    // for audio manager napi remove all device change callback
    if (flag == DeviceFlag::ALL_DEVICES_FLAG) {
        for (auto it = deviceChangeCbsMap_.begin(); it != deviceChangeCbsMap_.end();) {
            if ((*it).first.first == clientId && ((*it).first.second == DeviceFlag::INPUT_DEVICES_FLAG ||
                (*it).first.second == DeviceFlag::OUTPUT_DEVICES_FLAG)) {
                it = deviceChangeCbsMap_.erase(it);
            } else {
                it++;
            }
        }
    }
    // for routing manager napi remove all device change callback
    if (flag == DeviceFlag::ALL_L_D_DEVICES_FLAG) {
        for (auto it = deviceChangeCbsMap_.begin(); it != deviceChangeCbsMap_.end();) {
            if ((*it).first.first == clientId) {
                it = deviceChangeCbsMap_.erase(it);
            } else {
                it++;
            }
        }
    }

    AUDIO_INFO_LOG("UnsetDeviceChangeCallback:: deviceChangeCbsMap_ size: %{public}zu", deviceChangeCbsMap_.size());
    return SUCCESS;
}

int32_t AudioPolicyService::SetPreferOutputDeviceChangeCallback(const int32_t clientId,
    const sptr<IRemoteObject> &object, bool hasBTPermission)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    sptr<IStandardAudioRoutingManagerListener> callback = iface_cast<IStandardAudioRoutingManagerListener>(object);
    if (callback != nullptr) {
        callback->hasBTPermission_ = hasBTPermission;
        preferOutputDeviceCbsMap_[clientId] = callback;
    }

    return SUCCESS;
}

int32_t AudioPolicyService::UnsetPreferOutputDeviceChangeCallback(const int32_t clientId)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    if (preferOutputDeviceCbsMap_.erase(clientId) == 0) {
        AUDIO_ERR_LOG("client not present in %{public}s", __func__);
        return ERR_INVALID_OPERATION;
    }

    return SUCCESS;
}

int32_t AudioPolicyService::RegisterAudioRendererEventListener(int32_t clientPid, const sptr<IRemoteObject> &object,
    bool hasBTPermission, bool hasSysPermission)
{
    return streamCollector_.RegisterAudioRendererEventListener(clientPid, object, hasBTPermission, hasSysPermission);
}

int32_t AudioPolicyService::UnregisterAudioRendererEventListener(int32_t clientPid)
{
    return streamCollector_.UnregisterAudioRendererEventListener(clientPid);
}

int32_t AudioPolicyService::RegisterAudioCapturerEventListener(int32_t clientPid, const sptr<IRemoteObject> &object,
    bool hasBTPermission, bool hasSysPermission)
{
    return streamCollector_.RegisterAudioCapturerEventListener(clientPid, object, hasBTPermission, hasSysPermission);
}

int32_t AudioPolicyService::UnregisterAudioCapturerEventListener(int32_t clientPid)
{
    return streamCollector_.UnregisterAudioCapturerEventListener(clientPid);
}

static void UpdateRendererInfoWhenNoPermission(const unique_ptr<AudioRendererChangeInfo> &audioRendererChangeInfos,
    bool hasSystemPermission)
{
    if (!hasSystemPermission) {
        audioRendererChangeInfos->clientUID = 0;
        audioRendererChangeInfos->rendererState = RENDERER_INVALID;
    }
}

static void UpdateCapturerInfoWhenNoPermission(const unique_ptr<AudioCapturerChangeInfo> &audioCapturerChangeInfos,
    bool hasSystemPermission)
{
    if (!hasSystemPermission) {
        audioCapturerChangeInfos->clientUID = 0;
        audioCapturerChangeInfos->capturerState = CAPTURER_INVALID;
    }
}

static void UpdateDeviceInfo(DeviceInfo &deviceInfo, const sptr<AudioDeviceDescriptor> &desc, bool hasBTPermission,
    bool hasSystemPermission)
{
    deviceInfo.deviceType = desc->deviceType_;
    deviceInfo.deviceRole = desc->deviceRole_;
    deviceInfo.deviceId = desc->deviceId_;
    deviceInfo.channelMasks = desc->channelMasks_;
    deviceInfo.displayName = desc->displayName_;

    if (hasBTPermission) {
        deviceInfo.deviceName = desc->deviceName_;
        deviceInfo.macAddress = desc->macAddress_;
    } else {
        deviceInfo.deviceName = "";
        deviceInfo.macAddress = "";
    }

    if (hasSystemPermission) {
        deviceInfo.networkId = desc->networkId_;
        deviceInfo.volumeGroupId = desc->volumeGroupId_;
        deviceInfo.interruptGroupId = desc->interruptGroupId_;
    } else {
        deviceInfo.networkId = "";
        deviceInfo.volumeGroupId = GROUP_ID_NONE;
        deviceInfo.interruptGroupId = GROUP_ID_NONE;
    }
    deviceInfo.audioStreamInfo.samplingRate = desc->audioStreamInfo_.samplingRate;
    deviceInfo.audioStreamInfo.encoding = desc->audioStreamInfo_.encoding;
    deviceInfo.audioStreamInfo.format = desc->audioStreamInfo_.format;
    deviceInfo.audioStreamInfo.channels = desc->audioStreamInfo_.channels;
}

void AudioPolicyService::UpdateStreamChangeDeviceInfo(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo)
{
    if (mode == AUDIO_MODE_PLAYBACK) {
        std::vector<sptr<AudioDeviceDescriptor>> outputDevices = GetDevices(OUTPUT_DEVICES_FLAG);
        DeviceType activeDeviceType = currentActiveDevice_;
        DeviceRole activeDeviceRole = OUTPUT_DEVICE;
        for (sptr<AudioDeviceDescriptor> desc : outputDevices) {
            if ((desc->deviceType_ == activeDeviceType) && (desc->deviceRole_ == activeDeviceRole)) {
                UpdateDeviceInfo(streamChangeInfo.audioRendererChangeInfo.outputDeviceInfo, desc, true, true);
                break;
            }
        }
    } else {
        std::vector<sptr<AudioDeviceDescriptor>> inputDevices = GetDevices(INPUT_DEVICES_FLAG);
        DeviceType activeDeviceType = activeInputDevice_;
        DeviceRole activeDeviceRole = INPUT_DEVICE;
        for (sptr<AudioDeviceDescriptor> desc : inputDevices) {
            if ((desc->deviceType_ == activeDeviceType) && (desc->deviceRole_ == activeDeviceRole)) {
                UpdateDeviceInfo(streamChangeInfo.audioCapturerChangeInfo.inputDeviceInfo, desc, true, true);
                break;
            }
        }
    }
}

int32_t AudioPolicyService::RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
    const sptr<IRemoteObject> &object)
{
    UpdateStreamChangeDeviceInfo(mode, streamChangeInfo);
    return streamCollector_.RegisterTracker(mode, streamChangeInfo, object);
}

int32_t AudioPolicyService::UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo)
{
    AUDIO_INFO_LOG("Entered AudioPolicyService::%{public}s", __func__);
    UpdateStreamChangeDeviceInfo(mode, streamChangeInfo);
    return streamCollector_.UpdateTracker(mode, streamChangeInfo);
}

int32_t AudioPolicyService::GetCurrentRendererChangeInfos(vector<unique_ptr<AudioRendererChangeInfo>>
    &audioRendererChangeInfos, bool hasBTPermission, bool hasSystemPermission)
{
    int32_t status = streamCollector_.GetCurrentRendererChangeInfos(audioRendererChangeInfos);
    if (status != SUCCESS) {
        AUDIO_ERR_LOG("AudioPolicyServer:: Get renderer change info failed");
        return status;
    }

    std::vector<sptr<AudioDeviceDescriptor>> outputDevices = GetDevices(OUTPUT_DEVICES_FLAG);
    DeviceType activeDeviceType = currentActiveDevice_;
    DeviceRole activeDeviceRole = OUTPUT_DEVICE;
    for (sptr<AudioDeviceDescriptor> desc : outputDevices) {
        if ((desc->deviceType_ == activeDeviceType) && (desc->deviceRole_ == activeDeviceRole)) {
            size_t rendererInfosSize = audioRendererChangeInfos.size();
            for (size_t i = 0; i < rendererInfosSize; i++) {
                UpdateRendererInfoWhenNoPermission(audioRendererChangeInfos[i], hasSystemPermission);
                UpdateDeviceInfo(audioRendererChangeInfos[i]->outputDeviceInfo, desc, hasBTPermission,
                    hasSystemPermission);
            }
        }

        return status;
    }

    return status;
}

int32_t AudioPolicyService::GetCurrentCapturerChangeInfos(vector<unique_ptr<AudioCapturerChangeInfo>>
    &audioCapturerChangeInfos, bool hasBTPermission, bool hasSystemPermission)
{
    int status = streamCollector_.GetCurrentCapturerChangeInfos(audioCapturerChangeInfos);
    if (status != SUCCESS) {
        AUDIO_ERR_LOG("AudioPolicyServer:: Get capturer change info failed");
        return status;
    }

    std::vector<sptr<AudioDeviceDescriptor>> inputDevices = GetDevices(INPUT_DEVICES_FLAG);
    DeviceType activeDeviceType = activeInputDevice_;
    DeviceRole activeDeviceRole = INPUT_DEVICE;
    for (sptr<AudioDeviceDescriptor> desc : inputDevices) {
        if ((desc->deviceType_ == activeDeviceType) && (desc->deviceRole_ == activeDeviceRole)) {
            size_t capturerInfosSize = audioCapturerChangeInfos.size();
            for (size_t i = 0; i < capturerInfosSize; i++) {
                UpdateCapturerInfoWhenNoPermission(audioCapturerChangeInfos[i], hasSystemPermission);
                UpdateDeviceInfo(audioCapturerChangeInfos[i]->inputDeviceInfo, desc, hasBTPermission,
                    hasSystemPermission);
            }
        }

        return status;
    }

    return status;
}

void AudioPolicyService::RegisteredTrackerClientDied(pid_t pid)
{
    streamCollector_.RegisteredTrackerClientDied(static_cast<int32_t>(pid));
}

void AudioPolicyService::RegisteredStreamListenerClientDied(pid_t pid)
{
    streamCollector_.RegisteredStreamListenerClientDied(static_cast<int32_t>(pid));
}

int32_t AudioPolicyService::ReconfigureAudioChannel(const uint32_t &channelCount, DeviceType deviceType)
{
    if (currentActiveDevice_ != DEVICE_TYPE_FILE_SINK) {
        AUDIO_INFO_LOG("FILE_SINK_DEVICE is not active. Cannot reconfigure now");
        return ERROR;
    }

    std::string module = FILE_SINK;

    if (deviceType == DeviceType::DEVICE_TYPE_FILE_SINK) {
        CHECK_AND_RETURN_RET_LOG(channelCount <= CHANNEL_8 && channelCount >= MONO, ERROR, "Invalid sink channel");
        module = FILE_SINK;
    } else if (deviceType == DeviceType::DEVICE_TYPE_FILE_SOURCE) {
        CHECK_AND_RETURN_RET_LOG(channelCount <= CHANNEL_6 && channelCount >= MONO, ERROR, "Invalid src channel");
        module = FILE_SOURCE;
    } else {
        AUDIO_ERR_LOG("Invalid DeviceType");
        return ERROR;
    }

    if (IOHandles_.find(module) != IOHandles_.end()) {
        audioPolicyManager_.CloseAudioPort(IOHandles_[module]);
        IOHandles_.erase(module);
    }

    auto fileClass = deviceClassInfo_.find(ClassType::TYPE_FILE_IO);
    if (fileClass != deviceClassInfo_.end()) {
        auto moduleInfoList = fileClass->second;
        for (auto &moduleInfo : moduleInfoList) {
            if (module == moduleInfo.name) {
                moduleInfo.channels = to_string(channelCount);
                AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
                IOHandles_[moduleInfo.name] = ioHandle;
                audioPolicyManager_.SetDeviceActive(ioHandle, deviceType, module, true);
            }
        }
    }

    return SUCCESS;
}

// private methods
AudioIOHandle AudioPolicyService::GetAudioIOHandle(InternalDeviceType deviceType)
{
    AudioIOHandle ioHandle;
    switch (deviceType) {
        case InternalDeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_WIRED_HEADPHONES:
        case InternalDeviceType::DEVICE_TYPE_USB_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_EARPIECE:
        case InternalDeviceType::DEVICE_TYPE_SPEAKER:
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
            ioHandle = IOHandles_[PRIMARY_SPEAKER];
            break;
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
            ioHandle = IOHandles_[BLUETOOTH_SPEAKER];
            break;
        case InternalDeviceType::DEVICE_TYPE_MIC:
            ioHandle = IOHandles_[PRIMARY_MIC];
            break;
        case InternalDeviceType::DEVICE_TYPE_WAKEUP:
            ioHandle = IOHandles_[PRIMARY_WAKEUP];
            break;
        case InternalDeviceType::DEVICE_TYPE_FILE_SINK:
            ioHandle = IOHandles_[FILE_SINK];
            break;
        case InternalDeviceType::DEVICE_TYPE_FILE_SOURCE:
            ioHandle = IOHandles_[FILE_SOURCE];
            break;
        default:
            ioHandle = IOHandles_[PRIMARY_MIC];
            break;
    }
    return ioHandle;
}

InternalDeviceType AudioPolicyService::GetDeviceType(const std::string &deviceName)
{
    InternalDeviceType devType = InternalDeviceType::DEVICE_TYPE_NONE;
    if (deviceName == "Speaker") {
        devType = InternalDeviceType::DEVICE_TYPE_SPEAKER;
    } else if (deviceName == "Built_in_mic") {
        devType = InternalDeviceType::DEVICE_TYPE_MIC;
    } else if (deviceName == "Built_in_wakeup") {
        devType = InternalDeviceType::DEVICE_TYPE_WAKEUP;
    } else if (deviceName == "fifo_output" || deviceName == "fifo_input") {
        devType = DEVICE_TYPE_BLUETOOTH_SCO;
    } else if (deviceName == "file_sink") {
        devType = DEVICE_TYPE_FILE_SINK;
    } else if (deviceName == "file_source") {
        devType = DEVICE_TYPE_FILE_SOURCE;
    }

    return devType;
}

std::string AudioPolicyService::GetGroupName(const std::string& deviceName, const GroupType type)
{
    std::string groupName = GROUP_NAME_NONE;
    if (type == VOLUME_TYPE) {
        auto iter = volumeGroupData_.find(deviceName);
        if (iter != volumeGroupData_.end()) {
            groupName = iter->second;
        }
    } else {
        auto iter = interruptGroupData_.find(deviceName);
        if (iter != interruptGroupData_.end()) {
            groupName = iter->second;
        }
    }
    return groupName;
}

void AudioPolicyService::WriteDeviceChangedSysEvents(const vector<sptr<AudioDeviceDescriptor>> &desc, bool isConnected)
{
    for (auto deviceDescriptor : desc) {
        if (deviceDescriptor != nullptr) {
            if ((deviceDescriptor->deviceType_ == DEVICE_TYPE_WIRED_HEADSET)
                || (deviceDescriptor->deviceType_ == DEVICE_TYPE_USB_HEADSET)
                || (deviceDescriptor->deviceType_ == DEVICE_TYPE_WIRED_HEADPHONES)) {
                HiSysEventWrite(HiviewDFX::HiSysEvent::Domain::AUDIO, "HEADSET_CHANGE",
                    HiviewDFX::HiSysEvent::EventType::BEHAVIOR,
                    "ISCONNECT", isConnected ? 1 : 0,
                    "HASMIC", 1,
                    "DEVICETYPE", deviceDescriptor->deviceType_);
            }

            if (!isConnected) {
                continue;
            }

            if (deviceDescriptor->deviceRole_ == OUTPUT_DEVICE) {
                vector<SinkInput> sinkInputs = audioPolicyManager_.GetAllSinkInputs();
                for (SinkInput sinkInput : sinkInputs) {
                    HiSysEventWrite(HiviewDFX::HiSysEvent::Domain::AUDIO, "DEVICE_CHANGE",
                        HiviewDFX::HiSysEvent::EventType::BEHAVIOR,
                        "ISOUTPUT", 1,
                        "STREAMID", sinkInput.streamId,
                        "STREAMTYPE", sinkInput.streamType,
                        "DEVICETYPE", deviceDescriptor->deviceType_);
                }
            } else if (deviceDescriptor->deviceRole_ == INPUT_DEVICE) {
                vector<SourceOutput> sourceOutputs = audioPolicyManager_.GetAllSourceOutputs();
                for (SourceOutput sourceOutput : sourceOutputs) {
                    HiSysEventWrite(HiviewDFX::HiSysEvent::Domain::AUDIO, "DEVICE_CHANGE",
                        HiviewDFX::HiSysEvent::EventType::BEHAVIOR,
                        "ISOUTPUT", 0,
                        "STREAMID", sourceOutput.streamId,
                        "STREAMTYPE", sourceOutput.streamType,
                        "DEVICETYPE", deviceDescriptor->deviceType_);
                }
            }
        }
    }
}

void AudioPolicyService::UpdateTrackerDeviceChange(const vector<sptr<AudioDeviceDescriptor>> &desc)
{
    AUDIO_INFO_LOG("AudioPolicyService::%{public}s IN", __func__);
    for (sptr<AudioDeviceDescriptor> deviceDesc : desc) {
        if (deviceDesc->deviceRole_ == OUTPUT_DEVICE) {
            DeviceType activeDevice = currentActiveDevice_;
            auto isPresent = [&activeDevice] (const sptr<AudioDeviceDescriptor> &desc) {
                CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid device descriptor");
                return ((activeDevice == desc->deviceType_) && (OUTPUT_DEVICE == desc->deviceRole_));
            };

            auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent);
            if (itr != connectedDevices_.end()) {
                DeviceInfo outputDevice = {};
                UpdateDeviceInfo(outputDevice, *itr, true, true);
                streamCollector_.UpdateTracker(AUDIO_MODE_PLAYBACK, outputDevice);
            }
        }

        if (deviceDesc->deviceRole_ == INPUT_DEVICE) {
            DeviceType activeDevice = activeInputDevice_;
            auto isPresent = [&activeDevice] (const sptr<AudioDeviceDescriptor> &desc) {
                CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid device descriptor");
                return ((activeDevice == desc->deviceType_) && (INPUT_DEVICE == desc->deviceRole_));
            };

            auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent);
            if (itr != connectedDevices_.end()) {
                DeviceInfo inputDevice = {};
                UpdateDeviceInfo(inputDevice, *itr, true, true);
                streamCollector_.UpdateTracker(AUDIO_MODE_RECORD, inputDevice);
            }
        }
    }
}

void AudioPolicyService::UpdateGroupInfo(GroupType type, std::string groupName, int32_t& groupId, std::string networkId,
    bool connected, int32_t mappingId)
{
    ConnectType connectType = CONNECT_TYPE_LOCAL;
    if (networkId != LOCAL_NETWORK_ID) {
        connectType = CONNECT_TYPE_DISTRIBUTED;
    }
    if (type == GroupType::VOLUME_TYPE) {
        auto isPresent = [&groupName, &networkId] (const sptr<VolumeGroupInfo> &volumeInfo) {
            return ((groupName == volumeInfo->groupName_) || (networkId == volumeInfo->networkId_));
        };

        auto iter = std::find_if(volumeGroups_.begin(), volumeGroups_.end(), isPresent);
        if (iter != volumeGroups_.end()) {
            groupId = (*iter)->volumeGroupId_;
            // if status is disconnected, remove the group that has none audio device
            std::vector<sptr<AudioDeviceDescriptor>> devsInGroup = GetDevicesForGroup(type, groupId);
            if (!connected && devsInGroup.size() == 0) {
                volumeGroups_.erase(iter);
            }
            return;
        }
        if (groupName != GROUP_NAME_NONE && connected) {
            groupId = AudioGroupHandle::GetInstance().GetNextId(type);
            sptr<VolumeGroupInfo> volumeGroupInfo = new(std::nothrow) VolumeGroupInfo(groupId,
                mappingId, groupName, networkId, connectType);
            volumeGroups_.push_back(volumeGroupInfo);
        }
    } else {
        auto isPresent = [&groupName, &networkId] (const sptr<InterruptGroupInfo> &info) {
            return ((groupName == info->groupName_) || (networkId == info->networkId_));
        };

        auto iter = std::find_if(interruptGroups_.begin(), interruptGroups_.end(), isPresent);
        if (iter != interruptGroups_.end()) {
            groupId = (*iter)->interruptGroupId_;
            // if status is disconnected, remove the group that has none audio device
            std::vector<sptr<AudioDeviceDescriptor>> devsInGroup = GetDevicesForGroup(type, groupId);
            if (!connected && devsInGroup.size() == 0) {
                interruptGroups_.erase(iter);
            }
            return;
        }
        if (groupName != GROUP_NAME_NONE && connected) {
            groupId = AudioGroupHandle::GetInstance().GetNextId(type);
            sptr<InterruptGroupInfo> interruptGroupInfo = new(std::nothrow) InterruptGroupInfo(groupId, mappingId,
                groupName, networkId, connectType);
            interruptGroups_.push_back(interruptGroupInfo);
        }
    }
}

std::vector<sptr<OHOS::AudioStandard::AudioDeviceDescriptor>> AudioPolicyService::GetDevicesForGroup(GroupType type,
    int32_t groupId)
{
    std::vector<sptr<OHOS::AudioStandard::AudioDeviceDescriptor>> devices = {};
    for (auto devDes : connectedDevices_) {
        if (devDes == nullptr) {
            continue;
        }
        bool inVolumeGroup = type == VOLUME_TYPE && devDes->volumeGroupId_ == groupId;
        bool inInterruptGroup = type == INTERRUPT_TYPE && devDes->interruptGroupId_ == groupId;

        if (inVolumeGroup || inInterruptGroup) {
            sptr<AudioDeviceDescriptor> device = new AudioDeviceDescriptor(*devDes);
            devices.push_back(device);
        }
    }
    return devices;
}

void AudioPolicyService::UpdateDescWhenNoBTPermission(vector<sptr<AudioDeviceDescriptor>> &deviceDescs)
{
    AUDIO_WARNING_LOG("UpdateDescWhenNoBTPermission: No bt permission");

    for (sptr<AudioDeviceDescriptor> &desc : deviceDescs) {
        if ((desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP)
            || (desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_SCO)) {
            sptr<AudioDeviceDescriptor> copyDesc = new AudioDeviceDescriptor(desc);
            copyDesc->deviceName_ = "";
            copyDesc->macAddress_ = "";
            desc = copyDesc;
        }
    }
}

void AudioPolicyService::TriggerDeviceChangedCallback(const vector<sptr<AudioDeviceDescriptor>> &desc, bool isConnected)
{
    Trace trace("AudioPolicyService::TriggerDeviceChangedCallback");
    DeviceChangeAction deviceChangeAction;
    deviceChangeAction.type = isConnected ? DeviceChangeType::CONNECT : DeviceChangeType::DISCONNECT;

    WriteDeviceChangedSysEvents(desc, isConnected);

    for (auto it = deviceChangeCbsMap_.begin(); it != deviceChangeCbsMap_.end(); ++it) {
        deviceChangeAction.flag = it->first.second;
        deviceChangeAction.deviceDescriptors = DeviceFilterByFlag(it->first.second, desc);
        if (it->second && deviceChangeAction.deviceDescriptors.size() > 0) {
            if (!(it->second->hasBTPermission_)) {
                UpdateDescWhenNoBTPermission(deviceChangeAction.deviceDescriptors);
            }
            it->second->OnDeviceChange(deviceChangeAction);
        }
    }
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyService::DeviceFilterByFlag(DeviceFlag flag,
    const std::vector<sptr<AudioDeviceDescriptor>>& desc)
{
    std::vector<sptr<AudioDeviceDescriptor>> descRet;
    DeviceRole role = DEVICE_ROLE_NONE;
    switch (flag) {
        case DeviceFlag::ALL_DEVICES_FLAG:
            for (sptr<AudioDeviceDescriptor> var : desc) {
                if (var->networkId_ == LOCAL_NETWORK_ID) {
                    descRet.insert(descRet.end(), var);
                }
            }
            break;
        case DeviceFlag::ALL_DISTRIBUTED_DEVICES_FLAG:
            for (sptr<AudioDeviceDescriptor> var : desc) {
                if (var->networkId_ != LOCAL_NETWORK_ID) {
                    descRet.insert(descRet.end(), var);
                }
            }
            break;
        case DeviceFlag::ALL_L_D_DEVICES_FLAG:
            descRet = desc;
            break;
        case DeviceFlag::OUTPUT_DEVICES_FLAG:
        case DeviceFlag::INPUT_DEVICES_FLAG:
            role = flag == INPUT_DEVICES_FLAG ? INPUT_DEVICE : OUTPUT_DEVICE;
            for (sptr<AudioDeviceDescriptor> var : desc) {
                if (var->networkId_ == LOCAL_NETWORK_ID && var->deviceRole_ == role) {
                    descRet.insert(descRet.end(), var);
                }
            }
            break;
        case DeviceFlag::DISTRIBUTED_OUTPUT_DEVICES_FLAG:
        case DeviceFlag::DISTRIBUTED_INPUT_DEVICES_FLAG:
            role = flag == DISTRIBUTED_INPUT_DEVICES_FLAG ? INPUT_DEVICE : OUTPUT_DEVICE;
            for (sptr<AudioDeviceDescriptor> var : desc) {
                if (var->networkId_ != LOCAL_NETWORK_ID && var->deviceRole_ == role) {
                    descRet.insert(descRet.end(), var);
                }
            }
            break;
        default:
            AUDIO_INFO_LOG("AudioPolicyService::%{public}s:deviceFlag type are not supported", __func__);
            break;
    }
    return descRet;
}

bool AudioPolicyService::IsInputDevice(DeviceType deviceType) const
{
    switch (deviceType) {
        case DeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
        case DeviceType::DEVICE_TYPE_MIC:
        case DeviceType::DEVICE_TYPE_WAKEUP:
        case DeviceType::DEVICE_TYPE_USB_HEADSET:
            return true;
        default:
            return false;
    }
}

bool AudioPolicyService::IsOutputDevice(DeviceType deviceType) const
{
    switch (deviceType) {
        case DeviceType::DEVICE_TYPE_EARPIECE:
        case DeviceType::DEVICE_TYPE_SPEAKER:
        case DeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case DeviceType::DEVICE_TYPE_WIRED_HEADPHONES:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
        case DeviceType::DEVICE_TYPE_USB_HEADSET:
            return true;
        default:
            return false;
    }
}

DeviceRole AudioPolicyService::GetDeviceRole(DeviceType deviceType) const
{
    switch (deviceType) {
        case DeviceType::DEVICE_TYPE_SPEAKER:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
        case DeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case DeviceType::DEVICE_TYPE_WIRED_HEADPHONES:
        case DeviceType::DEVICE_TYPE_USB_HEADSET:
            return DeviceRole::OUTPUT_DEVICE;
        case DeviceType::DEVICE_TYPE_MIC:
        case DeviceType::DEVICE_TYPE_WAKEUP:
            return DeviceRole::INPUT_DEVICE;
        default:
            return DeviceRole::DEVICE_ROLE_NONE;
    }
}

DeviceRole AudioPolicyService::GetDeviceRole(const std::string &role)
{
    if (role == ROLE_SINK) {
        return DeviceRole::OUTPUT_DEVICE;
    } else if (role == ROLE_SOURCE) {
        return DeviceRole::INPUT_DEVICE;
    } else {
        return DeviceRole::DEVICE_ROLE_NONE;
    }
}

DeviceRole AudioPolicyService::GetDeviceRole(AudioPin pin) const
{
    switch (pin) {
        case OHOS::AudioStandard::AUDIO_PIN_NONE:
            return DeviceRole::DEVICE_ROLE_NONE;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_SPEAKER:
        case OHOS::AudioStandard::AUDIO_PIN_OUT_HEADSET:
        case OHOS::AudioStandard::AUDIO_PIN_OUT_LINEOUT:
        case OHOS::AudioStandard::AUDIO_PIN_OUT_HDMI:
        case OHOS::AudioStandard::AUDIO_PIN_OUT_USB:
        case OHOS::AudioStandard::AUDIO_PIN_OUT_USB_EXT:
        case OHOS::AudioStandard::AUDIO_PIN_OUT_DAUDIO_DEFAULT:
            return DeviceRole::OUTPUT_DEVICE;
        case OHOS::AudioStandard::AUDIO_PIN_IN_MIC:
        case OHOS::AudioStandard::AUDIO_PIN_IN_HS_MIC:
        case OHOS::AudioStandard::AUDIO_PIN_IN_LINEIN:
        case OHOS::AudioStandard::AUDIO_PIN_IN_USB_EXT:
        case OHOS::AudioStandard::AUDIO_PIN_IN_DAUDIO_DEFAULT:
            return DeviceRole::INPUT_DEVICE;
        default:
            return DeviceRole::DEVICE_ROLE_NONE;
    }
}

void AudioPolicyService::OnAudioLatencyParsed(uint64_t latency)
{
    audioLatencyInMsec_ = latency;
}

int32_t AudioPolicyService::GetAudioLatencyFromXml() const
{
    return audioLatencyInMsec_;
}

void AudioPolicyService::OnSinkLatencyParsed(uint32_t latency)
{
    sinkLatencyInMsec_ = latency;
}

uint32_t AudioPolicyService::GetSinkLatencyFromXml() const
{
    return sinkLatencyInMsec_;
}

void AudioPolicyService::UpdateInputDeviceInfo(DeviceType deviceType)
{
    AUDIO_DEBUG_LOG("Current input device is %{public}d", activeInputDevice_);

    switch (deviceType) {
        case DEVICE_TYPE_EARPIECE:
        case DEVICE_TYPE_SPEAKER:
        case DEVICE_TYPE_BLUETOOTH_A2DP:
            activeInputDevice_ = DEVICE_TYPE_MIC;
            break;
        case DEVICE_TYPE_FILE_SINK:
            activeInputDevice_ = DEVICE_TYPE_FILE_SOURCE;
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
        case DEVICE_TYPE_USB_HEADSET:
        case DEVICE_TYPE_BLUETOOTH_SCO:
            activeInputDevice_ = deviceType;
            break;
        default:
            break;
    }

    AUDIO_DEBUG_LOG("Input device updated to %{public}d", activeInputDevice_);
}

int32_t AudioPolicyService::UpdateStreamState(int32_t clientUid,
    StreamSetStateEventInternal &streamSetStateEventInternal)
{
    return streamCollector_.UpdateStreamState(clientUid, streamSetStateEventInternal);
}

DeviceType AudioPolicyService::GetDeviceTypeFromPin(AudioPin hdiPin)
{
    switch (hdiPin) {
        case OHOS::AudioStandard::AUDIO_PIN_NONE:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_SPEAKER:
        case OHOS::AudioStandard::AUDIO_PIN_OUT_DAUDIO_DEFAULT:
            return DeviceType::DEVICE_TYPE_SPEAKER;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_HEADSET:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_LINEOUT:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_HDMI:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_USB:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_OUT_USB_EXT:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_IN_MIC:
        case OHOS::AudioStandard::AUDIO_PIN_IN_DAUDIO_DEFAULT:
            return DeviceType::DEVICE_TYPE_MIC;
        case OHOS::AudioStandard::AUDIO_PIN_IN_HS_MIC:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_IN_LINEIN:
            break;
        case OHOS::AudioStandard::AUDIO_PIN_IN_USB_EXT:
            break;
        default:
            break;
    }
    return DeviceType::DEVICE_TYPE_DEFAULT;
}

std::vector<sptr<VolumeGroupInfo>> AudioPolicyService::GetVolumeGroupInfos()
{
    std::vector<sptr<VolumeGroupInfo>> volumeGroupInfos = {};

    for (auto& v : volumeGroups_) {
        sptr<VolumeGroupInfo> info = new(std::nothrow) VolumeGroupInfo(v->volumeGroupId_, v->mappingId_, v->groupName_,
            v->networkId_, v->connectType_);
        volumeGroupInfos.push_back(info);
    }
    return volumeGroupInfos;
}

void AudioPolicyService::RegiestPolicy()
{
    AUDIO_INFO_LOG("Enter RegiestPolicy");
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("RegiestPolicy g_adProxy null");
        return;
    }
    sptr<IRemoteObject> object = this->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("RegiestPolicy AsObject is nullptr");
        return;
    }
    int32_t ret = gsp->RegiestPolicyProvider(object);
    AUDIO_INFO_LOG("RegiestPolicy result:%{public}d", ret);
}

int32_t AudioPolicyService::GetProcessDeviceInfo(const AudioProcessConfig &config, DeviceInfo &deviceInfo)
{
    AUDIO_INFO_LOG("%{public}s", ProcessConfig::DumpProcessConfig(config).c_str());
    // todo
    // check process in routerMap, return target device for it
    // put the currentActiveDevice_ in deviceinfo, so it can create with current using device.
    // genarate the unique deviceid?

    if (config.audioMode == AUDIO_MODE_RECORD) {
        deviceInfo.deviceId = 1;
        if (config.isRemote) {
            deviceInfo.networkId = "remote_mmap_dmic";
        } else {
            deviceInfo.networkId = LOCAL_NETWORK_ID;
        }
        deviceInfo.deviceRole = INPUT_DEVICE;
        deviceInfo.deviceType = DEVICE_TYPE_MIC;
    } else {
        deviceInfo.deviceId = 6; // 6 for test
        if (config.isRemote) {
            deviceInfo.networkId = REMOTE_NETWORK_ID;
            deviceInfo.deviceType = DEVICE_TYPE_SPEAKER;
        } else {
            deviceInfo.networkId = LOCAL_NETWORK_ID;
            deviceInfo.deviceType = currentActiveDevice_;
        }
        deviceInfo.deviceRole = OUTPUT_DEVICE;
    }
    AudioStreamInfo targetStreamInfo = {SAMPLE_RATE_48000, ENCODING_PCM, SAMPLE_S16LE, STEREO}; // note: read from xml
    deviceInfo.audioStreamInfo = targetStreamInfo;
    deviceInfo.deviceName = "mmap_device";
    return SUCCESS;
}

int32_t AudioPolicyService::InitSharedVolume(std::shared_ptr<AudioSharedMemory> &buffer)
{
    CHECK_AND_RETURN_RET_LOG(policyVolumeMap_ != nullptr && policyVolumeMap_->GetBase() != nullptr,
        ERR_OPERATION_FAILED, "Get shared memory failed!");

    // init volume map
    // todo device
    for (size_t i = 0; i < IPolicyProvider::GetVolumeVectorSize(); i++) {
        float volFloat = GetSystemVolumeDb(g_volumeIndexVector[i].first);
        volumeVector_[i].isMute = false;
        volumeVector_[i].volumeFloat = volFloat;
        volumeVector_[i].volumeInt = 0;
    }
    buffer = policyVolumeMap_;

    return SUCCESS;
}

bool AudioPolicyService::GetSharedVolume(AudioStreamType streamType, DeviceType deviceType, Volume &vol)
{
    CHECK_AND_RETURN_RET_LOG(volumeVector_ != nullptr, false, "Get shared memory failed!");
    size_t index = 0;
    if (!IPolicyProvider::GetVolumeIndex(streamType, deviceType, index) ||
        index >= IPolicyProvider::GetVolumeVectorSize()) {
        return false;
    }
    vol.isMute = volumeVector_[index].isMute;
    vol.volumeFloat = volumeVector_[index].volumeFloat;
    vol.volumeInt = volumeVector_[index].volumeInt;
    return true;
}

bool AudioPolicyService::SetSharedVolume(AudioStreamType streamType, DeviceType deviceType, Volume vol)
{
    CHECK_AND_RETURN_RET_LOG(volumeVector_ != nullptr, false, "Set shared memory failed!");
    size_t index = 0;
    if (!IPolicyProvider::GetVolumeIndex(streamType, deviceType, index) ||
        index >= IPolicyProvider::GetVolumeVectorSize()) {
        return false;
    }
    volumeVector_[index].isMute = vol.isMute;
    volumeVector_[index].volumeFloat = vol.volumeFloat;
    volumeVector_[index].volumeInt = vol.volumeInt;
    return true;
}

void AudioPolicyService::SetParameterCallback(const std::shared_ptr<AudioParameterCallback>& callback)
{
    AUDIO_INFO_LOG("Enter SetParameterCallback");
    auto parameterChangeCbStub = new(std::nothrow) AudioManagerListenerStub();
    if (parameterChangeCbStub == nullptr) {
        AUDIO_ERR_LOG("SetParameterCallback parameterChangeCbStub null");
        return;
    }
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetParameterCallback g_adProxy null");
        return;
    }
    parameterChangeCbStub->SetParameterCallback(callback);

    sptr<IRemoteObject> object = parameterChangeCbStub->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("SetParameterCallback listenerStub object is nullptr");
        delete parameterChangeCbStub;
        return;
    }
    AUDIO_INFO_LOG("SetParameterCallback done");
    gsp->SetParameterCallback(object);
}

void AudioPolicyService::SetWakeupCloseCallback(const std::shared_ptr<WakeUpSourceCallback>& callback)
{
    AUDIO_INFO_LOG("Enter SetWakeupCloseCallback");
    auto wakeupCloseCbStub = new(std::nothrow) AudioManagerListenerStub();
    if (wakeupCloseCbStub == nullptr) {
        AUDIO_ERR_LOG("wakeupCloseCbStub is null");
        return;
    }
    wakeupCloseCbStub->SetWakeupCloseCallback(callback);
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetWakeupCloseCallback gsp null");
        return;
    }
    sptr<IRemoteObject> object = wakeupCloseCbStub->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("SetWakeupCloseCallback listenerStub object is nullptr");
        delete wakeupCloseCbStub;
        return;
    }
    gsp->SetWakeupCloseCallback(object);
}

int32_t AudioPolicyService::GetMaxRendererInstances()
{
    // init max renderer instances before kvstore start by local prop for bootanimation
    char currentMaxRendererInstances[100] = {0}; // 100 for system parameter usage
    auto ret = GetParameter("persist.multimedia.audio.maxrendererinstances", "16",
        currentMaxRendererInstances, sizeof(currentMaxRendererInstances));
    if (ret > 0) {
        maxRendererInstances_ = atoi(currentMaxRendererInstances);
        AUDIO_INFO_LOG("Get max renderer instances success %{public}d", maxRendererInstances_);
    } else {
        AUDIO_ERR_LOG("Get max renderer instances failed %{public}d", ret);
    }
    return maxRendererInstances_;
}

#ifdef BLUETOOTH_ENABLE
const sptr<IStandardAudioService> RegisterBluetoothDeathCallback()
{
    lock_guard<mutex> lock(g_btProxyMutex);
    if (g_btProxy == nullptr) {
        auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        if (samgr == nullptr) {
            AUDIO_ERR_LOG("RegisterBluetoothDeathCallback: get sa manager failed");
            return nullptr;
        }
        sptr<IRemoteObject> object = samgr->GetSystemAbility(BLUETOOTH_HOST_SYS_ABILITY_ID);
        if (object == nullptr) {
            AUDIO_ERR_LOG("RegisterBluetoothDeathCallback: get audio service remote object failed");
            return nullptr;
        }
        g_btProxy = iface_cast<IStandardAudioService>(object);
        if (g_btProxy == nullptr) {
            AUDIO_ERR_LOG("RegisterBluetoothDeathCallback: get audio service proxy failed");
            return nullptr;
        }

        // register death recipent
        sptr<AudioServerDeathRecipient> asDeathRecipient = new(std::nothrow) AudioServerDeathRecipient(getpid());
        if (asDeathRecipient != nullptr) {
            asDeathRecipient->SetNotifyCb(std::bind(&AudioPolicyService::BluetoothServiceCrashedCallback,
                std::placeholders::_1));
            bool result = object->AddDeathRecipient(asDeathRecipient);
            if (!result) {
                AUDIO_ERR_LOG("RegisterBluetoothDeathCallback: failed to add deathRecipient");
            }
        }
    }
    sptr<IStandardAudioService> gasp = g_btProxy;
    return gasp;
}

void AudioPolicyService::BluetoothServiceCrashedCallback(pid_t pid)
{
    AUDIO_INFO_LOG("Bluetooth sa crashed, will restore proxy in next call");
    lock_guard<mutex> lock(g_btProxyMutex);
    g_btProxy = nullptr;
    Bluetooth::AudioA2dpManager::DisconnectBluetoothA2dpSink();
}
#endif

void AudioPolicyService::RegisterBluetoothListener()
{
#ifdef BLUETOOTH_ENABLE
    AUDIO_INFO_LOG("Enter AudioPolicyService::RegisterBluetoothListener");
    Bluetooth::RegisterDeviceObserver(deviceStatusListener_->deviceObserver_);
    Bluetooth::AudioA2dpManager::RegisterBluetoothA2dpListener();
    Bluetooth::AudioHfpManager::RegisterBluetoothScoListener();
    isBtListenerRegistered = true;
    const sptr<IStandardAudioService> gsp = RegisterBluetoothDeathCallback();
#endif
}

void AudioPolicyService::UnregisterBluetoothListener()
{
#ifdef BLUETOOTH_ENABLE
    AUDIO_INFO_LOG("Enter AudioPolicyService::UnregisterBluetoothListener");
    Bluetooth::UnregisterDeviceObserver();
    Bluetooth::AudioA2dpManager::UnregisterBluetoothA2dpListener();
    Bluetooth::AudioHfpManager::UnregisterBluetoothScoListener();
    isBtListenerRegistered = false;
#endif
}

void AudioPolicyService::SubscribeAccessibilityConfigObserver()
{
#ifdef ACCESSIBILITY_ENABLE
    accessibilityConfigListener_->SubscribeObserver();
    AUDIO_INFO_LOG("Subscribe accessibility config observer successfully");
#endif
}

float AudioPolicyService::GetMinStreamVolume()
{
    return audioPolicyManager_.GetMinStreamVolume();
}

float AudioPolicyService::GetMaxStreamVolume()
{
    return audioPolicyManager_.GetMaxStreamVolume();
}

bool AudioPolicyService::IsVolumeUnadjustable()
{
    return audioPolicyManager_.IsVolumeUnadjustable();
}

void AudioPolicyService::GetStreamVolumeInfoMap(StreamVolumeInfoMap &streamVolumeInfoMap)
{
    return audioPolicyManager_.GetStreamVolumeInfoMap(streamVolumeInfoMap);
}

float AudioPolicyService::GetSystemVolumeInDb(AudioVolumeType volumeType, int32_t volumeLevel, DeviceType deviceType)
{
    return audioPolicyManager_.GetSystemVolumeInDb(volumeType, volumeLevel, deviceType);
}

int32_t AudioPolicyService::QueryEffectManagerSceneMode(SupportedEffectConfig& supportedEffectConfig)
{
    int32_t ret = audioEffectManager_.QueryEffectManagerSceneMode(supportedEffectConfig);
    return ret;
}

void AudioPolicyService::RegisterDataObserver()
{
    CreateDataShareHelperInstance();
    std::string devicesName = "";
    int32_t ret = GetDeviceNameFromDataShareHelper(devicesName);
    AUDIO_INFO_LOG("RegisterDataObserver::UpdateDisplayName local name [%{public}s]", devicesName.c_str());
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("Local UpdateDisplayName init device failed");
        return;
    }
    SetDisplayName(devicesName, true);
    RegisterNameMonitorHelper();
}

int32_t AudioPolicyService::SetPlaybackCapturerFilterInfos(const CaptureFilterOptions &options)
{
    LoadLoopback();
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetPlaybackCapturerFilterInfos error for g_adProxy null");
        return ERR_OPERATION_FAILED;
    }

    std::vector<int32_t> targetUsages;
    AUDIO_INFO_LOG("SetPlaybackCapturerFilterInfos");
    for (size_t i = 0; i < options.usages.size(); i++) {
        if (count(targetUsages.begin(), targetUsages.end(), options.usages[i]) == 0) {
            targetUsages.emplace_back(options.usages[i]); // deduplicate
        }
    }

    return gsp->SetSupportStreamUsage(targetUsages);
}

} // namespace AudioStandard
} // namespace OHOS
