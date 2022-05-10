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

#include "audio_bluetooth_manager.h"

#include "audio_info.h"
#include "bluetooth_a2dp_src_observer.h"
#include "bluetooth_def.h"
#include "bluetooth_types.h"
#include "bt_def.h"
#include "i_bluetooth_a2dp_src.h"
#include "i_bluetooth_host.h"
#include "iservice_registry.h"
#include "media_log.h"
#include "system_ability_definition.h"

namespace OHOS {
namespace Bluetooth {
using namespace bluetooth;
using namespace AudioStandard;

sptr<IBluetoothA2dpSrc> g_proxy = nullptr;
static sptr<BluetoothA2dpSrcObserver> g_btA2dpSrcObserverCallbacks = nullptr;
int g_playState = false;
RawAddress g_device;
IDeviceStatusObserver *g_deviceObserver = nullptr;

static void AudioOnPlayingStatusChanged(const RawAddress &device, int playingState, int error)
{
    MEDIA_INFO_LOG("AudioOnPlayingStatusChanged");
    g_playState = playingState;
}

static void AudioOnConfigurationChanged(const RawAddress &device, const BluetoothA2dpCodecInfo &info, int error)
{
    MEDIA_INFO_LOG("AudioOnConfigurationChanged");
}

static void AudioOnConnectionChanged(const RawAddress &device, int state)
{
    MEDIA_INFO_LOG("AudioOnConnectionChanged: device: %{public}s, state: %{public}d",
        device.GetAddress().c_str(), state);
    g_device = RawAddress(device);

    if (g_deviceObserver == nullptr) {
        MEDIA_INFO_LOG("observer is null");
        return;
    }

    if (state == STATE_TURN_ON) {
        g_deviceObserver->OnDeviceStatusUpdated(DEVICE_TYPE_BLUETOOTH_A2DP, true, nullptr);
    } else if (state == STATE_TURN_OFF) {
        g_deviceObserver->OnDeviceStatusUpdated(DEVICE_TYPE_BLUETOOTH_A2DP, false, nullptr);
    }
}

static BtA2dpAudioCallback g_hdiCallacks = {
    .OnPlayingStatusChanged = AudioOnPlayingStatusChanged,
    .OnConfigurationChanged = AudioOnConfigurationChanged,
    .OnConnectionStateChanged = AudioOnConnectionChanged,
};

int GetPlayingState()
{
    return g_playState;
}

RawAddress& GetDevice()
{
    return g_device;
}

void GetProxy()
{
    MEDIA_INFO_LOG("GetProxy start");
    sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!samgr) {
        MEDIA_INFO_LOG("GetProxy error: no samgr");
        return;
    }

    sptr<IRemoteObject> hostRemote = samgr->GetSystemAbility(BLUETOOTH_HOST_SYS_ABILITY_ID);
    if (!hostRemote) {
        MEDIA_INFO_LOG("A2dpSource::impl:GetProxy failed: no hostRemote");
        return;
    }

    sptr<IBluetoothHost> hostProxy = iface_cast<IBluetoothHost>(hostRemote);
    if (!hostProxy) {
        MEDIA_INFO_LOG("GetProxy error: host no proxy");
        return;
    }

    sptr<IRemoteObject> remote = hostProxy->GetProfile("A2dpSrcServer");
    if (!remote) {
        MEDIA_INFO_LOG("GetProxy error: no remote");
        return;
    }

    g_proxy = iface_cast<IBluetoothA2dpSrc>(remote);
    if (!g_proxy) {
        MEDIA_INFO_LOG("GetProxy error: no proxy");
        return;
    }
}

void RegisterObserver(IDeviceStatusObserver &observer)
{
    MEDIA_INFO_LOG("RegisterObserver start");
    if (g_proxy == nullptr) {
        MEDIA_ERR_LOG("proxy is null");
        return;
    }

    g_deviceObserver = &observer;
    g_btA2dpSrcObserverCallbacks = new BluetoothA2dpSrcObserver(&g_hdiCallacks);
    g_proxy->RegisterObserver(g_btA2dpSrcObserverCallbacks);
}
}
}
