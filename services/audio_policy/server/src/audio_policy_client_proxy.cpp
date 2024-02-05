/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "audio_policy_client_proxy.h"
#include "audio_log.h"
#include "audio_errors.h"
#include "audio_utils.h"
#include "audio_group_handle.h"

namespace OHOS {
namespace AudioStandard {
AudioPolicyClientProxy::AudioPolicyClientProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<IAudioPolicyClient>(impl)
{}

AudioPolicyClientProxy::~AudioPolicyClientProxy()
{}

void AudioPolicyClientProxy::OnVolumeKeyEvent(VolumeEvent volumeEvent)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }
    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_VOLUME_KEY_EVENT));
    data.WriteInt32(static_cast<int32_t>(volumeEvent.volumeType));
    data.WriteInt32(volumeEvent.volume);
    data.WriteBool(volumeEvent.updateUi);
    data.WriteInt32(volumeEvent.volumeGroupId);
    data.WriteString(volumeEvent.networkId);
    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != 0) {
        AUDIO_ERR_LOG("Error while sending volume key event %{public}d", error);
    }
    reply.ReadInt32();
}

void AudioPolicyClientProxy::OnAudioFocusInfoChange(
    const std::list<std::pair<AudioInterrupt, AudioFocuState>> &focusInfoList)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }
    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_FOCUS_INFO_CHANGED));
    size_t size = focusInfoList.size();
    data.WriteInt32(static_cast<int32_t>(size));
    for (auto iter = focusInfoList.begin(); iter != focusInfoList.end(); ++iter) {
        iter->first.Marshalling(data);
        data.WriteInt32(iter->second);
    }
    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != 0) {
        AUDIO_ERR_LOG("Error while sending focus change info: %{public}d", error);
    }
    reply.ReadInt32();
}

void AudioPolicyClientProxy::OnAudioFocusRequested(const AudioInterrupt &requestFocus)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }

    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_FOCUS_REQUEST_CHANGED));
    requestFocus.Marshalling(data);
    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("OnAudioFocusRequested failed, error: %{public}d", error);
    }
}

void AudioPolicyClientProxy::OnAudioFocusAbandoned(const AudioInterrupt &abandonFocus)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }

    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_FOCUS_ABANDON_CHANGED));
    abandonFocus.Marshalling(data);
    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != ERR_NONE) {
        AUDIO_ERR_LOG("OnAudioFocusAbandoned failed, error: %{public}d", error);
    }
}

void AudioPolicyClientProxy::OnDeviceChange(const DeviceChangeAction &deviceChangeAction)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }

    auto devices = deviceChangeAction.deviceDescriptors;
    size_t size = deviceChangeAction.deviceDescriptors.size();
    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_DEVICE_CHANGE));
    data.WriteInt32(deviceChangeAction.type);
    data.WriteInt32(deviceChangeAction.flag);
    data.WriteInt32(static_cast<int32_t>(size));
    for (size_t i = 0; i < size; i++) {
        devices[i]->Marshalling(data);
    }
    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != 0) {
        AUDIO_ERR_LOG("Error while sending device change info: %{public}d", error);
    }
    reply.ReadInt32();
}

void AudioPolicyClientProxy::OnRingerModeUpdated(const AudioRingerMode &ringerMode)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("AudioPolicyClientProxy::OnRingerModeUpdated: WriteInterfaceToken failed");
        return;
    }

    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_RINGERMODE_UPDATE));
    data.WriteInt32(static_cast<int32_t>(ringerMode));
    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != 0) {
        AUDIO_ERR_LOG("Error while sending ringer mode updated info: %{public}d", error);
    }
    reply.ReadInt32();
}

void AudioPolicyClientProxy::OnMicStateUpdated(const MicStateChangeEvent &micStateChangeEvent)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }

    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_MIC_STATE_UPDATED));
    data.WriteBool(micStateChangeEvent.mute);
    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != 0) {
        AUDIO_ERR_LOG("Error while sending ringer mode updated info: %{public}d", error);
    }
    reply.ReadInt32();
}

void AudioPolicyClientProxy::OnPreferredOutputDeviceUpdated(const std::vector<sptr<AudioDeviceDescriptor>> &desc)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }

    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_ACTIVE_OUTPUT_DEVICE_UPDATED));
    int32_t size = static_cast<int32_t>(desc.size());
    data.WriteInt32(size);
    for (int i = 0; i < size; i++) {
        desc[i]->Marshalling(data);
    }

    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != 0) {
        AUDIO_ERR_LOG("Error while sending preferred output device updated info: %{public}d", error);
    }
    reply.ReadInt32();
}

void AudioPolicyClientProxy::OnPreferredInputDeviceUpdated(const std::vector<sptr<AudioDeviceDescriptor>> &desc)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }

    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_ACTIVE_INPUT_DEVICE_UPDATED));
    int32_t size = static_cast<int32_t>(desc.size());
    data.WriteInt32(size);
    for (int i = 0; i < size; i++) {
        desc[i]->Marshalling(data);
    }

    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != 0) {
        AUDIO_ERR_LOG("Error while sending preferred input device updated info: %{public}d", error);
    }
    reply.ReadInt32();
}

void AudioPolicyClientProxy::OnRendererStateChange(
    std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }

    size_t size = audioRendererChangeInfos.size();
    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_RENDERERSTATE_CHANGE));
    data.WriteInt32(size);
    for (const std::unique_ptr<AudioRendererChangeInfo> &rendererChangeInfo: audioRendererChangeInfos) {
        if (!rendererChangeInfo) {
            AUDIO_ERR_LOG("Renderer change info null, something wrong!!");
            continue;
        }
        rendererChangeInfo->Marshalling(data, hasBTPermission_, hasSystemPermission_, apiVersion_);
    }
    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != 0) {
        AUDIO_ERR_LOG("Error while sending renderer state change info: %{public}d", error);
    }
    reply.ReadInt32();
}

void AudioPolicyClientProxy::UpdateCapturerDeviceInfo(
    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &capturerChangeInfos)
{
    if (!hasBTPermission_) {
        size_t capturerChangeInfoLength = capturerChangeInfos.size();
        for (size_t i = 0; i < capturerChangeInfoLength; i++) {
            if ((capturerChangeInfos[i]->inputDeviceInfo.deviceType == DEVICE_TYPE_BLUETOOTH_A2DP)
                || (capturerChangeInfos[i]->inputDeviceInfo.deviceType == DEVICE_TYPE_BLUETOOTH_SCO)) {
                capturerChangeInfos[i]->inputDeviceInfo.deviceName = "";
                capturerChangeInfos[i]->inputDeviceInfo.macAddress = "";
            }
        }
    }

    if (!hasSystemPermission_) {
        size_t capturerChangeInfoLength = capturerChangeInfos.size();
        for (size_t i = 0; i < capturerChangeInfoLength; i++) {
            capturerChangeInfos[i]->clientUID = 0;
            capturerChangeInfos[i]->capturerState = CAPTURER_INVALID;
            capturerChangeInfos[i]->inputDeviceInfo.networkId = "";
            capturerChangeInfos[i]->inputDeviceInfo.interruptGroupId = GROUP_ID_NONE;
            capturerChangeInfos[i]->inputDeviceInfo.volumeGroupId = GROUP_ID_NONE;
        }
    }
}

void AudioPolicyClientProxy::OnCapturerStateChange(
    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }

    UpdateCapturerDeviceInfo(audioCapturerChangeInfos);
    size_t size = audioCapturerChangeInfos.size();
    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_CAPTURERSTATE_CHANGE));
    data.WriteInt32(size);
    for (const std::unique_ptr<AudioCapturerChangeInfo> &capturerChangeInfo: audioCapturerChangeInfos) {
        if (!capturerChangeInfo) {
            AUDIO_ERR_LOG("Capturer change info null, something wrong!!");
            continue;
        }
        capturerChangeInfo->Marshalling(data, hasBTPermission_, hasSystemPermission_, apiVersion_);
    }

    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != 0) {
        AUDIO_ERR_LOG("Error while sending capturer state change info: %{public}d", error);
    }
    reply.ReadInt32();
}

void AudioPolicyClientProxy::OnRendererDeviceChange(const uint32_t sessionId,
    const DeviceInfo &deviceInfo, const AudioStreamDeviceChangeReason reason)
{
    MessageParcel data;
    MessageParcel reply;
    MessageOption option(MessageOption::TF_ASYNC);
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        AUDIO_ERR_LOG("WriteInterfaceToken failed");
        return;
    }

    data.WriteInt32(static_cast<int32_t>(AudioPolicyClientCode::ON_RENDERER_DEVICE_CHANGE));

    data.WriteUint32(sessionId);
    deviceInfo.Marshalling(data);
    data.WriteInt32(static_cast<int32_t>(reason));
    int error = Remote()->SendRequest(static_cast<uint32_t>(UPDATE_CALLBACK_CLIENT), data, reply, option);
    if (error != 0) {
        AUDIO_ERR_LOG("Error while sending DeviceChange: %{public}d", error);
    }
    reply.ReadInt32();
}
} // namespace AudioStandard
} // namespace OHOS
