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
#include "audio_focus_parser.h"
#include "audio_manager_base.h"
#include "audio_manager_proxy.h"
#include "iservice_registry.h"
#include "media_log.h"
#include "system_ability_definition.h"

#include "audio_policy_service.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;
static sptr<IStandardAudioService> g_sProxy = nullptr;

AudioPolicyService::~AudioPolicyService()
{
    MEDIA_ERR_LOG("~AudioPolicyService()");
    Deinit();
}

bool AudioPolicyService::Init(void)
{
    serviceFlag_.reset();
    mAudioPolicyManager.Init();
    if (!mConfigParser.LoadConfiguration()) {
        MEDIA_ERR_LOG("Audio Config Load Configuration failed");
        return false;
    }
    if (!mConfigParser.Parse()) {
        MEDIA_ERR_LOG("Audio Config Parse failed");
        return false;
    }

    std::unique_ptr<AudioFocusParser> audioFocusParser = make_unique<AudioFocusParser>();
    CHECK_AND_RETURN_RET_LOG(audioFocusParser != nullptr, false, "Failed to create AudioFocusParser");
    std::string AUDIO_FOCUS_CONFIG_FILE = "/etc/audio/audio_interrupt_policy_config.xml";

    if (audioFocusParser->LoadConfig(focusTable_[0][0])) {
        MEDIA_ERR_LOG("Audio Interrupt Load Configuration failed");
        return false;
    }

    if (mDeviceStatusListener->RegisterDeviceStatusListener(nullptr)) {
        MEDIA_ERR_LOG("[Policy Service] Register for device status events failed");
        return false;
    }

    return true;
}

void AudioPolicyService::InitKVStore()
{
    mAudioPolicyManager.InitKVStore();
}

bool AudioPolicyService::ConnectServiceAdapter()
{
    if (!mAudioPolicyManager.ConnectServiceAdapter()) {
        MEDIA_ERR_LOG("AudioPolicyService::ConnectServiceAdapter  Error in connecting to audio service adapter");
        return false;
    }

    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (samgr == nullptr) {
        MEDIA_ERR_LOG("[Policy Service] Get samgr failed");
        return false;
    }

    sptr<IRemoteObject> object = samgr->GetSystemAbility(AUDIO_DISTRIBUTED_SERVICE_ID);
    if (object == nullptr) {
        MEDIA_DEBUG_LOG("[Policy Service] audio service remote object is NULL.");
        return false;
    }

    g_sProxy = iface_cast<IStandardAudioService>(object);
    if (g_sProxy == nullptr) {
        MEDIA_DEBUG_LOG("[Policy Service] init g_sProxy is NULL.");
        return false;
    }

    if (serviceFlag_.count() != MIN_SERVICE_COUNT) {
        OnServiceConnected(AudioServiceIndex::AUDIO_SERVICE_INDEX);
    }

    return true;
}

void AudioPolicyService::Deinit(void)
{
    MEDIA_ERR_LOG("Policy service died. closing active ports");
    std::for_each(mIOHandles.begin(), mIOHandles.end(), [&](std::pair<std::string, AudioIOHandle> handle) {
        mAudioPolicyManager.CloseAudioPort(handle.second);
    });

    mIOHandles.clear();
    mDeviceStatusListener->UnRegisterDeviceStatusListener();
    return;
}

int32_t AudioPolicyService::SetAudioSessionCallback(AudioSessionCallback *callback)
{
    return mAudioPolicyManager.SetAudioSessionCallback(callback);
}

int32_t AudioPolicyService::SetStreamVolume(AudioStreamType streamType, float volume) const
{
    return mAudioPolicyManager.SetStreamVolume(streamType, volume);
}

float AudioPolicyService::GetStreamVolume(AudioStreamType streamType) const
{
    return mAudioPolicyManager.GetStreamVolume(streamType);
}

int32_t AudioPolicyService::SetStreamMute(AudioStreamType streamType, bool mute) const
{
    return mAudioPolicyManager.SetStreamMute(streamType, mute);
}

bool AudioPolicyService::GetStreamMute(AudioStreamType streamType) const
{
    return mAudioPolicyManager.GetStreamMute(streamType);
}

bool AudioPolicyService::IsStreamActive(AudioStreamType streamType) const
{
    return mAudioPolicyManager.IsStreamActive(streamType);
}

std::string AudioPolicyService::GetPortName(InternalDeviceType deviceType)
{
    std::string portName = PORT_NONE;
    switch (deviceType) {
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
            portName = PIPE_SINK;
            break;
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
            portName = BLUETOOTH_SPEAKER;
            break;
        case InternalDeviceType::DEVICE_TYPE_SPEAKER:
        case InternalDeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_USB_HEADSET:
            portName = PRIMARY_SPEAKER;
            break;
        case InternalDeviceType::DEVICE_TYPE_MIC:
            portName = PRIMARY_MIC;
            break;
        default:
            portName = PORT_NONE;
            break;
    }
    return portName;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioPolicyService::GetDevices(DeviceFlag deviceFlag)
{
    MEDIA_INFO_LOG("Entered AudioPolicyService::%{public}s", __func__);
    std::vector<sptr<AudioDeviceDescriptor>> deviceList = {};

    if (deviceFlag < DeviceFlag::OUTPUT_DEVICES_FLAG || deviceFlag > DeviceFlag::ALL_DEVICES_FLAG) {
        MEDIA_ERR_LOG("Invalid flag provided %{public}d", deviceFlag);
        return deviceList;
    }

    if (deviceFlag == DeviceFlag::ALL_DEVICES_FLAG) {
        return mConnectedDevices;
    }

    DeviceRole role = DeviceRole::OUTPUT_DEVICE;
    role = (deviceFlag == DeviceFlag::OUTPUT_DEVICES_FLAG) ? DeviceRole::OUTPUT_DEVICE : DeviceRole::INPUT_DEVICE;

    MEDIA_INFO_LOG("GetDevices mConnectedDevices size = [%{public}zu]", mConnectedDevices.size());
    for (const auto &device : mConnectedDevices) {
        if (device != nullptr && device->deviceRole_ == role) {
            auto devDesc = new(std::nothrow) AudioDeviceDescriptor(device->deviceType_, device->deviceRole_);
            deviceList.push_back(devDesc);
        }
    }

    MEDIA_INFO_LOG("GetDevices list size = [%{public}zu]", deviceList.size());
    return deviceList;
}

DeviceType AudioPolicyService::FetchHighPriorityDevice()
{
    MEDIA_INFO_LOG("Entered AudioPolicyService::%{public}s", __func__);
    DeviceType priorityDevice = DEVICE_TYPE_SPEAKER;

    for (const auto &device : priorityList) {
        auto isPresent = [&device] (const sptr<AudioDeviceDescriptor> &desc) {
            CHECK_AND_RETURN_RET_LOG(desc != nullptr, false, "FetchHighPriorityDevice device is nullptr");
            return desc->deviceType_ == device;
        };

        auto itr = std::find_if(mConnectedDevices.begin(), mConnectedDevices.end(), isPresent);
        if (itr != mConnectedDevices.end()) {
            priorityDevice = (*itr)->deviceType_;
            MEDIA_INFO_LOG("%{public}d is high priority device", priorityDevice);
            break;
        }
    }

    return priorityDevice;
}

void UpdateActiveDeviceRoute(InternalDeviceType deviceType)
{
    auto ret = SUCCESS;

    switch (deviceType) {
        case DEVICE_TYPE_USB_HEADSET:
        case DEVICE_TYPE_WIRED_HEADSET: {
            ret = g_sProxy->UpdateActiveDeviceRoute(deviceType, DeviceFlag::ALL_DEVICES_FLAG);
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "Failed to update the route for %{public}d", deviceType);
            break;
        }
        case DEVICE_TYPE_SPEAKER: {
            ret = g_sProxy->UpdateActiveDeviceRoute(deviceType, DeviceFlag::OUTPUT_DEVICES_FLAG);
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "Failed to update the route for %{public}d", deviceType);
            break;
        }
        case DEVICE_TYPE_MIC: {
            ret = g_sProxy->UpdateActiveDeviceRoute(deviceType, DeviceFlag::INPUT_DEVICES_FLAG);
            CHECK_AND_RETURN_LOG(ret == SUCCESS, "Failed to update the route for %{public}d", deviceType);
            break;
        }
        default:
            break;
    }
}

int32_t AudioPolicyService::ActivateNewDevice(DeviceType deviceType)
{
    int32_t result = SUCCESS;

    if (mCurrentActiveDevice == deviceType) {
        return result;
    }

    if (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        auto primaryModulesPos = deviceClassInfo_.find(ClassType::TYPE_A2DP);
        if (primaryModulesPos != deviceClassInfo_.end()) {
            auto moduleInfoList = primaryModulesPos->second;
            for (auto &moduleInfo : moduleInfoList) {
                if (mIOHandles.find(moduleInfo.name) == mIOHandles.end()) {
                    MEDIA_INFO_LOG("Load a2dp module [%{public}s]", moduleInfo.name.c_str());
                    AudioIOHandle ioHandle = mAudioPolicyManager.OpenAudioPort(moduleInfo);
                    CHECK_AND_RETURN_RET_LOG(ioHandle != ERR_OPERATION_FAILED && ioHandle != ERR_INVALID_HANDLE,
                                             ERR_OPERATION_FAILED, "OpenAudioPort failed %{public}d", ioHandle);
                    mIOHandles[moduleInfo.name] = ioHandle;
                }

                std::string activePort = GetPortName(mCurrentActiveDevice);
                MEDIA_INFO_LOG("port %{public}s, active device %{public}d", activePort.c_str(), mCurrentActiveDevice);
                mAudioPolicyManager.SuspendAudioDevice(activePort, true);
            }
        }
    } else if (mCurrentActiveDevice == DEVICE_TYPE_BLUETOOTH_A2DP) {
        std::string activePort = GetPortName(mCurrentActiveDevice);
        MEDIA_INFO_LOG("suspend a2dp first %{public}s", activePort.c_str());
        mAudioPolicyManager.SuspendAudioDevice(activePort, true);
    }

    AudioIOHandle ioHandle = GetAudioIOHandle(deviceType);
    std::string portName = GetPortName(deviceType);

    MEDIA_INFO_LOG("port name is %{public}s", portName.c_str());
    if (portName != PORT_NONE) {
        result = mAudioPolicyManager.SetDeviceActive(ioHandle, deviceType, portName, true);
        mAudioPolicyManager.SuspendAudioDevice(portName, false);
    } else {
        result = ERR_DEVICE_NOT_SUPPORTED;
    }

    if (result) {
        MEDIA_ERR_LOG("SetDeviceActive failed %{public}d", result);
        return result;
    }
#ifdef PRODUCT_M40
    UpdateActiveDeviceRoute(deviceType);
#endif
    return SUCCESS;
}

int32_t AudioPolicyService::SetDeviceActive(InternalDeviceType deviceType, bool active)
{
    MEDIA_DEBUG_LOG("[Policy Service] Device type[%{public}d] status[%{public}d]", deviceType, active);
    CHECK_AND_RETURN_RET_LOG(deviceType != DEVICE_TYPE_NONE, ERR_DEVICE_NOT_SUPPORTED, "Invalid device");

    int32_t result = SUCCESS;

    if (!active) {
        CHECK_AND_RETURN_RET_LOG(deviceType == mCurrentActiveDevice, SUCCESS, "This device is not active");
        if (mConnectedDevices.size() == mDefaultDeviceCount && deviceType == DEVICE_TYPE_SPEAKER) {
            deviceType = DEVICE_TYPE_BLUETOOTH_SCO;
        } else {
            deviceType = FetchHighPriorityDevice();
        }
    }

    if (deviceType == mCurrentActiveDevice) {
        MEDIA_ERR_LOG("Device already activated %{public}d. No need to activate again", mCurrentActiveDevice);
        return SUCCESS;
    }

    if (deviceType == DEVICE_TYPE_SPEAKER) {
        result = ActivateNewDevice(DEVICE_TYPE_SPEAKER);
        CHECK_AND_RETURN_RET_LOG(result == SUCCESS, ERR_OPERATION_FAILED, "Failed for speaker %{public}d", result);

        result = ActivateNewDevice(DEVICE_TYPE_MIC);
        CHECK_AND_RETURN_RET_LOG(result == SUCCESS, ERR_OPERATION_FAILED, "Failed for MIC %{public}d", result);
    } else {
        result = ActivateNewDevice(deviceType);
        CHECK_AND_RETURN_RET_LOG(result == SUCCESS, ERR_OPERATION_FAILED, "Activate failed %{public}d", result);
    }

    mCurrentActiveDevice = deviceType;
    return result;
}

bool AudioPolicyService::IsDeviceActive(InternalDeviceType deviceType) const
{
    MEDIA_INFO_LOG("Entered AudioPolicyService::%{public}s", __func__);
    return mCurrentActiveDevice == deviceType;
}

int32_t AudioPolicyService::SetRingerMode(AudioRingerMode ringMode)
{
    return mAudioPolicyManager.SetRingerMode(ringMode);
}

AudioRingerMode AudioPolicyService::GetRingerMode() const
{
    return mAudioPolicyManager.GetRingerMode();
}

int32_t AudioPolicyService::SetAudioScene(AudioScene audioScene)
{
    if (g_sProxy == nullptr) {
        MEDIA_DEBUG_LOG("AudioPolicyService::SetAudioScene g_sProxy is nullptr");
        return ERR_OPERATION_FAILED;
    }

    int32_t result = g_sProxy->SetAudioScene(audioScene);
    MEDIA_INFO_LOG("SetAudioScene return value from audio HAL: %{public}d", result);
    // As Audio HAL is stubbed now, we set and return
    mAudioScene = audioScene;
    MEDIA_INFO_LOG("AudioScene is set as: %{public}d", audioScene);

    return SUCCESS;
}

AudioScene AudioPolicyService::GetAudioScene() const
{
    MEDIA_INFO_LOG("GetAudioScene return value: %{public}d", mAudioScene);
    return mAudioScene;
}

bool AudioPolicyService::IsAudioInterruptEnabled() const
{
    return interruptEnabled_;
}

void AudioPolicyService::OnAudioInterruptEnable(bool enable)
{
    interruptEnabled_ = enable;
}

void AudioPolicyService::UpdateConnectedDevices(DeviceType devType, vector<sptr<AudioDeviceDescriptor>> &desc,
                                                bool isConnected)
{
    sptr<AudioDeviceDescriptor> audioDescriptor = nullptr;

    if (std::find(ioDeviceList.begin(), ioDeviceList.end(), devType) != ioDeviceList.end()) {
        MEDIA_INFO_LOG("Filling io device list for %{public}d", devType);
        audioDescriptor = new(std::nothrow) AudioDeviceDescriptor(devType, INPUT_DEVICE);
        desc.push_back(audioDescriptor);
        if (isConnected) {
            mConnectedDevices.insert(mConnectedDevices.begin(), audioDescriptor);
        }

        audioDescriptor = new(std::nothrow) AudioDeviceDescriptor(devType, OUTPUT_DEVICE);
        desc.push_back(audioDescriptor);
        if (isConnected) {
            mConnectedDevices.insert(mConnectedDevices.begin(), audioDescriptor);
        }
    } else {
        MEDIA_INFO_LOG("Filling non-io device list for %{public}d", devType);
        audioDescriptor = new(std::nothrow) AudioDeviceDescriptor(devType, GetDeviceRole(devType));
        desc.push_back(audioDescriptor);
        if (isConnected) {
            mConnectedDevices.insert(mConnectedDevices.begin(), audioDescriptor);
        }
    }
}

void AudioPolicyService::OnDeviceStatusUpdated(DeviceType devType, bool isConnected, void *privData)
{
    MEDIA_INFO_LOG("=== DEVICE STATUS CHANGED | TYPE[%{public}d] STATUS[%{public}d] ===", devType, isConnected);
    int32_t result = ERROR;

    // fill device change action for callback
    std::vector<sptr<AudioDeviceDescriptor>> deviceChangeDescriptor = {};

    auto isPresent = [&devType] (const sptr<AudioDeviceDescriptor> &descriptor) {
        return descriptor->deviceType_ == devType;
    };

    // If device already in list, remove it else do not modify the list
    mConnectedDevices.erase(std::remove_if(mConnectedDevices.begin(), mConnectedDevices.end(), isPresent),
                            mConnectedDevices.end());

    // new device found. If connected, add into active device list
    if (isConnected) {
        MEDIA_INFO_LOG("=== DEVICE CONNECTED === TYPE[%{public}d]", devType);
        result = ActivateNewDevice(devType);
        CHECK_AND_RETURN_LOG(result == SUCCESS, "Failed to activate new device %{public}d", devType);
        mCurrentActiveDevice = devType;
        UpdateConnectedDevices(devType, deviceChangeDescriptor, isConnected);
    } else {
        MEDIA_INFO_LOG("=== DEVICE DISCONNECTED === TYPE[%{public}d]", devType);
        UpdateConnectedDevices(devType, deviceChangeDescriptor, isConnected);

        auto priorityDev = FetchHighPriorityDevice();
        MEDIA_INFO_LOG("Priority device is [%{public}d]", priorityDev);

        if (priorityDev == DEVICE_TYPE_SPEAKER) {
            result = ActivateNewDevice(DEVICE_TYPE_SPEAKER);
            CHECK_AND_RETURN_LOG(result == SUCCESS, "Failed to activate new device [%{public}d]", result);

            result = ActivateNewDevice(DEVICE_TYPE_MIC);
            CHECK_AND_RETURN_LOG(result == SUCCESS, "Failed to activate new device [%{public}d]", result);
        } else {
            result = ActivateNewDevice(priorityDev);
            CHECK_AND_RETURN_LOG(result == SUCCESS, "Failed to activate new device [%{public}d]", result);
        }

        if (devType == DEVICE_TYPE_BLUETOOTH_A2DP) {
            if (mIOHandles.find(BLUETOOTH_SPEAKER) != mIOHandles.end()) {
                MEDIA_INFO_LOG("Closing a2dp device");
                mAudioPolicyManager.CloseAudioPort(mIOHandles[BLUETOOTH_SPEAKER]);
                mIOHandles.erase(BLUETOOTH_SPEAKER);
            }
        }

        mCurrentActiveDevice = priorityDev;
    }

    TriggerDeviceChangedCallback(deviceChangeDescriptor, isConnected);
    MEDIA_INFO_LOG("output device list = [%{public}zu]", mConnectedDevices.size());
}

void AudioPolicyService::OnServiceConnected(AudioServiceIndex serviceIndex)
{
    MEDIA_INFO_LOG("[module_load]::OnServiceConnected for [%{public}d]", serviceIndex);
    CHECK_AND_RETURN_LOG(serviceIndex >= HDI_SERVICE_INDEX && serviceIndex <= AUDIO_SERVICE_INDEX, "invalid index");

    // If audio service or hdi service is not ready, donot load default modules
    serviceFlag_.set(serviceIndex, true);
    if (serviceFlag_.count() != MIN_SERVICE_COUNT) {
        MEDIA_INFO_LOG("[module_load]::hdi service or audio service not up. Cannot load default module now");
        return;
    }

    int32_t result = ERROR;
    MEDIA_INFO_LOG("[module_load]::HDI and AUDIO SERVICE is READY. Loading default modules");
    auto primaryModulesPos = deviceClassInfo_.find(ClassType::TYPE_PRIMARY);
    if (primaryModulesPos != deviceClassInfo_.end()) {
        auto moduleInfoList = primaryModulesPos->second;
        for (auto &moduleInfo : moduleInfoList) {
            MEDIA_INFO_LOG("[module_load]::Load module[%{public}s]", moduleInfo.name.c_str());
            AudioIOHandle ioHandle = mAudioPolicyManager.OpenAudioPort(moduleInfo);
            if (ioHandle == ERR_OPERATION_FAILED || ioHandle == ERR_INVALID_HANDLE) {
                MEDIA_INFO_LOG("[module_load]::Open port failed");
                continue;
            }

            auto devType = GetDeviceType(moduleInfo.name);
            if (devType == DeviceType::DEVICE_TYPE_SPEAKER || devType == DeviceType::DEVICE_TYPE_MIC) {
                result = mAudioPolicyManager.SetDeviceActive(ioHandle, devType, moduleInfo.name, true);
                if (result != SUCCESS) {
                    MEDIA_ERR_LOG("[module_load]::Device failed %{public}d", devType);
                    break;
                }
            }

            // add new device into active device list
            sptr<AudioDeviceDescriptor> audioDescriptor = new(std::nothrow) AudioDeviceDescriptor(devType,
                GetDeviceRole(moduleInfo.role));
            mConnectedDevices.insert(mConnectedDevices.begin(), audioDescriptor);
            mDefaultDeviceCount++;
            mIOHandles[moduleInfo.name] = ioHandle;
        }
    }

    if (result == SUCCESS) {
        MEDIA_INFO_LOG("[module_load]::Setting speaker as active device on bootup");
        mCurrentActiveDevice = DEVICE_TYPE_SPEAKER;
    }
}

// Parser callbacks
void AudioPolicyService::OnXmlParsingCompleted(const std::unordered_map<ClassType, std::list<AudioModuleInfo>> &xmlData)
{
    MEDIA_INFO_LOG("AudioPolicyService::%{public}s, device class num [%{public}zu]", __func__, xmlData.size());
    if (xmlData.empty()) {
        MEDIA_ERR_LOG("failed to parse xml file. Received data is empty");
        return;
    }

    deviceClassInfo_ = xmlData;
}

int32_t AudioPolicyService::SetDeviceChangeCallback(const sptr<IRemoteObject> &object)
{
    MEDIA_INFO_LOG("Entered AudioPolicyService::%{public}s", __func__);

    auto callback = iface_cast<IStandardAudioPolicyManagerListener>(object);
    if (callback != nullptr) {
        deviceChangeCallbackList_.push_back(callback);
    }

    return SUCCESS;
}

// private methods
AudioIOHandle AudioPolicyService::GetAudioIOHandle(InternalDeviceType deviceType)
{
    AudioIOHandle ioHandle;
    switch (deviceType) {
        case InternalDeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_USB_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_SPEAKER:
            ioHandle = mIOHandles[PRIMARY_SPEAKER];
            break;
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
            ioHandle = mIOHandles[PIPE_SINK];
            break;
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
            ioHandle = mIOHandles[BLUETOOTH_SPEAKER];
            break;
        case InternalDeviceType::DEVICE_TYPE_MIC:
            ioHandle = mIOHandles[PRIMARY_MIC];
            break;
        default:
            ioHandle = mIOHandles[PRIMARY_MIC];
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
    } else if (deviceName == "fifo_output" || deviceName == "fifo_input") {
        devType = DEVICE_TYPE_BLUETOOTH_SCO;
    }

    return devType;
}

void AudioPolicyService::TriggerDeviceChangedCallback(const vector<sptr<AudioDeviceDescriptor>> &desc, bool isConnected)
{
    DeviceChangeAction deviceChangeAction;
    deviceChangeAction.deviceDescriptors = desc;
    deviceChangeAction.type = isConnected ? DeviceChangeType::CONNECT : DeviceChangeType::DISCONNECT;

    for (auto const &deviceChangedCallback : deviceChangeCallbackList_) {
        if (deviceChangedCallback) {
            deviceChangedCallback->OnDeviceChange(deviceChangeAction);
        }
    }
}

DeviceRole AudioPolicyService::GetDeviceRole(DeviceType deviceType) const
{
    switch (deviceType) {
        case DeviceType::DEVICE_TYPE_SPEAKER:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_SCO:
        case DeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
        case DeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case DeviceType::DEVICE_TYPE_USB_HEADSET:
            return DeviceRole::OUTPUT_DEVICE;
        case DeviceType::DEVICE_TYPE_MIC:
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
}
}
