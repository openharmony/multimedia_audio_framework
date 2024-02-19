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

#ifndef OH_AUDIO_RENDERER_H
#define OH_AUDIO_RENDERER_H

#include "native_audiorenderer.h"
#include "audio_renderer.h"
#include "audio_log.h"

namespace OHOS {
namespace AudioStandard {
class OHAudioRendererModeCallback : public AudioRendererWriteCallback {
public:
    OHAudioRendererModeCallback(OH_AudioRenderer_Callbacks callbacks, OH_AudioRenderer* audioRenderer, void* userData)
        : callbacks_(callbacks), ohAudioRenderer_(audioRenderer), userData_(userData)
    {
    }

    void OnWriteData(size_t length) override;

private:
    OH_AudioRenderer_Callbacks callbacks_;
    OH_AudioRenderer* ohAudioRenderer_;
    void* userData_;
};

class OHAudioRendererDeviceChangeCallback : public AudioRendererDeviceChangeCallback {
public:
    OHAudioRendererDeviceChangeCallback(OH_AudioRenderer_Callbacks callbacks, OH_AudioRenderer* audioRenderer,
        void* userData) : callbacks_(callbacks), ohAudioRenderer_(audioRenderer), userData_(userData)
    {
    }

    void OnStateChange(const DeviceInfo &deviceInfo) override;
    void RemoveAllCallbacks() override {};
private:
    OH_AudioRenderer_Callbacks callbacks_;
    OH_AudioRenderer* ohAudioRenderer_;
    void* userData_;
};

class OHAudioRendererDeviceChangeCallbackWithInfo : public AudioRendererOutputDeviceChangeCallback {
public:
    OHAudioRendererDeviceChangeCallbackWithInfo(OH_AudioRenderer_OutputDeviceChangeCallback callback,
        OH_AudioRenderer* audioRenderer, void* userData)
        : callback_(callback), ohAudioRenderer_(audioRenderer), userData_(userData)
    {
    }

    void OnOutputDeviceChange(const DeviceInfo &deviceInfo, const AudioStreamDeviceChangeReason reason) override;
private:
    OH_AudioRenderer_OutputDeviceChangeCallback callback_;
    OH_AudioRenderer* ohAudioRenderer_;
    void* userData_;
};

class OHAudioRendererCallback : public AudioRendererCallback {
public:
    OHAudioRendererCallback(OH_AudioRenderer_Callbacks callbacks, OH_AudioRenderer* audioRenderer,
        void* userData) : callbacks_(callbacks), ohAudioRenderer_(audioRenderer), userData_(userData)
    {
    }
    void OnInterrupt(const InterruptEvent &interruptEvent) override;
    void OnStateChange(const RendererState state, const StateChangeCmdType __attribute__((unused)) cmdType) override
    {
        AUDIO_DEBUG_LOG("OHAudioRendererCallback:: OnStateChange");
    }

private:
    OH_AudioRenderer_Callbacks callbacks_;
    OH_AudioRenderer* ohAudioRenderer_;
    void* userData_;
};

class OHServiceDiedCallback : public AudioRendererPolicyServiceDiedCallback {
public:
    OHServiceDiedCallback(OH_AudioRenderer_Callbacks callbacks, OH_AudioRenderer* audioRenderer,
        void* userData) : callbacks_(callbacks), ohAudioRenderer_(audioRenderer), userData_(userData)
    {
    }

    void OnAudioPolicyServiceDied() override;

private:
    OH_AudioRenderer_Callbacks callbacks_;
    OH_AudioRenderer* ohAudioRenderer_;
    void* userData_;
};

class OHAudioRendererErrorCallback : public AudioRendererErrorCallback {
public:
    OHAudioRendererErrorCallback(OH_AudioRenderer_Callbacks callbacks, OH_AudioRenderer* audioRenderer,
        void* userData) : callbacks_(callbacks), ohAudioRenderer_(audioRenderer), userData_(userData)
    {
    }

    void OnError(AudioErrors errorCode) override;

    OH_AudioStream_Result GetErrorResult(AudioErrors errorCode) const;

private:
    OH_AudioRenderer_Callbacks callbacks_;
    OH_AudioRenderer* ohAudioRenderer_;
    void* userData_;
};
class OHAudioRenderer {
    public:
        OHAudioRenderer();
        ~OHAudioRenderer();
        bool Initialize(const AudioRendererOptions &rendererOptions);
        bool Start();
        bool Pause();
        bool Stop();
        bool Flush();
        bool Release();
        RendererState GetCurrentState();
        void GetStreamId(uint32_t& streamId);
        AudioChannel GetChannelCount();
        int32_t GetSamplingRate();
        AudioSampleFormat GetSampleFormat();
        AudioEncodingType GetEncodingType();
        int64_t GetFramesWritten();
        void GetRendererInfo(AudioRendererInfo& rendererInfo);
        bool GetAudioTime(Timestamp &timestamp, Timestamp::Timestampbase base);
        int32_t GetFrameSizeInCallback();
        int32_t GetBufferDesc(BufferDesc &bufDesc) const;
        int32_t Enqueue(const BufferDesc &bufDesc) const;
        int32_t SetSpeed(float speed);
        float GetSpeed();

        void SetRendererCallback(OH_AudioRenderer_Callbacks callbacks, void* userData);
        void SetPreferredFrameSize(int32_t frameSize);

        void SetRendererOutputDeviceChangeCallback(OH_AudioRenderer_OutputDeviceChangeCallback callback,
            void *userData);
        bool IsFastRenderer();
    private:
        std::unique_ptr<AudioRenderer> audioRenderer_;
        std::shared_ptr<AudioRendererCallback> audioRendererCallback_;
        std::shared_ptr<OHAudioRendererDeviceChangeCallbackWithInfo> audioRendererDeviceChangeCallbackWithInfo_;
};
}  // namespace AudioStandard
}  // namespace OHOS

#endif // OH_AUDIO_RENDERER_H
