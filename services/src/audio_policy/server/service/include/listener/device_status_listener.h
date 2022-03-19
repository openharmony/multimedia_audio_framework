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

#ifndef ST_DEVICE_STATUS_LISTENER_H
#define ST_DEVICE_STATUS_LISTENER_H

#include <servmgr_hdi.h>

#include "audio_module_info.h"
#include "audio_events.h"
#include "idevice_status_observer.h"

namespace OHOS {
namespace AudioStandard {
class DeviceStatusListener {
public:
    DeviceStatusListener(IDeviceStatusObserver &observer);
    ~DeviceStatusListener();

    int32_t RegisterDeviceStatusListener(void *privData);
    int32_t UnRegisterDeviceStatusListener();

    IDeviceStatusObserver &deviceObserver_;
    void *privData_;

private:
    struct HDIServiceManager *hdiServiceManager_;
    struct ServiceStatusListener *listener_;
};
} // namespace AudioStandard
} // namespace OHOS

#endif // ST_DEVICE_STATUS_LISTENER_H
