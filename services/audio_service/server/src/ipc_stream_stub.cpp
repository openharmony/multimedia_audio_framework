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
#ifndef LOG_TAG
#define LOG_TAG "IpcStreamStub"
#endif

#include "ipc_stream_stub.h"
#include "audio_service_log.h"
#include "audio_errors.h"
#include "audio_process_config.h"
#include "audio_utils.h"

namespace OHOS {
namespace AudioStandard {
bool IpcStreamStub::CheckInterfaceToken(MessageParcel &data)
{
    static auto localDescriptor = IpcStream::GetDescriptor();
    auto remoteDescriptor = data.ReadInterfaceToken();
    if (remoteDescriptor != localDescriptor) {
        AUDIO_ERR_LOG("CheckInterFfaceToken failed.");
        return false;
    }
    return true;
}

int IpcStreamStub::OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option)
{
    if (!CheckInterfaceToken(data)) {
        return AUDIO_ERR;
    }
    Trace trace("IpcStream::Handle::" + std::to_string(code));
    if (code >= IpcStreamMsg::IPC_STREAM_MAX_MSG) {
        AUDIO_WARNING_LOG("OnRemoteRequest unsupported request code:%{public}d.", code);
        return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
    }
    return (this->*funcList_[code])(data, reply);
}

int32_t IpcStreamStub::HandleRegisterStreamListener(MessageParcel &data, MessageParcel &reply)
{
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    if (object == nullptr) {
        AUDIO_ERR_LOG("IpcStreamStub: HandleRegisterProcessCb obj is null");
        reply.WriteInt32(AUDIO_INVALID_PARAM);
        return AUDIO_INVALID_PARAM;
    }
    reply.WriteInt32(RegisterStreamListener(object));
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleResolveBuffer(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    std::shared_ptr<OHAudioBuffer> buffer;
    int32_t ret = ResolveBuffer(buffer);
    reply.WriteInt32(ret);
    if (ret == AUDIO_OK && buffer != nullptr) {
        OHAudioBuffer::WriteToParcel(buffer, reply);
    } else {
        AUDIO_ERR_LOG("error: ResolveBuffer failed.");
        return AUDIO_INVALID_PARAM;
    }

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleUpdatePosition(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    reply.WriteInt32(UpdatePosition());
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleGetAudioSessionID(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    uint32_t sessionId = 0;
    reply.WriteInt32(GetAudioSessionID(sessionId));
    reply.WriteUint32(sessionId);

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleStart(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    reply.WriteInt32(Start());
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandlePause(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    reply.WriteInt32(Pause());
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleStop(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    reply.WriteInt32(Stop());
    return AUDIO_OK;
}
int32_t IpcStreamStub::HandleRelease(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    reply.WriteInt32(Release());
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleFlush(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    reply.WriteInt32(Flush());
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleDrain(MessageParcel &data, MessageParcel &reply)
{
    bool stopFlag = data.ReadBool();
    AUDIO_INFO_LOG("stopFlag:%{public}d", stopFlag);
    reply.WriteInt32(Drain(stopFlag));
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleUpdatePlaybackCaptureConfig(MessageParcel &data, MessageParcel &reply)
{
    AudioPlaybackCaptureConfig config;
    int32_t ret = ProcessConfig::ReadInnerCapConfigFromParcel(config, data);
    CHECK_AND_RETURN_RET_LOG(ret == SUCCESS, AUDIO_ERR, "Read config failed");

    reply.WriteInt32(UpdatePlaybackCaptureConfig(config));

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleGetAudioTime(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    uint64_t framePos = 0;
    uint64_t timestamp = 0;
    reply.WriteInt32(GetAudioTime(framePos, timestamp));
    reply.WriteUint64(framePos);
    reply.WriteUint64(timestamp);
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleGetAudioPosition(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    uint64_t framePos = 0;
    uint64_t timestamp = 0;
    reply.WriteInt32(GetAudioPosition(framePos, timestamp));
    reply.WriteUint64(framePos);
    reply.WriteUint64(timestamp);
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleGetLatency(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    uint64_t latency = 0;
    reply.WriteInt32(GetLatency(latency));
    reply.WriteUint64(latency);

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleSetRate(MessageParcel &data, MessageParcel &reply)
{
    int32_t rate = data.ReadInt32();
    reply.WriteInt32(SetRate(rate));

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleGetRate(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    int32_t rate = -1;
    reply.WriteInt32(GetRate(rate));
    reply.WriteInt32(rate);

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleSetLowPowerVolume(MessageParcel &data, MessageParcel &reply)
{
    float lowPowerVolume = data.ReadFloat();
    reply.WriteInt32(SetLowPowerVolume(lowPowerVolume));

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleGetLowPowerVolume(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    float lowPowerVolume = 0.0;
    reply.WriteInt32(GetLowPowerVolume(lowPowerVolume));
    reply.WriteFloat(lowPowerVolume);

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleSetAudioEffectMode(MessageParcel &data, MessageParcel &reply)
{
    int32_t effectMode = data.ReadInt32();
    reply.WriteInt32(SetAudioEffectMode(effectMode));

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleGetAudioEffectMode(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    int32_t effectMode = -1;
    reply.WriteInt32(GetAudioEffectMode(effectMode));
    reply.WriteInt32(effectMode);

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleSetPrivacyType(MessageParcel &data, MessageParcel &reply)
{
    int32_t privacyType = data.ReadInt32();
    reply.WriteInt32(SetPrivacyType(privacyType));

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleGetPrivacyType(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    int32_t privacyType = -1;
    reply.WriteInt32(GetPrivacyType(privacyType));
    reply.WriteInt32(privacyType);

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleSetOffloadMode(MessageParcel &data, MessageParcel &reply)
{
    int32_t state = data.ReadInt32();
    bool isAppBack = data.ReadBool();
    reply.WriteInt32(SetOffloadMode(state, isAppBack));

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleUnsetOffloadMode(MessageParcel &data, MessageParcel &reply)
{
    reply.WriteInt32(UnsetOffloadMode());

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleGetOffloadApproximatelyCacheTime(MessageParcel &data, MessageParcel &reply)
{
    uint64_t timestamp = data.ReadUint64();
    uint64_t paWriteIndex = data.ReadUint64();
    uint64_t cacheTimeDsp = data.ReadUint64();
    uint64_t cacheTimePa = data.ReadUint64();
    reply.WriteInt32(GetOffloadApproximatelyCacheTime(timestamp, paWriteIndex, cacheTimeDsp, cacheTimePa));
    reply.WriteUint64(timestamp);
    reply.WriteUint64(paWriteIndex);
    reply.WriteUint64(cacheTimeDsp);
    reply.WriteUint64(cacheTimePa);

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleOffloadSetVolume(MessageParcel &data, MessageParcel &reply)
{
    float volume = data.ReadFloat();
    reply.WriteInt32(OffloadSetVolume(volume));

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleUpdateSpatializationState(MessageParcel &data, MessageParcel &reply)
{
    bool spatializationEnabled = data.ReadBool();
    bool headTrackingEnabled = data.ReadBool();
    reply.WriteInt32(UpdateSpatializationState(spatializationEnabled, headTrackingEnabled));
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleGetStreamManagerType(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    reply.WriteInt32(GetStreamManagerType());
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleSetSilentModeAndMixWithOthers(MessageParcel &data, MessageParcel &reply)
{
    bool on = data.ReadBool();
    reply.WriteInt32(SetSilentModeAndMixWithOthers(on));

    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleSetClientVolume(MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    reply.WriteInt32(SetClientVolume());
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleSetMute(MessageParcel &data, MessageParcel &reply)
{
    bool isMute = data.ReadBool();
    reply.WriteInt32(SetMute(isMute));
    return AUDIO_OK;
}

int32_t IpcStreamStub::HandleRegisterThreadPriority(MessageParcel &data, MessageParcel &reply)
{
    uint32_t tid = data.ReadUint32();
    std::string bundleName = data.ReadString();
    reply.WriteInt32(RegisterThreadPriority(tid, bundleName));
    return AUDIO_OK;
}
} // namespace AudioStandard
} // namespace OHOS
