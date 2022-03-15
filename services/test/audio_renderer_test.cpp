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
#include <vector>

#include "audio_renderer.h"
#include "media_log.h"
#include "pcm2wav.h"

using namespace std;
using namespace OHOS;
using namespace OHOS::AudioStandard;

namespace {
    constexpr int32_t ARGS_INDEX_THREE = 3;
    constexpr int32_t ARGS_INDEX_TWO = 2;
    constexpr int32_t ARGS_COUNT_TWO = 2;
    constexpr int32_t ARGS_COUNT_THREE = 3;
    constexpr int32_t ARGS_COUNT_FOUR = 4;
    constexpr int32_t SUCCESS = 0;
    constexpr int32_t STOP_BUFFER_POSITION = 700000;
    constexpr int32_t PAUSE_BUFFER_POSITION = 1400000;
    constexpr int32_t PAUSE_RENDER_TIME_SECONDS = 1;
    constexpr int32_t STOP_RENDER_TIME_SECONDS = 1;
    constexpr float TRACK_VOLUME = 0.2f;
}

class AudioRendererCallbackTestImpl : public AudioRendererCallback {
public:
    void OnInterrupt(const InterruptEvent &interruptEvent) override {}
    void OnStateChange(const RendererState state) override
    {
        MEDIA_DEBUG_LOG("AudioRendererCallbackTestImpl:: OnStateChange");

        switch (state) {
            case RENDERER_PREPARED:
                MEDIA_DEBUG_LOG("AudioRendererCallbackTestImpl: OnStateChange RENDERER_PREPARED");
                break;
            case RENDERER_RUNNING:
                MEDIA_DEBUG_LOG("AudioRendererCallbackTestImpl: OnStateChange RENDERER_RUNNING");
                break;
            case RENDERER_STOPPED:
                MEDIA_DEBUG_LOG("AudioRendererCallbackTestImpl: OnStateChange RENDERER_STOPPED");
                break;
            case RENDERER_PAUSED:
                MEDIA_DEBUG_LOG("AudioRendererCallbackTestImpl: OnStateChange RENDERER_PAUSED");
                break;
            case RENDERER_RELEASED:
                MEDIA_DEBUG_LOG("AudioRendererCallbackTestImpl: OnStateChange RENDERER_RELEASED");
                break;
            default:
                MEDIA_ERR_LOG("AudioRendererCallbackTestImpl: OnStateChange NOT A VALID state");
                break;
        }
    }
};

class AudioRendererTest {
public:
    void CheckSupportedParams() const
    {
        vector<AudioSampleFormat> supportedFormatList = AudioRenderer::GetSupportedFormats();
        MEDIA_INFO_LOG("AudioRendererTest: Supported formats:");
        for (auto i = supportedFormatList.begin(); i != supportedFormatList.end(); ++i) {
            MEDIA_INFO_LOG("AudioRendererTest: Format %{public}d", *i);
        }

        vector<AudioChannel> supportedChannelList = AudioRenderer::GetSupportedChannels();
        MEDIA_INFO_LOG("AudioRendererTest: Supported channels:");
        for (auto i = supportedChannelList.begin(); i != supportedChannelList.end(); ++i) {
            MEDIA_INFO_LOG("AudioRendererTest: channel %{public}d", *i);
        }

        vector<AudioEncodingType> supportedEncodingTypes
                                    = AudioRenderer::GetSupportedEncodingTypes();
        MEDIA_INFO_LOG("AudioRendererTest: Supported encoding types:");
        for (auto i = supportedEncodingTypes.begin(); i != supportedEncodingTypes.end(); ++i) {
            MEDIA_INFO_LOG("AudioRendererTest: encoding type %{public}d", *i);
        }

        vector<AudioSamplingRate> supportedSamplingRates = AudioRenderer::GetSupportedSamplingRates();
        MEDIA_INFO_LOG("AudioRendererTest: Supported sampling rates:");
        for (auto i = supportedSamplingRates.begin(); i != supportedSamplingRates.end(); ++i) {
            MEDIA_INFO_LOG("AudioRendererTest: sampling rate %{public}d", *i);
        }
    }

    void GetRendererStreamInfo(const unique_ptr<AudioRenderer> &audioRenderer) const
    {
        MEDIA_INFO_LOG("AudioRendererTest: GetRendererInfo:");
        AudioRendererInfo rendererInfo;
        if (audioRenderer->GetRendererInfo(rendererInfo) ==  SUCCESS) {
            MEDIA_INFO_LOG("AudioRendererTest: Get ContentType: %{public}d", rendererInfo.contentType);
            MEDIA_INFO_LOG("AudioRendererTest: Get StreamUsage: %{public}d", rendererInfo.streamUsage);
        } else {
            MEDIA_ERR_LOG("AudioRendererTest: GetStreamInfo failed");
        }

        MEDIA_INFO_LOG("AudioRendererTest: GetStreamInfo:");
        AudioStreamInfo streamInfo;
        if (audioRenderer->GetStreamInfo(streamInfo) ==  SUCCESS) {
            MEDIA_INFO_LOG("AudioRendererTest: Get AudioSamplingRate: %{public}d", streamInfo.samplingRate);
            MEDIA_INFO_LOG("AudioRendererTest: Get AudioEncodingType: %{public}d", streamInfo.encoding);
            MEDIA_INFO_LOG("AudioRendererTest: Get AudioSampleFormat: %{public}d", streamInfo.format);
            MEDIA_INFO_LOG("AudioRendererTest: Get AudioChannel: %{public}d", streamInfo.channels);
        } else {
            MEDIA_ERR_LOG("AudioRendererTest: GetStreamInfo failed");
        }
    }

    bool InitRender(const unique_ptr<AudioRenderer> &audioRenderer) const
    {
        MEDIA_INFO_LOG("AudioRendererTest: Starting renderer");
        if (!audioRenderer->Start()) {
            MEDIA_ERR_LOG("AudioRendererTest: Start failed");
            if (!audioRenderer->Release()) {
                MEDIA_ERR_LOG("AudioRendererTest: Release failed");
            }
            return false;
        }
        MEDIA_INFO_LOG("AudioRendererTest: Playback started");

        if (audioRenderer->SetVolume(TRACK_VOLUME) == SUCCESS) {
            MEDIA_INFO_LOG("AudioRendererTest: volume set to: %{public}f", audioRenderer->GetVolume());
        }

        return true;
    }

    bool TestPauseStop(const unique_ptr<AudioRenderer> &audioRenderer, bool &pauseTested, bool &stopTested,
                       FILE &wavFile) const
    {
        uint64_t currFilePos = ftell(&wavFile);
        if (!stopTested && (currFilePos > STOP_BUFFER_POSITION) && audioRenderer->Stop()) {
            stopTested = true;
            sleep(STOP_RENDER_TIME_SECONDS);
            MEDIA_INFO_LOG("Audio render resume");
            if (!audioRenderer->Start()) {
                MEDIA_ERR_LOG("resume stream failed");
                return false;
            }
        } else if (!pauseTested && (currFilePos > PAUSE_BUFFER_POSITION)
                   && audioRenderer->Pause()) {
            pauseTested = true;
            sleep(PAUSE_RENDER_TIME_SECONDS);
            MEDIA_INFO_LOG("Audio render resume");
            if (audioRenderer->SetVolume(1.0) == SUCCESS) {
                MEDIA_INFO_LOG("AudioRendererTest: after resume volume set to: %{public}f",
                               audioRenderer->GetVolume());
            }
            if (!audioRenderer->Flush()) {
                MEDIA_ERR_LOG("AudioRendererTest: flush failed");
                return false;
            }
            if (!audioRenderer->Start()) {
                MEDIA_ERR_LOG("resume stream failed");
                return false;
            }
        }

        return true;
    }

    bool GetBufferLen(const unique_ptr<AudioRenderer> &audioRenderer, size_t &bufferLen) const
    {
        if (audioRenderer->GetBufferSize(bufferLen)) {
            return false;
        }
        MEDIA_DEBUG_LOG("minimum buffer length: %{public}zu", bufferLen);

        uint32_t frameCount;
        if (audioRenderer->GetFrameCount(frameCount)) {
            return false;
        }
        MEDIA_INFO_LOG("AudioRendererTest: Frame count: %{public}d", frameCount);
        return true;
    }

    bool StartRender(const unique_ptr<AudioRenderer> &audioRenderer, FILE* wavFile) const
    {
        size_t bufferLen = 0;
        if (!GetBufferLen(audioRenderer, bufferLen)) {
            return false;
        }

        int32_t n = 2;
        auto buffer = std::make_unique<uint8_t[]>(n * bufferLen);
        if (buffer == nullptr) {
            MEDIA_ERR_LOG("AudioRendererTest: Failed to allocate buffer");
            return false;
        }

        size_t bytesToWrite = 0;
        size_t bytesWritten = 0;
        size_t minBytes = 4;
        uint64_t latency;
        bool stopTested = false;
        bool pauseTested = false;

        while (!feof(wavFile)) {
            bytesToWrite = fread(buffer.get(), 1, bufferLen, wavFile);
            bytesWritten = 0;
            MEDIA_INFO_LOG("AudioRendererTest: Bytes to write: %{public}zu", bytesToWrite);

            if (!TestPauseStop(audioRenderer, pauseTested, stopTested, *wavFile)) {
                break;
            }

            if (audioRenderer->GetLatency(latency)) {
                MEDIA_ERR_LOG("AudioRendererTest: GetLatency failed");
                break;
            }

            while ((bytesWritten < bytesToWrite) && ((bytesToWrite - bytesWritten) > minBytes)) {
                bytesWritten += audioRenderer->Write(buffer.get() + bytesWritten,
                                                     bytesToWrite - bytesWritten);
                MEDIA_INFO_LOG("AudioRendererTest: Bytes written: %{public}zu", bytesWritten);
                if (bytesWritten < 0) {
                    break;
                }
            }
        }

        if (!audioRenderer->Drain()) {
            MEDIA_ERR_LOG("AudioRendererTest: Drain failed");
        }

        return true;
    }

    bool TestPlayback(int argc, char *argv[]) const
    {
        MEDIA_INFO_LOG("AudioRendererTest: TestPlayback start ");

        int numBase = 10;
        wav_hdr wavHeader;
        size_t headerSize = sizeof(wav_hdr);
        char *inputPath = argv[1];
        char path[PATH_MAX + 1] = {0x00};
        if ((strlen(inputPath) > PATH_MAX) || (realpath(inputPath, path) == nullptr)) {
            MEDIA_ERR_LOG("Invalid path");
            return false;
        }
        MEDIA_INFO_LOG("AudioRendererTest: path = %{public}s", path);
        FILE* wavFile = fopen(path, "rb");
        if (wavFile == nullptr) {
            MEDIA_INFO_LOG("AudioRendererTest: Unable to open wave file");
            return false;
        }
        size_t bytesRead = fread(&wavHeader, 1, headerSize, wavFile);
        MEDIA_INFO_LOG("AudioRendererTest: Header Read in bytes %{public}zu", bytesRead);

        ContentType contentType = ContentType::CONTENT_TYPE_MUSIC;
        StreamUsage streamUsage = StreamUsage::STREAM_USAGE_MEDIA;

        if (argc > ARGS_COUNT_THREE) {
            contentType = static_cast<ContentType>(strtol(argv[ARGS_INDEX_TWO], NULL, numBase));
            streamUsage = static_cast<StreamUsage>(strtol(argv[ARGS_INDEX_THREE], NULL, numBase));
        }
        int32_t bufferMsec = 0;
        if (argc > ARGS_COUNT_FOUR) {
            bufferMsec = strtol(argv[ARGS_COUNT_FOUR], NULL, numBase);
        }

        AudioRendererOptions rendererOptions = {};
        rendererOptions.streamInfo.encoding = AudioEncodingType::ENCODING_PCM;
        rendererOptions.streamInfo.samplingRate = static_cast<AudioSamplingRate>(wavHeader.SamplesPerSec);
        rendererOptions.streamInfo.format = static_cast<AudioSampleFormat>(wavHeader.bitsPerSample);
        rendererOptions.streamInfo.channels = static_cast<AudioChannel>(wavHeader.NumOfChan);
        rendererOptions.rendererInfo.contentType = contentType;
        rendererOptions.rendererInfo.streamUsage = streamUsage;
        rendererOptions.rendererInfo.rendererFlags = 0;

        unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(rendererOptions);

        if (audioRenderer == nullptr) {
            MEDIA_ERR_LOG("AudioRendererTest: Create failed");
            fclose(wavFile);
            return false;
        }

        int32_t ret = 0;
        shared_ptr<AudioRendererCallback> cb1 = make_shared<AudioRendererCallbackTestImpl>();
        ret = audioRenderer->SetRendererCallback(cb1);
        if (ret) {
            MEDIA_ERR_LOG("AudioRendererTest: SetRendererCallback failed %{public}d", ret);
            fclose(wavFile);
            return false;
        }

        GetRendererStreamInfo(audioRenderer);

        CheckSupportedParams();
        MEDIA_ERR_LOG("AudioRendererTest: buffermsec = %{public}d", bufferMsec);
        int32_t status = audioRenderer->SetBufferDuration(bufferMsec);
        if (status) {
            MEDIA_ERR_LOG("Failed to set buffer duration");
        }

        if (!InitRender(audioRenderer)) {
            MEDIA_ERR_LOG("AudioRendererTest: Init render failed");
            fclose(wavFile);
            return false;
        }

        if (!StartRender(audioRenderer, wavFile)) {
            MEDIA_ERR_LOG("AudioRendererTest: Start render failed");
            fclose(wavFile);
            return false;
        }

        if (!audioRenderer->Stop()) {
            MEDIA_ERR_LOG("AudioRendererTest: Stop failed");
        }

        if (!audioRenderer->Release()) {
            MEDIA_ERR_LOG("AudioRendererTest: Release failed");
        }

        fclose(wavFile);
        MEDIA_INFO_LOG("AudioRendererTest: TestPlayback end");

        return true;
    }
};

int main(int argc, char *argv[])
{
    MEDIA_INFO_LOG("AudioRendererTest: Render test in");

    if (argv == nullptr) {
        MEDIA_ERR_LOG("AudioRendererTest: argv is null");
        return 0;
    }

    if (argc < ARGS_COUNT_TWO || argc == ARGS_COUNT_THREE) {
        MEDIA_ERR_LOG("AudioRendererTest: incorrect argc. Enter either 2 or 4 args");
        return 0;
    }

    MEDIA_INFO_LOG("AudioRendererTest: argc=%{public}d", argc);
    MEDIA_INFO_LOG("file path argv[1]=%{public}s", argv[1]);

    AudioRendererTest testObj;
    bool ret = testObj.TestPlayback(argc, argv);

    return ret;
}
