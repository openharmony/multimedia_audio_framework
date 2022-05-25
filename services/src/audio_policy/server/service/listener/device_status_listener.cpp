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

#include "device_status_listener.h"

#include <hdf_io_service_if.h>
#include <securec.h>

#include "audio_bluetooth_manager.h"
#include "audio_errors.h"
#include "audio_events.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
namespace {
#ifdef PRODUCT_M40
    const std::string AUDIO_HDI_SERVICE_NAME = "audio_adapter_service";
#else
    const std::string AUDIO_HDI_SERVICE_NAME = "audio_hdi_service";
#endif
    const std::string AUDIO_BLUETOOTH_HDI_SERVICE_NAME = "audio_bluetooth_hdi_service";
}

const uint8_t EVENT_PARAMS = 2;

static DeviceType GetInternalDeviceType(AudioDeviceType hdiDeviceType)
{
    DeviceType internalDeviceType = DEVICE_TYPE_NONE;

    switch (hdiDeviceType) {
        case AudioDeviceType::HDF_AUDIO_HEADSET:
            internalDeviceType = DEVICE_TYPE_WIRED_HEADSET;
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

static void OnServiceStatusReceived(struct ServiceStatusListener *listener, struct ServiceStatus *serviceStatus)
{
    CHECK_AND_RETURN_LOG(serviceStatus != nullptr, "Invalid ServiceStatus");
    std::string info = serviceStatus->info;
    AUDIO_DEBUG_LOG("OnServiceStatusReceived: [service name:%{public}s] [status:%{public}d] [info:%{public}s]",
                    serviceStatus->serviceName, serviceStatus->status, info.c_str());

    if (serviceStatus->serviceName == AUDIO_HDI_SERVICE_NAME) {
        DeviceStatusListener *devListener = reinterpret_cast<DeviceStatusListener *>(listener->priv);
        CHECK_AND_RETURN_LOG(devListener != nullptr, "Invalid deviceStatusListener");

        if (serviceStatus->status == SERVIE_STATUS_START) {
            devListener->deviceObserver_.OnServiceConnected(AudioServiceIndex::HDI_SERVICE_INDEX);
        } else if (serviceStatus->status == SERVIE_STATUS_CHANGE && !info.empty()) {
            AudioDeviceType hdiDeviceType = HDF_AUDIO_DEVICE_UNKOWN;
            AudioEventType hdiEventType = HDF_AUDIO_EVENT_UNKOWN;
            if (sscanf_s(info.c_str(), "EVENT_TYPE=%d;DEVICE_TYPE=%d", &hdiEventType, &hdiDeviceType) < EVENT_PARAMS) {
                AUDIO_ERR_LOG("[DeviceStatusListener]: Failed to scan info string");
                return;
            }

            DeviceType internalDevice = GetInternalDeviceType(hdiDeviceType);
            bool isConnected = (hdiEventType == HDF_AUDIO_DEVICE_ADD) ? true : false;
            AudioStreamInfo streamInfo = {};
            devListener->deviceObserver_.OnDeviceStatusUpdated(internalDevice, isConnected, devListener->privData_,
                "", streamInfo);
        }
    } else if (serviceStatus->serviceName == AUDIO_BLUETOOTH_HDI_SERVICE_NAME) {
        DeviceStatusListener *devListener = reinterpret_cast<DeviceStatusListener *>(listener->priv);
        CHECK_AND_RETURN_LOG(devListener != nullptr, "Invalid deviceStatusListener");
        if (serviceStatus->status == SERVIE_STATUS_START) {
            AUDIO_INFO_LOG("Bluetooth hdi service started");
            OHOS::Bluetooth::RegisterObserver(devListener->deviceObserver_);
        } else if (serviceStatus->status == SERVIE_STATUS_STOP) {
            AUDIO_INFO_LOG("Bluetooth hdi service stopped");
            OHOS::Bluetooth::DeRegisterObserver();
        }
    }
}

DeviceStatusListener::DeviceStatusListener(IDeviceStatusObserver &observer)
    : deviceObserver_(observer), hdiServiceManager_(nullptr), listener_(nullptr) {}

DeviceStatusListener::~DeviceStatusListener() = default;

int32_t DeviceStatusListener::RegisterDeviceStatusListener(void *privData)
{
    hdiServiceManager_ = HDIServiceManagerGet();
    if (hdiServiceManager_ == nullptr) {
        AUDIO_ERR_LOG("[DeviceStatusListener]: Get HDI service manager failed");
        return ERR_OPERATION_FAILED;
    }

    privData_ = privData;
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
    privData_ = nullptr;

    return SUCCESS;
}
} // namespace AudioStandard
} // namespace OHOS
