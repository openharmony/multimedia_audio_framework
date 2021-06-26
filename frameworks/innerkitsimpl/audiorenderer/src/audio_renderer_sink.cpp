/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#include <unistd.h>

#include "audio_errors.h"
#include "audio_renderer_sink.h"
#include "media_log.h"

namespace OHOS {
namespace AudioStandard {
#define AUDIO_CHANNELCOUNT 2
#define AUDIO_SAMPLE_RATE_48K 48000
#define DEEP_BUFFER_RENDER_PERIOD_SIZE 4096
#define INT_32_MAX 0x7fffffff
#define PERIOD_SIZE 1024

namespace {
const int32_t HALF_FACTOR = 2;
const int32_t MAX_AUDIO_ADAPTER_NUM = 3;
const float DEFAULT_VOLUME_LEVEL = 1.0f;
}

#ifdef DUMPFILE
const char *g_audioOutTestFilePath = "/data/local/tmp/audioout_test.pcm";
#endif // DUMPFILE

AudioRendererSink::AudioRendererSink()
    : rendererInited_(false), started_(false), paused_(false), leftVolume_(DEFAULT_VOLUME_LEVEL),
      rightVolume_(DEFAULT_VOLUME_LEVEL), audioManager_(nullptr), audioAdapter_(nullptr), audioRender_(nullptr)
{
    attr_ = {};
#ifdef DUMPFILE
    pfd = nullptr;
#endif // DUMPFILE
}

AudioRendererSink::~AudioRendererSink()
{
    DeInit();
}

AudioRendererSink* AudioRendererSink::GetInstance()
{
    static AudioRendererSink audioRenderer_;

    return &audioRenderer_;
}

void AudioRendererSink::DeInit()
{
    started_ = false;
    rendererInited_ = false;
    if ((audioRender_ != nullptr) && (audioAdapter_ != nullptr)) {
        audioAdapter_->DestroyRender(audioAdapter_, audioRender_);
    }
    audioRender_ = nullptr;

    if ((audioManager_ != nullptr) && (audioAdapter_ != nullptr)) {
        audioManager_->UnloadAdapter(audioManager_, audioAdapter_);
    }
    audioAdapter_ = nullptr;
    audioManager_ = nullptr;

    dlclose(handle_);

#ifdef DUMPFILE
    if (pfd) {
        fclose(pfd);
        pfd = nullptr;
    }
#endif // DUMPFILE
}

int32_t InitAttrs(struct AudioSampleAttributes *attrs)
{
    /* Initialization of audio parameters for playback */
    attrs->format = AUDIO_FORMAT_PCM_16_BIT;
    attrs->channelCount = AUDIO_CHANNELCOUNT;
    attrs->sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs->interleaved = 0;
    attrs->type = AUDIO_IN_MEDIA;
    attrs->period = DEEP_BUFFER_RENDER_PERIOD_SIZE;
    /* PERIOD_SIZE * 16 * attrs->channelCount / 8 */
    attrs->frameSize = PERIOD_SIZE * 16 * attrs->channelCount / 8;
    attrs->isBigEndian = false;
    attrs->isSignedData = true;
    /* DEEP_BUFFER_RENDER_PERIOD_SIZE / (16 * attrs->channelCount / 8) */
    attrs->startThreshold = DEEP_BUFFER_RENDER_PERIOD_SIZE / (16 * attrs->channelCount / 8);
    attrs->stopThreshold = INT_32_MAX;
    attrs->silenceThreshold = 0;

    return SUCCESS;
}

static int32_t SwitchAdapter(struct AudioAdapterDescriptor *descs,
    const char *adapterNameCase, enum AudioPortDirection portFlag,
    struct AudioPort *renderPort, int32_t size)
{
    for (int32_t index = 0; index < size; index++) {
        struct AudioAdapterDescriptor *desc = &descs[index];
        if (desc == nullptr) {
            MEDIA_ERR_LOG("SwitchAdapter Fail AudioAdapterDescriptor desc NULL");
            return ERR_INVALID_INDEX;  // note: return invalid index not invalid handle
        }
        if (!strcmp(desc->adapterName, adapterNameCase)) {
            for (uint32_t port = 0; ((desc != NULL) && (port < desc->portNum)); port++) {
                // Only find out the port of out in the sound card
                if (desc->ports[port].dir == portFlag) {
                    *renderPort = desc->ports[port];
                    return index;
                }
            }
        }
    }
    MEDIA_ERR_LOG("SwitchAdapter Fail");

    return ERR_INVALID_INDEX;
}

int32_t AudioRendererSink::Init(AudioSinkAttr &attr)
{
    attr_ = attr;
    struct AudioPort renderPort;
    const char *adapterNameCase = "usb";  // Set sound card information
    enum AudioPortDirection port = PORT_OUT; // Set port information
    char resolvedPath[100] = "/system/lib/libhdi_audio.z.so";
    struct AudioManager *(*getAudioManager)() = nullptr;

    handle_ = dlopen(resolvedPath, 1);
    if (handle_ == nullptr) {
        MEDIA_ERR_LOG("Open so Fail");
        return ERR_INVALID_HANDLE;
    }
    getAudioManager = (struct AudioManager* (*)())(dlsym(handle_, "GetAudioManagerFuncs"));
    audioManager_ = getAudioManager();
    if (audioManager_ == nullptr) {
        return ERR_INVALID_HANDLE;
    }

    int32_t ret = 0;
    int32_t size = -1;
    struct AudioAdapterDescriptor *descs = nullptr;
    audioManager_->GetAllAdapters(audioManager_, &descs, &size);
    if (size > MAX_AUDIO_ADAPTER_NUM || size == 0 || descs == nullptr || ret < 0) {
        MEDIA_ERR_LOG("Get adapters Fail");
        return ERR_NOT_STARTED;
    }

    // Get qualified sound card and port
    int32_t index = SwitchAdapter(descs, adapterNameCase, port, &renderPort, size);
    if (index < 0) {
        MEDIA_ERR_LOG("Switch Adapter Fail");
        return ERR_NOT_STARTED;
    }

    struct AudioAdapterDescriptor *desc = &descs[index];
    if (audioManager_->LoadAdapter(audioManager_, desc, &audioAdapter_) != 0) {
        MEDIA_ERR_LOG("Load Adapter Fail");
        return ERR_NOT_STARTED;
    }
    if (audioAdapter_ == nullptr) {
        MEDIA_ERR_LOG("Load audio device failed");
        return ERR_NOT_STARTED;
    }

    // Initialization port information, can fill through mode and other parameters
    ret = audioAdapter_->InitAllPorts(audioAdapter_);
    if (ret != 0) {
        MEDIA_ERR_LOG("InitAllPorts failed");
        return ERR_NOT_STARTED;
    }

    struct AudioSampleAttributes param;
    InitAttrs(&param);
    param.sampleRate = attr_.sampleRate;
    param.channelCount = attr_.channel;

    struct AudioDeviceDescriptor deviceDesc;
    deviceDesc.portId = renderPort.portId;
    deviceDesc.pins = PIN_OUT_SPEAKER;
    deviceDesc.desc = nullptr;
    ret = audioAdapter_->CreateRender(audioAdapter_, &deviceDesc, &param, &audioRender_);
    if (ret != 0 || audioRender_ == nullptr) {
        MEDIA_ERR_LOG("AudioDeviceCreateRender failed");
        audioManager_->UnloadAdapter(audioManager_, audioAdapter_);
        return ERR_NOT_STARTED;
    }
    rendererInited_ = true;

#ifdef DUMPFILE
    pfd = fopen(g_audioOutTestFilePath, "wb+");
    if (pfd == nullptr) {
        MEDIA_ERR_LOG("Error opening pcm test file!");
    }
#endif // DUMPFILE

    return SUCCESS;
}

int32_t AudioRendererSink::RenderFrame(char &data, uint64_t len, uint64_t &writeLen)
{
    int32_t ret;
    if (audioRender_ == nullptr) {
        MEDIA_ERR_LOG("Audio Render Handle is nullptr!");
        return ERR_INVALID_HANDLE;
    }

#ifdef DUMPFILE
    size_t writeResult = fwrite((void*)&data, 1, len, pfd);
    if (writeResult != len) {
        MEDIA_ERR_LOG("Failed to write the file.");
    }
#endif // DUMPFILE

    ret = audioRender_->RenderFrame(audioRender_, (void*)&data, len, &writeLen);
    if (ret != 0) {
        MEDIA_ERR_LOG("RenderFrame failed ret: %{public}x", ret);
        return ERR_WRITE_FAILED;
    }

    return SUCCESS;
}

int32_t AudioRendererSink::Start(void)
{
    if (!started_ && audioRender_ != nullptr) {
        audioRender_->control.Start((AudioHandle)audioRender_);
        started_ = true;
    }
    return SUCCESS;
}

int32_t AudioRendererSink::SetVolume(float left, float right)
{
    float volume;
    if (audioRender_ == nullptr) {
        MEDIA_ERR_LOG("AudioRendererSink::SetVolume failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    leftVolume_ = left;
    rightVolume_ = right;
    if ((leftVolume_ == 0) && (rightVolume_ != 0)) {
        volume = rightVolume_;
    } else if ((leftVolume_ != 0) && (rightVolume_ == 0)) {
        volume = leftVolume_;
    } else {
        volume = (leftVolume_ + rightVolume_) / HALF_FACTOR;
    }
    audioRender_->volume.SetVolume(reinterpret_cast<AudioHandle>(audioRender_), volume);
    return SUCCESS;
}

int32_t AudioRendererSink::GetVolume(float &left, float &right)
{
    left = leftVolume_;
    right = rightVolume_;
    return SUCCESS;
}

int32_t AudioRendererSink::Stop(void)
{
    if (started_ && audioRender_ != nullptr) {
        audioRender_->control.Stop(reinterpret_cast<AudioHandle>(audioRender_));
    }
    started_ = false;
    return SUCCESS;
}

int32_t AudioRendererSink::Pause(void)
{
    if (started_ && audioRender_ != nullptr) {
        audioRender_->control.Pause(reinterpret_cast<AudioHandle>(audioRender_));
    }
    paused_ = true;
    return SUCCESS;
}

int32_t AudioRendererSink::Resume(void)
{
    audioRender_->control.Resume(reinterpret_cast<AudioHandle>(audioRender_));
    paused_ = false;
    return SUCCESS;
}

int32_t AudioRendererSink::Reset(void)
{
    if (started_ && audioRender_ != nullptr) {
        audioRender_->control.Flush(reinterpret_cast<AudioHandle>(audioRender_));
    }
    return SUCCESS;
}

int32_t AudioRendererSink::Flush(void)
{
    if (started_ && audioRender_ != nullptr) {
        audioRender_->control.Flush(reinterpret_cast<AudioHandle>(audioRender_));
    }
    return SUCCESS;
}
} // namespace AudioStandard
} // namespace OHOS

#ifdef __cplusplus
extern "C" {
#endif

using namespace OHOS::AudioStandard;

AudioRendererSink* g_audioRendrSinkInstance = AudioRendererSink::GetInstance();

int32_t AudioRendererSinkInit(AudioSinkAttr *attr)
{
    int32_t ret;

    if (g_audioRendrSinkInstance->rendererInited_)
        return SUCCESS;

    ret = g_audioRendrSinkInstance->Init(*attr);
    return ret;
}

void AudioRendererSinkDeInit()
{
    if (g_audioRendrSinkInstance->rendererInited_)
        g_audioRendrSinkInstance->DeInit();
}

int32_t AudioRendererSinkStop()
{
    int32_t ret;

    if (!g_audioRendrSinkInstance->rendererInited_)
        return SUCCESS;

    ret = g_audioRendrSinkInstance->Stop();
    return ret;
}

int32_t AudioRendererSinkStart()
{
    int32_t ret;

    if (!g_audioRendrSinkInstance->rendererInited_) {
        MEDIA_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    ret = g_audioRendrSinkInstance->Start();
    return ret;
}

int32_t AudioRendererRenderFrame(char &data, uint64_t len, uint64_t &writeLen)
{
    int32_t ret;

    if (!g_audioRendrSinkInstance->rendererInited_) {
        MEDIA_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    ret = g_audioRendrSinkInstance->RenderFrame(data, len, writeLen);
    return ret;
}

int32_t AudioRendererSinkSetVolume(float left, float right)
{
    int32_t ret;

    if (!g_audioRendrSinkInstance->rendererInited_) {
        MEDIA_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    ret = g_audioRendrSinkInstance->SetVolume(left, right);
    return ret;
}

#ifdef __cplusplus
}
#endif
