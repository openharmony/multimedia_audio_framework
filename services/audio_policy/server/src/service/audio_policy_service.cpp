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
#include "audio_spatialization_service.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;

static const std::string INNER_CAPTURER_SINK_NAME = "InnerCapturer";
static const std::string RECEIVER_SINK_NAME = "Receiver";
static const std::string SINK_NAME_FOR_CAPTURE_SUFFIX = "_CAP";
static const std::string MONITOR_SOURCE_SUFFIX = ".monitor";

static const std::string SETTINGS_DATA_BASE_URI =
    "datashare:///com.ohos.settingsdata/entry/settingsdata/SETTINGSDATA?Proxy=true";
static const std::string SETTINGS_DATA_EXT_URI = "datashare:///com.ohos.settingsdata.DataAbility";
static const std::string SETTINGS_DATA_FIELD_KEYWORD = "KEYWORD";
static const std::string SETTINGS_DATA_FIELD_VALUE = "VALUE";
static const std::string PREDICATES_STRING = "settings.general.device_name";
static const char MAX_RENDERER_INSTANCE[100] = "128"; // 100 for system parameter usage
const uint32_t PCM_8_BIT = 8;
const uint32_t PCM_16_BIT = 16;
const uint32_t PCM_24_BIT = 24;
const uint32_t PCM_32_BIT = 32;
const uint32_t BT_BUFFER_ADJUSTMENT_FACTOR = 50;
const uint32_t ABS_VOLUME_SUPPORT_RETRY_INTERVAL_IN_MICROSECONDS = 10000;
const uint32_t USER_NOT_SELECT_BT = 1;
const uint32_t USER_SELECT_BT = 2;
const std::string AUDIO_SERVICE_PKG = "audio_manager_service";
const uint32_t MEDIA_SERVICE_UID = 1013;
const uint32_t AUDIO_UID = 1041;
std::shared_ptr<DataShare::DataShareHelper> g_dataShareHelper = nullptr;
static sptr<IStandardAudioService> g_adProxy = nullptr;
#ifdef BLUETOOTH_ENABLE
static sptr<IStandardAudioService> g_btProxy = nullptr;
#endif
static int32_t startDeviceId = 1;
static int32_t startMicrophoneId = 1;
mutex g_adProxyMutex;
mutex g_dataShareHelperMutex;
#ifdef BLUETOOTH_ENABLE
mutex g_btProxyMutex;
#endif
bool AudioPolicyService::isBtListenerRegistered = false;

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
    audioDeviceManager_.ParseDeviceXml();
    audioPnpServer_.init();

    if (!configParser_.LoadConfiguration()) {
        AUDIO_ERR_LOG("Audio Config Load Configuration failed");
        return false;
    }
    if (!configParser_.Parse()) {
        AUDIO_ERR_LOG("Audio Config Parse failed");
        return false;
    }
    MaxRenderInstanceInit();

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

    int32_t status = deviceStatusListener_->RegisterDeviceStatusListener();
    CHECK_AND_RETURN_RET_LOG(status != SUCCESS, false, "[Policy Service] Register for device status events failed");

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
    audioPnpServer_.StopPnpServer();

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
    int32_t result = SUCCESS;
    // if current active device's type is DEVICE_TYPE_BLUETOOTH_A2DP and it support absolute volume, set
    // its absolute volume value.

    if (IsStreamActive(streamType) && currentActiveDevice_.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
        result = SetA2dpDeviceVolume(activeBTDevice_, volumeLevel);
#ifdef BLUETOOTH_ENABLE
        if (result == SUCCESS) {
            // set to avrcp device
            SetOffloadVolume(volumeLevel);
            return Bluetooth::AudioA2dpManager::SetDeviceAbsVolume(activeBTDevice_, volumeLevel);
        } else {
            AUDIO_ERR_LOG("AudioPolicyService::SetSystemVolumeLevel set abs volume failed");
        }
#endif
    }

    result = audioPolicyManager_.SetSystemVolumeLevel(streamType, volumeLevel, isFromVolumeKey);
    if (result == SUCCESS && streamType == STREAM_VOICE_CALL) {
        SetVoiceCallVolume(volumeLevel);
    }
    // todo
    Volume vol = {false, 1.0f, 0};
    vol.volumeFloat = GetSystemVolumeInDb(streamType, volumeLevel, currentActiveDevice_.deviceType_);
    SetSharedVolume(streamType, currentActiveDevice_.deviceType_, vol);

    if (result == SUCCESS) {
        SetOffloadVolume(volumeLevel);
    }
    return result;
}

void AudioPolicyService::SetVoiceCallVolume(int32_t volumeLevel)
{
    Trace trace("AudioPolicyService::SetVoiceCallVolume" + std::to_string(volumeLevel));
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

void AudioPolicyService::SetOffloadVolume(int32_t volume)
{
    DeviceType dev = GetActiveOutputDevice();
    if (!(dev == DEVICE_TYPE_SPEAKER || dev == DEVICE_TYPE_BLUETOOTH_A2DP)) {
        return;
    }
    const sptr <IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("gsp null");
        return;
    }
    float volumeDb;
    if (dev == DEVICE_TYPE_BLUETOOTH_A2DP) {
        volumeDb = static_cast<float>(volume) / static_cast<float>(audioPolicyManager_.GetMaxVolumeLevel(STREAM_MUSIC));
    } else {
        volumeDb = GetSystemVolumeInDb(STREAM_MUSIC, volume, currentActiveDevice_.deviceType_);
    }
    gsp->OffloadSetVolume(volumeDb);
}

void AudioPolicyService::SetVolumeForSwitchDevice(DeviceType deviceType)
{
    Trace trace("AudioPolicyService::SetVolumeForSwitchDevice:" + std::to_string(deviceType));
    // Load volume from KvStore and set volume for each stream type
    audioPolicyManager_.SetVolumeForSwitchDevice(deviceType);

    // The volume of voice_call needs to be adjusted separately
    if (audioScene_ == AUDIO_SCENE_PHONE_CALL) {
        SetVoiceCallVolume(GetSystemVolumeLevel(STREAM_VOICE_CALL));
    }
    if (deviceType == DEVICE_TYPE_SPEAKER) {
        SetOffloadVolume(GetSystemVolumeLevel(STREAM_MUSIC));
    } else if (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        int32_t volume;
        if (GetA2dpDeviceVolume(activeBTDevice_, volume)) {
            SetOffloadVolume(volume);
        } else {
            SetOffloadVolume(GetSystemVolumeLevel(STREAM_MUSIC));
        }
    }
}

std::string AudioPolicyService::GetVolumeGroupType(DeviceType deviceType)
{
    std::string volumeGroupType = "";
    switch (deviceType) {
        case DEVICE_TYPE_EARPIECE:
        case DEVICE_TYPE_SPEAKER:
            volumeGroupType = "build-in";
            break;
        case DEVICE_TYPE_BLUETOOTH_A2DP:
        case DEVICE_TYPE_BLUETOOTH_SCO:
            volumeGroupType = "wireless";
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
        case DEVICE_TYPE_USB_HEADSET:
        case DEVICE_TYPE_USB_ARM_HEADSET:
            volumeGroupType = "wired";
            break;
        default:
            AUDIO_ERR_LOG("GetVolumeGroupType: device %{public}d is not supported", deviceType);
            break;
    }
    return volumeGroupType;
}

int32_t AudioPolicyService::GetSystemVolumeLevel(AudioStreamType streamType, bool isFromVolumeKey) const
{
    {
        // if current active device's type is DEVICE_TYPE_BLUETOOTH_A2DP and it support absolute volume, get
        // its absolute volume value.
        std::lock_guard<std::mutex> lock(a2dpDeviceMapMutex_);
        if (IsStreamActive(streamType) && currentActiveDevice_.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
            auto configInfoPos = connectedA2dpDeviceMap_.find(activeBTDevice_);
            if (configInfoPos != connectedA2dpDeviceMap_.end()
                && configInfoPos->second.absVolumeSupport) {
                return configInfoPos->second.mute ? 0 : configInfoPos->second.volumeLevel;
            } else {
                AUDIO_ERR_LOG("Get absolute volume failed for activeBTDevice :[%{public}s]", activeBTDevice_.c_str());
            }
        }
    }
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


void AudioPolicyService::SetOffloadMode()
{
    if (!GetOffloadAvailableFromXml()) {
        AUDIO_INFO_LOG("Offload not available, skipped");
        return;
    }

    AUDIO_INFO_LOG("sessionId: %{public}d, PowerState: %{public}d, isAppBack: %{public}d",
        *offloadSessionID_, static_cast<int32_t>(currentPowerState_), currentOffloadSessionIsBackground_);

    streamCollector_.SetOffloadMode(*offloadSessionID_, static_cast<int32_t>(currentPowerState_),
        currentOffloadSessionIsBackground_);
}

void AudioPolicyService::ResetOffloadMode()
{
    AUDIO_DEBUG_LOG("Doing reset offload mode!");

    if (!CheckActiveOutputDeviceSupportOffload()) {
        AUDIO_DEBUG_LOG("Resetting offload not available on this output device! Release.");
        OffloadStreamReleaseCheck(*offloadSessionID_);
        return;
    }

    int32_t runningStreamId = streamCollector_.GetRunningStream(STREAM_MUSIC);
    if (runningStreamId == -1) {
        AUDIO_DEBUG_LOG("No running STREAM_MUSIC, wont restart offload");
        runningStreamId = streamCollector_.GetRunningStream(STREAM_SPEECH);
        if (runningStreamId == -1) {
            AUDIO_DEBUG_LOG("No running STREAM_SPEECH, wont restart offload");
            return;
        }
    }
    OffloadStreamSetCheck(runningStreamId);
}

void AudioPolicyService::OffloadStreamSetCheck(uint32_t sessionId)
{
    if (!GetOffloadAvailableFromXml()) {
        AUDIO_INFO_LOG("Offload not available, skipped for set");
        return;
    }

    if (!CheckActiveOutputDeviceSupportOffload()) {
        AUDIO_INFO_LOG("Offload not available on current output device, skipped");
        return;
    }

    AudioStreamType streamType = GetStreamType(sessionId);
    if ((streamType != STREAM_MUSIC) && (streamType != STREAM_SPEECH)) {
        AUDIO_DEBUG_LOG("StreamType not allowed get offload mode, Skipped");
        return;
    }

    int32_t offloadUID = GetUid(sessionId);
    if (offloadUID == -1) {
        AUDIO_DEBUG_LOG("offloadUID not valid, Skipped");
        return;
    }
    if (offloadUID == MEDIA_SERVICE_UID || offloadUID == AUDIO_UID) { // not support avplayer in current version
        AUDIO_DEBUG_LOG("Skip avplayer out of offload mode");
        return;
    }

    auto CallingUid = IPCSkeleton::GetCallingUid();
    if (CallingUid == MEDIA_SERVICE_UID || CallingUid == AUDIO_UID) { // not support avplayer in current version
        AUDIO_DEBUG_LOG("Skip avplayer out of offload mode");
        return;
    }

    AUDIO_INFO_LOG("sessionId[%{public}d] UID[%{public}d] StreamType[%{public}d] Getting offload stream",
        sessionId, offloadUID, streamType);
    lock_guard<mutex> lock(offloadMutex_);

    if (!offloadSessionID_.has_value()) {
        offloadSessionID_ = sessionId;

        AUDIO_DEBUG_LOG("sessionId[%{public}d] try get offload stream", sessionId);
        SetOffloadMode();
    } else {
        if (sessionId == *(offloadSessionID_)) {
            AUDIO_DEBUG_LOG("sessionId[%{public}d] is already get offload stream", sessionId);
        } else {
            AUDIO_DEBUG_LOG("sessionId[%{public}d] no get offload, current offload sessionId[%{public}d]",
                sessionId, *(offloadSessionID_));
        }
    }

    return;
}

void AudioPolicyService::OffloadStreamReleaseCheck(uint32_t sessionId)
{
    if (!GetOffloadAvailableFromXml()) {
        AUDIO_INFO_LOG("Offload not available, skipped for release");
        return;
    }

    lock_guard<mutex> lock(offloadMutex_);

    if (((*offloadSessionID_) == sessionId) && offloadSessionID_.has_value()) {
        AUDIO_DEBUG_LOG("Doing unset offload mode!");
        streamCollector_.UnsetOffloadMode(*offloadSessionID_);
        offloadSessionID_.reset();
        AUDIO_DEBUG_LOG("sessionId[%{public}d] release offload stream", sessionId);
    } else {
        if (offloadSessionID_.has_value()) {
            AUDIO_DEBUG_LOG("sessionId[%{public}d] stopping stream not get offload, current offload [%{public}d]",
                sessionId, *offloadSessionID_);
        } else {
            AUDIO_DEBUG_LOG("sessionId[%{public}d] stopping stream not get offload, current offload stream is None",
                sessionId);
        }
    }
    return;
}

bool AudioPolicyService::CheckActiveOutputDeviceSupportOffload()
{
    DeviceType dev = GetActiveOutputDevice();
    return dev == DEVICE_TYPE_SPEAKER || (dev == DEVICE_TYPE_BLUETOOTH_A2DP && a2dpOffloadFlag_ == A2DP_OFFLOAD);
}

void AudioPolicyService::SetOffloadAvailableFromXML(AudioModuleInfo &moduleInfo)
{
    if (moduleInfo.name == "Speaker") {
        for (auto &portInfo : moduleInfo.ports) {
            if ((portInfo.adapterName == "primary") && (portInfo.offloadEnable == "1")) {
                isOffloadAvailable_ = true;
            }
        }
    }
}

bool AudioPolicyService::GetOffloadAvailableFromXml() const
{
    return isOffloadAvailable_;
}

void AudioPolicyService::HandlePowerStateChanged(PowerMgr::PowerState state)
{
    if (!CheckActiveOutputDeviceSupportOffload()) {
        return;
    }
    if (currentPowerState_ == state) {
        return;
    }
    currentPowerState_ = state;
    if (offloadSessionID_.has_value()) {
        AUDIO_DEBUG_LOG("SetOffloadMode! Offload power is state = %{public}d", state);
        SetOffloadMode();
    }
}

float AudioPolicyService::GetSingleStreamVolume(int32_t streamId) const
{
    return streamCollector_.GetSingleStreamVolume(streamId);
}

int32_t AudioPolicyService::SetStreamMute(AudioStreamType streamType, bool mute)
{
    int32_t result = SUCCESS;
    // if current active device's type is DEVICE_TYPE_BLUETOOTH_A2DP and it support absolute volume, set
    // its absolute volume value.
    if (IsStreamActive(streamType) && currentActiveDevice_.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
        std::lock_guard<std::mutex> lock(a2dpDeviceMapMutex_);
        auto configInfoPos = connectedA2dpDeviceMap_.find(activeBTDevice_);
        if (configInfoPos == connectedA2dpDeviceMap_.end() || !configInfoPos->second.absVolumeSupport) {
            AUDIO_WARNING_LOG("SetStreamMute failed for macAddress:[%{public}s]", activeBTDevice_.c_str());
        } else {
            configInfoPos->second.mute = mute;
#ifdef BLUETOOTH_ENABLE
            // set to avrcp device
            if (mute) {
                return Bluetooth::AudioA2dpManager::SetDeviceAbsVolume(activeBTDevice_, 0);
            } else {
                return Bluetooth::AudioA2dpManager::SetDeviceAbsVolume(activeBTDevice_,
                    configInfoPos->second.volumeLevel);
            }
#endif
        }
    }
    result = audioPolicyManager_.SetStreamMute(streamType, mute);

    Volume vol = {false, 1.0f, 0};
    vol.isMute = mute;
    vol.volumeInt = GetSystemVolumeLevel(streamType);
    vol.volumeFloat = GetSystemVolumeInDb(streamType, vol.volumeInt, currentActiveDevice_.deviceType_);
    SetSharedVolume(streamType, currentActiveDevice_.deviceType_, vol);
    return result;
}

int32_t AudioPolicyService::SetSourceOutputStreamMute(int32_t uid, bool setMute) const
{
    int32_t status = audioPolicyManager_.SetSourceOutputStreamMute(uid, setMute);
    if (status > 0) {
        streamCollector_.UpdateCapturerInfoMuteStatus(uid, setMute);
    }
    return status;
}


bool AudioPolicyService::GetStreamMute(AudioStreamType streamType) const
{
    if (IsStreamActive(streamType) && currentActiveDevice_.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
        std::lock_guard<std::mutex> lock(a2dpDeviceMapMutex_);
        auto configInfoPos = connectedA2dpDeviceMap_.find(activeBTDevice_);
        if (configInfoPos == connectedA2dpDeviceMap_.end() || !configInfoPos->second.absVolumeSupport) {
            AUDIO_WARNING_LOG("GetStreamMute failed for macAddress:[%{public}s]", activeBTDevice_.c_str());
        } else {
            return configInfoPos->second.mute;
        }
    }
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
    AUDIO_DEBUG_LOG("NotifyRemoteRenderState move [%{public}zu] of all [%{public}zu]sink-inputs to local.",
        targetSinkInputs.size(), sinkInputs.size());
    sptr<AudioDeviceDescriptor> localDevice = new(std::nothrow) AudioDeviceDescriptor();
    if (localDevice == nullptr) {
        AUDIO_ERR_LOG("Device error: null device.");
        return;
    }
    localDevice->networkId_ = LOCAL_NETWORK_ID;
    localDevice->deviceRole_ = DeviceRole::OUTPUT_DEVICE;
    localDevice->deviceType_ = DeviceType::DEVICE_TYPE_SPEAKER;

    int32_t ret;
    if (localDevice->deviceType_ != currentActiveDevice_.deviceType_) {
        AUDIO_WARNING_LOG("device[%{public}d] not active, use device[%{public}d] instead.",
            static_cast<int32_t>(localDevice->deviceType_), static_cast<int32_t>(currentActiveDevice_.deviceType_));
        ret = MoveToLocalOutputDevice(targetSinkInputs, new AudioDeviceDescriptor(currentActiveDevice_));
    } else {
        ret = MoveToLocalOutputDevice(targetSinkInputs, localDevice);
    }
    CHECK_AND_RETURN_LOG((ret == SUCCESS), "MoveToLocalOutputDevice failed!");

    // Suspend device, notify audio stream manager that device has been changed.
    ret = audioPolicyManager_.SuspendAudioDevice(networkId, true);
    CHECK_AND_RETURN_LOG((ret == SUCCESS), "SuspendAudioDevice failed!");

    std::vector<sptr<AudioDeviceDescriptor>> desc = {};
    desc.push_back(localDevice);
    UpdateTrackerDeviceChange(desc);
    OnPreferredOutputDeviceUpdated(currentActiveDevice_);
    AUDIO_DEBUG_LOG("NotifyRemoteRenderState success");
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
                && connectedDevices_[i]->networkId_ == audioDeviceDescriptors->networkId_
                && connectedDevices_[i]->macAddress_ == audioDeviceDescriptors->macAddress_) {
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
    if (audioRendererFilter->rendererInfo.rendererFlags == STREAM_FLAG_FAST) {
        return SelectFastOutputDevice(audioRendererFilter, audioDeviceDescriptors[0]);
    }
    if (audioDeviceDescriptors[0]->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP ||
        audioDeviceDescriptors[0]->deviceType_ == DEVICE_TYPE_BLUETOOTH_SCO) {
        audioDeviceDescriptors[0]->isEnable_ = true;
        audioDeviceManager_.UpdateDevicesListInfo(audioDeviceDescriptors[0], ENABLE_UPDATE);
    }
    StreamUsage strUsage = audioRendererFilter->rendererInfo.streamUsage;
    if (strUsage == STREAM_USAGE_VOICE_COMMUNICATION || strUsage == STREAM_USAGE_VOICE_MODEM_COMMUNICATION) {
        audioStateManager_.SetPerferredCallRenderDevice(audioDeviceDescriptors[0]);
    } else {
        audioStateManager_.SetPerferredMediaRenderDevice(audioDeviceDescriptors[0]);
    }
    if (currentActiveDevice_.deviceType_ == DEVICE_TYPE_BLUETOOTH_SCO &&
       audioDeviceDescriptors[0]->deviceType_ != DEVICE_TYPE_BLUETOOTH_SCO) {
        Bluetooth::SendUserSelectionEvent(DEVICE_TYPE_BLUETOOTH_SCO,
            currentActiveDevice_.macAddress_, USER_NOT_SELECT_BT);
    }
    if (currentActiveDevice_.deviceType_ != DEVICE_TYPE_BLUETOOTH_SCO &&
       audioDeviceDescriptors[0]->deviceType_ == DEVICE_TYPE_BLUETOOTH_SCO) {
        Bluetooth::SendUserSelectionEvent(DEVICE_TYPE_BLUETOOTH_SCO,
            audioDeviceDescriptors[0]->macAddress_, USER_SELECT_BT);
    }
    if (currentActiveDevice_.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP &&
       audioDeviceDescriptors[0]->deviceType_ != DEVICE_TYPE_BLUETOOTH_A2DP) {
        Bluetooth::SendUserSelectionEvent(DEVICE_TYPE_BLUETOOTH_A2DP,
            currentActiveDevice_.macAddress_, USER_NOT_SELECT_BT);
    }
    if (currentActiveDevice_.deviceType_ != DEVICE_TYPE_BLUETOOTH_A2DP &&
       audioDeviceDescriptors[0]->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
        Bluetooth::SendUserSelectionEvent(DEVICE_TYPE_BLUETOOTH_A2DP,
            audioDeviceDescriptors[0]->macAddress_, USER_SELECT_BT);
    }
    std::string networkId = audioDeviceDescriptors[0]->networkId_;
    DeviceType deviceType = audioDeviceDescriptors[0]->deviceType_;
    if ((deviceType != DEVICE_TYPE_BLUETOOTH_A2DP) || (networkId != LOCAL_NETWORK_ID)) {
        preA2dpOffloadFlag_ = NO_A2DP_DEVICE;
        a2dpOffloadFlag_ = NO_A2DP_DEVICE;
    } else {
        UpdateA2dpOffloadFlagForAllStream(deviceType);
    }
    FetchDevice(true);
    FetchDevice(false);
    return SUCCESS;
}

int32_t AudioPolicyService::SelectFastOutputDevice(sptr<AudioRendererFilter> audioRendererFilter,
    sptr<AudioDeviceDescriptor> deviceDescriptor)
{
    AUDIO_INFO_LOG("SelectFastOutputDevice for uid[%{public}d] device[%{public}s]", audioRendererFilter->uid,
        deviceDescriptor->networkId_.c_str());
    // note: check if stream is already running
    // if is running, call moveProcessToEndpoint.

    // otherwises, keep router info in the map
    std::lock_guard<std::mutex> lock(routerMapMutex_);
    fastRouterMap_[audioRendererFilter->uid] = std::make_pair(deviceDescriptor->networkId_, OUTPUT_DEVICE);
    return SUCCESS;
}

std::vector<SinkInput> AudioPolicyService::FilterSinkInputs(int32_t sessionId)
{
    // find sink-input id with audioRendererFilter
    std::vector<SinkInput> targetSinkInputs = {};
    std::vector<SinkInput> sinkInputs = audioPolicyManager_.GetAllSinkInputs();

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
        if (sessionId == sinkInputs[i].streamId) {
            targetSinkInputs.push_back(sinkInputs[i]);
        }
    }
    return targetSinkInputs;
}

std::vector<SourceOutput> AudioPolicyService::FilterSourceOutputs(int32_t sessionId)
{
    std::vector<SourceOutput> targetSourceOutputs = {};
    std::vector<SourceOutput> sourceOutputs = audioPolicyManager_.GetAllSourceOutputs();

    for (size_t i = 0; i < sourceOutputs.size(); i++) {
        AUDIO_DEBUG_LOG("sourceOutput[%{public}zu]:%{public}s", i, PrintSourceOutput(sourceOutputs[i]).c_str());
        if (sessionId == sourceOutputs[i].streamId) {
            targetSourceOutputs.push_back(sourceOutputs[i]);
        }
    }
    return targetSourceOutputs;
}

std::vector<SinkInput> AudioPolicyService::FilterSinkInputs(sptr<AudioRendererFilter> audioRendererFilter,
    bool moveAll)
{
    int32_t targetUid = audioRendererFilter->uid;
    AudioStreamType targetStreamType = audioRendererFilter->streamType;
    // find sink-input id with audioRendererFilter
    std::vector<SinkInput> targetSinkInputs = {};
    std::vector<SinkInput> sinkInputs = audioPolicyManager_.GetAllSinkInputs();

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
    return targetSinkInputs;
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

    // start move.
    uint32_t sinkId = -1; // invalid sink id, use sink name instead.
    std::string sinkName = GetSinkPortName(localDeviceDescriptor->deviceType_);
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

    std::lock_guard<std::shared_mutex> lock(deviceStatusUpdateSharedMutex_);
    connectedDevices_.erase(std::remove_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent),
        connectedDevices_.end());
    UpdateDisplayName(remoteDeviceDescriptor);
    connectedDevices_.insert(connectedDevices_.begin(), remoteDeviceDescriptor);
    AddMicrophoneDescriptor(remoteDeviceDescriptor);
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

int32_t AudioPolicyService::SelectFastInputDevice(sptr<AudioCapturerFilter> audioCapturerFilter,
    sptr<AudioDeviceDescriptor> deviceDescriptor)
{
    // note: check if stream is already running
    // if is running, call moveProcessToEndpoint.

    // otherwises, keep router info in the map
    std::lock_guard<std::mutex> lock(routerMapMutex_);
    fastRouterMap_[audioCapturerFilter->uid] = std::make_pair(deviceDescriptor->networkId_, INPUT_DEVICE);
    AUDIO_INFO_LOG("SelectFastInputDevice for uid[%{public}d] device[%{public}s]", audioCapturerFilter->uid,
        deviceDescriptor->networkId_.c_str());
    return SUCCESS;
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

    if (audioCapturerFilter->capturerInfo.capturerFlags == STREAM_FLAG_FAST && audioDeviceDescriptors.size() == 1) {
        return SelectFastInputDevice(audioCapturerFilter, audioDeviceDescriptors[0]);
    }

    AudioScene scene = GetAudioScene(true);
    SourceType srcType = audioCapturerFilter->capturerInfo.sourceType;
    if (scene == AUDIO_SCENE_PHONE_CALL || scene == AUDIO_SCENE_PHONE_CHAT ||
        srcType == SOURCE_TYPE_VOICE_COMMUNICATION) {
        audioStateManager_.SetPerferredCallCaptureDevice(audioDeviceDescriptors[0]);
    } else {
        audioStateManager_.SetPerferredRecordCaptureDevice(audioDeviceDescriptors[0]);
    }
    FetchDevice(false);
    return SUCCESS;
}

int32_t AudioPolicyService::MoveToLocalInputDevice(std::vector<SourceOutput> sourceOutputs,
    sptr<AudioDeviceDescriptor> localDeviceDescriptor)
{
    AUDIO_INFO_LOG("MoveToLocalInputDevice start");
    // check
    if (LOCAL_NETWORK_ID != localDeviceDescriptor->networkId_) {
        AUDIO_ERR_LOG("MoveToLocalInputDevice failed: not a local device.");
        return ERR_INVALID_OPERATION;
    }

    // start move.
    uint32_t sourceId = -1; // invalid source id, use source name instead.
    std::string sourceName = GetSourcePortName(localDeviceDescriptor->deviceType_);
    for (size_t i = 0; i < sourceOutputs.size(); i++) {
        if (audioPolicyManager_.MoveSourceOutputByIndexOrName(sourceOutputs[i].paStreamId, sourceId, sourceName)
            != SUCCESS) {
            AUDIO_DEBUG_LOG("move [%{public}d] to local failed", sourceOutputs[i].paStreamId);
            return ERROR;
        }
    }

    return SUCCESS;
}

int32_t AudioPolicyService::MoveToRemoteInputDevice(std::vector<SourceOutput> sourceOutputs,
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
    for (size_t i = 0; i < sourceOutputs.size(); i++) {
        if (audioPolicyManager_.MoveSourceOutputByIndexOrName(sourceOutputs[i].paStreamId, sourceId, moduleName)
            != SUCCESS) {
            AUDIO_DEBUG_LOG("move [%{public}d] failed", sourceOutputs[i].paStreamId);
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

    return streamCollector_.IsStreamActive(streamType);
}

void AudioPolicyService::ConfigDistributedRoutingRole(const sptr<AudioDeviceDescriptor> descriptor, CastType type)
{
    StoreDistributedRoutingRoleInfo(descriptor, type);
    FetchDevice(true);
    FetchDevice(false);
}

void AudioPolicyService::StoreDistributedRoutingRoleInfo(const sptr<AudioDeviceDescriptor> descriptor, CastType type)
{
    distributedRoutingInfo_.descriptor = descriptor;
    distributedRoutingInfo_.type = type;
}

DistributedRoutingInfo AudioPolicyService::GetDistributedRoutingRoleInfo()
{
    return distributedRoutingInfo_;
}

bool AudioPolicyService::IsIncomingDeviceInRemoteDevice(vector<unique_ptr<AudioDeviceDescriptor>> &descriptors,
    sptr<AudioDeviceDescriptor> incomingDevice)
{
    for (const auto &desc : descriptors) {
        if (desc != nullptr) {
            if (desc->deviceRole_ == incomingDevice->deviceRole_ &&
                desc->deviceType_ == incomingDevice->deviceType_ &&
                desc->interruptGroupId_ == incomingDevice->interruptGroupId_ &&
                desc->volumeGroupId_ == incomingDevice->volumeGroupId_ &&
                desc->networkId_ == incomingDevice->networkId_ &&
                desc->macAddress_ == incomingDevice->macAddress_) {
                return true;
            }
        }
    }
    return false;
}

std::string AudioPolicyService::GetSinkPortName(InternalDeviceType deviceType)
{
    std::string portName = PORT_NONE;
    if (deviceType == DEVICE_TYPE_USB_HEADSET && isArmUsbDevice_) {
        deviceType = DEVICE_TYPE_USB_ARM_HEADSET;
    }
    switch (deviceType) {
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
            if (a2dpOffloadFlag_ == A2DP_OFFLOAD) {
                portName = PRIMARY_SPEAKER;
            } else {
                portName = BLUETOOTH_SPEAKER;
            }
            break;
        case InternalDeviceType::DEVICE_TYPE_EARPIECE:
        case InternalDeviceType::DEVICE_TYPE_SPEAKER:
        case InternalDeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_WIRED_HEADPHONES:
        case InternalDeviceType::DEVICE_TYPE_USB_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
            portName = PRIMARY_SPEAKER;
            break;
        case InternalDeviceType::DEVICE_TYPE_USB_ARM_HEADSET:
            portName = USB_SPEAKER;
            break;
        case InternalDeviceType::DEVICE_TYPE_FILE_SINK:
            portName = FILE_SINK;
            break;
        default:
            portName = PORT_NONE;
            break;
    }

    return portName;
}

std::string AudioPolicyService::GetSourcePortName(InternalDeviceType deviceType)
{
    std::string portName = PORT_NONE;
    if (deviceType == DEVICE_TYPE_USB_HEADSET && isArmUsbDevice_) {
        deviceType = DEVICE_TYPE_USB_ARM_HEADSET;
    }
    switch (deviceType) {
        case InternalDeviceType::DEVICE_TYPE_MIC:
            portName = PRIMARY_MIC;
            break;
        case InternalDeviceType::DEVICE_TYPE_USB_ARM_HEADSET:
            portName = USB_MIC;
            break;
        case InternalDeviceType::DEVICE_TYPE_WAKEUP:
            portName = PRIMARY_WAKEUP;
            break;
        case InternalDeviceType::DEVICE_TYPE_FILE_SOURCE:
            portName = FILE_SOURCE;
            break;
        default:
            portName = PORT_NONE;
            break;
    }

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
AudioModuleInfo AudioPolicyService::ConstructWakeUpAudioModuleInfo(int32_t wakeupNo)
{
    AudioModuleInfo audioModuleInfo = {};
    audioModuleInfo.lib = "libmodule-hdi-source.z.so";
    audioModuleInfo.format = "s16le";

    audioModuleInfo.name = WAKEUP_NAMES[wakeupNo];
    audioModuleInfo.networkId = "LocalDevice";

    audioModuleInfo.adapterName = "primary";
    audioModuleInfo.className = "primary";
    audioModuleInfo.fileName = "";

    audioModuleInfo.channels = "1";
    audioModuleInfo.rate = "16000";
    audioModuleInfo.bufferSize = "1280";
    audioModuleInfo.OpenMicSpeaker = "1";

    audioModuleInfo.sourceType = std::to_string(SourceType::SOURCE_TYPE_WAKEUP);
    return audioModuleInfo;
}

void AudioPolicyService::OnPreferredOutputDeviceUpdated(const AudioDeviceDescriptor& deviceDescriptor)
{
    Trace trace("AudioPolicyService::OnPreferredOutputDeviceUpdated:" + std::to_string(deviceDescriptor.deviceType_));
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    if (audioPolicyServerHandler_ != nullptr) {
        audioPolicyServerHandler_->SendPreferredOutputDeviceUpdated();
    }
    spatialDeviceMap_.insert(make_pair(deviceDescriptor.macAddress_, deviceDescriptor.deviceType_));
    UpdateEffectDefaultSink(deviceDescriptor.deviceType_);
    AudioSpatializationService::GetAudioSpatializationService().UpdateCurrentDevice(deviceDescriptor.macAddress_);
    ResetOffloadMode();
}

void AudioPolicyService::OnPreferredInputDeviceUpdated(DeviceType deviceType, std::string networkId)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    if (audioPolicyServerHandler_ != nullptr) {
        audioPolicyServerHandler_->SendPreferredInputDeviceUpdated();
    }
}

void AudioPolicyService::OnPreferredDeviceUpdated(const AudioDeviceDescriptor& activeOutputDevice,
    DeviceType activeInputDevice)
{
    OnPreferredOutputDeviceUpdated(activeOutputDevice);
    OnPreferredInputDeviceUpdated(activeInputDevice, LOCAL_NETWORK_ID);
}

int32_t AudioPolicyService::SetWakeUpAudioCapturer([[maybe_unused]] InternalAudioCapturerOptions options)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    int32_t wakeupNo = 0;
    {
        std::lock_guard<std::mutex> lock(wakeupCountMutex_);
        if (wakeupCount_ < 0) {
            AUDIO_ERR_LOG("wakeupCount_ = %{public}d", wakeupCount_);
            wakeupCount_ = 0;
        }
        if (wakeupCount_ >= WAKEUP_LIMIT) {
            return ERROR;
        }
        wakeupNo = wakeupCount_;
        wakeupCount_++;
    }

    AudioModuleInfo moduleInfo = ConstructWakeUpAudioModuleInfo(wakeupNo);
    AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
    CHECK_AND_RETURN_RET_LOG(ioHandle != OPEN_PORT_FAILURE, ERR_OPERATION_FAILED,
        "OpenAudioPort failed %{public}d", ioHandle);

    {
        std::lock_guard<std::mutex> lck(ioHandlesMutex_);
        IOHandles_[moduleInfo.name] = ioHandle;
    }

    AUDIO_DEBUG_LOG("SetWakeUpAudioCapturer Active Success!");
    return wakeupNo;
}

int32_t AudioPolicyService::CloseWakeUpAudioCapturer()
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    {
        std::lock_guard<std::mutex> lock(wakeupCountMutex_);
        wakeupCount_--;
        if (wakeupCount_ > 0) {
            return SUCCESS;
        }
        for (auto key : WAKEUP_NAMES) {
            AudioIOHandle ioHandle;
            {
                std::lock_guard<std::mutex> lck(ioHandlesMutex_);
                auto ioHandleIter = IOHandles_.find(std::string(key));
                if (ioHandleIter == IOHandles_.end()) {
                    AUDIO_ERR_LOG("CloseWakeUpAudioCapturer failed");
                    continue;
                } else {
                    ioHandle = ioHandleIter->second;
                    IOHandles_.erase(ioHandleIter);
                }
            }
            audioPolicyManager_.CloseAudioPort(ioHandle);
        }
    }
    return SUCCESS;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyService::GetDevices(DeviceFlag deviceFlag)
{
    AUDIO_DEBUG_LOG("Entered %{public}s", __func__);

    std::shared_lock<std::shared_mutex> lock(deviceStatusUpdateSharedMutex_);

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

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyService::GetPreferredOutputDeviceDescriptors(
    AudioRendererInfo &rendererInfo, std::string networkId)
{
    std::vector<sptr<AudioDeviceDescriptor>> deviceList = {};
    if (rendererInfo.streamUsage <= STREAM_USAGE_UNKNOWN ||
        rendererInfo.streamUsage > STREAM_USAGE_VOICE_MODEM_COMMUNICATION) {
        sptr<AudioDeviceDescriptor> devDesc = new(std::nothrow) AudioDeviceDescriptor(currentActiveDevice_);
        deviceList.push_back(devDesc);
        return deviceList;
    }
    if (networkId == LOCAL_NETWORK_ID) {
        unique_ptr<AudioDeviceDescriptor> desc = audioRouterCenter_.FetchOutputDevice(rendererInfo.streamUsage, -1);
        sptr<AudioDeviceDescriptor> devDesc = new(std::nothrow) AudioDeviceDescriptor(*desc);
        deviceList.push_back(devDesc);
    } else {
        vector<unique_ptr<AudioDeviceDescriptor>> descs = audioDeviceManager_.GetRemoteRenderDevices();
        for (auto &desc : descs) {
            sptr<AudioDeviceDescriptor> devDesc = new(std::nothrow) AudioDeviceDescriptor(*desc);
            deviceList.push_back(devDesc);
        }
    }

    return deviceList;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyService::GetPreferredInputDeviceDescriptors(
    AudioCapturerInfo &captureInfo, std::string networkId)
{
    std::vector<sptr<AudioDeviceDescriptor>> deviceList = {};
    if (captureInfo.sourceType <= SOURCE_TYPE_INVALID ||
        captureInfo.sourceType > SOURCE_TYPE_MAX) {
        sptr<AudioDeviceDescriptor> devDesc = new(std::nothrow) AudioDeviceDescriptor(currentActiveInputDevice_);
        deviceList.push_back(devDesc);
        return deviceList;
    }
    if (networkId == LOCAL_NETWORK_ID) {
        unique_ptr<AudioDeviceDescriptor> desc = audioRouterCenter_.FetchInputDevice(captureInfo.sourceType, -1);
        if (desc->deviceType_ == DEVICE_TYPE_NONE && captureInfo.sourceType == SOURCE_TYPE_PLAYBACK_CAPTURE) {
            desc->deviceType_ = DEVICE_TYPE_MIC;
            desc->deviceRole_ = INPUT_DEVICE;
        }
        sptr<AudioDeviceDescriptor> devDesc = new(std::nothrow) AudioDeviceDescriptor(*desc);
        deviceList.push_back(devDesc);
    } else {
        vector<unique_ptr<AudioDeviceDescriptor>> descs = audioDeviceManager_.GetRemoteCaptureDevices();
        for (auto &desc : descs) {
            sptr<AudioDeviceDescriptor> devDesc = new(std::nothrow) AudioDeviceDescriptor(*desc);
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
            if ((audioScene_ != AUDIO_SCENE_DEFAULT) && (desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP)) {
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
    AUDIO_INFO_LOG("FetchHighPriorityDevice: priorityDevice: %{public}d, currentActiveDevice_.deviceType_: %{public}d",
        priorityDevice, currentActiveDevice_.deviceType_);
    return priorityDevice;
}

void AudioPolicyService::UpdateActiveDeviceRoute(InternalDeviceType deviceType)
{
    AUDIO_INFO_LOG("UpdateActiveDeviceRoute Device type[%{public}d]", deviceType);

    if (g_adProxy == nullptr) {
        AUDIO_ERR_LOG("Audio Server Proxy is null");
        return;
    }
    auto ret = SUCCESS;

    if (deviceType == DEVICE_TYPE_USB_HEADSET && isArmUsbDevice_) {
        deviceType = DEVICE_TYPE_USB_ARM_HEADSET;
    }

    switch (deviceType) {
        case DEVICE_TYPE_BLUETOOTH_SCO:
        case DEVICE_TYPE_USB_HEADSET:
        case DEVICE_TYPE_WIRED_HEADSET:
        case DEVICE_TYPE_USB_ARM_HEADSET: {
            ret = g_adProxy->UpdateActiveDeviceRoute(deviceType, DeviceFlag::ALL_DEVICES_FLAG);
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "Failed to update the route for %{public}d", deviceType);
            break;
        }
        case DEVICE_TYPE_WIRED_HEADPHONES:
        case DEVICE_TYPE_BLUETOOTH_A2DP:
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

void AudioPolicyService::SelectNewOutputDevice(unique_ptr<AudioRendererChangeInfo> &rendererChangeInfo,
    unique_ptr<AudioDeviceDescriptor> &outputDevice, bool isStreamStatusUpdated = false)
{
    std::vector<SinkInput> targetSinkInputs = FilterSinkInputs(rendererChangeInfo->sessionId);

    // MoveSinkInputByIndexOrName
    auto ret = (outputDevice->networkId_ == LOCAL_NETWORK_ID)
                ? MoveToLocalOutputDevice(targetSinkInputs, new AudioDeviceDescriptor(*outputDevice))
                : MoveToRemoteOutputDevice(targetSinkInputs, new AudioDeviceDescriptor(*outputDevice));
    CHECK_AND_RETURN_LOG((ret == SUCCESS), "Move sink input %{public}d to device %{public}d failed!",
        rendererChangeInfo->sessionId, outputDevice->deviceType_);
    if (isUpdateRouteSupported_) {
        UpdateActiveDeviceRoute(outputDevice->deviceType_);
    }
    UpdateDeviceInfo(rendererChangeInfo->outputDeviceInfo, new AudioDeviceDescriptor(*outputDevice), true, true);
    if (!isStreamStatusUpdated) {
        streamCollector_.UpdateRendererDeviceInfo(rendererChangeInfo->clientUID, rendererChangeInfo->sessionId,
            rendererChangeInfo->outputDeviceInfo);
    }
    ResetOffloadMode();
}

void AudioPolicyService::SelectNewInputDevice(unique_ptr<AudioCapturerChangeInfo> &capturerChangeInfo,
    unique_ptr<AudioDeviceDescriptor> &inputDevice, bool isStreamStatusUpdated = false)
{
    std::vector<SourceOutput> targetSourceOutputs = FilterSourceOutputs(capturerChangeInfo->sessionId);

    // MoveSourceOuputByIndexName
    auto ret = (inputDevice->networkId_ == LOCAL_NETWORK_ID)
                ? MoveToLocalInputDevice(targetSourceOutputs, new AudioDeviceDescriptor(*inputDevice))
                : MoveToRemoteInputDevice(targetSourceOutputs, new AudioDeviceDescriptor(*inputDevice));
    CHECK_AND_RETURN_LOG((ret == SUCCESS), "Move source output %{public}d to device %{public}d failed!",
        capturerChangeInfo->sessionId, inputDevice->deviceType_);
    if (isUpdateRouteSupported_) {
        UpdateActiveDeviceRoute(inputDevice->deviceType_);
    }
    UpdateDeviceInfo(capturerChangeInfo->inputDeviceInfo, new AudioDeviceDescriptor(*inputDevice), true, true);
    if (!isStreamStatusUpdated) {
        streamCollector_.UpdateCapturerDeviceInfo(capturerChangeInfo->clientUID, capturerChangeInfo->sessionId,
            capturerChangeInfo->inputDeviceInfo);
    }
}

void AudioPolicyService::FetchOutputDeviceWhenNoRunningStream()
{
    AUDIO_INFO_LOG("fetch output device when no running stream");
    unique_ptr<AudioDeviceDescriptor> desc = audioRouterCenter_.FetchOutputDevice(STREAM_USAGE_MEDIA, -1);
    if (desc->deviceType_ == DEVICE_TYPE_NONE || IsSameDevice(desc, currentActiveDevice_)) {
        AUDIO_INFO_LOG("output device is not change");
        return;
    }
    if (GetVolumeGroupType(currentActiveDevice_.deviceType_) != GetVolumeGroupType(desc->deviceType_)) {
        SetVolumeForSwitchDevice(desc->deviceType_);
    }
    currentActiveDevice_ = AudioDeviceDescriptor(*desc);
    AUDIO_INFO_LOG("currentActiveDevice update %{public}d", currentActiveDevice_.deviceType_);
    OnPreferredOutputDeviceUpdated(currentActiveDevice_);
}

void AudioPolicyService::FetchInputDeviceWhenNoRunningStream()
{
    AUDIO_INFO_LOG("fetch input device when no running stream");
    unique_ptr<AudioDeviceDescriptor> desc = audioRouterCenter_.FetchInputDevice(SOURCE_TYPE_MIC, -1);
    if (desc->deviceType_ == DEVICE_TYPE_NONE || IsSameDevice(desc, currentActiveInputDevice_)) {
        AUDIO_INFO_LOG("input device is not change");
        return;
    }
    currentActiveInputDevice_ = AudioDeviceDescriptor(*desc);
    AUDIO_INFO_LOG("currentActiveInputDevice update %{public}d", currentActiveInputDevice_.deviceType_);
    OnPreferredInputDeviceUpdated(currentActiveInputDevice_.deviceType_, currentActiveInputDevice_.networkId_);
}

int32_t AudioPolicyService::ActivateA2dpDevice(unique_ptr<AudioDeviceDescriptor> &desc,
    vector<unique_ptr<AudioRendererChangeInfo>> &rendererChangeInfos, bool isStreamStatusUpdated)
{
    sptr<AudioDeviceDescriptor> deviceDesc = new AudioDeviceDescriptor(*desc);
    int32_t ret = SwitchActiveA2dpDevice(deviceDesc);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("Active A2DP device failed, retrigger fetch output device");
        deviceDesc->exceptionFlag_ = true;
        audioDeviceManager_.UpdateDevicesListInfo(deviceDesc, EXCEPTION_FLAG_UPDATE);
        FetchOutputDevice(rendererChangeInfos, isStreamStatusUpdated);
        return ERROR;
    }
    return SUCCESS;
}

int32_t AudioPolicyService::HandleScoDeviceFetched(unique_ptr<AudioDeviceDescriptor> &desc,
    vector<unique_ptr<AudioRendererChangeInfo>> &rendererChangeInfos, bool &isStreamStatusUpdated)
{
#ifdef BLUETOOTH_ENABLE
        int32_t ret = Bluetooth::AudioHfpManager::SetActiveHfpDevice(desc->macAddress_);
        if (ret != SUCCESS) {
            AUDIO_ERR_LOG("Active hfp device failed, retrigger fetch output device.");
            desc->exceptionFlag_ = true;
            audioDeviceManager_.UpdateDevicesListInfo(new AudioDeviceDescriptor(*desc), EXCEPTION_FLAG_UPDATE);
            FetchOutputDevice(rendererChangeInfos, isStreamStatusUpdated);
            return ERROR;
        }
        if (desc->connectState_ == DEACTIVE_CONNECTED) {
            Bluetooth::AudioHfpManager::ConnectScoWithAudioScene(audioScene_);
            return ERROR;
        }
#endif
    return SUCCESS;
}

void AudioPolicyService::FetchOutputDevice(vector<unique_ptr<AudioRendererChangeInfo>> &rendererChangeInfos,
    bool isStreamStatusUpdated = false)
{
    AUDIO_INFO_LOG("Fetch output device for %{public}zu stream", rendererChangeInfos.size());
    bool needUpdateActiveDevice = true;
    bool isUpdateActiveDevice = false;
    AudioDeviceDescriptor preActiveDevice = currentActiveDevice_;
    for (auto &rendererChangeInfo : rendererChangeInfos) {
        StreamUsage usage = rendererChangeInfo->rendererInfo.streamUsage;
        RendererState rendererState = rendererChangeInfo->rendererState;
        if ((usage == STREAM_USAGE_VOICE_MODEM_COMMUNICATION && audioScene_ != AUDIO_SCENE_PHONE_CALL) ||
            (usage != STREAM_USAGE_VOICE_MODEM_COMMUNICATION && rendererState != RENDERER_RUNNING)) {
            AUDIO_INFO_LOG("stream %{public}d not running, no need fetch device", rendererChangeInfo->sessionId);
            continue;
        }
        unique_ptr<AudioDeviceDescriptor> desc = audioRouterCenter_.FetchOutputDevice(usage,
            rendererChangeInfo->clientUID);
        DeviceInfo outputDeviceInfo = rendererChangeInfo->outputDeviceInfo;
        if (desc->deviceType_ == DEVICE_TYPE_NONE || (IsSameDevice(desc, outputDeviceInfo) &&
            !sameDeviceSwitchFlag_)) {
            AUDIO_INFO_LOG("stream %{public}d device not change, no need move device", rendererChangeInfo->sessionId);
            continue;
        }
        if (desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
            int32_t ret = ActivateA2dpDevice(desc, rendererChangeInfos, isStreamStatusUpdated);
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "activate a2dp [%{public}s] failed", desc->macAddress_.c_str());
            OffloadStartPlayingIfOffloadMode(rendererChangeInfo->sessionId);
        } else if (desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_SCO) {
            int32_t ret = HandleScoDeviceFetched(desc, rendererChangeInfos, isStreamStatusUpdated);
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "sco [%{public}s] is not connected yet", desc->macAddress_.c_str());
        }
        if (needUpdateActiveDevice) {
            if (!IsSameDevice(desc, currentActiveDevice_)) {
                currentActiveDevice_ = AudioDeviceDescriptor(*desc);
                AUDIO_INFO_LOG("currentActiveDevice update %{public}d", currentActiveDevice_.deviceType_);
                isUpdateActiveDevice = true;
            }
            needUpdateActiveDevice = false;
        }
        SelectNewOutputDevice(rendererChangeInfo, desc);
    }
    sameDeviceSwitchFlag_ = false;
    if (isUpdateActiveDevice) {
        if (GetVolumeGroupType(currentActiveDevice_.deviceType_) != GetVolumeGroupType(preActiveDevice.deviceType_)) {
            SetVolumeForSwitchDevice(currentActiveDevice_.deviceType_);
        }
        OnPreferredOutputDeviceUpdated(currentActiveDevice_);
    } else if (needUpdateActiveDevice) {
        FetchOutputDeviceWhenNoRunningStream();
    }
}

void AudioPolicyService::OffloadStartPlayingIfOffloadMode(uint64_t sessionId)
{
#ifdef BLUETOOTH_ENABLE
    if (a2dpOffloadFlag_ == A2DP_OFFLOAD) {
        Bluetooth::AudioA2dpManager::OffloadStartPlaying(std::vector<int32_t>(1, sessionId));
    }
#endif
}

bool AudioPolicyService::IsSameDevice(unique_ptr<AudioDeviceDescriptor> &desc, DeviceInfo &deviceInfo)
{
    if (desc->networkId_ == deviceInfo.networkId && desc->deviceType_ == deviceInfo.deviceType &&
        desc->macAddress_ == deviceInfo.macAddress && desc->connectState_ == deviceInfo.connectState) {
        return true;
    } else {
        return false;
    }
}

bool AudioPolicyService::IsSameDevice(unique_ptr<AudioDeviceDescriptor> &desc, AudioDeviceDescriptor &deviceDesc)
{
    if (desc->networkId_ == deviceDesc.networkId_ && desc->deviceType_ == deviceDesc.deviceType_ &&
        desc->macAddress_ == deviceDesc.macAddress_ && desc->connectState_ == deviceDesc.connectState_) {
        return true;
    } else {
        return false;
    }
}

void AudioPolicyService::FetchInputDevice(vector<unique_ptr<AudioCapturerChangeInfo>> &capturerChangeInfos,
    bool isStreamStatusUpdated = false)
{
    AUDIO_INFO_LOG("Fetch input device for %{public}zu stream", capturerChangeInfos.size());
    bool needUpdateActiveDevice = true;
    bool isUpdateActiveDevice = false;
    for (auto &capturerChangeInfo : capturerChangeInfos) {
        SourceType sourceType = capturerChangeInfo->capturerInfo.sourceType;
        if ((sourceType == SOURCE_TYPE_VIRTUAL_CAPTURE && audioScene_ != AUDIO_SCENE_PHONE_CALL) ||
            (sourceType != SOURCE_TYPE_VIRTUAL_CAPTURE && capturerChangeInfo->capturerState != CAPTURER_RUNNING)) {
            AUDIO_INFO_LOG("stream %{public}d not running, no need fetch device", capturerChangeInfo->sessionId);
            continue;
        }
        unique_ptr<AudioDeviceDescriptor> desc = audioRouterCenter_.FetchInputDevice(sourceType,
            capturerChangeInfo->clientUID);
        DeviceInfo inputDeviceInfo = capturerChangeInfo->inputDeviceInfo;
        if (desc->deviceType_ == DEVICE_TYPE_NONE || IsSameDevice(desc, inputDeviceInfo)) {
            AUDIO_INFO_LOG("stream %{public}d device not change, no need move device", capturerChangeInfo->sessionId);
            continue;
        }
        if (desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_SCO) {
#ifdef BLUETOOTH_ENABLE
            int32_t ret = Bluetooth::AudioHfpManager::SetActiveHfpDevice(desc->macAddress_);
            if (ret != SUCCESS) {
                AUDIO_ERR_LOG("Active hfp device failed, retrigger fetch output device");
                desc->exceptionFlag_ = true;
                audioDeviceManager_.UpdateDevicesListInfo(new AudioDeviceDescriptor(*desc), EXCEPTION_FLAG_UPDATE);
                FetchInputDevice(capturerChangeInfos, isStreamStatusUpdated);
                return;
            }
            if (desc->connectState_ == DEACTIVE_CONNECTED) {
                Bluetooth::AudioHfpManager::ConnectScoWithAudioScene(audioScene_);
                return;
            }
#endif
        }
        if (needUpdateActiveDevice) {
            if (!IsSameDevice(desc, currentActiveInputDevice_)) {
                currentActiveInputDevice_ = AudioDeviceDescriptor(*desc);
                AUDIO_INFO_LOG("currentActiveInputDevice update %{public}d", currentActiveInputDevice_.deviceType_);
                isUpdateActiveDevice = true;
            }
            needUpdateActiveDevice = false;
        }
        // move sourceoutput to target device
        SelectNewInputDevice(capturerChangeInfo, desc, isStreamStatusUpdated);
    }
    if (isUpdateActiveDevice) {
        OnPreferredInputDeviceUpdated(currentActiveInputDevice_.deviceType_, currentActiveInputDevice_.networkId_);
    } else if (needUpdateActiveDevice) {
        FetchInputDeviceWhenNoRunningStream();
    }
}

void AudioPolicyService::FetchDevice(bool isOutputDevice = true)
{
    AUDIO_DEBUG_LOG("Entered %{public}s", __func__);
    if (isOutputDevice) {
        vector<unique_ptr<AudioRendererChangeInfo>> rendererChangeInfos;
        streamCollector_.GetCurrentRendererChangeInfos(rendererChangeInfos);
        FetchOutputDevice(rendererChangeInfos, false);
    } else {
        vector<unique_ptr<AudioCapturerChangeInfo>> capturerChangeInfos;
        streamCollector_.GetCurrentCapturerChangeInfos(capturerChangeInfos);
        FetchInputDevice(capturerChangeInfos, false);
    }
}

int32_t AudioPolicyService::SetMicrophoneMute(bool isMute)
{
    AUDIO_DEBUG_LOG("SetMicrophoneMute state[%{public}d]", isMute);
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    CHECK_AND_RETURN_RET_LOG(gsp != nullptr, ERR_OPERATION_FAILED, "Service proxy unavailable");
    int32_t ret = gsp->SetMicrophoneMute(isMute);
    if (ret == SUCCESS) {
        isMicrophoneMute_ = isMute;
        streamCollector_.UpdateCapturerInfoMuteStatus(0, isMute);
    }
    return ret;
}

bool AudioPolicyService::IsMicrophoneMute()
{
    AUDIO_DEBUG_LOG("Enter IsMicrophoneMute");
    return isMicrophoneMute_;
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

static string ParseAudioFormat(string format)
{
    if (format == "AUDIO_FORMAT_PCM_16_BIT") {
        return "s16";
    } else if (format == "AUDIO_FORMAT_PCM_24_BIT") {
        return "s24";
    } else if (format == "AUDIO_FORMAT_PCM_32_BIT") {
        return "s32";
    } else {
        return "";
    }
}

static void GetUsbModuleInfo(AudioModuleInfo &moduleInfo, string deviceInfo)
{
    if (moduleInfo.role == "sink") {
        auto sinkRate_begin = deviceInfo.find("sink_rate:");
        auto sinkRate_end = deviceInfo.find_first_of(";", sinkRate_begin);
        moduleInfo.rate = deviceInfo.substr(sinkRate_begin + std::strlen("sink_rate:"),
            sinkRate_end - sinkRate_begin - std::strlen("sink_rate:"));
        auto sinkFormat_begin = deviceInfo.find("sink_format:");
        auto sinkFormat_end = deviceInfo.find_first_of(";", sinkFormat_begin);
        string format = deviceInfo.substr(sinkFormat_begin + std::strlen("sink_format:"),
            sinkFormat_end - sinkFormat_begin - std::strlen("sink_format:"));
        moduleInfo.format = ParseAudioFormat(format);
    } else {
        auto sourceRate_begin = deviceInfo.find("source_rate:");
        auto sourceRate_end = deviceInfo.find_first_of(";", sourceRate_begin);
        moduleInfo.rate = deviceInfo.substr(sourceRate_begin + std::strlen("source_rate:"),
            sourceRate_end - sourceRate_begin - std::strlen("source_rate:"));
        auto sourceFormat_begin = deviceInfo.find("source_format:");
        auto sourceFormat_end = deviceInfo.find_first_of(";", sourceFormat_begin);
        string format = deviceInfo.substr(sourceFormat_begin + std::strlen("source_format:"),
            sourceFormat_end - sourceFormat_begin - std::strlen("source_format:"));
        moduleInfo.format = ParseAudioFormat(format);
    }
}

int32_t AudioPolicyService::SelectNewDevice(DeviceRole deviceRole, const sptr<AudioDeviceDescriptor> &deviceDescriptor)
{
    Trace trace("AudioPolicyService::SelectNewDevice:" + std::to_string(deviceDescriptor->deviceType_));
    int32_t result = SUCCESS;
    DeviceType deviceType = deviceDescriptor->deviceType_;
    if (deviceType == DEVICE_TYPE_USB_HEADSET && isArmUsbDevice_) {
        deviceType = DEVICE_TYPE_USB_ARM_HEADSET;
    }
    if (deviceRole == DeviceRole::OUTPUT_DEVICE) {
        std::string activePort = GetSinkPortName(currentActiveDevice_.deviceType_);
        AUDIO_INFO_LOG("SelectNewDevice: port %{public}s, active device %{public}d",
            activePort.c_str(), currentActiveDevice_.deviceType_);
        audioPolicyManager_.SuspendAudioDevice(activePort, true);
    }

    std::string portName = (deviceRole == DeviceRole::OUTPUT_DEVICE) ?
        GetSinkPortName(deviceType) : GetSourcePortName(deviceType);
    CHECK_AND_RETURN_RET_LOG(portName != PORT_NONE, ERR_INVALID_PARAM,
        "SelectNewDevice: Invalid port name %{public}s", portName.c_str());

    if (deviceRole == DeviceRole::OUTPUT_DEVICE) {
        int32_t muteDuration = 200000; // us
        std::thread switchThread(&AudioPolicyService::KeepPortMute, this, muteDuration, portName, deviceType);
        switchThread.detach(); // add another sleep before switch local can avoid pop in some case
    }

    if (deviceDescriptor->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP &&
        deviceDescriptor->macAddress_ != activeBTDevice_) {
        // switch to a connected a2dp device
        result = SwitchActiveA2dpDevice(deviceDescriptor);
        CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "SelectNewDevice: HandleActiveA2dpDevice failed.");
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
        if (GetVolumeGroupType(currentActiveDevice_.deviceType_) != GetVolumeGroupType(deviceDescriptor->deviceType_)) {
            SetVolumeForSwitchDevice(deviceDescriptor->deviceType_);
        }
        currentActiveDevice_.deviceType_ = deviceDescriptor->deviceType_;
        currentActiveDevice_.macAddress_ = deviceDescriptor->macAddress_;
        OnPreferredOutputDeviceUpdated(currentActiveDevice_);
    } else {
        currentActiveInputDevice_.deviceType_ = deviceDescriptor->deviceType_;
        OnPreferredInputDeviceUpdated(currentActiveInputDevice_.deviceType_, LOCAL_NETWORK_ID);
    }
    return SUCCESS;
}

int32_t AudioPolicyService::SwitchActiveA2dpDevice(const sptr<AudioDeviceDescriptor> &deviceDescriptor)
{
    auto iter = connectedA2dpDeviceMap_.find(deviceDescriptor->macAddress_);
    CHECK_AND_RETURN_RET_LOG(iter != connectedA2dpDeviceMap_.end(), ERR_INVALID_PARAM,
        "SelectNewDevice: the target A2DP device doesn't exist.");
    int32_t result = ERROR;
#ifdef BLUETOOTH_ENABLE
    if (Bluetooth::AudioA2dpManager::GetActiveA2dpDevice() == deviceDescriptor->macAddress_) {
        AUDIO_INFO_LOG("a2dp device [%{public}s] is already active", deviceDescriptor->macAddress_.c_str());
        return SUCCESS;
    }
    AUDIO_INFO_LOG("SelectNewDevice::a2dp device name [%{public}s]", (deviceDescriptor->deviceName_).c_str());
    result = Bluetooth::AudioA2dpManager::SetActiveA2dpDevice(deviceDescriptor->macAddress_);
    CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "SetActiveA2dpDevice failed %{public}d", result);
    activeBTDevice_ = deviceDescriptor->macAddress_;
    if (a2dpOffloadFlag_ != A2DP_OFFLOAD) {
        result = LoadA2dpModule(DEVICE_TYPE_BLUETOOTH_A2DP);
    }
    CHECK_AND_RETURN_RET_LOG(result == SUCCESS, ERR_OPERATION_FAILED, "LoadA2dpModule failed %{public}d", result);
#endif
    return result;
}

int32_t AudioPolicyService::LoadA2dpModule(DeviceType deviceType)
{
    std::list<AudioModuleInfo> moduleInfoList;
    {
        std::lock_guard<std::mutex> deviceInfoLock(deviceClassInfoMutex_);
        auto primaryModulesPos = deviceClassInfo_.find(ClassType::TYPE_A2DP);
        if (primaryModulesPos == deviceClassInfo_.end()) {
            AUDIO_ERR_LOG("A2dp module is not exist in the configuration file");
            return ERR_OPERATION_FAILED;
        }
        moduleInfoList = primaryModulesPos->second;
    }
    for (auto &moduleInfo : moduleInfoList) {
        std::lock_guard<std::mutex> ioHandleLock(ioHandlesMutex_);
        if (IOHandles_.find(moduleInfo.name) == IOHandles_.end()) {
            // a2dp device connects for the first time
            AUDIO_INFO_LOG("Load a2dp module [%{public}s]", moduleInfo.name.c_str());
            AudioStreamInfo audioStreamInfo = {};
            GetActiveDeviceStreamInfo(deviceType, audioStreamInfo);
            uint32_t bufferSize = (audioStreamInfo.samplingRate * GetSampleFormatValue(audioStreamInfo.format) *
                audioStreamInfo.channels) / (PCM_8_BIT * BT_BUFFER_ADJUSTMENT_FACTOR);
            AUDIO_INFO_LOG("LoadA2dpModule: a2dp rate: %{public}d, format: %{public}d, channel: %{public}d",
                audioStreamInfo.samplingRate, audioStreamInfo.format, audioStreamInfo.channels);
            moduleInfo.channels = to_string(audioStreamInfo.channels);
            moduleInfo.rate = to_string(audioStreamInfo.samplingRate);
            moduleInfo.format = ConvertToHDIAudioFormat(audioStreamInfo.format);
            moduleInfo.bufferSize = to_string(bufferSize);
            moduleInfo.renderInIdleState = "1";

            AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
            CHECK_AND_RETURN_RET_LOG(ioHandle != OPEN_PORT_FAILURE, ERR_OPERATION_FAILED,
                "LoadA2dpModule: OpenAudioPort failed %{public}d", ioHandle);
            IOHandles_[moduleInfo.name] = ioHandle;
        } else {
            // At least one a2dp device is already connected. A new a2dp device is connecting.
            // Need to reload a2dp module when switching to a2dp device.
            int32_t result = ReloadA2dpAudioPort(moduleInfo);
            CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result, "ReloadA2dpAudioPort failed %{public}d", result);
        }
    }

    return SUCCESS;
}

int32_t AudioPolicyService::HandleA2dpOffloadDeviceSuspend(DeviceType deviceType)
{
    UpdateActiveDeviceRoute(DEVICE_TYPE_SPEAKER);
    HandleActiveDevice(deviceType);
    return SUCCESS;
}

int32_t AudioPolicyService::ReloadA2dpAudioPort(AudioModuleInfo &moduleInfo)
{
    AUDIO_INFO_LOG("ReloadA2dpAudioPort: switch device from a2dp to another a2dp, reload a2dp module");

    // Firstly, unload the existing a2dp sink.
    AudioIOHandle activateDeviceIOHandle = IOHandles_[BLUETOOTH_SPEAKER];
    int32_t result = audioPolicyManager_.CloseAudioPort(activateDeviceIOHandle);
    CHECK_AND_RETURN_RET_LOG(result == SUCCESS, result,
        "ReloadA2dpAudioPort: CloseAudioPort failed %{public}d", result);

    // Load a2dp sink module again with the configuration of active a2dp device.
    AudioStreamInfo audioStreamInfo = {};
    GetActiveDeviceStreamInfo(DEVICE_TYPE_BLUETOOTH_A2DP, audioStreamInfo);
    uint32_t bufferSize = (audioStreamInfo.samplingRate * GetSampleFormatValue(audioStreamInfo.format) *
        audioStreamInfo.channels) / (PCM_8_BIT * BT_BUFFER_ADJUSTMENT_FACTOR);
    AUDIO_INFO_LOG("ReloadA2dpAudioPort: a2dp rate: %{public}d, format: %{public}d, channel: %{public}d",
        audioStreamInfo.samplingRate, audioStreamInfo.format, audioStreamInfo.channels);
    moduleInfo.channels = to_string(audioStreamInfo.channels);
    moduleInfo.rate = to_string(audioStreamInfo.samplingRate);
    moduleInfo.format = ConvertToHDIAudioFormat(audioStreamInfo.format);
    moduleInfo.bufferSize = to_string(bufferSize);
    moduleInfo.renderInIdleState = "1";
    AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
    CHECK_AND_RETURN_RET_LOG(ioHandle != OPEN_PORT_FAILURE, ERR_OPERATION_FAILED,
        "ReloadA2dpAudioPort: OpenAudioPort failed %{public}d", ioHandle);
    IOHandles_[moduleInfo.name] = ioHandle;

    return SUCCESS;
}

int32_t AudioPolicyService::LoadUsbModule(string deviceInfo)
{
    std::list<AudioModuleInfo> moduleInfoList;
    {
        std::lock_guard<std::mutex> deviceInfoLock(deviceClassInfoMutex_);
        auto usbModulesPos = deviceClassInfo_.find(ClassType::TYPE_USB);
        if (usbModulesPos == deviceClassInfo_.end()) {
            return ERR_OPERATION_FAILED;
        }
        moduleInfoList = usbModulesPos->second;
    }
    for (auto &moduleInfo : moduleInfoList) {
        AUDIO_INFO_LOG("[module_load]::load module[%{public}s]", moduleInfo.name.c_str());
        if (IOHandles_.find(moduleInfo.name) == IOHandles_.end()) {
            GetUsbModuleInfo(moduleInfo, deviceInfo);
            AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
            CHECK_AND_RETURN_RET_LOG(ioHandle != OPEN_PORT_FAILURE, ERR_OPERATION_FAILED,
                "OpenAudioPort failed %{public}d", ioHandle);
            IOHandles_[moduleInfo.name] = ioHandle;
        }
    }

    return SUCCESS;
}

int32_t AudioPolicyService::LoadDefaultUsbModule()
{
    std::list<AudioModuleInfo> moduleInfoList;
    {
        std::lock_guard<std::mutex> deviceInfoLock(deviceClassInfoMutex_);
        auto usbModulesPos = deviceClassInfo_.find(ClassType::TYPE_USB);
        if (usbModulesPos == deviceClassInfo_.end()) {
            return ERR_OPERATION_FAILED;
        }
        moduleInfoList = usbModulesPos->second;
    }
    for (auto &moduleInfo : moduleInfoList) {
        AUDIO_INFO_LOG("[module_load]::load default module[%{public}s]", moduleInfo.name.c_str());
        if (IOHandles_.find(moduleInfo.name) == IOHandles_.end()) {
            AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
            CHECK_AND_RETURN_RET_LOG(ioHandle != OPEN_PORT_FAILURE, ERR_OPERATION_FAILED,
                "OpenAudioPort failed %{public}d", ioHandle);
            IOHandles_[moduleInfo.name] = ioHandle;
        }
    }

    return SUCCESS;
}

int32_t AudioPolicyService::HandleActiveDevice(DeviceType deviceType)
{
    if (GetVolumeGroupType(currentActiveDevice_.deviceType_) != GetVolumeGroupType(deviceType)) {
        SetVolumeForSwitchDevice(deviceType);
    }
    if (deviceType == DEVICE_TYPE_USB_HEADSET && isArmUsbDevice_) {
        deviceType = DEVICE_TYPE_USB_ARM_HEADSET;
    }
    if (isUpdateRouteSupported_) {
        UpdateActiveDeviceRoute(deviceType);
    }
    std::string sinkPortName = GetSinkPortName(deviceType);
    std::string sourcePortName = GetSourcePortName(deviceType);
    if (sinkPortName == PORT_NONE && sourcePortName == PORT_NONE) {
        AUDIO_ERR_LOG("HandleActiveDevice failed for sinkPortName and sourcePortName are none");
        return ERR_OPERATION_FAILED;
    }
    if (sinkPortName != PORT_NONE) {
        AudioIOHandle ioHandle = GetSinkIOHandle(deviceType);
        audioPolicyManager_.SetDeviceActive(ioHandle, deviceType, sinkPortName, true);
        audioPolicyManager_.SuspendAudioDevice(sinkPortName, false);
    }
    if (sourcePortName != PORT_NONE) {
        AudioIOHandle ioHandle = GetSourceIOHandle(deviceType);
        audioPolicyManager_.SetDeviceActive(ioHandle, deviceType, sourcePortName, true);
        audioPolicyManager_.SuspendAudioDevice(sourcePortName, false);
    }
    UpdateInputDeviceInfo(deviceType);

    return SUCCESS;
}

int32_t AudioPolicyService::HandleA2dpDevice(DeviceType deviceType)
{
    Trace trace("AudioPolicyService::HandleA2dpDevice");
    int32_t ret = LoadA2dpModule(deviceType);
    preA2dpOffloadFlag_ = A2DP_NOT_OFFLOAD;
    a2dpOffloadFlag_ = A2DP_NOT_OFFLOAD;
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("load A2dp module failed");
        return ERR_OPERATION_FAILED;
    }
    return SUCCESS;
}

int32_t AudioPolicyService::HandleArmUsbDevice(DeviceType deviceType)
{
    Trace trace("AudioPolicyService::HandleArmUsbDevice");

    if (deviceType == DEVICE_TYPE_USB_HEADSET) {
        string deviceInfo = "";
        if (g_adProxy != nullptr) {
            deviceInfo = g_adProxy->GetAudioParameter("get_usb_info");
            AUDIO_INFO_LOG("device info from usb hal is %{public}s", deviceInfo.c_str());
        }
        int32_t ret;
        if (!deviceInfo.empty()) {
            ret = LoadUsbModule(deviceInfo);
        } else {
            ret = LoadDefaultUsbModule();
        }
        if (ret != SUCCESS) {
            AUDIO_ERR_LOG ("load usb module failed");
            return ERR_OPERATION_FAILED;
        }
        std::string activePort = GetSinkPortName(DEVICE_TYPE_USB_ARM_HEADSET);
        AUDIO_INFO_LOG("port %{public}s, active device %{public}d", activePort.c_str(), DEVICE_TYPE_USB_ARM_HEADSET);
    } else if (currentActiveDevice_.deviceType_ == DEVICE_TYPE_USB_HEADSET) {
        std::string activePort = GetSinkPortName(DEVICE_TYPE_USB_ARM_HEADSET);
        audioPolicyManager_.SuspendAudioDevice(activePort, true);
    }

    return SUCCESS;
}

int32_t AudioPolicyService::HandleFileDevice(DeviceType deviceType)
{
    AUDIO_INFO_LOG("HandleFileDevice");

    std::string sinkPortName = GetSinkPortName(deviceType);
    std::string sourcePortName = GetSourcePortName(deviceType);
    if (sinkPortName == PORT_NONE && sourcePortName == PORT_NONE) {
        AUDIO_ERR_LOG("HandleFileDevice failed for sinkPortName and sourcePortName are none");
        return ERR_OPERATION_FAILED;
    }
    if (sinkPortName != PORT_NONE) {
        AudioIOHandle ioHandle = GetSinkIOHandle(deviceType);
        audioPolicyManager_.SetDeviceActive(ioHandle, deviceType, sinkPortName, true);
        audioPolicyManager_.SuspendAudioDevice(sinkPortName, false);
    }
    if (sourcePortName != PORT_NONE) {
        AudioIOHandle ioHandle = GetSourceIOHandle(deviceType);
        audioPolicyManager_.SetDeviceActive(ioHandle, deviceType, sourcePortName, true);
        audioPolicyManager_.SuspendAudioDevice(sourcePortName, false);
    }
    if (isUpdateRouteSupported_) {
        UpdateActiveDeviceRoute(deviceType);
    }

    UpdateInputDeviceInfo(deviceType);
    return SUCCESS;
}

int32_t AudioPolicyService::ActivateNormalNewDevice(DeviceType deviceType, bool isSceneActivation = false)
{
    bool isVolumeSwitched = false;
    if (isUpdateRouteSupported_ && !isSceneActivation) {
        if (GetDeviceRole(deviceType) == OUTPUT_DEVICE) {
            int32_t muteDuration = 1200000; // us
            std::string sinkPortName = GetSinkPortName(deviceType);
            CHECK_AND_RETURN_RET_LOG(sinkPortName != PORT_NONE,
                ERR_OPERATION_FAILED,
                "Invalid port %{public}s",
                sinkPortName.c_str());
            std::thread switchThread(&AudioPolicyService::KeepPortMute, this, muteDuration, sinkPortName, deviceType);
            switchThread.detach();
            int32_t beforSwitchDelay = 300000; // 300 ms
            usleep(beforSwitchDelay);
        }
        UpdateActiveDeviceRoute(deviceType);
        if (GetDeviceRole(deviceType) == OUTPUT_DEVICE) {
            if (GetVolumeGroupType(currentActiveDevice_.deviceType_) != GetVolumeGroupType(deviceType)) {
                SetVolumeForSwitchDevice(deviceType);
            }
            isVolumeSwitched = true;
        }
    }
    if (GetDeviceRole(deviceType) == OUTPUT_DEVICE && !isVolumeSwitched &&
        GetVolumeGroupType(currentActiveDevice_.deviceType_) != GetVolumeGroupType(deviceType)) {
        SetVolumeForSwitchDevice(deviceType);
    }
    UpdateInputDeviceInfo(deviceType);
    return SUCCESS;
}

int32_t AudioPolicyService::ActivateNewDevice(DeviceType deviceType, bool isSceneActivation = false)
{
    AUDIO_INFO_LOG("Switch device: [%{public}d]-->[%{public}d]", currentActiveDevice_.deviceType_, deviceType);
    int32_t result = SUCCESS;
    ResetOffloadMode();

    if (currentActiveDevice_.deviceType_ == deviceType) {
        if (deviceType != DEVICE_TYPE_BLUETOOTH_A2DP || currentActiveDevice_.macAddress_ == activeBTDevice_) {
            return result;
        }
    }
    if (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        result = HandleA2dpDevice(deviceType);
        return result;
    }
    if (isArmUsbDevice_ && deviceType == DEVICE_TYPE_USB_HEADSET) {
        result = HandleArmUsbDevice(deviceType);
        return result;
    }
    if (deviceType == DEVICE_TYPE_FILE_SINK) {
        result = HandleFileDevice(deviceType);
        return result;
    }
    if (currentActiveDevice_.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
        result = HandleA2dpDevice(deviceType);
        return result;
    }
    if (isArmUsbDevice_ && currentActiveDevice_.deviceType_ == DEVICE_TYPE_USB_HEADSET) {
        result = HandleArmUsbDevice(deviceType);
        return result;
    }
    if (currentActiveDevice_.deviceType_ == DEVICE_TYPE_FILE_SINK) {
        result = HandleFileDevice(deviceType);
        return result;
    }
    return ActivateNormalNewDevice(deviceType, isSceneActivation);
}

void AudioPolicyService::KeepPortMute(int32_t muteDuration, std::string portName, DeviceType deviceType)
{
    Trace trace("AudioPolicyService::KeepPortMute:" + portName + " for " + std::to_string(muteDuration) + "us");
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

    // Activate new device if its already connected
    auto isPresent = [&deviceType] (const sptr<AudioDeviceDescriptor> &desc) {
        CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "SetDeviceActive::Invalid device descriptor");
        return ((deviceType == desc->deviceType_) || (deviceType == DEVICE_TYPE_FILE_SINK));
    };

    vector<unique_ptr<AudioDeviceDescriptor>> callDevices = GetAvailableDevices(CALL_OUTPUT_DEVICES);
    std::vector<sptr<AudioDeviceDescriptor>> deviceList = {};
    for (auto &desc : callDevices) {
        sptr<AudioDeviceDescriptor> devDesc = new(std::nothrow) AudioDeviceDescriptor(*desc);
        deviceList.push_back(devDesc);
    }

    auto itr = std::find_if(deviceList.begin(), deviceList.end(), isPresent);
    CHECK_AND_RETURN_RET_LOG(itr != deviceList.end(), ERR_OPERATION_FAILED,
        "Requested device not available %{public}d ", deviceType);

    if (!active) {
        audioStateManager_.SetPerferredCallRenderDevice(new(std::nothrow) AudioDeviceDescriptor());
        if (currentActiveDevice_.deviceType_ == DEVICE_TYPE_BLUETOOTH_SCO &&
            deviceType == DEVICE_TYPE_BLUETOOTH_SCO) {
            Bluetooth::SendUserSelectionEvent(DEVICE_TYPE_BLUETOOTH_SCO,
                currentActiveDevice_.macAddress_, USER_NOT_SELECT_BT);
        }
    } else {
        audioStateManager_.SetPerferredCallRenderDevice(*itr);
        if (currentActiveDevice_.deviceType_ == DEVICE_TYPE_BLUETOOTH_SCO &&
            deviceType != DEVICE_TYPE_BLUETOOTH_SCO) {
            Bluetooth::SendUserSelectionEvent(DEVICE_TYPE_BLUETOOTH_SCO,
                currentActiveDevice_.macAddress_, USER_NOT_SELECT_BT);
        }
        if (currentActiveDevice_.deviceType_ != DEVICE_TYPE_BLUETOOTH_SCO &&
            deviceType == DEVICE_TYPE_BLUETOOTH_SCO) {
            Bluetooth::SendUserSelectionEvent(DEVICE_TYPE_BLUETOOTH_SCO,
                (*itr)->macAddress_, USER_SELECT_BT);
        }
    }
    FetchDevice(true);
    return SUCCESS;
}

bool AudioPolicyService::IsDeviceActive(InternalDeviceType deviceType) const
{
    AUDIO_INFO_LOG("Entered AudioPolicyService::%{public}s", __func__);
    return currentActiveDevice_.deviceType_ == deviceType;
}

DeviceType AudioPolicyService::GetActiveOutputDevice() const
{
    return currentActiveDevice_.deviceType_;
}

unique_ptr<AudioDeviceDescriptor> AudioPolicyService::GetActiveOutputDeviceDescriptor() const
{
    return make_unique<AudioDeviceDescriptor>(currentActiveDevice_);
}

DeviceType AudioPolicyService::GetActiveInputDevice() const
{
    return currentActiveInputDevice_.deviceType_;
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

    int32_t result = SUCCESS;
    if (currentActiveDevice_.deviceType_ == DEVICE_TYPE_USB_HEADSET && isArmUsbDevice_) {
        result = gsp->SetAudioScene(audioScene, DEVICE_TYPE_USB_ARM_HEADSET);
    } else {
        result = gsp->SetAudioScene(audioScene, currentActiveDevice_.deviceType_);
    }
    CHECK_AND_RETURN_RET_LOG(result == SUCCESS, ERR_OPERATION_FAILED, "SetAudioScene failed [%{public}d]", result);

    if (audioScene_ == AUDIO_SCENE_DEFAULT) {
        audioStateManager_.SetPerferredCallRenderDevice(new(std::nothrow) AudioDeviceDescriptor());
        audioStateManager_.SetPerferredCallCaptureDevice(new(std::nothrow) AudioDeviceDescriptor());
#ifdef BLUETOOTH_ENABLE
        Bluetooth::AudioHfpManager::DisconnectSco();
#endif
    }

    // fetch input&output device
    FetchDevice(true);
    FetchDevice(false);

    if (audioScene_ == AUDIO_SCENE_PHONE_CALL) {
        // Make sure the STREAM_VOICE_CALL volume is set before the calling starts.
        SetVoiceCallVolume(GetSystemVolumeLevel(STREAM_VOICE_CALL));
    }

    return SUCCESS;
}

void AudioPolicyService::AddEarpiece()
{
    if (localDevicesType_.compare("phone") != 0) {
        return;
    }
    sptr<AudioDeviceDescriptor> audioDescriptor =
        new (std::nothrow) AudioDeviceDescriptor(DEVICE_TYPE_EARPIECE, OUTPUT_DEVICE);
    // Use speaker streaminfo for earpiece cap
    auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(),
        [](const sptr<AudioDeviceDescriptor> &devDesc) {
        CHECK_AND_RETURN_RET_LOG(devDesc != nullptr, false, "Invalid device descriptor");
        return (devDesc->deviceType_ == DEVICE_TYPE_SPEAKER);
    });
    if (itr != connectedDevices_.end()) {
        audioDescriptor->SetDeviceCapability((*itr)->audioStreamInfo_, 0);
    }
    audioDescriptor->deviceId_ = startDeviceId++;
    UpdateDisplayName(audioDescriptor);
    audioDeviceManager_.AddNewDevice(audioDescriptor);
    connectedDevices_.insert(connectedDevices_.begin(), audioDescriptor);
    AUDIO_INFO_LOG("Add earpiece to device list");
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
    std::lock_guard<std::mutex> lock(a2dpDeviceMapMutex_);
    if (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        auto configInfoPos = connectedA2dpDeviceMap_.find(activeBTDevice_);
        if (configInfoPos != connectedA2dpDeviceMap_.end()) {
            streamInfo.samplingRate = *configInfoPos->second.streamInfo.samplingRate.rbegin();
            streamInfo.format = configInfoPos->second.streamInfo.format;
            streamInfo.channels = *configInfoPos->second.streamInfo.channels.rbegin();
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

void AudioPolicyService::UpdateConnectedDevicesWhenConnectingForOutputDevice(
    const AudioDeviceDescriptor &deviceDescriptor, std::vector<sptr<AudioDeviceDescriptor>> &desc,
    sptr<AudioDeviceDescriptor> &audioDescriptor)
{
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
    auto isPresent = [&deviceDescriptor] (const sptr<AudioDeviceDescriptor> &descriptor) {
        if (deviceDescriptor.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP &&
            descriptor->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
            return descriptor->macAddress_ == deviceDescriptor.macAddress_;
        } else {
            return ((descriptor->deviceType_ == deviceDescriptor.deviceType_) &&
                (descriptor->networkId_ == deviceDescriptor.networkId_));
        }
    };
    //If the device is a2dp && not the first connection, use the first connection deviceId and erase the itr
    auto it  = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent);
    if (it != connectedDevices_.end() && (*it)->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
        audioDescriptor->deviceId_ = (*it)->deviceId_;
        connectedDevices_.erase(it);
    } else {
        audioDescriptor->deviceId_ = startDeviceId++;
    }
    desc.push_back(audioDescriptor);
    UpdateDisplayName(audioDescriptor);
    connectedDevices_.insert(connectedDevices_.begin(), audioDescriptor);
    audioDeviceManager_.AddNewDevice(audioDescriptor);
    audioStateManager_.SetPerferredMediaRenderDevice(new(std::nothrow) AudioDeviceDescriptor());
    audioStateManager_.SetPerferredCallRenderDevice(new(std::nothrow) AudioDeviceDescriptor());
}

void AudioPolicyService::UpdateConnectedDevicesWhenConnectingForInputDevice(
    const AudioDeviceDescriptor &deviceDescriptor, std::vector<sptr<AudioDeviceDescriptor>> &desc,
    sptr<AudioDeviceDescriptor> &audioDescriptor)
{
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
    auto isPresent = [&deviceDescriptor] (const sptr<AudioDeviceDescriptor> &descriptor) {
        if (deviceDescriptor.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP &&
            descriptor->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
            return descriptor->macAddress_ == deviceDescriptor.macAddress_;
        } else {
            return ((descriptor->deviceType_ == deviceDescriptor.deviceType_) &&
                (descriptor->networkId_ == deviceDescriptor.networkId_));
        }
    };
    //If the device is a2dp && not the first connection, use the first connection deviceId and erase the it
    auto it  = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent);
    if (it != connectedDevices_.end() && (*it)->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
        audioDescriptor->deviceId_ = (*it)->deviceId_;
        connectedDevices_.erase(it);
    } else {
        audioDescriptor->deviceId_ = startDeviceId++;
    }
    desc.push_back(audioDescriptor);
    UpdateDisplayName(audioDescriptor);
    connectedDevices_.insert(connectedDevices_.begin(), audioDescriptor);
    AddMicrophoneDescriptor(audioDescriptor);
    audioDeviceManager_.AddNewDevice(audioDescriptor);
    audioStateManager_.SetPerferredCallCaptureDevice(new(std::nothrow) AudioDeviceDescriptor());
    audioStateManager_.SetPerferredRecordCaptureDevice(new(std::nothrow) AudioDeviceDescriptor());
}

void AudioPolicyService::UpdateConnectedDevicesWhenConnecting(const AudioDeviceDescriptor &deviceDescriptor,
    std::vector<sptr<AudioDeviceDescriptor>> &desc)
{
    sptr<AudioDeviceDescriptor> audioDescriptor = nullptr;
    if (IsOutputDevice(deviceDescriptor.deviceType_)) {
        UpdateConnectedDevicesWhenConnectingForOutputDevice(deviceDescriptor, desc, audioDescriptor);
        if (deviceDescriptor.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
            UpdateA2dpOffloadFlagForAllStream(deviceDescriptor.deviceType_);
        }
    }
    if (IsInputDevice(deviceDescriptor.deviceType_)) {
        UpdateConnectedDevicesWhenConnectingForInputDevice(deviceDescriptor, desc, audioDescriptor);
    }
}

void AudioPolicyService::UpdateConnectedDevicesWhenDisconnecting(const AudioDeviceDescriptor& deviceDescriptor,
    std::vector<sptr<AudioDeviceDescriptor>> &desc)
{
    AUDIO_INFO_LOG("[%{public}s], devType:[%{public}d]", __func__, deviceDescriptor.deviceType_);
    auto isPresent = [&deviceDescriptor](const sptr<AudioDeviceDescriptor>& descriptor) {
        if (descriptor->deviceType_ == deviceDescriptor.deviceType_ &&
            descriptor->networkId_ == deviceDescriptor.networkId_) {
            if (descriptor->deviceType_ != DEVICE_TYPE_BLUETOOTH_A2DP) {
                return true;
            } else {
                // if the disconnecting device is A2DP, need to compare mac address in addition.
                return descriptor->macAddress_ == deviceDescriptor.macAddress_;
            }
        }
        return false;
    };

    // Remember the disconnected device descriptor and remove it
    for (auto it = connectedDevices_.begin(); it != connectedDevices_.end();) {
        it = find_if(it, connectedDevices_.end(), isPresent);
        if (it != connectedDevices_.end()) {
            if ((*it)->deviceId_ == audioStateManager_.GetPerferredMediaRenderDevice()->deviceId_) {
                audioStateManager_.SetPerferredMediaRenderDevice(new(std::nothrow) AudioDeviceDescriptor());
            }
            if ((*it)->deviceId_ == audioStateManager_.GetPerferredCallRenderDevice()->deviceId_) {
                audioStateManager_.SetPerferredCallRenderDevice(new(std::nothrow) AudioDeviceDescriptor());
            }
            if ((*it)->deviceId_ == audioStateManager_.GetPerferredCallCaptureDevice()->deviceId_) {
                audioStateManager_.SetPerferredCallCaptureDevice(new(std::nothrow) AudioDeviceDescriptor());
            }
            if ((*it)->deviceId_ == audioStateManager_.GetPerferredRecordCaptureDevice()->deviceId_) {
                audioStateManager_.SetPerferredRecordCaptureDevice(new(std::nothrow) AudioDeviceDescriptor());
            }
            desc.push_back(*it);
            it = connectedDevices_.erase(it);
        }
    }

    sptr<AudioDeviceDescriptor> devDesc
        = new (std::nothrow) AudioDeviceDescriptor(deviceDescriptor);
    audioDeviceManager_.RemoveNewDevice(devDesc);
    RemoveMicrophoneDescriptor(devDesc);
    if (deviceDescriptor.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
        preA2dpOffloadFlag_ = NO_A2DP_DEVICE;
        a2dpOffloadFlag_ = NO_A2DP_DEVICE;
    }
}

void AudioPolicyService::OnPnpDeviceStatusUpdated(DeviceType devType, bool isConnected)
{
    CHECK_AND_RETURN_LOG(devType != DEVICE_TYPE_NONE, "devType is none type");
    if (!hasModulesLoaded) {
        AUDIO_WARNING_LOG("modules has not loaded");
        pnpDeviceList_.push_back({devType, isConnected});
        return;
    }
    if (g_adProxy == nullptr) {
        GetAudioServerProxy();
    }
    AudioStreamInfo streamInfo = {};
    OnDeviceStatusUpdated(devType, isConnected, "", "", streamInfo);
}

void AudioPolicyService::UpdateLocalGroupInfo(bool isConnected, const std::string& macAddress,
    const std::string& deviceName, const DeviceStreamInfo& streamInfo, AudioDeviceDescriptor& deviceDesc)
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
    const std::string& deviceName, const DeviceStreamInfo& streamInfo)
{
    AUDIO_INFO_LOG("[%{public}s], macAddress:[%{public}s]", __func__, macAddress.c_str());
    {
        std::lock_guard<std::mutex> lock(a2dpDeviceMapMutex_);
        if (devType == DEVICE_TYPE_BLUETOOTH_A2DP) {
            A2dpDeviceConfigInfo configInfo = {streamInfo, false};
            connectedA2dpDeviceMap_.insert(make_pair(macAddress, configInfo));
            activeBTDevice_ = macAddress;
            sameDeviceSwitchFlag_ = true;
        }
    }

    if (isArmUsbDevice_ && devType == DEVICE_TYPE_USB_HEADSET) {
        int32_t result = HandleArmUsbDevice(devType);
        return result;
    }

    return SUCCESS;
}

int32_t AudioPolicyService::HandleLocalDeviceDisconnected(DeviceType devType, const std::string& macAddress)
{
    if (devType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        bool isActiveA2dpDevice = true;
        UpdateActiveA2dpDeviceWhenDisconnecting(macAddress, isActiveA2dpDevice);
        if (!isActiveA2dpDevice) {
            // The disconnecting a2dp device is not active.
            return SUCCESS;
        }
    }

    if (devType == DEVICE_TYPE_USB_HEADSET && isArmUsbDevice_) {
        if (IOHandles_.find(USB_SPEAKER) != IOHandles_.end()) {
            audioPolicyManager_.CloseAudioPort(IOHandles_[USB_SPEAKER]);
            IOHandles_.erase(USB_SPEAKER);
        }
        if (IOHandles_.find(USB_MIC) != IOHandles_.end()) {
            audioPolicyManager_.CloseAudioPort(IOHandles_[USB_MIC]);
            IOHandles_.erase(USB_MIC);
        }
    }
    return SUCCESS;
}

void AudioPolicyService::UpdateActiveA2dpDeviceWhenDisconnecting(const std::string& macAddress,
    bool& isActiveA2dpDevice)
{
    std::unique_lock<std::mutex> lock(a2dpDeviceMapMutex_);
    connectedA2dpDeviceMap_.erase(macAddress);

    if (connectedA2dpDeviceMap_.size() == 0) {
        isActiveA2dpDevice = true;
        activeBTDevice_ = "";
        if (IOHandles_.find(BLUETOOTH_SPEAKER) != IOHandles_.end()) {
            audioPolicyManager_.CloseAudioPort(IOHandles_[BLUETOOTH_SPEAKER]);
            IOHandles_.erase(BLUETOOTH_SPEAKER);
        }
        audioPolicyManager_.SetAbsVolumeScene(false);
        return;
    }

    // other a2dp devices is still connected.
    if (activeBTDevice_ == macAddress) {
        // the active a2dp device is disconnecting.
        AUDIO_INFO_LOG("HandleLocalDeviceDisconnected: The active a2dp device is disconnecting");
        isActiveA2dpDevice = true;
        activeBTDevice_ = (connectedA2dpDeviceMap_.begin())->first;
        A2dpDeviceConfigInfo configInfo = (connectedA2dpDeviceMap_.begin())->second;
        if (configInfo.absVolumeSupport) {
            audioPolicyManager_.SetAbsVolumeScene(true);
        }
        lock.unlock();

#ifdef BLUETOOTH_ENABLE
        Bluetooth::AudioA2dpManager::SetActiveA2dpDevice(activeBTDevice_);
#endif
    } else {
        // The disconnecting a2dp device is not active.
        AUDIO_INFO_LOG("The disconnecting a2dp device is not active. No need to update active device");
        audioPolicyManager_.SetAbsVolumeScene(false);
        isActiveA2dpDevice = false;
    }
}

DeviceType AudioPolicyService::FindConnectedHeadset()
{
    DeviceType retType = DEVICE_TYPE_NONE;
    for (const auto& devDesc: connectedDevices_) {
        if ((devDesc->deviceType_ == DEVICE_TYPE_WIRED_HEADSET) ||
            (devDesc->deviceType_ == DEVICE_TYPE_WIRED_HEADPHONES) ||
            (devDesc->deviceType_ == DEVICE_TYPE_USB_HEADSET) ||
            (devDesc->deviceType_ == DEVICE_TYPE_USB_ARM_HEADSET)) {
            retType = devDesc->deviceType_;
            break;
        }
    }
    return retType;
}

int32_t AudioPolicyService::HandleSpecialDeviceType(DeviceType &devType, bool &isConnected)
{
    // usb device needs to be distinguished form arm or hifi
    if (devType == DEVICE_TYPE_USB_HEADSET && isConnected) {
        if (g_adProxy == nullptr) {
            return ERROR;
        }
        const std::string value = g_adProxy->GetAudioParameter("need_change_usb_device");
        AUDIO_INFO_LOG("get value %{public}s  from hal when usb device connect", value.c_str());
        if (value == "false") {
            isArmUsbDevice_ = true;
        }
    }

    // Special logic for extern cable, need refactor
    if (devType == DEVICE_TYPE_EXTERN_CABLE) {
        if (!isConnected) {
            AUDIO_INFO_LOG("Extern cable disconnected, do nothing");
            return ERROR;
        }
        DeviceType connectedHeadsetType = FindConnectedHeadset();
        if (connectedHeadsetType == DEVICE_TYPE_NONE) {
            AUDIO_INFO_LOG("Extern cable connect without headset connected before, do nothing");
            return ERROR;
        }
        devType = connectedHeadsetType;
        isConnected = false;
    }

    return SUCCESS;
}

void AudioPolicyService::ResetToSpeaker(DeviceType devType)
{
    if (devType == DEVICE_TYPE_BLUETOOTH_SCO || (devType == DEVICE_TYPE_USB_HEADSET && !isArmUsbDevice_) ||
        devType == DEVICE_TYPE_WIRED_HEADSET || devType == DEVICE_TYPE_WIRED_HEADPHONES) {
        UpdateActiveDeviceRoute(DEVICE_TYPE_SPEAKER);
    }
}

void AudioPolicyService::OnDeviceStatusUpdated(DeviceType devType, bool isConnected, const std::string& macAddress,
    const std::string& deviceName, const AudioStreamInfo& streamInfo)
{
    AUDIO_INFO_LOG("Device connection state updated | TYPE[%{public}d] STATUS[%{public}d]", devType, isConnected);

    std::lock_guard<std::shared_mutex> lock(deviceStatusUpdateSharedMutex_);

    int32_t result = ERROR;
    result = HandleSpecialDeviceType(devType, isConnected);
    CHECK_AND_RETURN_LOG(result == SUCCESS, "handle special deviceType failed.");
    AudioDeviceDescriptor deviceDesc(devType, GetDeviceRole(devType));
    UpdateLocalGroupInfo(isConnected, macAddress, deviceName, streamInfo, deviceDesc);

    // fill device change action for callback
    std::vector<sptr<AudioDeviceDescriptor>> deviceChangeDescriptor = {};
    auto isPresent = [&devType, &macAddress] (const sptr<AudioDeviceDescriptor> &descriptor) {
        if (devType == DEVICE_TYPE_BLUETOOTH_A2DP && descriptor->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
            return descriptor->macAddress_ == macAddress;
        } else {
            return descriptor->deviceType_ == devType;
        }
    };
    if (isConnected) {
        // If device already in list, remove it else do not modify the list
        connectedDevices_.erase(std::remove_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent),
            connectedDevices_.end());
        UpdateConnectedDevicesWhenConnecting(deviceDesc, deviceChangeDescriptor);
        result = HandleLocalDeviceConnected(devType, macAddress, deviceName, streamInfo);
        CHECK_AND_RETURN_LOG(result == SUCCESS, "Connect local device failed.");
    } else {
        UpdateConnectedDevicesWhenDisconnecting(deviceDesc, deviceChangeDescriptor);
        result = HandleLocalDeviceDisconnected(devType, macAddress);
        ResetToSpeaker(devType);
        if (devType == DEVICE_TYPE_USB_HEADSET && isArmUsbDevice_) {
            isArmUsbDevice_ = false;
        }
        CHECK_AND_RETURN_LOG(result == SUCCESS, "Disconnect local device failed.");
    }

    // fetch input&output device
    FetchDevice(true);
    FetchDevice(false);
    TriggerDeviceChangedCallback(deviceChangeDescriptor, isConnected);
    TriggerAvailableDeviceChangedCallback(deviceChangeDescriptor, isConnected);
}

void AudioPolicyService::OnDeviceStatusUpdated(AudioDeviceDescriptor &desc, bool isConnected)
{
    DeviceType devType = desc.deviceType_;
    string macAddress = desc.macAddress_;
    string deviceName = desc.deviceName_;

    AudioStreamInfo streamInfo = {};
#ifdef BLUETOOTH_ENABLE
    if (devType == DEVICE_TYPE_BLUETOOTH_A2DP && isConnected) {
        int32_t ret = Bluetooth::AudioA2dpManager::GetA2dpDeviceStreamInfo(macAddress, streamInfo);
        if (ret != SUCCESS) {
            AUDIO_ERR_LOG("Get a2dp device stream info failed!");
            return;
        }
    }
#endif

    AUDIO_INFO_LOG("Device connection state updated | TYPE[%{public}d] STATUS[%{public}d]", devType, isConnected);

    std::lock_guard<std::shared_mutex> lock(deviceStatusUpdateSharedMutex_);

    UpdateLocalGroupInfo(isConnected, macAddress, deviceName, streamInfo, desc);

    // fill device change action for callback
    std::vector<sptr<AudioDeviceDescriptor>> deviceChangeDescriptor = {};
    auto isPresent = [&devType, &macAddress] (const sptr<AudioDeviceDescriptor> &descriptor) {
        if (devType == DEVICE_TYPE_BLUETOOTH_A2DP && descriptor->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) {
            return descriptor->macAddress_ == macAddress;
        } else {
            return descriptor->deviceType_ == devType;
        }
    };
    bool isDeviceChanged = true;
    if (isConnected) {
        auto itr  = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent);
        if (itr != connectedDevices_.end()) {
            isDeviceChanged = false;
        }
        UpdateConnectedDevicesWhenConnecting(desc, deviceChangeDescriptor);
        int32_t result = HandleLocalDeviceConnected(devType, macAddress, deviceName, streamInfo);
        CHECK_AND_RETURN_LOG(result == SUCCESS, "Connect local device failed.");
    } else {
        UpdateConnectedDevicesWhenDisconnecting(desc, deviceChangeDescriptor);
        int32_t result = HandleLocalDeviceDisconnected(devType, macAddress);
        CHECK_AND_RETURN_LOG(result == SUCCESS, "Disconnect local device failed.");
    }

    // fetch input&output device
    FetchDevice(true);
    FetchDevice(false);
    if (isDeviceChanged) {
        TriggerDeviceChangedCallback(deviceChangeDescriptor, isConnected);
    }
    TriggerAvailableDeviceChangedCallback(deviceChangeDescriptor, isConnected);
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

void AudioPolicyService::UpdateA2dpOffloadFlagBySpatialService(
    const std::string& macAddress, std::unordered_map<uint32_t, bool> &sessionIDToSpatializationEnableMap)
{
    auto it = spatialDeviceMap_.find(macAddress);
    DeviceType spatialDevice;
    if (it != spatialDeviceMap_.end()) {
        spatialDevice = it->second;
    } else {
        AUDIO_DEBUG_LOG("we can't find the spatialDevice of hvs");
        spatialDevice = DEVICE_TYPE_NONE;
    }
    UpdateA2dpOffloadFlagForAllStream(sessionIDToSpatializationEnableMap, spatialDevice);
}

void AudioPolicyService::UpdateA2dpOffloadFlagForAllStream(
    std::unordered_map<uint32_t, bool> &sessionIDToSpatializationEnableMap, DeviceType deviceType)
{
#ifdef BLUETOOTH_ENABLE
    vector<Bluetooth::A2dpStreamInfo> allSessionInfos;
    Bluetooth::A2dpStreamInfo a2dpStreamInfo;
    vector<unique_ptr<AudioRendererChangeInfo>> audioRendererChangeInfos;
    streamCollector_.GetCurrentRendererChangeInfos(audioRendererChangeInfos);
    for (auto &changeInfo : audioRendererChangeInfos) {
        if (changeInfo->rendererState != RENDERER_RUNNING) {
            Bluetooth::AudioA2dpManager::OffloadStopPlaying(std::vector<int32_t>(1, changeInfo->sessionId));
            continue;
        }
        Bluetooth::AudioA2dpManager::OffloadStartPlaying(std::vector<int32_t>(1, changeInfo->sessionId));
        a2dpStreamInfo.sessionId = changeInfo->sessionId;
        a2dpStreamInfo.streamType = GetStreamType(changeInfo->sessionId);
        if (sessionIDToSpatializationEnableMap.count(static_cast<uint32_t>(a2dpStreamInfo.sessionId))) {
            a2dpStreamInfo.isSpatialAudio =
                sessionIDToSpatializationEnableMap[static_cast<uint32_t>(a2dpStreamInfo.sessionId)];
        } else {
            a2dpStreamInfo.isSpatialAudio = 0;
        }
        allSessionInfos.push_back(a2dpStreamInfo);
    }

    UpdateA2dpOffloadFlag(allSessionInfos, deviceType);

    if (((preA2dpOffloadFlag_ == A2DP_NOT_OFFLOAD) || (preA2dpOffloadFlag_ == NO_A2DP_DEVICE))
        && (a2dpOffloadFlag_ == A2DP_OFFLOAD)) {
        HandleA2dpDeviceInOffload();
        return;
    }

    if ((preA2dpOffloadFlag_ == A2DP_OFFLOAD) && (a2dpOffloadFlag_ == A2DP_OFFLOAD)) {
        GetA2dpOffloadCodecAndSendToDsp();
        return;
    }
#endif
    AUDIO_DEBUG_LOG("deviceType %{public}d", deviceType);
}

void AudioPolicyService::UpdateA2dpOffloadFlagForAllStream(DeviceType deviceType)
{
#ifdef BLUETOOTH_ENABLE
    vector<Bluetooth::A2dpStreamInfo> allSessionInfos;
    Bluetooth::A2dpStreamInfo a2dpStreamInfo;
    vector<unique_ptr<AudioRendererChangeInfo>> audioRendererChangeInfos;
    streamCollector_.GetCurrentRendererChangeInfos(audioRendererChangeInfos);
    for (auto &changeInfo : audioRendererChangeInfos) {
        if (changeInfo->rendererState != RENDERER_RUNNING) {
            Bluetooth::AudioA2dpManager::OffloadStopPlaying(std::vector<int32_t>(1, changeInfo->sessionId));
            continue;
        }
        Bluetooth::AudioA2dpManager::OffloadStartPlaying(std::vector<int32_t>(1, changeInfo->sessionId));
        a2dpStreamInfo.sessionId = changeInfo->sessionId;
        a2dpStreamInfo.streamType = GetStreamType(changeInfo->sessionId);
        StreamUsage tempStreamUsage = changeInfo->rendererInfo.streamUsage;
        AudioSpatializationState spatialState =
                AudioSpatializationService::GetAudioSpatializationService().GetSpatializationState(tempStreamUsage);
        a2dpStreamInfo.isSpatialAudio = spatialState.spatializationEnabled;
        allSessionInfos.push_back(a2dpStreamInfo);
    }

    UpdateA2dpOffloadFlag(allSessionInfos, deviceType);

    if (((preA2dpOffloadFlag_ == A2DP_NOT_OFFLOAD) || (preA2dpOffloadFlag_ == NO_A2DP_DEVICE))
        && (a2dpOffloadFlag_ == A2DP_OFFLOAD)) {
        HandleA2dpDeviceInOffload();
        return;
    }

    if ((preA2dpOffloadFlag_ == A2DP_OFFLOAD) && (a2dpOffloadFlag_ == A2DP_OFFLOAD)) {
        GetA2dpOffloadCodecAndSendToDsp();
        return;
    }
#endif
    AUDIO_DEBUG_LOG("deviceType %{public}d", deviceType);
}

void AudioPolicyService::OnDeviceConfigurationChanged(DeviceType deviceType, const std::string &macAddress,
    const std::string &deviceName, const AudioStreamInfo &streamInfo)
{
    AUDIO_DEBUG_LOG("OnDeviceConfigurationChanged in");
    // only for the active a2dp device.
    if ((deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) && !macAddress.compare(activeBTDevice_)
        && IsDeviceActive(deviceType)) {
        UpdateA2dpOffloadFlagForAllStream();
        if (!IsConfigurationUpdated(deviceType, streamInfo)) {
            AUDIO_DEBUG_LOG("Audio configuration same");
            if ((lastBTDevice_ != macAddress) && (preA2dpOffloadFlag_ == A2DP_OFFLOAD) &&
                (a2dpOffloadFlag_ == A2DP_NOT_OFFLOAD)) {
                HandleA2dpDeviceOutOffload();
            }
            lastBTDevice_ = macAddress;
            return;
        }

        uint32_t bufferSize = (streamInfo.samplingRate * GetSampleFormatValue(streamInfo.format)
            * streamInfo.channels) / (PCM_8_BIT * BT_BUFFER_ADJUSTMENT_FACTOR);
        AUDIO_DEBUG_LOG("Updated buffer size: %{public}d", bufferSize);
        connectedA2dpDeviceMap_[macAddress].streamInfo = streamInfo;
        if ((preA2dpOffloadFlag_ == A2DP_OFFLOAD) && (a2dpOffloadFlag_ == A2DP_NOT_OFFLOAD)) {
            HandleA2dpDeviceOutOffload();
            lastBTDevice_ = macAddress;
            return;
        }
        if (a2dpOffloadFlag_ != A2DP_OFFLOAD) {
            ReloadA2dpOffloadOnDeviceChanged(deviceType, macAddress, deviceName, streamInfo);
        }
    }
    lastBTDevice_ = macAddress;
}

void AudioPolicyService::ReloadA2dpOffloadOnDeviceChanged(DeviceType deviceType, const std::string &macAddress,
    const std::string &deviceName, const AudioStreamInfo &streamInfo)
{
    uint32_t bufferSize = (streamInfo.samplingRate * GetSampleFormatValue(streamInfo.format)
        * streamInfo.channels) / (PCM_8_BIT * BT_BUFFER_ADJUSTMENT_FACTOR);

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
                std::string currentActivePort = GetSinkPortName(currentActiveDevice_.deviceType_);
                AudioIOHandle activateDeviceIOHandle = IOHandles_[BLUETOOTH_SPEAKER];
                audioPolicyManager_.SuspendAudioDevice(currentActivePort, true);
                audioPolicyManager_.CloseAudioPort(activateDeviceIOHandle);

                // Load bt sink module again with new configuration
                AUDIO_DEBUG_LOG("Reload a2dp module [%{public}s]", moduleInfo.name.c_str());
                AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
                CHECK_AND_RETURN_LOG(ioHandle != OPEN_PORT_FAILURE, "OpenAudioPort failed %{public}d", ioHandle);
                IOHandles_[moduleInfo.name] = ioHandle;
                std::string portName = GetSinkPortName(deviceType);
                audioPolicyManager_.SetDeviceActive(ioHandle, deviceType, portName, true);
                audioPolicyManager_.SuspendAudioDevice(portName, false);

                auto isPresent = [&macAddress] (const sptr<AudioDeviceDescriptor> &descriptor) {
                    return descriptor->macAddress_ == macAddress;
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

void AudioPolicyService::RemoveDeviceInFastRouterMap(std::string networkId)
{
    std::lock_guard<std::mutex> lock(routerMapMutex_);
    std::unordered_map<int32_t, std::pair<std::string, DeviceRole>>::iterator it;
    for (it = fastRouterMap_.begin();it != fastRouterMap_.end();) {
        if (it->second.first == networkId) {
            fastRouterMap_.erase(it++);
        } else {
            it++;
        }
    }
}

void AudioPolicyService::SetDisplayName(const std::string &deviceName, bool isLocalDevice)
{
    for (const auto& deviceInfo : connectedDevices_) {
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
    g_dataShareHelper = DataShare::DataShareHelper::Creator(remoteObject, SETTINGS_DATA_BASE_URI,
        SETTINGS_DATA_EXT_URI);
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
    CHECK_AND_BREAK_LOG(g_dataShareHelper != nullptr, "RegisterNameMonitorHelper g_dataShareHelper is NULL.");
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

void AudioPolicyService::HandleOfflineDistributedDevice()
{
    std::vector<sptr<AudioDeviceDescriptor>> deviceChangeDescriptor = {};
    for (auto deviceDesc : connectedDevices_) {
        if (deviceDesc->networkId_ != LOCAL_NETWORK_ID) {
            const std::string networkId = deviceDesc->networkId_;
            UpdateConnectedDevicesWhenDisconnecting(deviceDesc, deviceChangeDescriptor);
            std::string moduleName = GetRemoteModuleName(networkId, GetDeviceRole(deviceDesc->deviceType_));
            if (IOHandles_.find(moduleName) != IOHandles_.end()) {
                audioPolicyManager_.CloseAudioPort(IOHandles_[moduleName]);
                IOHandles_.erase(moduleName);
            }
            RemoveDeviceInRouterMap(moduleName);
            RemoveDeviceInFastRouterMap(networkId);
            if (GetDeviceRole(deviceDesc->deviceType_) == DeviceRole::INPUT_DEVICE) {
                remoteCapturerSwitch_ = true;
            }
        }
    }
    FetchDevice(true);
    FetchDevice(false);
    TriggerDeviceChangedCallback(deviceChangeDescriptor, false);
    TriggerAvailableDeviceChangedCallback(deviceChangeDescriptor, false);
}

int32_t AudioPolicyService::HandleDistributedDeviceUpdate(DStatusInfo &statusInfo,
    std::vector<sptr<AudioDeviceDescriptor>> &deviceChangeDescriptor)
{
    std::lock_guard<std::shared_mutex> lock(deviceStatusUpdateSharedMutex_);
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
    if (statusInfo.isConnected) {
        for (auto devDes : connectedDevices_) {
            if (devDes->deviceType_ == devType && devDes->networkId_ == networkId) {
                return ERROR;
            }
        }
        int32_t ret = ActivateNewDevice(statusInfo.networkId, devType,
            statusInfo.connectType == ConnectType::CONNECT_TYPE_DISTRIBUTED);
        CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, ERROR, "DEVICE online but open audio device failed.");
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
        RemoveDeviceInFastRouterMap(networkId);
    }
    return SUCCESS;
}

void AudioPolicyService::OnDeviceStatusUpdated(DStatusInfo statusInfo, bool isStop)
{
    AUDIO_INFO_LOG("Device connection updated | HDI_PIN[%{public}d] CONNECT_STATUS[%{public}d] NETWORKID[%{public}s]",
        statusInfo.hdiPin, statusInfo.isConnected, statusInfo.networkId);
    if (isStop) {
        HandleOfflineDistributedDevice();
        return;
    }
    std::vector<sptr<AudioDeviceDescriptor>> deviceChangeDescriptor = {};
    int32_t ret = HandleDistributedDeviceUpdate(statusInfo, deviceChangeDescriptor);
    CHECK_AND_RETURN_LOG(ret == SUCCESS, "HandleDistributedDeviceUpdate return directly.");
    FetchDevice(true);
    FetchDevice(false);
    TriggerDeviceChangedCallback(deviceChangeDescriptor, statusInfo.isConnected);
    TriggerAvailableDeviceChangedCallback(deviceChangeDescriptor, statusInfo.isConnected);
    DeviceType devType = GetDeviceTypeFromPin(statusInfo.hdiPin);
    if (GetDeviceRole(devType) == DeviceRole::INPUT_DEVICE) {
        remoteCapturerSwitch_ = true;
    }
}

bool AudioPolicyService::OpenPortAndAddDeviceOnServiceConnected(AudioModuleInfo &moduleInfo)
{
    auto devType = GetDeviceType(moduleInfo.name);
    if (devType != DEVICE_TYPE_MIC) {
        AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
        if (ioHandle == OPEN_PORT_FAILURE) {
            AUDIO_INFO_LOG("[module_load]::Open port failed");
            return false;
        }
        IOHandles_[moduleInfo.name] = ioHandle;
        if (devType == DEVICE_TYPE_SPEAKER) {
            auto result = audioPolicyManager_.SetDeviceActive(ioHandle, devType, moduleInfo.name, true);
            if (result != SUCCESS) {
                AUDIO_ERR_LOG("[module_load]::Device failed %{public}d", devType);
                return false;
            }
        }
    }

    if (devType == DEVICE_TYPE_MIC) {
        primaryMicModuleInfo_ = moduleInfo;
    }

    if (devType == DEVICE_TYPE_SPEAKER || devType == DEVICE_TYPE_MIC) {
        AddAudioDevice(moduleInfo, devType);
    }
    return true;
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
                if (OpenPortAndAddDeviceOnServiceConnected(moduleInfo)) {
                    result = SUCCESS;
                }
                SetOffloadAvailableFromXML(moduleInfo);
            }
        }
    }

    if (result == SUCCESS) {
        AUDIO_INFO_LOG("[module_load]::Setting speaker as active device on bootup");
        hasModulesLoaded = true;
        unique_ptr<AudioDeviceDescriptor> outDevice = audioDeviceManager_.GetRenderDefaultDevice();
        currentActiveDevice_ = AudioDeviceDescriptor(*outDevice);
        ResetOffloadMode();
        unique_ptr<AudioDeviceDescriptor> inDevice = audioDeviceManager_.GetCaptureDefaultDevice();
        currentActiveInputDevice_ = AudioDeviceDescriptor(*inDevice);
        SetVolumeForSwitchDevice(currentActiveDevice_.deviceType_);
        OnPreferredDeviceUpdated(currentActiveDevice_, currentActiveInputDevice_.deviceType_);
        AddEarpiece();
        for (auto it = pnpDeviceList_.begin(); it != pnpDeviceList_.end(); ++it) {
            OnPnpDeviceStatusUpdated((*it).first, (*it).second);
        }
        audioEffectManager_.SetMasterSinkAvailable();
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

void AudioPolicyService::OnForcedDeviceSelected(DeviceType devType, const std::string &macAddress)
{
    if (macAddress.empty()) {
        AUDIO_ERR_LOG("OnForcedDeviceSelected failed as the macAddress is empty!");
        return;
    }
    std::vector<unique_ptr<AudioDeviceDescriptor>> bluetoothDevices =
        audioDeviceManager_.GetAvailableBluetoothDevice(devType, macAddress);
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors;
    for (auto &dec : bluetoothDevices) {
        if (dec->deviceRole_ == DeviceRole::OUTPUT_DEVICE) {
            sptr<AudioDeviceDescriptor> tempDec = new(std::nothrow) AudioDeviceDescriptor(*dec);
            audioDeviceDescriptors.push_back(move(tempDec));
        }
    }
    int32_t res = DeviceParamsCheck(DeviceRole::OUTPUT_DEVICE, audioDeviceDescriptors);
    CHECK_AND_RETURN_LOG(res == SUCCESS, "OnForcedDeviceSelected DeviceParamsCheck no success");
    if (devType == DEVICE_TYPE_BLUETOOTH_SCO) {
        audioStateManager_.SetPerferredCallRenderDevice(audioDeviceDescriptors[0]);
    } else {
        audioStateManager_.SetPerferredMediaRenderDevice(audioDeviceDescriptors[0]);
    }
    FetchDevice(true);
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
    Trace trace("AudioPolicyService::OnPreferredOutputDeviceUpdated:" + std::to_string(deviceType));
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
        case DeviceType::DEVICE_TYPE_USB_ARM_HEADSET:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_SCO: {
            const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
            CHECK_AND_RETURN_LOG(gsp != nullptr, "Service proxy unavailable");
            std::string sinkName = GetSinkPortName(deviceType);
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
    if (!moduleInfo.supportedRate_.empty() && !moduleInfo.supportedChannels_.empty()) {
        DeviceStreamInfo streamInfo = {};
        for (auto supportedRate : moduleInfo.supportedRate_) {
            streamInfo.samplingRate.insert(static_cast<AudioSamplingRate>(supportedRate));
        }
        for (auto supportedChannels : moduleInfo.supportedChannels_) {
            streamInfo.channels.insert(static_cast<AudioChannel>(supportedChannels));
        }
        audioDescriptor->SetDeviceCapability(streamInfo, 0);
    }

    audioDescriptor->deviceId_ = startDeviceId++;
    UpdateDisplayName(audioDescriptor);
    audioDeviceManager_.AddNewDevice(audioDescriptor);
    connectedDevices_.insert(connectedDevices_.begin(), audioDescriptor);
    AddMicrophoneDescriptor(audioDescriptor);
}

// Parser callbacks
void AudioPolicyService::OnXmlParsingCompleted(const std::unordered_map<ClassType, std::list<AudioModuleInfo>> &xmlData)
{
    AUDIO_INFO_LOG("%{public}s, device class num [%{public}zu]", __func__, xmlData.size());
    if (xmlData.empty()) {
        AUDIO_ERR_LOG("failed to parse xml file. Received data is empty");
        return;
    }

    deviceClassInfo_ = xmlData;
}

void AudioPolicyService::OnVolumeGroupParsed(std::unordered_map<std::string, std::string>& volumeGroupData)
{
    AUDIO_INFO_LOG("%{public}s, group data num [%{public}zu]", __func__, volumeGroupData.size());
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

void AudioPolicyService::AddAudioPolicyClientProxyMap(int32_t clientPid, const sptr<IAudioPolicyClient>& cb)
{
    if (audioPolicyServerHandler_ != nullptr) {
        audioPolicyServerHandler_->AddAudioPolicyClientProxyMap(clientPid, cb);
    }
}

void AudioPolicyService::ReduceAudioPolicyClientProxyMap(pid_t clientPid)
{
    if (audioPolicyServerHandler_ != nullptr) {
        audioPolicyServerHandler_->RemoveAudioPolicyClientProxyMap(clientPid);
    }
}

int32_t AudioPolicyService::SetAvailableDeviceChangeCallback(const int32_t clientId, const AudioDeviceUsage usage,
    const sptr<IRemoteObject> &object, bool hasBTPermission)
{
    sptr<IStandardAudioPolicyManagerListener> callback = iface_cast<IStandardAudioPolicyManagerListener>(object);

    if (callback != nullptr) {
        callback->hasBTPermission_ = hasBTPermission;

        if (audioPolicyServerHandler_ != nullptr) {
            audioPolicyServerHandler_->AddAvailableDeviceChangeMap(clientId, usage, callback);
        }
    }

    return SUCCESS;
}

int32_t AudioPolicyService::UnsetAvailableDeviceChangeCallback(const int32_t clientId, AudioDeviceUsage usage)
{
    AUDIO_INFO_LOG("Entered %{public}s", __func__);

    if (audioPolicyServerHandler_ != nullptr) {
        audioPolicyServerHandler_->RemoveAvailableDeviceChangeMap(clientId, usage);
    }
    return SUCCESS;
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

static bool HasLowLatencyCapability(DeviceType deviceType, bool isRemote)
{
    // Distributed devices are low latency devices
    if (isRemote) {
        return true;
    }

    switch (deviceType) {
        case DeviceType::DEVICE_TYPE_EARPIECE:
        case DeviceType::DEVICE_TYPE_SPEAKER:
        case DeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case DeviceType::DEVICE_TYPE_WIRED_HEADPHONES:
        case DeviceType::DEVICE_TYPE_USB_HEADSET:
            return true;

        case DeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
            return false;
        default:
            return false;
    }
}

void AudioPolicyService::UpdateDeviceInfo(DeviceInfo &deviceInfo, const sptr<AudioDeviceDescriptor> &desc,
    bool hasBTPermission, bool hasSystemPermission)
{
    deviceInfo.deviceType = desc->deviceType_;
    deviceInfo.deviceRole = desc->deviceRole_;
    deviceInfo.deviceId = desc->deviceId_;
    deviceInfo.channelMasks = desc->channelMasks_;
    deviceInfo.channelIndexMasks = desc->channelIndexMasks_;
    deviceInfo.displayName = desc->displayName_;
    deviceInfo.connectState = desc->connectState_;

    if (hasBTPermission) {
        deviceInfo.deviceName = desc->deviceName_;
        deviceInfo.macAddress = desc->macAddress_;
    } else {
        deviceInfo.deviceName = "";
        deviceInfo.macAddress = "";
    }

    deviceInfo.isLowLatencyDevice = HasLowLatencyCapability(deviceInfo.deviceType,
        desc->networkId_ != LOCAL_NETWORK_ID);

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

void AudioPolicyService::UpdateStreamChangeDeviceInfoForPlayback(AudioStreamChangeInfo &streamChangeInfo)
{
    std::vector<sptr<AudioDeviceDescriptor>> outputDevices = GetDevices(OUTPUT_DEVICES_FLAG);
    DeviceType activeDeviceType = currentActiveDevice_.deviceType_;
    DeviceRole activeDeviceRole = OUTPUT_DEVICE;
    for (sptr<AudioDeviceDescriptor> desc : outputDevices) {
        if ((desc->deviceType_ == activeDeviceType) && (desc->deviceRole_ == activeDeviceRole)) {
            if (activeDeviceType == DEVICE_TYPE_BLUETOOTH_A2DP &&
                desc->macAddress_ != currentActiveDevice_.macAddress_) {
                // This A2DP device is not the active A2DP device. Skip it.
                continue;
            }
            UpdateDeviceInfo(streamChangeInfo.audioRendererChangeInfo.outputDeviceInfo, desc, true, true);
            break;
        }
    }
}

void AudioPolicyService::UpdateStreamChangeDeviceInfoForRecord(AudioStreamChangeInfo &streamChangeInfo)
{
    std::vector<sptr<AudioDeviceDescriptor>> inputDevices = GetDevices(INPUT_DEVICES_FLAG);
    DeviceType activeDeviceType = currentActiveInputDevice_.deviceType_;
    DeviceRole activeDeviceRole = INPUT_DEVICE;
    for (sptr<AudioDeviceDescriptor> desc : inputDevices) {
        if ((desc->deviceType_ == activeDeviceType) && (desc->deviceRole_ == activeDeviceRole)) {
            int32_t sessionId = streamChangeInfo.audioCapturerChangeInfo.sessionId;
            AddAudioCapturerMicrophoneDescriptor(sessionId, activeDeviceType);
            break;
        }
    }
}

int32_t AudioPolicyService::RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
    const sptr<IRemoteObject> &object)
{
    if (mode == AUDIO_MODE_RECORD) {
        UpdateStreamChangeDeviceInfoForRecord(streamChangeInfo);
    }
    return streamCollector_.RegisterTracker(mode, streamChangeInfo, object);
}

int32_t AudioPolicyService::UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo)
{
    AUDIO_INFO_LOG("Entered AudioPolicyService::%{public}s", __func__);
    if (mode == AUDIO_MODE_PLAYBACK) {
        if (streamChangeInfo.audioRendererChangeInfo.rendererState == RENDERER_RUNNING) {
            vector<unique_ptr<AudioRendererChangeInfo>> rendererChangeInfos;
            rendererChangeInfos.push_back(
                make_unique<AudioRendererChangeInfo>(streamChangeInfo.audioRendererChangeInfo));
            streamCollector_.GetRendererStreamInfo(streamChangeInfo, *rendererChangeInfos[0]);
            FetchOutputDevice(rendererChangeInfos, true);
            streamChangeInfo.audioRendererChangeInfo.outputDeviceInfo = rendererChangeInfos[0]->outputDeviceInfo;
        }
    } else if (mode == AUDIO_MODE_RECORD) {
        if (streamChangeInfo.audioCapturerChangeInfo.capturerState == CAPTURER_RUNNING) {
            vector<unique_ptr<AudioCapturerChangeInfo>> capturerChangeInfos;
            capturerChangeInfos.push_back(
                make_unique<AudioCapturerChangeInfo>(streamChangeInfo.audioCapturerChangeInfo));
            streamCollector_.GetCapturerStreamInfo(streamChangeInfo, *capturerChangeInfos[0]);
            FetchInputDevice(capturerChangeInfos, true);
            streamChangeInfo.audioCapturerChangeInfo.inputDeviceInfo = capturerChangeInfos[0]->inputDeviceInfo;
        }
        UpdateStreamChangeDeviceInfoForRecord(streamChangeInfo);
        std::lock_guard<std::mutex> lock(microphonesMutex_);
        if (streamChangeInfo.audioCapturerChangeInfo.capturerState == CAPTURER_RELEASED) {
            audioCaptureMicrophoneDescriptor_.erase(streamChangeInfo.audioCapturerChangeInfo.sessionId);
        }
    }
    int32_t ret = streamCollector_.UpdateTracker(mode, streamChangeInfo);
    UpdateA2dpOffloadFlagForAllStream(DEVICE_TYPE_BLUETOOTH_A2DP);
    return ret;
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
    DeviceType activeDeviceType = currentActiveDevice_.deviceType_;
    DeviceRole activeDeviceRole = OUTPUT_DEVICE;
    for (sptr<AudioDeviceDescriptor> desc : outputDevices) {
        if ((desc->deviceType_ == activeDeviceType) && (desc->deviceRole_ == activeDeviceRole)) {
            if (activeDeviceType == DEVICE_TYPE_BLUETOOTH_A2DP &&
                desc->macAddress_ != currentActiveDevice_.macAddress_) {
                // This A2DP device is not the active A2DP device. Skip it.
                continue;
            }
            size_t rendererInfosSize = audioRendererChangeInfos.size();
            for (size_t i = 0; i < rendererInfosSize; i++) {
                UpdateRendererInfoWhenNoPermission(audioRendererChangeInfos[i], hasSystemPermission);
                UpdateDeviceInfo(audioRendererChangeInfos[i]->outputDeviceInfo, desc, hasBTPermission,
                    hasSystemPermission);
            }
            break;
        }
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
    DeviceType activeDeviceType = currentActiveInputDevice_.deviceType_;
    DeviceRole activeDeviceRole = INPUT_DEVICE;
    for (sptr<AudioDeviceDescriptor> desc : inputDevices) {
        if ((desc->deviceType_ == activeDeviceType) && (desc->deviceRole_ == activeDeviceRole)) {
            size_t capturerInfosSize = audioCapturerChangeInfos.size();
            for (size_t i = 0; i < capturerInfosSize; i++) {
                UpdateCapturerInfoWhenNoPermission(audioCapturerChangeInfos[i], hasSystemPermission);
                UpdateDeviceInfo(audioCapturerChangeInfos[i]->inputDeviceInfo, desc, hasBTPermission,
                    hasSystemPermission);
            }
            break;
        }
    }

    return status;
}

void AudioPolicyService::RegisteredTrackerClientDied(pid_t uid)
{
    RemoveAudioCapturerMicrophoneDescriptor(static_cast<int32_t>(uid));
    streamCollector_.RegisteredTrackerClientDied(static_cast<int32_t>(uid));
}

int32_t AudioPolicyService::ReconfigureAudioChannel(const uint32_t &channelCount, DeviceType deviceType)
{
    if (currentActiveDevice_.deviceType_ != DEVICE_TYPE_FILE_SINK) {
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
AudioIOHandle AudioPolicyService::GetSinkIOHandle(InternalDeviceType deviceType)
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
        case InternalDeviceType::DEVICE_TYPE_USB_ARM_HEADSET:
            ioHandle = IOHandles_[USB_SPEAKER];
            break;
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
            ioHandle = IOHandles_[BLUETOOTH_SPEAKER];
            break;
        case InternalDeviceType::DEVICE_TYPE_FILE_SINK:
            ioHandle = IOHandles_[FILE_SINK];
            break;
        default:
            ioHandle = IOHandles_[PRIMARY_SPEAKER];
            break;
    }
    return ioHandle;
}

AudioIOHandle AudioPolicyService::GetSourceIOHandle(InternalDeviceType deviceType)
{
    AudioIOHandle ioHandle;
    switch (deviceType) {
        case InternalDeviceType::DEVICE_TYPE_USB_ARM_HEADSET:
            ioHandle = IOHandles_[USB_MIC];
            break;
        case InternalDeviceType::DEVICE_TYPE_MIC:
            ioHandle = IOHandles_[PRIMARY_MIC];
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

    DeviceType activeDevice = DEVICE_TYPE_NONE;
    auto isOutputDevicePresent = [&activeDevice, this] (const sptr<AudioDeviceDescriptor> &desc) {
        CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid device descriptor");
        if ((activeDevice == desc->deviceType_) && (OUTPUT_DEVICE == desc->deviceRole_)) {
            if (activeDevice == DEVICE_TYPE_BLUETOOTH_A2DP) {
                // If the device type is A2DP, need to compare mac address in addition.
                return desc->macAddress_ == currentActiveDevice_.macAddress_;
            }
            return true;
        }
        return false;
    };
    auto isInputDevicePresent = [&activeDevice] (const sptr<AudioDeviceDescriptor> &desc) {
        CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid device descriptor");
        return ((activeDevice == desc->deviceType_) && (INPUT_DEVICE == desc->deviceRole_));
    };

    for (sptr<AudioDeviceDescriptor> deviceDesc : desc) {
        if (deviceDesc->deviceRole_ == OUTPUT_DEVICE) {
            activeDevice = currentActiveDevice_.deviceType_;
            auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isOutputDevicePresent);
            if (itr != connectedDevices_.end()) {
                DeviceInfo outputDevice = {};
                UpdateDeviceInfo(outputDevice, *itr, true, true);
                streamCollector_.UpdateTracker(AUDIO_MODE_PLAYBACK, outputDevice);
            }
        }

        if (deviceDesc->deviceRole_ == INPUT_DEVICE) {
            activeDevice = currentActiveInputDevice_.deviceType_;
            auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isInputDevicePresent);
            if (itr != connectedDevices_.end()) {
                DeviceInfo inputDevice = {};
                UpdateDeviceInfo(inputDevice, *itr, true, true);
                UpdateAudioCapturerMicrophoneDescriptor((*itr)->deviceType_);
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
        if ((desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP) || (desc->deviceType_ == DEVICE_TYPE_BLUETOOTH_SCO)) {
            sptr<AudioDeviceDescriptor> copyDesc = new AudioDeviceDescriptor(desc);
            copyDesc->deviceName_ = "";
            copyDesc->macAddress_ = "";
            desc = copyDesc;
        }
    }
}

int32_t AudioPolicyService::SetDeviceAbsVolumeSupported(const std::string &macAddress, const bool support)
{
    std::lock_guard<std::mutex> lock(a2dpDeviceMapMutex_);
    // Maximum number of attempts, preventing situations where a2dp device has not yet finished coming online.
    int maxRetries = 3;
    int retryCount = 0;
    while (retryCount < maxRetries) {
        retryCount++;
        auto configInfoPos = connectedA2dpDeviceMap_.find(macAddress);
        if (configInfoPos != connectedA2dpDeviceMap_.end()) {
            configInfoPos->second.absVolumeSupport = support;
            audioPolicyManager_.SetAbsVolumeScene(support);
            break;
        }
        if (retryCount == maxRetries) {
            AUDIO_ERR_LOG("SetDeviceAbsVolumeSupported failed, can't find device for macAddress:[%{public}s]",
                macAddress.c_str());
            return ERROR;
        }
        usleep(ABS_VOLUME_SUPPORT_RETRY_INTERVAL_IN_MICROSECONDS);
    }

    AUDIO_INFO_LOG("SetDeviceAbsVolumeSupported success for macAddress:[%{public}s], support: %{public}d",
        macAddress.c_str(), support);
    return SUCCESS;
}

bool AudioPolicyService::IsAbsVolumeScene() const
{
    return audioPolicyManager_.IsAbsVolumeScene();
}

int32_t AudioPolicyService::SetA2dpDeviceVolume(const std::string &macAddress, const int32_t volumeLevel)
{
    std::lock_guard<std::mutex> lock(a2dpDeviceMapMutex_);
    auto configInfoPos = connectedA2dpDeviceMap_.find(macAddress);
    if (configInfoPos == connectedA2dpDeviceMap_.end() || !configInfoPos->second.absVolumeSupport) {
        AUDIO_ERR_LOG("SetA2dpDeviceVolume failed for macAddress:[%{public}s]", macAddress.c_str());
        return ERROR;
    }
    configInfoPos->second.volumeLevel = volumeLevel;
    SetOffloadVolume(volumeLevel);
    if (volumeLevel > 0) {
        configInfoPos->second.mute = false;
    }
    AUDIO_INFO_LOG("SetA2dpDeviceVolume success for macaddress:[%{public}s], volume value:[%{public}d]",
        macAddress.c_str(), volumeLevel);
    return SUCCESS;
}

int32_t AudioPolicyService::GetA2dpDeviceVolume(const std::string& macAddress, int32_t& volumeLevel)
{
    std::lock_guard<std::mutex> lock(a2dpDeviceMapMutex_);
    auto configInfoPos = connectedA2dpDeviceMap_.find(macAddress);
    if (configInfoPos == connectedA2dpDeviceMap_.end() || !configInfoPos->second.absVolumeSupport) {
        AUDIO_ERR_LOG("SetA2dpDeviceVolume failed for macAddress:[%{public}s]", macAddress.c_str());
        return ERROR;
    }
    volumeLevel = configInfoPos->second.volumeLevel;
    return SUCCESS;
}

void AudioPolicyService::TriggerDeviceChangedCallback(const vector<sptr<AudioDeviceDescriptor>> &desc, bool isConnected)
{
    Trace trace("AudioPolicyService::TriggerDeviceChangedCallback");

    WriteDeviceChangedSysEvents(desc, isConnected);
    if (audioPolicyServerHandler_ != nullptr) {
        audioPolicyServerHandler_->SendDeviceChangedCallback(desc, isConnected);
    }
}

bool AudioPolicyService::IsInputDevice(DeviceType deviceType) const
{
    switch (deviceType) {
        case DeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
        case DeviceType::DEVICE_TYPE_MIC:
        case DeviceType::DEVICE_TYPE_WAKEUP:
        case DeviceType::DEVICE_TYPE_USB_HEADSET:
        case DeviceType::DEVICE_TYPE_USB_ARM_HEADSET:
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
        case DeviceType::DEVICE_TYPE_USB_ARM_HEADSET:
            return true;
        default:
            return false;
    }
}

DeviceRole AudioPolicyService::GetDeviceRole(DeviceType deviceType) const
{
    switch (deviceType) {
        case DeviceType::DEVICE_TYPE_EARPIECE:
        case DeviceType::DEVICE_TYPE_SPEAKER:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
        case DeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case DeviceType::DEVICE_TYPE_WIRED_HEADPHONES:
        case DeviceType::DEVICE_TYPE_USB_HEADSET:
        case DeviceType::DEVICE_TYPE_USB_ARM_HEADSET:
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
    AUDIO_DEBUG_LOG("Current input device is %{public}d", currentActiveInputDevice_.deviceType_);

    switch (deviceType) {
        case DEVICE_TYPE_EARPIECE:
        case DEVICE_TYPE_SPEAKER:
        case DEVICE_TYPE_BLUETOOTH_A2DP:
            currentActiveInputDevice_.deviceType_ = DEVICE_TYPE_MIC;
            break;
        case DEVICE_TYPE_FILE_SINK:
            currentActiveInputDevice_.deviceType_ = DEVICE_TYPE_FILE_SOURCE;
            break;
        case DEVICE_TYPE_USB_ARM_HEADSET:
            currentActiveInputDevice_.deviceType_ = DEVICE_TYPE_USB_HEADSET;
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
        case DEVICE_TYPE_USB_HEADSET:
        case DEVICE_TYPE_BLUETOOTH_SCO:
            currentActiveInputDevice_.deviceType_ = deviceType;
            break;
        default:
            break;
    }

    AUDIO_DEBUG_LOG("Input device updated to %{public}d", currentActiveInputDevice_.deviceType_);
}

int32_t AudioPolicyService::UpdateStreamState(int32_t clientUid,
    StreamSetStateEventInternal &streamSetStateEventInternal)
{
    return streamCollector_.UpdateStreamState(clientUid, streamSetStateEventInternal);
}

AudioStreamType AudioPolicyService::GetStreamType(int32_t sessionId)
{
    return streamCollector_.GetStreamType(sessionId);
}

int32_t AudioPolicyService::GetUid(int32_t sessionId)
{
    return streamCollector_.GetUid(sessionId);
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
        case OHOS::AudioStandard::AUDIO_PIN_OUT_USB_HEADSET:
        case OHOS::AudioStandard::AUDIO_PIN_IN_USB_HEADSET:
            return DeviceType::DEVICE_TYPE_USB_ARM_HEADSET;
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
    sptr<PolicyProviderWrapper> wrapper = new(std::nothrow) PolicyProviderWrapper(this);
    CHECK_AND_RETURN_LOG(wrapper != nullptr, "Get null PolicyProviderWrapper");
    sptr<IRemoteObject> object = wrapper->AsObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("RegiestPolicy AsObject is nullptr");
        return;
    }
    int32_t ret = gsp->RegiestPolicyProvider(object);
    AUDIO_DEBUG_LOG("RegiestPolicy result:%{public}d", ret);
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
        deviceInfo.networkId = LOCAL_NETWORK_ID;
        deviceInfo.deviceRole = INPUT_DEVICE;
        deviceInfo.deviceType = currentActiveInputDevice_.deviceType_;
    } else {
        deviceInfo.deviceId = 6; // 6 for test
        deviceInfo.networkId = LOCAL_NETWORK_ID;
        deviceInfo.deviceType = currentActiveDevice_.deviceType_;
        deviceInfo.deviceRole = OUTPUT_DEVICE;
    }
    AudioStreamInfo targetStreamInfo = {SAMPLE_RATE_48000, ENCODING_PCM, SAMPLE_S16LE, STEREO}; // note: read from xml
    deviceInfo.audioStreamInfo = targetStreamInfo;
    deviceInfo.deviceName = "mmap_device";
    std::lock_guard<std::mutex> lock(routerMapMutex_);
    if (fastRouterMap_.count(config.appInfo.appUid) &&
        fastRouterMap_[config.appInfo.appUid].second == deviceInfo.deviceRole) {
        deviceInfo.networkId = fastRouterMap_[config.appInfo.appUid].first;
        AUDIO_INFO_LOG("use networkid in fastRouterMap_ :%{public}s ", deviceInfo.networkId.c_str());
    }
    return SUCCESS;
}

int32_t AudioPolicyService::InitSharedVolume(std::shared_ptr<AudioSharedMemory> &buffer)
{
    AUDIO_INFO_LOG("Enter InitSharedVolume");
    CHECK_AND_RETURN_RET_LOG(policyVolumeMap_ != nullptr && policyVolumeMap_->GetBase() != nullptr,
        ERR_OPERATION_FAILED, "Get shared memory failed!");

    // init volume map
    // todo device
    for (size_t i = 0; i < IPolicyProvider::GetVolumeVectorSize(); i++) {
        int32_t currentVolumeLevel = audioPolicyManager_.GetSystemVolumeLevel(g_volumeIndexVector[i].first, false);
        float volFloat =
            GetSystemVolumeInDb(g_volumeIndexVector[i].first, currentVolumeLevel, currentActiveDevice_.deviceType_);
        volumeVector_[i].isMute = false;
        volumeVector_[i].volumeFloat = volFloat;
        volumeVector_[i].volumeInt = 0;
    }
    buffer = policyVolumeMap_;

    return SUCCESS;
}

bool AudioPolicyService::GetSharedVolume(AudioVolumeType streamType, DeviceType deviceType, Volume &vol)
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

bool AudioPolicyService::SetSharedVolume(AudioVolumeType streamType, DeviceType deviceType, Volume vol)
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
    g_adProxy->NotifyStreamVolumeChanged(streamType, vol.volumeFloat);
    return true;
}

void AudioPolicyService::SetParameterCallback(const std::shared_ptr<AudioParameterCallback>& callback)
{
    AUDIO_INFO_LOG("Enter SetParameterCallback");
    sptr<AudioManagerListenerStub> parameterChangeCbStub = new(std::nothrow) AudioManagerListenerStub();
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
    AUDIO_DEBUG_LOG("SetParameterCallback done");
    gsp->SetParameterCallback(object);
}

void AudioPolicyService::MaxRenderInstanceInit()
{
    // init max renderer instances before kvstore start by local prop for bootanimation
    char currentMaxRendererInstances[100] = {0}; // 100 for system parameter usage
    auto ret = GetParameter("persist.multimedia.audio.maxrendererinstances", MAX_RENDERER_INSTANCE,
        currentMaxRendererInstances, sizeof(currentMaxRendererInstances));
    if (ret > 0) {
        maxRendererInstances_ = atoi(currentMaxRendererInstances);
        AUDIO_INFO_LOG("Get max renderer instances success %{public}d", maxRendererInstances_);
    } else {
        AUDIO_ERR_LOG("Get max renderer instances failed %{public}d", ret);
    }
}

int32_t AudioPolicyService::GetMaxRendererInstances()
{
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
    isBtListenerRegistered = false;
    Bluetooth::AudioA2dpManager::DisconnectBluetoothA2dpSink();
}
#endif

void AudioPolicyService::RegisterBluetoothListener()
{
#ifdef BLUETOOTH_ENABLE
    AUDIO_INFO_LOG("Enter AudioPolicyService::RegisterBluetoothListener");
    Bluetooth::RegisterDeviceObserver(deviceStatusListener_->deviceObserver_);
    if (isBtListenerRegistered) {
        AUDIO_INFO_LOG("audio policy service already register bt listerer, return");
        return;
    }
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

float AudioPolicyService::GetSystemVolumeInDb(AudioVolumeType volumeType, int32_t volumeLevel,
    DeviceType deviceType) const
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

int32_t AudioPolicyService::SetPlaybackCapturerFilterInfos(const AudioPlaybackCaptureConfig &config)
{
    if (!config.silentCapture) {
        LoadLoopback();
    }
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    if (gsp == nullptr) {
        AUDIO_ERR_LOG("SetPlaybackCapturerFilterInfos error for g_adProxy null");
        return ERR_OPERATION_FAILED;
    }

    if (gsp->SetCaptureSilentState(config.silentCapture)) {
        AUDIO_ERR_LOG("SetPlaybackCapturerFilterInfos, SetCaptureSilentState failed");
        return ERR_OPERATION_FAILED;
    }

    std::vector<int32_t> targetUsages;
    AUDIO_INFO_LOG("SetPlaybackCapturerFilterInfos");
    for (size_t i = 0; i < config.filterOptions.usages.size(); i++) {
        if (count(targetUsages.begin(), targetUsages.end(), config.filterOptions.usages[i]) == 0) {
            targetUsages.emplace_back(config.filterOptions.usages[i]); // deduplicate
        }
    }

    return gsp->SetSupportStreamUsage(targetUsages);
}

void AudioPolicyService::UpdateOutputDeviceSelectedByCalling(DeviceType deviceType)
{
    if ((deviceType == DEVICE_TYPE_DEFAULT) || (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP)) {
        return;
    }
    auto uid = IPCSkeleton::GetCallingUid();
    std::lock_guard<std::mutex> lock(outputDeviceSelectedByCallingMutex_);
    if (deviceType == DEVICE_TYPE_NONE) {
        outputDeviceSelectedByCalling_.erase(uid);
        return;
    }
    outputDeviceSelectedByCalling_[uid] = deviceType;
}

bool AudioPolicyService::IsConnectedOutputDevice(const sptr<AudioDeviceDescriptor> &desc)
{
    DeviceType deviceType = desc->deviceType_;

    if (desc->deviceRole_ != DeviceRole::OUTPUT_DEVICE) {
        AUDIO_ERR_LOG("Not output device!");
        return false;
    }

    auto isPresent = [&deviceType] (const sptr<AudioDeviceDescriptor> &desc) {
        CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid device descriptor");
        if (deviceType == DEVICE_TYPE_FILE_SINK) {
            return false;
        }
        return ((deviceType == desc->deviceType_) && (desc->deviceRole_ == DeviceRole::OUTPUT_DEVICE));
    };

    auto itr = std::find_if(connectedDevices_.begin(), connectedDevices_.end(), isPresent);
    if (itr == connectedDevices_.end()) {
        AUDIO_ERR_LOG("Device not available");
        return false;
    }

    return true;
}

int32_t AudioPolicyService::GetHardwareOutputSamplingRate(const sptr<AudioDeviceDescriptor> &desc)
{
    int32_t rate = 48000;

    if (desc == nullptr) {
        AUDIO_ERR_LOG("desc is null!");
        return -1;
    }

    if (!IsConnectedOutputDevice(desc)) {
        return -1;
    }

    DeviceType clientDevType = desc->deviceType_;
    for (const auto &device : deviceClassInfo_) {
        auto moduleInfoList = device.second;
        for (auto &moduleInfo : moduleInfoList) {
            auto serverDevType = GetDeviceType(moduleInfo.name);
            if (clientDevType == serverDevType) {
                rate = atoi(moduleInfo.rate.c_str());
                return rate;
            }
        }
    }

    return rate;
}

void AudioPolicyService::AddMicrophoneDescriptor(sptr<AudioDeviceDescriptor> &deviceDescriptor)
{
    std::lock_guard<std::mutex> lock(microphonesMutex_);
    if (deviceDescriptor->deviceRole_ == INPUT_DEVICE &&
        deviceDescriptor->deviceType_ != DEVICE_TYPE_FILE_SOURCE) {
        auto isPresent = [&deviceDescriptor](const sptr<MicrophoneDescriptor> &desc) {
            CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid device descriptor");
            return desc->deviceType_ == deviceDescriptor->deviceType_;
        };

        auto iter = std::find_if(connectedMicrophones_.begin(), connectedMicrophones_.end(), isPresent);
        if (iter == connectedMicrophones_.end()) {
            sptr<MicrophoneDescriptor> micDesc = new (std::nothrow) MicrophoneDescriptor(startMicrophoneId++,
                deviceDescriptor->deviceType_);
            connectedMicrophones_.push_back(micDesc);
        }
    }
}

void AudioPolicyService::RemoveMicrophoneDescriptor(sptr<AudioDeviceDescriptor> &deviceDescriptor)
{
    std::lock_guard<std::mutex> lock(microphonesMutex_);
    auto isPresent = [&deviceDescriptor](const sptr<MicrophoneDescriptor> &desc) {
        CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid device descriptor");
        return desc->deviceType_ == deviceDescriptor->deviceType_;
    };

    auto iter = std::find_if(connectedMicrophones_.begin(), connectedMicrophones_.end(), isPresent);
    if (iter != connectedMicrophones_.end()) {
        connectedMicrophones_.erase(iter);
    }
}

void AudioPolicyService::AddAudioCapturerMicrophoneDescriptor(int32_t sessionId, DeviceType devType)
{
    std::lock_guard<std::mutex> lock(microphonesMutex_);
    auto isPresent = [&devType] (const sptr<MicrophoneDescriptor> &desc) {
        CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid microphone descriptor");
        return (devType == desc->deviceType_);
    };

    auto itr = std::find_if(connectedMicrophones_.begin(), connectedMicrophones_.end(), isPresent);
    if (itr != connectedMicrophones_.end()) {
        audioCaptureMicrophoneDescriptor_[sessionId] = *itr;
    }
}

vector<sptr<MicrophoneDescriptor>> AudioPolicyService::GetAudioCapturerMicrophoneDescriptors(int32_t sessionId)
{
    std::lock_guard<std::mutex> lock(microphonesMutex_);
    vector<sptr<MicrophoneDescriptor>> descList = {};
    const auto desc = audioCaptureMicrophoneDescriptor_.find(sessionId);
    if (desc != audioCaptureMicrophoneDescriptor_.end()) {
        sptr<MicrophoneDescriptor> micDesc = new (std::nothrow) MicrophoneDescriptor(desc->second);
        descList.push_back(micDesc);
    }
    return descList;
}

vector<sptr<MicrophoneDescriptor>> AudioPolicyService::GetAvailableMicrophones()
{
    return connectedMicrophones_;
}

void AudioPolicyService::UpdateAudioCapturerMicrophoneDescriptor(DeviceType devType)
{
    std::lock_guard<std::mutex> lock(microphonesMutex_);
    auto isPresent = [&devType] (const sptr<MicrophoneDescriptor> &desc) {
        CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "Invalid microphone descriptor");
        return (devType == desc->deviceType_);
    };

    auto itr = std::find_if(connectedMicrophones_.begin(), connectedMicrophones_.end(), isPresent);
    if (itr != connectedMicrophones_.end()) {
        for (auto& desc : audioCaptureMicrophoneDescriptor_) {
            if (desc.second->deviceType_ != devType) {
                desc.second = *itr;
            }
        }
    }
}

void AudioPolicyService::RemoveAudioCapturerMicrophoneDescriptor(int32_t uid)
{
    std::lock_guard<std::mutex> lock(microphonesMutex_);
    vector<unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
    streamCollector_.GetCurrentCapturerChangeInfos(audioCapturerChangeInfos);

    for (auto &info : audioCapturerChangeInfos) {
        if (info->clientUID != uid && info->createrUID != uid) {
            continue;
        }
        audioCaptureMicrophoneDescriptor_.erase(info->sessionId);
    }
}

std::tuple<SourceType, uint32_t, uint32_t> AudioPolicyService::FetchTargetInfoForSessionAdd(
    const SessionInfo sessionInfo)
{
    uint32_t highestSupportedRate = *(primaryMicModuleInfo_.supportedRate_.rbegin());
    uint32_t highestSupportedChannels = *(primaryMicModuleInfo_.supportedChannels_.rbegin());
    SourceType targetSourceType;
    uint32_t targetRate;
    uint32_t targetChannels;
    if (sessionInfo.sourceType == SOURCE_TYPE_VOICE_COMMUNICATION) {
        targetSourceType = SOURCE_TYPE_VOICE_COMMUNICATION;
        targetRate = sessionInfo.rate;
        targetChannels = sessionInfo.channels;
        if (primaryMicModuleInfo_.supportedRate_.count(targetRate) == 0) {
            AUDIO_INFO_LOG("targetRate: %{public}u is not supported rate, using highestSupportedRate: %{public}u",
                targetRate, highestSupportedRate);
            targetRate = highestSupportedRate;
        }
        if (primaryMicModuleInfo_.supportedChannels_.count(targetChannels) == 0) {
            AUDIO_INFO_LOG(
                "targetChannels: %{public}u is not supported rate, using highestSupportedChannels: %{public}u",
                targetChannels, highestSupportedChannels);
            targetChannels = highestSupportedChannels;
        }
    } else if (sessionInfo.sourceType == SOURCE_TYPE_VOICE_CALL) {
        targetSourceType = SOURCE_TYPE_VOICE_CALL;
        targetRate = highestSupportedRate;
        targetChannels = highestSupportedChannels;
    } else {
        // For normal sourcetype, continue to use the default value
        targetSourceType = SOURCE_TYPE_MIC;
        targetRate = highestSupportedRate;
        targetChannels = highestSupportedChannels;
    }
    return {targetSourceType, targetRate, targetChannels};
}

void AudioPolicyService::OnCapturerSessionRemoved(uint64_t sessionID)
{
    if (sessionWithSpecialSourceType_.count(sessionID) > 0) {
        sessionWithSpecialSourceType_.erase(sessionID);
        return;
    }

    if (sessionWithNormalSourceType_.count(sessionID) > 0) {
        sessionWithNormalSourceType_.erase(sessionID);
        if (!sessionWithNormalSourceType_.empty()) {
            return;
        }
        {
            AudioIOHandle activateDeviceIOHandle;
            std::lock_guard<std::mutex> lck(ioHandlesMutex_);
            activateDeviceIOHandle = IOHandles_[PRIMARY_MIC];

            int32_t result = audioPolicyManager_.CloseAudioPort(activateDeviceIOHandle);
            CHECK_AND_RETURN_LOG(result == SUCCESS,
                "OnCapturerSessionRemoved: CloseAudioPort failed %{public}d", result);

            IOHandles_.erase(PRIMARY_MIC);
        }
        return;
    }

    AUDIO_INFO_LOG("Sessionid:%{public}" PRIu64 " not added, directly placed into sessionIdisRemovedSet_", sessionID);
    sessionIdisRemovedSet_.insert(sessionID);
}

void AudioPolicyService::OnCapturerSessionAdded(uint64_t sessionID, SessionInfo sessionInfo)
{
    if (sessionIdisRemovedSet_.count(sessionID) > 0) {
        sessionIdisRemovedSet_.erase(sessionID);
        AUDIO_INFO_LOG("sessionID: %{public}" PRIu64 " had already been removed earlier", sessionID);
        return;
    }
    if (specialSourceTypeSet_.count(sessionInfo.sourceType) == 0) {
        auto [targetSourceType, targetRate, targetChannels] = FetchTargetInfoForSessionAdd(sessionInfo);
        bool isSourceLoaded = !sessionWithNormalSourceType_.empty();
        bool needReloadSource = (isSourceLoaded &&
            ((targetSourceType != currentSourceType) || (currentRate != targetRate)));
        std::lock_guard<std::mutex> lck(ioHandlesMutex_);
        if (needReloadSource) {
            AudioIOHandle activateDeviceIOHandle;
            {
                activateDeviceIOHandle = IOHandles_[PRIMARY_MIC];

                int32_t result = audioPolicyManager_.CloseAudioPort(activateDeviceIOHandle);
                CHECK_AND_RETURN_LOG(result == SUCCESS,
                    "CapturerSessionAdded: CloseAudioPort failed %{public}d", result);

                IOHandles_.erase(PRIMARY_MIC);
            }
        }
        if (needReloadSource || !(isSourceLoaded)) {
            auto moduleInfo = primaryMicModuleInfo_;
            moduleInfo.rate = std::to_string(targetRate);
            moduleInfo.channels = std::to_string(targetChannels);
            moduleInfo.sourceType = std::to_string(targetSourceType);
            AudioIOHandle ioHandle = audioPolicyManager_.OpenAudioPort(moduleInfo);
            CHECK_AND_RETURN_LOG(ioHandle != OPEN_PORT_FAILURE,
                "CapturerSessionAdded: OpenAudioPort failed %{public}d", ioHandle);

            IOHandles_[PRIMARY_MIC] = ioHandle;
            if (FetchHighPriorityDevice(false) == DEVICE_TYPE_MIC &&
                currentActiveInputDevice_.deviceType_ == DEVICE_TYPE_MIC) {
                audioPolicyManager_.SetDeviceActive(ioHandle, DEVICE_TYPE_MIC, moduleInfo.name, true);
            }

            currentRate = targetRate;
            currentSourceType = targetSourceType;
        }
        sessionWithNormalSourceType_[sessionID] = sessionInfo;
    } else {
        sessionWithSpecialSourceType_[sessionID] = sessionInfo;
    }
    AUDIO_INFO_LOG("sessionID: %{public}" PRIu64 " OnCapturerSessionAdded end", sessionID);
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyService::DeviceFilterByUsage(AudioDeviceUsage usage,
    const std::vector<sptr<AudioDeviceDescriptor>>& descs)
{
    std::vector<unique_ptr<AudioDeviceDescriptor>> audioDeviceDescriptors;

    unordered_map<AudioDevicePrivacyType, list<DevicePrivacyInfo>> devicePrivacyMaps =
        audioDeviceManager_.GetDevicePrivacyMaps();
    for (const auto &dev : descs) {
        for (const auto &devicePrivacy : devicePrivacyMaps) {
            list<DevicePrivacyInfo> deviceInfos = devicePrivacy.second;
            audioDeviceManager_.GetAvailableDevicesWithUsage(usage, deviceInfos, dev, audioDeviceDescriptors);
        }
    }
    std::vector<sptr<AudioDeviceDescriptor>> deviceDescriptors;
    for (const auto &dec : audioDeviceDescriptors) {
        sptr<AudioDeviceDescriptor> tempDec = new(std::nothrow) AudioDeviceDescriptor(*dec);
        deviceDescriptors.push_back(move(tempDec));
    }
    return deviceDescriptors;
}

void AudioPolicyService::TriggerAvailableDeviceChangedCallback(
    const vector<sptr<AudioDeviceDescriptor>> &desc, bool isConnected)
{
    Trace trace("AudioPolicyService::TriggerAvailableDeviceChangedCallback");

    WriteDeviceChangedSysEvents(desc, isConnected);

    if (audioPolicyServerHandler_ != nullptr) {
        audioPolicyServerHandler_->SendAvailableDeviceChange(desc, isConnected);
    }
}

std::vector<unique_ptr<AudioDeviceDescriptor>> AudioPolicyService::GetAvailableDevices(AudioDeviceUsage usage)
{
    std::vector<unique_ptr<AudioDeviceDescriptor>> audioDeviceDescriptors;

    audioDeviceDescriptors = audioDeviceManager_.GetAvailableDevicesByUsage(usage);
    return audioDeviceDescriptors;
}

int32_t AudioPolicyService::OffloadStartPlaying(const std::vector<int32_t> &sessionsId,
    const std::vector<int32_t> &streamTypes, bool isNewDeviceActive)
{
#ifdef BLUETOOTH_ENABLE
    if (isNewDeviceActive) {
        GetA2dpOffloadCodecAndSendToDsp();
        return Bluetooth::AudioA2dpManager::OffloadStartPlaying(sessionsId);
    }

    vector<int32_t> allRunningSessions;
    vector<Bluetooth::A2dpStreamInfo> allSessionInfos;
    Bluetooth::A2dpStreamInfo a2dpStreamInfo;
    vector<unique_ptr<AudioRendererChangeInfo>> audioRendererChangeInfos;
    streamCollector_.GetCurrentRendererChangeInfos(audioRendererChangeInfos);
    for (auto &changeInfo : audioRendererChangeInfos) {
        if (changeInfo->rendererState != RENDERER_RUNNING) {
            Bluetooth::AudioA2dpManager::OffloadStopPlaying(std::vector<int32_t>(1, changeInfo->sessionId));
            continue;
        }
        allRunningSessions.push_back(changeInfo->sessionId);
        a2dpStreamInfo.sessionId = changeInfo->sessionId;
        a2dpStreamInfo.streamType = GetStreamType(changeInfo->sessionId);
        StreamUsage tempStreamUsage = changeInfo->rendererInfo.streamUsage;
        AudioSpatializationState spatialState =
                AudioSpatializationService::GetAudioSpatializationService().GetSpatializationState(tempStreamUsage);
        a2dpStreamInfo.isSpatialAudio = spatialState.spatializationEnabled;
        allSessionInfos.push_back(a2dpStreamInfo);
    }

    for (auto &sessionId : sessionsId) {
        auto iter = find(allRunningSessions.begin(), allRunningSessions.end(), sessionId);
        if (iter == allRunningSessions.end()) {
            a2dpStreamInfo = {sessionId, GetStreamType(sessionId), 0, 0};
            allSessionInfos.push_back(a2dpStreamInfo);
        }
    }
    UpdateA2dpOffloadFlag(allSessionInfos);
    if (a2dpOffloadFlag_ == A2DP_OFFLOAD) {
        GetA2dpOffloadCodecAndSendToDsp();
        return Bluetooth::AudioA2dpManager::OffloadStartPlaying(sessionsId);
    }
#endif
    return SUCCESS;
}

int32_t AudioPolicyService::OffloadStopPlaying(const std::vector<int32_t> &sessionsId)
{
#ifdef BLUETOOTH_ENABLE
    if (a2dpOffloadFlag_ != A2DP_OFFLOAD) {
        return ERROR;
    }
    return Bluetooth::AudioA2dpManager::OffloadStopPlaying(sessionsId);
#else
    return ERROR;
#endif
}

void AudioPolicyService::GetA2dpOffloadCodecAndSendToDsp()
{
#ifdef BLUETOOTH_ENABLE
    if (currentActiveDevice_.deviceType_ != DEVICE_TYPE_BLUETOOTH_A2DP) {
        return;
    }
    Bluetooth::BluetoothRemoteDevice bluetoothRemoteDevice_ = Bluetooth::AudioA2dpManager::GetCurrentActiveA2dpDevice();
    Bluetooth::A2dpOffloadCodecStatus offloadCodeStatus = Bluetooth::A2dpSource::GetProfile()->
        GetOffloadCodecStatus(bluetoothRemoteDevice_);
    std::string key = "AUDIO_EXT_PARAM_KEY_A2DP_OFFLOAD_CONFIG";
    std::string value = std::to_string(offloadCodeStatus.offloadInfo.mediaPacketHeader) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.mPt) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.ssrc) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.boundaryFlag) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.broadcastFlag) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.codecType) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.maxLatency) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.scmsTEnable) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.sampleRate) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.encodedAudioBitrate) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.bitsPerSample) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.chMode) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.aclHdl) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.l2cRcid) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.mtu) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.codecSpecific0) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.codecSpecific1) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.codecSpecific2) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.codecSpecific3) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.codecSpecific4) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.codecSpecific5) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.codecSpecific6) + ","
        + std::to_string(offloadCodeStatus.offloadInfo.codecSpecific7) + ";";
    const sptr<IStandardAudioService> gsp = GetAudioServerProxy();
    gsp->SetAudioParameter(key, value);
    AUDIO_INFO_LOG("update offloadcodec[%{public}s]", value.c_str());
#endif
}

#ifdef BLUETOOTH_ENABLE
void AudioPolicyService::UpdateA2dpOffloadFlag(const std::vector<Bluetooth::A2dpStreamInfo> &allActiveSessions,
    DeviceType deviceType)
{
    auto receiveOffloadFlag = a2dpOffloadFlag_;
    if ((deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) || (currentActiveDevice_.deviceType_ == DEVICE_TYPE_BLUETOOTH_A2DP &&
        currentActiveDevice_.networkId_ == LOCAL_NETWORK_ID && deviceType == DEVICE_TYPE_NONE)) {
        receiveOffloadFlag = static_cast<BluetoothOffloadState>(Bluetooth::AudioA2dpManager::A2dpOffloadSessionRequest(
            allActiveSessions));
    } else {
        receiveOffloadFlag = NO_A2DP_DEVICE;
    }
    if (receiveOffloadFlag != a2dpOffloadFlag_) {
        if (((preA2dpOffloadFlag_ == A2DP_NOT_OFFLOAD) || (preA2dpOffloadFlag_ == NO_A2DP_DEVICE))
            && (a2dpOffloadFlag_ == A2DP_OFFLOAD)) {
            HandleA2dpDeviceInOffload();
        }
        if ((preA2dpOffloadFlag_ == A2DP_OFFLOAD) && (a2dpOffloadFlag_ == A2DP_NOT_OFFLOAD)) {
            HandleA2dpDeviceOutOffload();
        }
        a2dpOffloadFlag_ = receiveOffloadFlag;
    }
    if (a2dpOffloadFlag_ != preA2dpOffloadFlag_) {
        sameDeviceSwitchFlag_ = true;
    }
    if ((a2dpOffloadFlag_ != A2DP_OFFLOAD) && (a2dpOffloadFlag_ != NO_A2DP_DEVICE) &&
        (a2dpOffloadFlag_ != A2DP_NOT_OFFLOAD)) {
        AUDIO_ERR_LOG("A2dpOffloadSessionRequest failed");
    }
    AUDIO_INFO_LOG("a2dpOffloadFlag_ change from %d to %d", preA2dpOffloadFlag_, a2dpOffloadFlag_);
    if (((preA2dpOffloadFlag_ == A2DP_NOT_OFFLOAD) || (preA2dpOffloadFlag_ == NO_A2DP_DEVICE))
        && (a2dpOffloadFlag_ == A2DP_OFFLOAD)) {
        HandleA2dpDeviceInOffload();
        return;
    }
}
#endif

int32_t AudioPolicyService::HandleA2dpDeviceOutOffload()
{
#ifdef BLUETOOTH_ENABLE
    if (a2dpOffloadFlag_ == A2DP_OFFLOAD) {
        return ERROR;
    }
    lock_guard<mutex> lock(switchA2dpOffloadMutex_);
    if (preA2dpOffloadFlag_ == a2dpOffloadFlag_) {
        return SUCCESS;
    }
    AUDIO_INFO_LOG("Handle A2dpDevice Out Offload");
    ResetOffloadMode();
    GetA2dpOffloadCodecAndSendToDsp();
    std::vector<int32_t> allSessions;
    GetAllRunningStreamSessionAndType(allSessions);
    OffloadStopPlaying(allSessions);

    vector<unique_ptr<AudioRendererChangeInfo>> rendererChangeInfos;
    streamCollector_.GetCurrentRendererChangeInfos(rendererChangeInfos);
    FetchOutputDevice(rendererChangeInfos, false);

    int32_t ret = HandleA2dpOffloadDeviceSuspend(DEVICE_TYPE_BLUETOOTH_A2DP);
    preA2dpOffloadFlag_ = a2dpOffloadFlag_;
    return ret;
#else
    return ERROR;
#endif
}

int32_t AudioPolicyService::HandleA2dpDeviceInOffload()
{
#ifdef BLUETOOTH_ENABLE
    if (a2dpOffloadFlag_ != A2DP_OFFLOAD) {
        return ERROR;
    }
    lock_guard<mutex> lock(switchA2dpOffloadMutex_);
    if (preA2dpOffloadFlag_ == a2dpOffloadFlag_) {
        return SUCCESS;
    }
    AUDIO_INFO_LOG("Handle A2dpDevice In Offload");
    ResetOffloadMode();
    GetA2dpOffloadCodecAndSendToDsp();
    std::vector<int32_t> allSessions;
    std::vector<int32_t> streamTypes;
    GetAllRunningStreamSessionAndType(allSessions, streamTypes);
    bool isNewdeviceActive = true;

    vector<unique_ptr<AudioRendererChangeInfo>> rendererChangeInfos;
    streamCollector_.GetCurrentRendererChangeInfos(rendererChangeInfos);
    FetchOutputDevice(rendererChangeInfos, false);

    std::string activePort = BLUETOOTH_SPEAKER;
    audioPolicyManager_.SuspendAudioDevice(activePort, true);
    preA2dpOffloadFlag_ = a2dpOffloadFlag_;
    return OffloadStartPlaying(allSessions, streamTypes, isNewdeviceActive);
#else
    return ERROR;
#endif
}

void AudioPolicyService::GetAllRunningStreamSessionAndType(std::vector<int32_t> &allSessions,
    std::vector<int32_t> &streamTypes, bool doStop)
{
#ifdef BLUETOOTH_ENABLE
    vector<unique_ptr<AudioRendererChangeInfo>> rendererChangeInfos;
    streamCollector_.GetCurrentRendererChangeInfos(rendererChangeInfos);
    for (auto &changeInfo : rendererChangeInfos) {
        if (changeInfo->rendererState != RENDERER_RUNNING) {
            if (doStop) {
                Bluetooth::AudioA2dpManager::OffloadStopPlaying(std::vector<int32_t>(1, changeInfo->sessionId));
            }
            Bluetooth::AudioA2dpManager::OffloadStartPlaying(std::vector<int32_t>(1, changeInfo->sessionId));
            continue;
        }
        allSessions.push_back(changeInfo->sessionId);
        streamTypes.push_back(GetStreamType(changeInfo->sessionId));
    }
#endif
}

void AudioPolicyService::GetAllRunningStreamSessionAndType(std::vector<int32_t> &allSessions, bool doStop)
{
#ifdef BLUETOOTH_ENABLE
    vector<unique_ptr<AudioRendererChangeInfo>> rendererChangeInfos;
    streamCollector_.GetCurrentRendererChangeInfos(rendererChangeInfos);
    for (auto &changeInfo : rendererChangeInfos) {
        if (changeInfo->rendererState != RENDERER_RUNNING) {
            if (doStop) {
                Bluetooth::AudioA2dpManager::OffloadStopPlaying(std::vector<int32_t>(1, changeInfo->sessionId));
            }
            Bluetooth::AudioA2dpManager::OffloadStartPlaying(std::vector<int32_t>(1, changeInfo->sessionId));
            continue;
        }
        allSessions.push_back(changeInfo->sessionId);
    }
#endif
}

void AudioPolicyService::OnScoStateChanged(const std::string &macAddress, bool isConnnected)
{
    AUDIO_INFO_LOG("[OnScoStateChanged] macAddress: %{public}s, isConnnected: %{public}d", macAddress.c_str(),
        isConnnected);
    audioDeviceManager_.UpdateScoState(macAddress, isConnnected);
    FetchDevice(true);
    FetchDevice(false);
}

void AudioPolicyService::OnDeviceInfoUpdated(AudioDeviceDescriptor &desc, const DeviceInfoUpdateCommand updateCommand)
{
    AUDIO_INFO_LOG("[OnDeviceInfoUpdated]  updateCommand: %{public}d", updateCommand);
    if (updateCommand == ENABLE_UPDATE && desc.isEnable_ == true) {
        unique_ptr<AudioDeviceDescriptor> userSelectMediaDevice =
            AudioStateManager::GetAudioStateManager().GetPerferredMediaRenderDevice();
        unique_ptr<AudioDeviceDescriptor> userSelectCallDevice =
            AudioStateManager::GetAudioStateManager().GetPerferredCallRenderDevice();
        if ((userSelectMediaDevice->deviceType_ == desc.deviceType_ &&
            userSelectMediaDevice->macAddress_ == desc.macAddress_) ||
            (userSelectCallDevice->deviceType_ == desc.deviceType_ &&
            userSelectCallDevice->macAddress_ == desc.macAddress_)) {
            AUDIO_INFO_LOG("Current enable state has been set true during user selection, no need to be set again.");
            return;
        }
    }
    sptr<AudioDeviceDescriptor> audioDescriptor = new(std::nothrow) AudioDeviceDescriptor(desc);
    audioDeviceManager_.UpdateDevicesListInfo(audioDescriptor, updateCommand);
    FetchDevice(true);
    FetchDevice(false);
}
} // namespace AudioStandard
} // namespace OHOS
