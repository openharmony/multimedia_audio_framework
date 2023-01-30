/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "audio_container_base.h"
#include "audio_stream_death_recipient.h"
#include "audio_log.h"
#include "unistd.h"
#include "securec.h"
#include "system_ability_definition.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {

static bool g_isConnected = false;
static sptr<IAudioContainerService> g_sProxy = nullptr;

void AudioContainerBase::InitAudioStreamManagerGa(int serviceId)
{
    AUDIO_ERR_LOG("AudioContainerBase::InitAudioStreamManagerGa serviceId %d", serviceId);
    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    CHECK_AND_RETURN_LOG(samgr != nullptr, "AudioContainerBase::init failed");

    sptr<IRemoteObject> object = samgr->GetSystemAbility(serviceId);
    CHECK_AND_RETURN_LOG(object != nullptr, "AudioContainerBase::object is NULL");

    g_sProxy = iface_cast<IAudioContainerService>(object);
    CHECK_AND_RETURN_LOG(g_sProxy != nullptr, "AudioContainerBase::init g_sProxy is NULL");
    g_isConnected = true;
    RegisterAudioStreamDeathRecipient(object);
    AUDIO_INFO_LOG("AudioContainerBase::init g_sProxy is assigned.");
}

void AudioContainerBase::RegisterAudioStreamDeathRecipient(sptr<IRemoteObject> &object)
{
    AUDIO_INFO_LOG("AudioContainerBase Register audio renderer death recipient");
    pid_t pid = 0;
    sptr<AudioStreamDeathRecipient> recipient = new(std::nothrow) AudioStreamDeathRecipient(pid);
    if (recipient != nullptr) {
        recipient->SetServerDiedCallback([](auto &&PH1) {
            AudioStreamDeathRecipient(std::forward<decltype(PH1)>(PH1));
        });
        bool result = object->AddDeathRecipient(recipient);
        if (!result) {
            AUDIO_ERR_LOG("AudioContainerBase failed to add recipient");
        }
    }
}

void AudioContainerBase::AudioStreamDied(pid_t pid)
{
    AUDIO_INFO_LOG("AudioContainerBase, Audio renderer died, reestablish connection");
    g_isConnected = false;
}

int32_t AudioContainerBase::Initialize(ASClientType eClientType, int serviceId)
{
    if (g_sProxy == nullptr) {
        InitAudioStreamManagerGa(serviceId);
    }

    return g_sProxy->InitializeGa(eClientType);
}

int32_t AudioContainerBase::CreateStream(AudioStreamParams audioParams, AudioStreamType audioType)
{
    return g_sProxy->CreateStreamGa(audioParams, audioType);
}

int32_t AudioContainerBase::GetSessionID(uint32_t &sessionID, const int32_t &trackId) const
{
    return g_sProxy->GetSessionIDGa(sessionID, trackId);
}

int32_t AudioContainerBase::StartStream(const int32_t &trackId)
{
    return g_sProxy->StartStreamGa(trackId);
}

int32_t AudioContainerBase::PauseStream(const int32_t &trackId)
{
    return g_sProxy->PauseStreamGa(trackId);
}

int32_t AudioContainerBase::StopStream(const int32_t &trackId)
{
    return g_sProxy->StopStreamGa(trackId);
}

int32_t AudioContainerBase::FlushStream(const int32_t &trackId)
{
    return g_sProxy->FlushStreamGa(trackId);
}

int32_t AudioContainerBase::DrainStream(const int32_t &trackId)
{
    return g_sProxy->DrainStreamGa(trackId);
}

int32_t AudioContainerBase::SetAudioRenderMode(AudioRenderMode renderMode, const int32_t &trackId)
{
    return g_sProxy->SetAudioRenderModeGa(renderMode, trackId);
}

AudioRenderMode AudioContainerBase::GetAudioRenderMode()
{
    return g_sProxy->GetAudioRenderModeGa();
}

int32_t AudioContainerBase::SetStreamVolume(uint32_t sessionID, uint32_t volume)
{
    return g_sProxy->SetStreamVolumeGa(sessionID, volume);
}

size_t AudioContainerBase::WriteStreamInCb(const StreamBuffer &stream, int32_t &pError, const int32_t &trackId)
{
    return g_sProxy->WriteStreamInCbGa(stream, pError, trackId);
}

size_t AudioContainerBase::WriteStream(const StreamBuffer &stream, int32_t &pError, const int32_t &trackId)
{
    return g_sProxy->WriteStreamGa(stream, pError, trackId);
}

int32_t AudioContainerBase::ReadStream(StreamBuffer &stream, bool isBlocking, const int32_t &trackId)
{
    return g_sProxy->ReadStreamGa(stream, isBlocking, trackId);
}

int32_t AudioContainerBase::ReleaseStream(const int32_t &trackId)
{
    return g_sProxy->ReleaseStreamGa(trackId);
}

int32_t AudioContainerBase::GetMinimumBufferSize(size_t &minBufferSize, const int32_t &trackId) const
{
    return g_sProxy->GetMinimumBufferSizeGa(minBufferSize, trackId);
}

int32_t AudioContainerBase::GetMinimumFrameCount(uint32_t &frameCount, const int32_t &trackId) const
{
    return g_sProxy->GetMinimumFrameCountGa(frameCount, trackId);
}

int32_t AudioContainerBase::GetAudioStreamParams(AudioStreamParams& audioParams, const int32_t &trackId)
{
    return g_sProxy->GetAudioStreamParamsGa(audioParams, trackId);
}

uint32_t AudioContainerBase::GetStreamVolume(uint32_t sessionID)
{
    return g_sProxy->GetStreamVolumeGa();
}

int32_t AudioContainerBase::GetCurrentTimeStamp(uint64_t &timeStamp, const int32_t &trackId)
{
    return g_sProxy->GetCurrentTimeStampGa(timeStamp, trackId);
}

int32_t AudioContainerBase::GetAudioLatency(uint64_t &latency, const int32_t &trackId) const
{
    return g_sProxy->GetAudioLatencyGa(latency, trackId);
}

int32_t AudioContainerBase::SetStreamType(AudioStreamType audioStreamType, const int32_t &trackId)
{
    return g_sProxy->SetStreamTypeGa(audioStreamType, trackId);
}

int32_t AudioContainerBase::SetStreamVolume(float volume, const int32_t &trackId)
{
    return g_sProxy->SetStreamVolumeGa(volume, trackId);
}

float AudioContainerBase::GetStreamVolume()
{
    return g_sProxy->GetStreamVolumeGa();
}

int32_t AudioContainerBase::SetStreamRenderRate(AudioRendererRate audioRendererRate, const int32_t &trackId)
{
    return g_sProxy->SetStreamRenderRateGa(audioRendererRate, trackId);
}

AudioRendererRate AudioContainerBase::GetStreamRenderRate()
{
    return g_sProxy->GetStreamRenderRateGa();
}

void AudioContainerBase::SaveStreamCallback(const std::weak_ptr<AudioStreamCallback> &callback)
{
    return g_sProxy->SaveStreamCallbackGa(callback);
}

int32_t AudioContainerBase::SetBufferSizeInMsec(int32_t bufferSizeInMsec)
{
    return g_sProxy->SetBufferSizeInMsecGa(bufferSizeInMsec);
}

void AudioContainerBase::SetRendererPositionCallback(int64_t markPosition,
    const std::shared_ptr<RendererPositionCallback> &callback)
{
    return g_sProxy->SetRendererPositionCallbackGa(markPosition, callback);
}

void AudioContainerBase::UnsetRendererPositionCallback()
{
    return g_sProxy->UnsetRendererPositionCallbackGa();
}

void AudioContainerBase::SetRendererPeriodPositionCallback(int64_t periodPosition,
    const std::shared_ptr<RendererPeriodPositionCallback> &callback)
{
    return g_sProxy->SetRendererPeriodPositionCallbackGa(periodPosition, callback);
}

void AudioContainerBase::UnsetRendererPeriodPositionCallback()
{
    return g_sProxy->UnsetRendererPeriodPositionCallbackGa();
}

void AudioContainerBase::SetCapturerPositionCallback(int64_t markPosition,
    const std::shared_ptr<CapturerPositionCallback> &callback)
{
    return g_sProxy->SetCapturerPositionCallbackGa(markPosition, callback);
}

void AudioContainerBase::UnsetCapturerPositionCallback()
{
    return g_sProxy->UnsetCapturerPositionCallbackGa();
}

void AudioContainerBase::SetCapturerPeriodPositionCallback(int64_t periodPosition,
    const std::shared_ptr<CapturerPeriodPositionCallback> &callback)
{
    return g_sProxy->SetCapturerPeriodPositionCallbackGa(periodPosition, callback);
}

void AudioContainerBase::UnsetCapturerPeriodPositionCallback()
{
    return g_sProxy->UnsetCapturerPeriodPositionCallbackGa();
}

void AudioContainerBase::OnTimeOut()
{
    AUDIO_ERR_LOG("Inside read timeout callback");
}

int32_t AudioContainerBase::SetAudioCaptureMode(AudioCaptureMode captureMode)
{
    return g_sProxy->SetAudioCaptureMode(captureMode);
}

int32_t AudioContainerBase::SaveReadCallback(const std::weak_ptr<AudioCapturerReadCallback> &callback)
{
    return g_sProxy->SaveReadCallback(callback);
}

AudioCaptureMode AudioContainerBase::GetAudioCaptureMode()
{
    return g_sProxy->GetAudioCaptureMode();
}

void AudioContainerBase::SetAppCachePath(const std::string cachePath)
{
    AUDIO_INFO_LOG("SetAppCachePath cachePath %{public}s", cachePath.c_str());
}

int32_t AudioContainerBase::SaveWriteCallback(const std::weak_ptr<AudioRendererWriteCallback> &callback,
    const int32_t &trackId)
{
    AUDIO_INFO_LOG("AudioContainerBase::SaveWriteCallback");
    return g_sProxy->SaveWriteCallbackGa(callback, trackId);
}
} // namespace AudioStandard
} // namespace OHOS
