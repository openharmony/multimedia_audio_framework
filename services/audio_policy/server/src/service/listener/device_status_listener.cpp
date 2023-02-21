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

#include <hdf_io_service_if.h>
#include <securec.h>

#include "audio_errors.h"
#include "audio_events.h"
#include "audio_log.h"

#ifdef BLUETOOTH_ENABLE
#include "audio_bluetooth_manager.h"
#endif

#include "device_status_listener.h"

namespace OHOS {
namespace AudioStandard {
const std::string AUDIO_HDI_SERVICE_NAME = "audio_hdi_service";
const std::string AUDIO_HDI_PNP_SERVICE_NAME = "audio_hdi_pnp_service";
const std::string AUDIO_BLUETOOTH_HDI_SERVICE_NAME = "audio_bluetooth_hdi_service";

const std::string DAUDIO_HDI_SERVICE_NAME = "daudio_primary_service";
const uint8_t EVENT_PARAMS = 2;
const uint8_t D_EVENT_PARAMS = 5;

static DeviceType GetInternalDeviceType(AudioDeviceType hdiDeviceType)
{
    DeviceType internalDeviceType = DEVICE_TYPE_NONE;

    switch (hdiDeviceType) {
        case AudioDeviceType::HDF_AUDIO_HEADSET:
            internalDeviceType = DEVICE_TYPE_WIRED_HEADSET;
            break;
        case AudioDeviceType::HDF_AUDIO_HEADPHONE:
            internalDeviceType = DEVICE_TYPE_WIRED_HEADPHONES;
            break;
        case AudioDeviceType::HDF_AUDIO_USB_HEADSET:
            internalDeviceType = DEVICE_TYPE_USB_HEADSET;
            break;
        default:
            internalDeviceType = DEVICE_TYPE_NONE;
            break;
    }

    return internalDeviceType;
}

static void ReceviceDistributedInfo(struct ServiceStatus* serviceStatus, std::string & info,
    DeviceStatusListener * devListener)
{
    if (serviceStatus->status == SERVIE_STATUS_START) {
        AUDIO_DEBUG_LOG("distributed service online");
    } else if (serviceStatus->status == SERVIE_STATUS_CHANGE && !info.empty()) {
        DStatusInfo statusInfo;
        statusInfo.connectType = ConnectType::CONNECT_TYPE_DISTRIBUTED;
        AudioEventType hdiEventType = HDF_AUDIO_EVENT_UNKOWN;
        if (sscanf_s(info.c_str(), "EVENT_TYPE=%d;NID=%[^;];PIN=%d;VID=%d;IID=%d", &hdiEventType,
            statusInfo.networkId, sizeof(statusInfo.networkId), &(statusInfo.hdiPin), &(statusInfo.mappingVolumeId),
            &(statusInfo.mappingInterruptId)) < D_EVENT_PARAMS) {
            AUDIO_ERR_LOG("[DeviceStatusListener]: Failed to scan info string");
            return;
        }

        statusInfo.isConnected = (hdiEventType == HDF_AUDIO_DEVICE_ADD) ? true : false;
        devListener->deviceObserver_.OnDeviceStatusUpdated(statusInfo);
    }
}

static void OnDeviceStatusChange(const std::string &info, DeviceStatusListener *devListener)
{
    CHECK_AND_RETURN_LOG(!info.empty(), "OnDeviceStatusChange invalid info");
    AudioDeviceType hdiDeviceType = HDF_AUDIO_DEVICE_UNKOWN;
    AudioEventType hdiEventType = HDF_AUDIO_EVENT_UNKOWN;
    if (sscanf_s(info.c_str(), "EVENT_TYPE=%d;DEVICE_TYPE=%d", &hdiEventType, &hdiDeviceType) < EVENT_PARAMS) {
        AUDIO_WARNING_LOG("[DeviceStatusListener]: Failed to scan info string %{public}s", info.c_str());
        return;
    }

    DeviceType internalDevice = GetInternalDeviceType(hdiDeviceType);
    CHECK_AND_RETURN_LOG(internalDevice != DEVICE_TYPE_NONE, "Unsupported device %{public}d", hdiDeviceType);

    bool isConnected = (hdiEventType == HDF_AUDIO_DEVICE_ADD) ? true : false;
    AudioStreamInfo streamInfo = {};
    devListener->deviceObserver_.OnDeviceStatusUpdated(internalDevice, isConnected, "", "", streamInfo);
}

static void OnServiceStatusReceived(struct ServiceStatusListener *listener, struct ServiceStatus *serviceStatus)
{
    CHECK_AND_RETURN_LOG(serviceStatus != nullptr, "Invalid ServiceStatus");
    std::string info = serviceStatus->info;
    AUDIO_INFO_LOG("OnServiceStatusReceived: [service name:%{public}s] [status:%{public}d] [info:%{public}s]",
        serviceStatus->serviceName, serviceStatus->status, info.c_str());

    DeviceStatusListener *devListener = reinterpret_cast<DeviceStatusListener *>(listener->priv);
    CHECK_AND_RETURN_LOG(devListener != nullptr, "Invalid deviceStatusListener");
    if (serviceStatus->serviceName == AUDIO_HDI_SERVICE_NAME) {
        if (serviceStatus->status == SERVIE_STATUS_START) {
            devListener->deviceObserver_.OnServiceConnected(AudioServiceIndex::HDI_SERVICE_INDEX);
        } else if (serviceStatus->status == SERVIE_STATUS_STOP) {
            devListener->deviceObserver_.OnServiceDisconnected(AudioServiceIndex::HDI_SERVICE_INDEX);
        } else if (serviceStatus->status == SERVIE_STATUS_CHANGE) {
            OnDeviceStatusChange(info, devListener);
        }
    } else if (serviceStatus->serviceName == AUDIO_HDI_PNP_SERVICE_NAME) {
        if (serviceStatus->status == SERVIE_STATUS_CHANGE) {
            OnDeviceStatusChange(info, devListener);
        }
    } else if (serviceStatus->serviceName == AUDIO_BLUETOOTH_HDI_SERVICE_NAME) {
#ifdef BLUETOOTH_ENABLE
        if (serviceStatus->status == SERVIE_STATUS_START) {
            AUDIO_INFO_LOG("Bluetooth hdi service started");
            Bluetooth::AudioA2dpManager::ConnectBluetoothA2dpSink();
        } else if (serviceStatus->status == SERVIE_STATUS_STOP) {
            AUDIO_INFO_LOG("Bluetooth hdi service stopped");
        }
#endif
    } else if (serviceStatus->serviceName == DAUDIO_HDI_SERVICE_NAME) {
        ReceviceDistributedInfo(serviceStatus, info, devListener);
    } else {
        AUDIO_DEBUG_LOG("unkown service name.");
    }
}

DeviceStatusListener::DeviceStatusListener(IDeviceStatusObserver &observer)
    : deviceObserver_(observer), hdiServiceManager_(nullptr), listener_(nullptr) {}

DeviceStatusListener::~DeviceStatusListener() = default;

int32_t DeviceStatusListener::RegisterDeviceStatusListener()
{
    hdiServiceManager_ = HDIServiceManagerGet();
    if (hdiServiceManager_ == nullptr) {
        AUDIO_ERR_LOG("[DeviceStatusListener]: Get HDI service manager failed");
        return ERR_OPERATION_FAILED;
    }

    listener_ = HdiServiceStatusListenerNewInstance();
    listener_->callback = OnServiceStatusReceived;
    listener_->priv = (void *)this;
    int32_t status = hdiServiceManager_->RegisterServiceStatusListener(hdiServiceManager_, listener_,
                                                                       DeviceClass::DEVICE_CLASS_AUDIO);
    if (status != HDF_SUCCESS) {
        AUDIO_ERR_LOG("[DeviceStatusListener]: Register service status listener failed");
        return ERR_OPERATION_FAILED;
    }

    return SUCCESS;
}

int32_t DeviceStatusListener::UnRegisterDeviceStatusListener()
{
    if ((hdiServiceManager_ == nullptr) || (listener_ == nullptr)) {
        return ERR_ILLEGAL_STATE;
    }

    int32_t status = hdiServiceManager_->UnregisterServiceStatusListener(hdiServiceManager_, listener_);
    if (status != HDF_SUCCESS) {
        AUDIO_ERR_LOG("[DeviceStatusListener]: UnRegister service status listener failed");
        return ERR_OPERATION_FAILED;
    }

    hdiServiceManager_ = nullptr;
    listener_ = nullptr;

    return SUCCESS;
}
} // namespace AudioStandard
} // namespace OHOS
