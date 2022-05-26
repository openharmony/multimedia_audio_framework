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

#include "audio_errors.h"
#include "audio_info.h"
#include "bluetooth_a2dp_src_observer.h"
#include "bluetooth_def.h"
#include "bluetooth_types.h"
#include "bt_def.h"
#include "i_bluetooth_a2dp_src.h"
#include "i_bluetooth_host.h"
#include "iservice_registry.h"
#include "audio_log.h"
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

static bool GetAudioStreamInfo(BluetoothA2dpCodecInfo codecInfo, AudioStreamInfo &audioStreamInfo)
{
    switch (codecInfo.sampleRate) {
        case A2DP_SBC_SAMPLE_RATE_48000_USER:
            audioStreamInfo.samplingRate = SAMPLE_RATE_48000;
            break;
        case A2DP_SBC_SAMPLE_RATE_44100_USER:
            audioStreamInfo.samplingRate = SAMPLE_RATE_44100;
            break;
        case A2DP_SBC_SAMPLE_RATE_32000_USER:
            audioStreamInfo.samplingRate = SAMPLE_RATE_32000;
            break;
        case A2DP_SBC_SAMPLE_RATE_16000_USER:
            audioStreamInfo.samplingRate = SAMPLE_RATE_16000;
            break;
        default:
            return false;
    }

    switch (codecInfo.bitsPerSample) {
        case A2DP_SAMPLE_BITS_16_USER:
            audioStreamInfo.format = SAMPLE_S16LE;
            break;
        case A2DP_SAMPLE_BITS_24_USER:
            audioStreamInfo.format = SAMPLE_S24LE;
            break;
        case A2DP_SAMPLE_BITS_32_USER:
            audioStreamInfo.format = SAMPLE_S32LE;
            break;
        default:
            return false;
    }

    switch (codecInfo.channelMode) {
        case A2DP_SBC_CHANNEL_MODE_STEREO_USER:
            audioStreamInfo.channels = STEREO;
            break;
        case A2DP_SBC_CHANNEL_MODE_MONO_USER:
            audioStreamInfo.channels = MONO;
            break;
        default:
            return false;
    }

    return true;
}

static void AudioOnPlayingStatusChanged(const RawAddress &device, int playingState, int error)
{
    AUDIO_INFO_LOG("AudioOnPlayingStatusChanged");
    g_playState = playingState;
}

static void AudioOnConfigurationChanged(const RawAddress &device, const BluetoothA2dpCodecInfo &info, int error)
{
    AUDIO_INFO_LOG("AudioOnConfigurationChanged: sampleRate: %{public}d, channels: %{public}d, format: %{public}d",
        info.sampleRate, info.channelMode, info.bitsPerSample);
    AudioStreamInfo audioStreamInfo = {};
    if (!GetAudioStreamInfo(info, audioStreamInfo)) {
        AUDIO_ERR_LOG("AudioOnConfigurationChanged: Unsupported a2dp codec info");
        return;
    }

    g_deviceObserver->OnDeviceConfigurationChanged(DEVICE_TYPE_BLUETOOTH_A2DP, device.GetAddress(), audioStreamInfo);
}

static void AudioOnConnectionChanged(const RawAddress &device, int state)
{
    AUDIO_INFO_LOG("AudioOnConnectionChanged: state: %{public}d", state);
    g_device = RawAddress(device);

    if (g_deviceObserver == nullptr) {
        AUDIO_INFO_LOG("observer is null");
        return;
    }

    BluetoothA2dpCodecStatus codecStatus = g_proxy->GetCodecStatus(device);
    AUDIO_DEBUG_LOG("BluetoothA2dpCodecStatus: sampleRate: %{public}d, channels: %{public}d, format: %{public}d",
        codecStatus.codecInfo.sampleRate, codecStatus.codecInfo.channelMode, codecStatus.codecInfo.bitsPerSample);

    AudioStreamInfo audioStreamInfo = {};

    if (state == STATE_TURN_ON) {
        if (!GetAudioStreamInfo(codecStatus.codecInfo, audioStreamInfo)) {
            AUDIO_ERR_LOG("AudioOnConnectionChanged: Unsupported a2dp codec info");
            return;
        }

        g_deviceObserver->OnDeviceStatusUpdated(DEVICE_TYPE_BLUETOOTH_A2DP, true, nullptr, device.GetAddress(),
            audioStreamInfo);
    } else if (state == STATE_TURN_OFF) {
        g_deviceObserver->OnDeviceStatusUpdated(DEVICE_TYPE_BLUETOOTH_A2DP, false, nullptr, device.GetAddress(),
            audioStreamInfo);
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

int32_t GetProxy()
{
    AUDIO_INFO_LOG("GetProxy start");
    sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!samgr) {
        AUDIO_ERR_LOG("GetProxy error: no samgr");
        return ERROR;
    }

    sptr<IRemoteObject> hostRemote = samgr->GetSystemAbility(BLUETOOTH_HOST_SYS_ABILITY_ID);
    if (!hostRemote) {
        AUDIO_ERR_LOG("A2dpSource::impl:GetProxy failed: no hostRemote");
        return ERROR;
    }

    sptr<IBluetoothHost> hostProxy = iface_cast<IBluetoothHost>(hostRemote);
    if (!hostProxy) {
        AUDIO_ERR_LOG("GetProxy error: host no proxy");
        return ERROR;
    }

    sptr<IRemoteObject> remote = hostProxy->GetProfile("A2dpSrcServer");
    if (!remote) {
        AUDIO_ERR_LOG("GetProxy error: no remote");
        return ERROR;
    }

    g_proxy = iface_cast<IBluetoothA2dpSrc>(remote);
    if (!g_proxy) {
        AUDIO_ERR_LOG("GetProxy error: no proxy");
        return ERROR;
    }

    return SUCCESS;
}

int32_t RegisterObserver(IDeviceStatusObserver &observer)
{
    AUDIO_INFO_LOG("RegisterObserver start");
    if (g_proxy == nullptr) {
        if (GetProxy()) {
            AUDIO_ERR_LOG("proxy is null");
            return ERROR;
        }
    }

    g_deviceObserver = &observer;
    g_btA2dpSrcObserverCallbacks = new BluetoothA2dpSrcObserver(&g_hdiCallacks);
    g_proxy->RegisterObserver(g_btA2dpSrcObserverCallbacks);
    return SUCCESS;
}

void DeRegisterObserver()
{
    if ((g_deviceObserver != nullptr) && (g_btA2dpSrcObserverCallbacks != nullptr)) {
        AUDIO_INFO_LOG("DeRegisterObserver start");
        g_deviceObserver = nullptr;
        g_proxy->DeregisterObserver(g_btA2dpSrcObserverCallbacks);
    }
}
}
}
