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

#include <cstring>
#include <dlfcn.h>
#include <string>

#include "audio_errors.h"
#include "audio_log.h"
#include "remote_audio_capturer_source.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
#ifdef DEBUG_CAPTURE_DUMP
const char *g_audioOutTestFilePath = "/data/local/tmp/remote_audio_capture.pcm";
#endif // DEBUG_CAPTURE_DUMP

std::map<std::string, RemoteAudioCapturerSource *> RemoteAudioCapturerSource::allRemoteSources;
RemoteAudioCapturerSource *RemoteAudioCapturerSource::GetInstance(std::string deviceNetworkId)
{
    RemoteAudioCapturerSource *audioCapturer_ = nullptr;
    // check if it is in our map
    if (allRemoteSources.count(deviceNetworkId)) {
        return allRemoteSources[deviceNetworkId];
    } else {
        audioCapturer_ = new(std::nothrow) RemoteAudioCapturerSource(deviceNetworkId);
        AUDIO_DEBUG_LOG("new Daudio device source:[%{public}s]", deviceNetworkId.c_str());
        allRemoteSources[deviceNetworkId] = audioCapturer_;
    }
    CHECK_AND_RETURN_RET_LOG((audioCapturer_ != nullptr), nullptr, "null audioCapturer!");
    return audioCapturer_;
}

RemoteAudioCapturerSource::RemoteAudioCapturerSource(std::string deviceNetworkId)
{
    this->deviceNetworkId_ = deviceNetworkId;
    capturerInited_ = false;
    audioManager_ = nullptr;
    audioAdapter_ = nullptr;
    audioCapture_ = nullptr;
}

RemoteAudioCapturerSource::~RemoteAudioCapturerSource()
{
    if (capturerInited_ == true) {
        DeInit();
    } else {
        AUDIO_INFO_LOG("RemoteAudioCapturerSource has already DeInit.");
    }
}

void RemoteAudioCapturerSource::DeInit()
{
    capturerInited_ = false;

    if ((audioCapture_ != nullptr) && (audioAdapter_ != nullptr)) {
        audioAdapter_->DestroyCapture(audioAdapter_, audioCapture_);
    }
    audioCapture_ = nullptr;

    if ((audioManager_ != nullptr) && (audioAdapter_ != nullptr)) {
        if (routeHandle_ != -1) {
            audioAdapter_->ReleaseAudioRoute(audioAdapter_, routeHandle_);
        }
        audioManager_->UnloadAdapter(audioManager_, audioAdapter_);
    }
    audioAdapter_ = nullptr;
    audioManager_ = nullptr;

    // remove map recorder.
    RemoteAudioCapturerSource *temp = allRemoteSources[this->deviceNetworkId_];
    if (temp != nullptr) {
        delete temp;
        temp = nullptr;
        allRemoteSources.erase(this->deviceNetworkId_);
    }
#ifdef DEBUG_CAPTURE_DUMP
    if (pfd != nullptr) {
        fclose(pfd);
        pfd = nullptr;
    }
#endif // DEBUG_CAPTURE_DUMP
}

int32_t SwitchAdapterCapture(struct AudioAdapterDescriptor *descs, int32_t size, const std::string &adapterNameCase,
    struct AudioPort &capturePort)
{
    if (descs == nullptr) {
        return ERROR;
    }

    for (int32_t index = 0; index < size; index++) {
        struct AudioAdapterDescriptor *desc = &descs[index];
        if (desc == nullptr) {
            continue;
        }
        if (adapterNameCase.compare(desc->adapterName) != 0) {
            continue;
        }
        for (uint32_t port = 0; port < desc->portNum; port++) {
            // Only find out the port of in in the sound card
            if (desc->ports[port].portId == PIN_IN_MIC) {
                capturePort = desc->ports[port];
                return index;
            }
        }
    }
    AUDIO_ERR_LOG("SwitchAdapterCapture Fail");

    return ERR_INVALID_INDEX;
}

int32_t RemoteAudioCapturerSource::CreateCapture(struct AudioPort &capturePort)
{
    int32_t ret;
    struct AudioSampleAttributes param;
    param.type = AUDIO_IN_MEDIA;
    param.period = deepBufferCapturePeriodSize;
    param.streamId = internalInputStreamId;
    param.isSignedData = true;
    param.stopThreshold = maxInt32;
    param.silenceThreshold = audioBufferSize;
    // User needs to set
    param.sampleRate = attr_.sampleRate;
    param.format = attr_.format;
    param.isBigEndian = attr_.isBigEndian;
    param.channelCount = attr_.channel;
    param.silenceThreshold = attr_.bufferSize;
    param.frameSize = param.format * param.channelCount;
    param.startThreshold = deepBufferCapturePeriodSize / (param.frameSize);

    struct AudioDeviceDescriptor deviceDesc;
    deviceDesc.portId = capturePort.portId;
    deviceDesc.pins = PIN_IN_MIC;
    deviceDesc.desc = nullptr;

    ret = audioAdapter_->CreateCapture(audioAdapter_, &deviceDesc, &param, &audioCapture_);
    if (audioCapture_ == nullptr || ret < 0) {
        AUDIO_ERR_LOG("Create capture failed");
        return ERR_NOT_STARTED;
    }

    return SUCCESS;
}

int32_t RemoteAudioCapturerSource::Init(IAudioSourceAttr &attr)
{
    attr_ = attr;
    int32_t ret;
    int32_t index;
    int32_t size = 0;
    struct AudioAdapterDescriptor *descs = nullptr;
    if (InitAudioManager() != 0) {
        AUDIO_ERR_LOG("Init audio manager Fail");
        return ERR_INVALID_HANDLE;
    }
    ret = audioManager_->GetAllAdapters(audioManager_, &descs, &size);
    // adapters is 0~3
    if (size == 0 || descs == nullptr || ret != 0) {
        AUDIO_ERR_LOG("Get adapters Fail");
        return ERR_NOT_STARTED;
    }
    // Get qualified sound card and port
    std::string adapterNameCase = attr_.deviceNetworkId;
    index = SwitchAdapterCapture(descs, size, adapterNameCase, audioPort);
    if (index < 0) {
        AUDIO_ERR_LOG("Switch Adapter Capture Fail");
        return ERR_NOT_STARTED;
    }
    struct AudioAdapterDescriptor *desc = &descs[index];
    if (audioManager_->LoadAdapter(audioManager_, desc, &audioAdapter_) != 0) {
        AUDIO_ERR_LOG("Load Adapter Fail");
        return ERR_NOT_STARTED;
    }
    if (audioAdapter_ == nullptr) {
        AUDIO_ERR_LOG("Load audio device failed");
        return ERR_NOT_STARTED;
    }

    // Inittialization port information, can fill through mode and other parameters
    ret = audioAdapter_->InitAllPorts(audioAdapter_);
    if (ret != 0) {
        AUDIO_ERR_LOG("InitAllPorts failed");
        return ERR_DEVICE_INIT;
    }
    if (CreateCapture(audioPort) != 0) {
        AUDIO_ERR_LOG("Create capture failed");
        return ERR_NOT_STARTED;
    }
    capturerInited_ = true;

#ifdef DEBUG_CAPTURE_DUMP
    pfd = fopen(g_audioOutTestFilePath, "rb");
    if (pfd == nullptr) {
        AUDIO_ERR_LOG("Error opening pcm test file!");
    }
#endif // DEBUG_CAPTURE_DUMP

    return SUCCESS;
}

bool RemoteAudioCapturerSource::IsInited(void)
{
    return capturerInited_;
}

int32_t RemoteAudioCapturerSource::InitAudioManager()
{
    AUDIO_INFO_LOG("RemoteAudioCapturerSource: Initialize audio proxy manager");
#ifdef PRODUCT_M40
#ifdef __aarch64__
    char resolvedPath[100] = "/vendor/lib64/libdaudio_client.z.so";
#else
    char resolvedPath[100] = "/vendor/lib/libdaudio_client.z.so";
#endif
    struct AudioManager *(*GetAudioManagerFuncs)() = nullptr;

    void *handle_ = dlopen(resolvedPath, 1);
    if (handle_ == nullptr) {
        AUDIO_ERR_LOG("Open so Fail");
        return ERR_INVALID_HANDLE;
    }
    AUDIO_INFO_LOG("dlopen successful");

    GetAudioManagerFuncs = (struct AudioManager *(*)())(dlsym(handle_, "GetAudioManagerFuncs"));
    if (GetAudioManagerFuncs == nullptr) {
        return ERR_INVALID_HANDLE;
    }
    AUDIO_INFO_LOG("GetAudioManagerFuncs done");

    audioManager_ = GetAudioManagerFuncs();
    if (audioManager_ == nullptr) {
        return ERR_INVALID_HANDLE;
    }
    AUDIO_INFO_LOG("daudio manager created");
#else
    audioManager_ = GetAudioManagerFuncs();
#endif // PRODUCT_M40
    CHECK_AND_RETURN_RET_LOG((audioManager_ != nullptr), ERR_INVALID_HANDLE, "Initialize audio proxy failed!");

    return SUCCESS;
}

int32_t RemoteAudioCapturerSource::CaptureFrame(char *frame, uint64_t requestBytes, uint64_t &replyBytes)
{
    int32_t ret;
    CHECK_AND_RETURN_RET_LOG((audioCapture_ != nullptr), ERR_INVALID_HANDLE, "Audio capture Handle is nullptr!");

    ret = audioCapture_->CaptureFrame(audioCapture_, frame, requestBytes, &replyBytes);
    if (ret < 0) {
        AUDIO_ERR_LOG("Capture Frame Fail");
        return ERR_READ_FAILED;
    }

#ifdef DEBUG_CAPTURE_DUMP
    if (feof(pfd)) {
        AUDIO_INFO_LOG("End of the file reached, start reading from beginning");
        rewind(pfd);
    }
    size_t writeResult = fread(frame, 1, requestBytes, pfd);
    if (writeResult != requestBytes) {
        AUDIO_ERR_LOG("Failed to read the file.");
    }
#endif // DEBUG_CAPTURE_DUMP

    return SUCCESS;
}

int32_t RemoteAudioCapturerSource::Start(void)
{
    AUDIO_INFO_LOG("RemoteAudioCapturerSource::Start in");
    int32_t ret;
    CHECK_AND_RETURN_RET_LOG((audioCapture_ != nullptr), ERR_INVALID_HANDLE, "Audio capture Handle is nullptr!");
    if (!started_) {
        ret = audioCapture_->control.Start((AudioHandle)audioCapture_);
        if (ret < 0) {
            return ERR_NOT_STARTED;
        }
        started_ = true;
    }
    started_ = true;
    return SUCCESS;
}

int32_t RemoteAudioCapturerSource::SetVolume(float left, float right)
{
    // remote setvolume may not supported
    float volume = 0.5;
    CHECK_AND_RETURN_RET_LOG((audioCapture_ != nullptr), ERR_INVALID_HANDLE, "Audio capture Handle is nullptr!");

    float leftVolume_ = left;
    float rightVolume_ = right;
    float half = 0.5;
    if ((leftVolume_ == 0) && (rightVolume_ != 0)) {
        volume = rightVolume_;
    } else if ((leftVolume_ != 0) && (rightVolume_ == 0)) {
        volume = leftVolume_;
    } else {
        volume = (leftVolume_ + rightVolume_) * half;
    }

    int32_t ret = audioCapture_->volume.SetVolume(reinterpret_cast<AudioHandle>(audioCapture_), volume);
    AUDIO_INFO_LOG("remote setVolume(%{public}f, %{public}f):%{public}d", left, right, ret);
    return ret;
}

int32_t RemoteAudioCapturerSource::GetVolume(float &left, float &right)
{
    float val = 0.0;
    audioCapture_->volume.GetVolume((AudioHandle)audioCapture_, &val);
    left = val;
    right = val;

    return SUCCESS;
}

int32_t RemoteAudioCapturerSource::SetMute(bool isMute)
{
    int32_t ret;
    CHECK_AND_RETURN_RET_LOG((audioCapture_ != nullptr), ERR_INVALID_HANDLE, "Audio capture Handle is nullptr!");

    ret = audioCapture_->volume.SetMute((AudioHandle)audioCapture_, isMute);
    if (ret != 0) {
        AUDIO_ERR_LOG("SetMute failed from hdi");
    }

    micMuteState_ = isMute;
    return SUCCESS;
}

int32_t RemoteAudioCapturerSource::GetMute(bool &isMute)
{
    int32_t ret;
    CHECK_AND_RETURN_RET_LOG((audioCapture_ != nullptr), ERR_INVALID_HANDLE, "Audio capture Handle is nullptr!");

    bool isHdiMute = false;
    ret = audioCapture_->volume.GetMute((AudioHandle)audioCapture_, &isHdiMute);
    if (ret != 0) {
        AUDIO_ERR_LOG("AudioCapturerSource::GetMute failed from hdi");
    }

    isMute = micMuteState_;
    return SUCCESS;
}

int32_t SetInputPortPin(DeviceType inputDevice, AudioRouteNode &source)
{
    int32_t ret = SUCCESS;

    switch (inputDevice) {
        case DEVICE_TYPE_MIC:
            source.ext.device.type = PIN_IN_MIC;
            source.ext.device.desc = "pin_in_mic";
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
            source.ext.device.type = PIN_IN_HS_MIC;
            source.ext.device.desc = "pin_in_hs_mic";
            break;
        case DEVICE_TYPE_USB_HEADSET:
            source.ext.device.type = PIN_IN_USB_EXT;
            source.ext.device.desc = "pin_in_usb_ext";
            break;
        default:
            ret = ERR_NOT_SUPPORTED;
            break;
    }

    return ret;
}

int32_t RemoteAudioCapturerSource::SetInputRoute(DeviceType inputDevice)
{
    AudioRouteNode source = {};
    AudioRouteNode sink = {};

    int32_t ret = SetInputPortPin(inputDevice, source);
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("AudioCapturerSource: OpenOutput FAILED: %{public}d", ret);
        return ret;
    }

    source.portId = audioPort.portId;
    source.role = AUDIO_PORT_SOURCE_ROLE;
    source.type = AUDIO_PORT_DEVICE_TYPE;
    source.ext.device.moduleId = 0;

    sink.portId = 0;
    sink.role = AUDIO_PORT_SINK_ROLE;
    sink.type = AUDIO_PORT_MIX_TYPE;
    sink.ext.mix.moduleId = 0;
    sink.ext.mix.streamId = internalInputStreamId;

    AudioRoute route = {
        .sourcesNum = 1,
        .sources = &source,
        .sinksNum = 1,
        .sinks = &sink,
    };

    if (audioAdapter_ == nullptr) {
        AUDIO_ERR_LOG("SetInputRoute failed, audioAdapter_ is null");
        return ERR_INVALID_PARAM;
    }
    ret = audioAdapter_->UpdateAudioRoute(audioAdapter_, &route, &routeHandle_);
    AUDIO_DEBUG_LOG("UpdateAudioRoute returns: %{public}d", ret);
    if (ret != 0) {
        AUDIO_ERR_LOG("UpdateAudioRoute failed");
        return ERR_OPERATION_FAILED;
    }
    return SUCCESS;
}

AudioCategory GetAudioCategory(AudioScene audioScene)
{
    AudioCategory audioCategory;
    switch (audioScene) {
        case AUDIO_SCENE_DEFAULT:
            audioCategory = AUDIO_IN_MEDIA;
            break;
        case AUDIO_SCENE_RINGING:
            audioCategory = AUDIO_IN_RINGTONE;
            break;
        case AUDIO_SCENE_PHONE_CALL:
            audioCategory = AUDIO_IN_CALL;
            break;
        case AUDIO_SCENE_PHONE_CHAT:
            audioCategory = AUDIO_IN_COMMUNICATION;
            break;
        default:
            audioCategory = AUDIO_IN_MEDIA;
            break;
    }
    AUDIO_DEBUG_LOG("RemoteAudioCapturerSource: Audio category returned is: %{public}d", audioCategory);

    return audioCategory;
}

int32_t RemoteAudioCapturerSource::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    CHECK_AND_RETURN_RET_LOG((audioCapture_ != nullptr), ERR_INVALID_HANDLE, "Audio capture Handle is nullptr!");
    struct AudioSceneDescriptor scene;
    scene.scene.id = GetAudioCategory(audioScene);
    scene.desc.pins = PIN_IN_MIC;
    if (audioCapture_->scene.SelectScene == nullptr) {
        AUDIO_ERR_LOG("AudioCapturerSource: Select scene nullptr");
        return ERR_OPERATION_FAILED;
    }

    AUDIO_INFO_LOG("AudioCapturerSource::SelectScene start");
    int32_t ret = audioCapture_->scene.SelectScene((AudioHandle)audioCapture_, &scene);
    AUDIO_INFO_LOG("AudioCapturerSource::SelectScene over");
    if (ret < 0) {
        AUDIO_ERR_LOG("AudioCapturerSource: Select scene FAILED: %{public}d", ret);
        return ERR_OPERATION_FAILED;
    }
    AUDIO_INFO_LOG("AudioCapturerSource::Select audio scene SUCCESS: %{public}d", audioScene);
    return SUCCESS;
}

uint64_t RemoteAudioCapturerSource::GetTransactionId()
{
    AUDIO_INFO_LOG("RemoteAudioCapturerSource::GetTransactionId in");
    return reinterpret_cast<uint64_t>(audioCapture_);
}

int32_t RemoteAudioCapturerSource::Stop(void)
{
    AUDIO_INFO_LOG("RemoteAudioCapturerSource::Stop in");
    int32_t ret;
    if (started_ && audioCapture_ != nullptr) {
        ret = audioCapture_->control.Stop(reinterpret_cast<AudioHandle>(audioCapture_));
        if (ret < 0) {
            AUDIO_ERR_LOG("Stop capture Failed");
            return ERR_OPERATION_FAILED;
        }
    }
    started_ = false;
    return SUCCESS;
}

int32_t RemoteAudioCapturerSource::Pause(void)
{
    int32_t ret;
    if (started_ && audioCapture_ != nullptr) {
        ret = audioCapture_->control.Pause(reinterpret_cast<AudioHandle>(audioCapture_));
        if (ret != 0) {
            AUDIO_ERR_LOG("pause capture Failed");
            return ERR_OPERATION_FAILED;
        }
    }
    paused_ = true;
    return SUCCESS;
}

int32_t RemoteAudioCapturerSource::Resume(void)
{
    int32_t ret;
    if (paused_ && audioCapture_ != nullptr) {
        ret = audioCapture_->control.Resume(reinterpret_cast<AudioHandle>(audioCapture_));
        if (ret != 0) {
            AUDIO_ERR_LOG("resume capture Failed");
            return ERR_OPERATION_FAILED;
        }
    }
    paused_ = false;
    return SUCCESS;
}

int32_t RemoteAudioCapturerSource::Reset(void)
{
    if (started_ && audioCapture_ != nullptr) {
        audioCapture_->control.Flush(reinterpret_cast<AudioHandle>(audioCapture_));
    }
    return SUCCESS;
}

int32_t RemoteAudioCapturerSource::Flush(void)
{
    if (started_ && audioCapture_ != nullptr) {
        audioCapture_->control.Flush(reinterpret_cast<AudioHandle>(audioCapture_));
    }
    return SUCCESS;
}
} // namespace AudioStandard
} // namesapce OHOS

