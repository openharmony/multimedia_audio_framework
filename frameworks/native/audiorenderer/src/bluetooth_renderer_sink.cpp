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

#include "bluetooth_renderer_sink.h"

#include <cstring>
#include <dlfcn.h>
#include <string>
#include <unistd.h>

#include "audio_errors.h"
#include "media_log.h"

using namespace std;
using namespace OHOS::HDI::Audio_Bluetooth;

namespace OHOS {
namespace AudioStandard {
namespace {
const int32_t HALF_FACTOR = 2;
const int32_t MAX_AUDIO_ADAPTER_NUM = 4;
const float DEFAULT_VOLUME_LEVEL = 1.0f;
const uint32_t AUDIO_CHANNELCOUNT = 2;
const uint32_t AUDIO_SAMPLE_RATE_48K = 48000;
const uint32_t DEEP_BUFFER_RENDER_PERIOD_SIZE = 4096;
const uint32_t INT_32_MAX = 0x7fffffff;
const uint32_t PCM_8_BIT = 8;
const uint32_t PCM_16_BIT = 16;
const uint32_t PCM_24_BIT = 24;
const uint32_t PCM_32_BIT = 32;
}

#ifdef BT_DUMPFILE
const char *g_audioOutTestFilePath = "/data/local/tmp/audioout_bt.pcm";
#endif // BT_DUMPFILE

BluetoothRendererSink::BluetoothRendererSink()
    : rendererInited_(false), started_(false), paused_(false), leftVolume_(DEFAULT_VOLUME_LEVEL),
      rightVolume_(DEFAULT_VOLUME_LEVEL), audioManager_(nullptr), audioAdapter_(nullptr), audioRender_(nullptr),
      handle_(nullptr)
{
    attr_ = {};
#ifdef BT_DUMPFILE
    pfd = nullptr;
#endif // BT_DUMPFILE
}

BluetoothRendererSink::~BluetoothRendererSink()
{
    DeInit();
}

BluetoothRendererSink *BluetoothRendererSink::GetInstance()
{
    static BluetoothRendererSink audioRenderer_;

    return &audioRenderer_;
}

void BluetoothRendererSink::DeInit()
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

#ifdef BT_DUMPFILE
    if (pfd) {
        fclose(pfd);
        pfd = nullptr;
    }
#endif // BT_DUMPFILE
}

int32_t InitAttrs(struct AudioSampleAttributes &attrs)
{
    /* Initialization of audio parameters for playback */
    attrs.format = AUDIO_FORMAT_PCM_16_BIT;
    attrs.frameSize = PCM_16_BIT * attrs.channelCount / PCM_8_BIT;
    attrs.channelCount = AUDIO_CHANNELCOUNT;
    attrs.sampleRate = AUDIO_SAMPLE_RATE_48K;
    attrs.interleaved = 0;
    attrs.type = AUDIO_IN_MEDIA;
    attrs.period = DEEP_BUFFER_RENDER_PERIOD_SIZE;
    attrs.isBigEndian = false;
    attrs.isSignedData = true;
    attrs.startThreshold = DEEP_BUFFER_RENDER_PERIOD_SIZE / (attrs.frameSize);
    attrs.stopThreshold = INT_32_MAX;
    attrs.silenceThreshold = 0;

    return SUCCESS;
}

static int32_t SwitchAdapter(struct AudioAdapterDescriptor *descs, string adapterNameCase,
    enum AudioPortDirection portFlag, struct AudioPort &renderPort, int32_t size)
{
    MEDIA_INFO_LOG("BluetoothRendererSink: adapterNameCase: %{public}s", adapterNameCase.c_str());
    if (descs == nullptr) {
        return ERROR;
    }

    for (int32_t index = 0; index < size; index++) {
        struct AudioAdapterDescriptor *desc = &descs[index];
        if (desc == nullptr) {
            continue;
        }
        MEDIA_INFO_LOG("BluetoothRendererSink: adapter name for %{public}d: %{public}s", index, desc->adapterName);
        if (!strcmp(desc->adapterName, adapterNameCase.c_str())) {
            for (uint32_t port = 0; port < desc->portNum; port++) {
                // Only find out the port of out in the sound card
                if (desc->ports[port].dir == portFlag) {
                    renderPort = desc->ports[port];
                    MEDIA_INFO_LOG("BluetoothRendererSink: index found %{public}d", index);
                    return index;
                }
            }
        }
    }
    MEDIA_ERR_LOG("SwitchAdapter Fail");

    return ERR_INVALID_INDEX;
}

int32_t BluetoothRendererSink::InitAudioManager()
{
    MEDIA_INFO_LOG("BluetoothRendererSink: Initialize audio proxy manager");

    char resolvedPath[100] = "/vendor/lib/libaudio_bluetooth_hdi_proxy_server.z.so";
    struct AudioProxyManager *(*getAudioManager)() = nullptr;

    handle_ = dlopen(resolvedPath, 1);
    if (handle_ == nullptr) {
        MEDIA_ERR_LOG("Open so Fail");
        return ERR_INVALID_HANDLE;
    }
    MEDIA_INFO_LOG("dlopen successful");

    getAudioManager = (struct AudioProxyManager *(*)())(dlsym(handle_, "GetAudioProxyManagerFuncs"));
    if (getAudioManager == nullptr) {
        return ERR_INVALID_HANDLE;
    }
    MEDIA_INFO_LOG("getaudiomanager done");

    audioManager_ = getAudioManager();
    if (audioManager_ == nullptr) {
        return ERR_INVALID_HANDLE;
    }
    MEDIA_INFO_LOG("audio manager created");

    return 0;
}

uint32_t PcmFormatToBits(AudioFormat format)
{
    switch (format) {
        case AUDIO_FORMAT_PCM_8_BIT:
            return PCM_8_BIT;
        case AUDIO_FORMAT_PCM_16_BIT:
            return PCM_16_BIT;
        case AUDIO_FORMAT_PCM_24_BIT:
            return PCM_24_BIT;
        case AUDIO_FORMAT_PCM_32_BIT:
            return PCM_32_BIT;
        default:
            return PCM_24_BIT;
    };
}

int32_t BluetoothRendererSink::CreateRender(struct AudioPort &renderPort)
{
    MEDIA_INFO_LOG("Create render in");
    int32_t ret;
    struct AudioSampleAttributes param;
    InitAttrs(param);
    param.sampleRate = attr_.sampleRate;
    param.channelCount = attr_.channel;
    param.format = attr_.format;
    param.frameSize = PcmFormatToBits(param.format) * param.channelCount / PCM_8_BIT;
    param.startThreshold = DEEP_BUFFER_RENDER_PERIOD_SIZE / (param.frameSize);
    MEDIA_ERR_LOG("BluetoothRendererSink Create render format: %{public}d", param.format);
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
    MEDIA_INFO_LOG("create render done");

    return 0;
}

int32_t BluetoothRendererSink::Init(AudioSinkAttr &attr)
{
    MEDIA_INFO_LOG("BluetoothRendererSink::Init");
    attr_ = attr;

    string adapterNameCase = "bt_a2dp";  // Set sound card information
    enum AudioPortDirection port = PORT_OUT; // Set port information

    if (InitAudioManager() != 0) {
        MEDIA_ERR_LOG("Init audio manager Fail");
        return ERR_NOT_STARTED;
    }

    int32_t size = 0;
    int32_t ret;
    struct AudioAdapterDescriptor *descs = nullptr;
    ret = audioManager_->GetAllAdapters(audioManager_, &descs, &size);
    if (size > MAX_AUDIO_ADAPTER_NUM || size == 0 || descs == nullptr || ret != 0) {
        MEDIA_ERR_LOG("Get adapters Fail");
        return ERR_NOT_STARTED;
    }

    // Get qualified sound card and port
    int32_t index = SwitchAdapter(descs, adapterNameCase, port, audioPort, size);
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

    if (CreateRender(audioPort) != 0) {
        MEDIA_ERR_LOG("Create render faied");
        return ERR_NOT_STARTED;
    }

    rendererInited_ = true;

#ifdef BT_DUMPFILE
    pfd = fopen(g_audioOutTestFilePath, "wb+");
    if (pfd == nullptr) {
        MEDIA_ERR_LOG("Error opening pcm test file!");
    }
#endif // BT_DUMPFILE

    return SUCCESS;
}

int32_t BluetoothRendererSink::RenderFrame(char &data, uint64_t len, uint64_t &writeLen)
{
    MEDIA_INFO_LOG("Bluetooth Render: RenderFrame in");
    int32_t ret;
    if (audioRender_ == nullptr) {
        MEDIA_ERR_LOG("Bluetooth Render Handle is nullptr!");
        return ERR_INVALID_HANDLE;
    }

#ifdef BT_DUMPFILE
    size_t writeResult = fwrite((void*)&data, 1, len, pfd);
    if (writeResult != len) {
        MEDIA_ERR_LOG("Failed to write the file.");
    }
#endif // BT_DUMPFILE

    ret = audioRender_->RenderFrame(audioRender_, (void*)&data, len, &writeLen);
    MEDIA_INFO_LOG("Bluetooth Render: after RenderFrame");
    if (ret != 0) {
        MEDIA_ERR_LOG("A2dp RenderFrame failed ret: %{public}x", ret);
        return ERR_WRITE_FAILED;
    }
    MEDIA_INFO_LOG("Bluetooth Render: RenderFrame SUCCESS");

    return SUCCESS;
}

int32_t BluetoothRendererSink::Start(void)
{
    int32_t ret;

    if (!started_) {
        ret = audioRender_->control.Start(reinterpret_cast<AudioHandle>(audioRender_));
        if (!ret) {
            started_ = true;
            return SUCCESS;
        } else {
            MEDIA_ERR_LOG("BluetoothRendererSink::Start failed!");
            return ERR_NOT_STARTED;
        }
    }

    return SUCCESS;
}

int32_t BluetoothRendererSink::SetVolume(float left, float right)
{
    int32_t ret;
    float volume;

    if (audioRender_ == nullptr) {
        MEDIA_ERR_LOG("BluetoothRendererSink::SetVolume failed audioRender_ null");
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

    ret = audioRender_->volume.SetVolume(reinterpret_cast<AudioHandle>(audioRender_), volume);
    if (ret) {
        MEDIA_ERR_LOG("BluetoothRendererSink::Set volume failed!");
    }

    return ret;
}

int32_t BluetoothRendererSink::GetVolume(float &left, float &right)
{
    left = leftVolume_;
    right = rightVolume_;
    return SUCCESS;
}

int32_t BluetoothRendererSink::GetLatency(uint32_t *latency)
{
    if (audioRender_ == nullptr) {
        MEDIA_ERR_LOG("BluetoothRendererSink: GetLatency failed audio render null");
        return ERR_INVALID_HANDLE;
    }

    if (!latency) {
        MEDIA_ERR_LOG("BluetoothRendererSink: GetLatency failed latency null");
        return ERR_INVALID_PARAM;
    }

    uint32_t hdiLatency;
    if (audioRender_->GetLatency(audioRender_, &hdiLatency) == 0) {
        *latency = hdiLatency;
        return SUCCESS;
    } else {
        return ERR_OPERATION_FAILED;
    }
}

int32_t BluetoothRendererSink::Stop(void)
{
    MEDIA_INFO_LOG("BluetoothRendererSink::Stop in");
    int32_t ret;

    if (audioRender_ == nullptr) {
        MEDIA_ERR_LOG("BluetoothRendererSink::Stop failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    if (started_) {
        MEDIA_INFO_LOG("BluetoothRendererSink::Stop control before");
        ret = audioRender_->control.Stop(reinterpret_cast<AudioHandle>(audioRender_));
        MEDIA_INFO_LOG("BluetoothRendererSink::Stop control after");
        if (!ret) {
            started_ = false;
            return SUCCESS;
        } else {
            MEDIA_ERR_LOG("BluetoothRendererSink::Stop failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return SUCCESS;
}

int32_t BluetoothRendererSink::Pause(void)
{
    int32_t ret;

    if (audioRender_ == nullptr) {
        MEDIA_ERR_LOG("BluetoothRendererSink::Pause failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    if (!started_) {
        MEDIA_ERR_LOG("BluetoothRendererSink::Pause invalid state!");
        return ERR_OPERATION_FAILED;
    }

    if (!paused_) {
        ret = audioRender_->control.Pause(reinterpret_cast<AudioHandle>(audioRender_));
        if (!ret) {
            paused_ = true;
            return SUCCESS;
        } else {
            MEDIA_ERR_LOG("BluetoothRendererSink::Pause failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return SUCCESS;
}

int32_t BluetoothRendererSink::Resume(void)
{
    int32_t ret;

    if (audioRender_ == nullptr) {
        MEDIA_ERR_LOG("BluetoothRendererSink::Resume failed audioRender_ null");
        return ERR_INVALID_HANDLE;
    }

    if (!started_) {
        MEDIA_ERR_LOG("BluetoothRendererSink::Resume invalid state!");
        return ERR_OPERATION_FAILED;
    }

    if (paused_) {
        ret = audioRender_->control.Resume(reinterpret_cast<AudioHandle>(audioRender_));
        if (!ret) {
            paused_ = false;
            return SUCCESS;
        } else {
            MEDIA_ERR_LOG("BluetoothRendererSink::Resume failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return SUCCESS;
}

int32_t BluetoothRendererSink::Reset(void)
{
    int32_t ret;

    if (started_ && audioRender_ != nullptr) {
        ret = audioRender_->control.Flush(reinterpret_cast<AudioHandle>(audioRender_));
        if (!ret) {
            return SUCCESS;
        } else {
            MEDIA_ERR_LOG("BluetoothRendererSink::Reset failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return ERR_OPERATION_FAILED;
}

int32_t BluetoothRendererSink::Flush(void)
{
    int32_t ret;

    if (started_ && audioRender_ != nullptr) {
        ret = audioRender_->control.Flush(reinterpret_cast<AudioHandle>(audioRender_));
        if (!ret) {
            return SUCCESS;
        } else {
            MEDIA_ERR_LOG("BluetoothRendererSink::Flush failed!");
            return ERR_OPERATION_FAILED;
        }
    }

    return ERR_OPERATION_FAILED;
}
} // namespace AudioStandard
} // namespace OHOS

#ifdef __cplusplus
extern "C" {
#endif

using namespace OHOS::AudioStandard;

BluetoothRendererSink *g_bluetoothRendrSinkInstance = BluetoothRendererSink::GetInstance();

int32_t BluetoothRendererSinkInit(AudioSinkAttr *attr)
{
    int32_t ret;

    if (g_bluetoothRendrSinkInstance->rendererInited_)
        return SUCCESS;

    ret = g_bluetoothRendrSinkInstance->Init(*attr);
    return ret;
}

void BluetoothRendererSinkDeInit()
{
    if (g_bluetoothRendrSinkInstance->rendererInited_)
        g_bluetoothRendrSinkInstance->DeInit();
}

int32_t BluetoothRendererSinkStop()
{
    int32_t ret;

    if (!g_bluetoothRendrSinkInstance->rendererInited_)
        return SUCCESS;

    ret = g_bluetoothRendrSinkInstance->Stop();
    return ret;
}

int32_t BluetoothRendererSinkStart()
{
    int32_t ret;

    if (!g_bluetoothRendrSinkInstance->rendererInited_) {
        MEDIA_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    ret = g_bluetoothRendrSinkInstance->Start();
    return ret;
}

int32_t BluetoothRendererRenderFrame(char &data, uint64_t len, uint64_t &writeLen)
{
    int32_t ret;

    if (!g_bluetoothRendrSinkInstance->rendererInited_) {
        MEDIA_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    ret = g_bluetoothRendrSinkInstance->RenderFrame(data, len, writeLen);
    return ret;
}

int32_t BluetoothRendererSinkSetVolume(float left, float right)
{
    int32_t ret;

    if (!g_bluetoothRendrSinkInstance->rendererInited_) {
        MEDIA_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    ret = g_bluetoothRendrSinkInstance->SetVolume(left, right);
    return ret;
}

int32_t BluetoothRendererSinkGetLatency(uint32_t *latency)
{
    int32_t ret;

    if (!g_bluetoothRendrSinkInstance->rendererInited_) {
        MEDIA_ERR_LOG("audioRenderer Not Inited! Init the renderer first\n");
        return ERR_NOT_STARTED;
    }

    if (!latency) {
        MEDIA_ERR_LOG("BluetoothRendererSinkGetLatency failed latency null");
        return ERR_INVALID_PARAM;
    }

    ret = g_bluetoothRendrSinkInstance->GetLatency(latency);
    return ret;
}
#ifdef __cplusplus
}
#endif
