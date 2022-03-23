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

#include "audio_capturer.h"
#include "audio_renderer.h"
#include "media_log.h"
#include "pcm2wav.h"

#include <thread>

using namespace std;
using namespace OHOS;
using namespace OHOS::AudioStandard;

namespace AudioTestConstants {
    constexpr int32_t ARGS_INDEX_RENDERER_TEST_PATH = 1;
    constexpr int32_t ARGS_INDEX_CAPTURER_TEST_PATH = 2;
    constexpr int32_t ARGS_COUNT_THREE = 3;
    constexpr int32_t SUCCESS = 0;
}

class AudioVoIPTest {
public:
    bool InitRender(const unique_ptr<AudioRenderer> &audioRenderer, const AudioRendererParams &rendererParams) const
    {
        if (audioRenderer->SetParams(rendererParams) !=  AudioTestConstants::SUCCESS) {
            MEDIA_ERR_LOG("AudioVoIPTest: Set audio renderer parameters failed");
            if (!audioRenderer->Release()) {
                MEDIA_ERR_LOG("AudioVoIPTest: Release failed");
            }
            return false;
        }
        MEDIA_INFO_LOG("AudioVoIPTest: Playback renderer created");

        MEDIA_INFO_LOG("AudioVoIPTest: Starting renderer");
        if (!audioRenderer->Start()) {
            MEDIA_ERR_LOG("AudioVoIPTest: Start failed");
            if (!audioRenderer->Release()) {
                MEDIA_ERR_LOG("AudioVoIPTest: Release failed");
            }
            return false;
        }
        MEDIA_INFO_LOG("AudioVoIPTest: Playback started");

        return true;
    }

    bool StartRender(const unique_ptr<AudioRenderer> &audioRenderer, FILE* wavFile) const
    {
        size_t bufferLen = 0;
        if (audioRenderer->GetBufferSize(bufferLen)) {
            return false;
        }
        MEDIA_DEBUG_LOG("minimum buffer length: %{public}zu", bufferLen);

        int32_t n = 2;
        auto buffer = std::make_unique<uint8_t[]>(n * bufferLen);
        if (buffer == nullptr) {
            MEDIA_ERR_LOG("AudioVoIPTest: Failed to allocate buffer");
            return false;
        }

        size_t bytesToWrite = 0;
        size_t bytesWritten = 0;
        size_t minBytes = 4;

        while (!feof(wavFile)) {
            bytesToWrite = fread(buffer.get(), 1, bufferLen, wavFile);
            bytesWritten = 0;
            MEDIA_INFO_LOG("AudioVoIPTest: Bytes to write: %{public}zu", bytesToWrite);

            while ((bytesWritten < bytesToWrite) && ((bytesToWrite - bytesWritten) > minBytes)) {
                bytesWritten += audioRenderer->Write(buffer.get() + bytesWritten,
                                                     bytesToWrite - bytesWritten);
                MEDIA_INFO_LOG("AudioVoIPTest: Bytes written: %{public}zu", bytesWritten);
                if (bytesWritten < 0) {
                    break;
                }
            }
        }

        if (!audioRenderer->Drain()) {
            MEDIA_ERR_LOG("AudioVoIPTest: Drain failed");
        }

        return true;
    }

    bool TestPlayback(char *inputPath) const
    {
        MEDIA_INFO_LOG("AudioVoIPTest: TestPlayback start ");

        wav_hdr wavHeader;
        size_t headerSize = sizeof(wav_hdr);
        char path[PATH_MAX + 1] = {0x00};
        if ((strlen(inputPath) > PATH_MAX) || (realpath(inputPath, path) == nullptr)) {
            MEDIA_ERR_LOG("Invalid path");
            return false;
        }
        MEDIA_INFO_LOG("AudioVoIPTest: path = %{public}s", path);
        FILE* wavFile = fopen(path, "rb");
        if (wavFile == nullptr) {
            MEDIA_INFO_LOG("AudioVoIPTest: Unable to open wave file");
            return false;
        }
        size_t bytesRead = fread(&wavHeader, 1, headerSize, wavFile);
        MEDIA_INFO_LOG("AudioVoIPTest: Header Read in bytes %{public}zu", bytesRead);

        AudioStreamType streamType = AudioStreamType::STREAM_VOICE_CALL;
        unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(streamType);

        AudioRendererParams rendererParams;
        rendererParams.sampleFormat = static_cast<AudioSampleFormat>(wavHeader.bitsPerSample);
        rendererParams.sampleRate = static_cast<AudioSamplingRate>(wavHeader.SamplesPerSec);
        rendererParams.channelCount = static_cast<AudioChannel>(wavHeader.NumOfChan);
        rendererParams.encodingType = static_cast<AudioEncodingType>(ENCODING_PCM);
        if (!InitRender(audioRenderer, rendererParams)) {
            MEDIA_ERR_LOG("AudioVoIPTest: Init render failed");
            fclose(wavFile);
            return false;
        }

        if (!StartRender(audioRenderer, wavFile)) {
            MEDIA_ERR_LOG("AudioVoIPTest: Start render failed");
            fclose(wavFile);
            return false;
        }

        if (!audioRenderer->Stop()) {
            MEDIA_ERR_LOG("AudioVoIPTest: Stop failed");
        }

        if (!audioRenderer->Release()) {
            MEDIA_ERR_LOG("AudioVoIPTest: Release failed");
        }

        fclose(wavFile);
        MEDIA_INFO_LOG("AudioVoIPTest: TestPlayback end");

        return true;
    }

    bool InitCapture(const unique_ptr<AudioCapturer> &audioCapturer, const AudioCapturerParams &capturerParams) const
    {
        if (audioCapturer->SetParams(capturerParams) != AudioTestConstants::SUCCESS) {
            MEDIA_ERR_LOG("Set audio stream parameters failed");
            audioCapturer->Release();
            return false;
        }
        MEDIA_INFO_LOG("Capture stream created");

        MEDIA_INFO_LOG("Starting Stream");
        if (!audioCapturer->Start()) {
            MEDIA_ERR_LOG("Start stream failed");
            audioCapturer->Release();
            return false;
        }
        MEDIA_INFO_LOG("Capturing started");

        return true;
    }

    bool StartCapture(const unique_ptr<AudioCapturer> &audioCapturer, bool isBlocking, FILE *pFile) const
    {
        size_t bufferLen;
        if (audioCapturer->GetBufferSize(bufferLen) < 0) {
            MEDIA_ERR_LOG(" GetMinimumBufferSize failed");
            return false;
        }

        auto buffer = std::make_unique<uint8_t[]>(bufferLen);
        if (buffer == nullptr) {
            MEDIA_ERR_LOG("AudioVoIPTest: Failed to allocate buffer");
            return false;
        }

        size_t size = 1;
        size_t numBuffersToCapture = 256;
        while (numBuffersToCapture) {
            size_t bytesRead = 0;
            while (bytesRead < bufferLen) {
                int32_t len = audioCapturer->Read(*(buffer.get() + bytesRead), bufferLen - bytesRead, isBlocking);
                if (len >= 0) {
                    bytesRead += len;
                } else {
                    bytesRead = len;
                    break;
                }
            }
            if (bytesRead < 0) {
                MEDIA_ERR_LOG("Bytes read failed. error code %{public}zu", bytesRead);
                break;
            } else if (bytesRead == 0) {
                continue;
            }

            if (fwrite(buffer.get(), size, bytesRead, pFile) != bytesRead) {
                MEDIA_ERR_LOG("error occured in fwrite");
            }
            numBuffersToCapture--;
        }

        return true;
    }

    bool TestRecording(char *capturePath) const
    {
        MEDIA_INFO_LOG("TestCapture start ");

        unique_ptr<AudioCapturer> audioCapturer = AudioCapturer::Create(AudioStreamType::STREAM_VOICE_CALL);

        AudioCapturerParams capturerParams;
        capturerParams.audioSampleFormat = SAMPLE_S16LE;
        capturerParams.samplingRate = SAMPLE_RATE_44100;
        capturerParams.audioChannel = AudioChannel::STEREO;
        capturerParams.audioEncoding = ENCODING_PCM;
        if (!InitCapture(audioCapturer, capturerParams)) {
            MEDIA_ERR_LOG("Initialize capturer failed");
            return false;
        }

        bool isBlocking = true;
        FILE *pFile = fopen(capturePath, "wb");
        if (pFile == nullptr) {
            MEDIA_INFO_LOG("AudioVoIPTest: Unable to open file");
            return false;
        }

        if (!StartCapture(audioCapturer, isBlocking, pFile)) {
            MEDIA_ERR_LOG("Start capturer failed");
            fclose(pFile);
            return false;
        }

        fflush(pFile);
        if (!audioCapturer->Flush()) {
            MEDIA_ERR_LOG("AudioVoIPTest: flush failed");
        }

        if (!audioCapturer->Stop()) {
            MEDIA_ERR_LOG("AudioVoIPTest: Stop failed");
        }

        if (!audioCapturer->Release()) {
            MEDIA_ERR_LOG("AudioVoIPTest: Release failed");
        }
        fclose(pFile);
        MEDIA_INFO_LOG("TestCapture end");

        return true;
    }
};

int main(int argc, char *argv[])
{
    MEDIA_INFO_LOG("AudioVoIPTest: Render test in");

    if ((argv == nullptr) || (argc < AudioTestConstants::ARGS_COUNT_THREE)) {
        MEDIA_ERR_LOG("AudioVoIPTest: argv is null");
        return 0;
    }

    MEDIA_INFO_LOG("AudioVoIPTest: argc=%d", argc);
    MEDIA_INFO_LOG("AudioVoIPTest: renderer test path = %{public}s",
        argv[AudioTestConstants::ARGS_INDEX_RENDERER_TEST_PATH]);
    MEDIA_INFO_LOG("AudioVoIPTest: capturer test path = %{public}s",
        argv[AudioTestConstants::ARGS_INDEX_CAPTURER_TEST_PATH]);

    AudioVoIPTest *renderTestObj = new(std::nothrow) AudioVoIPTest();
    if (!renderTestObj) {
        MEDIA_ERR_LOG("AudioVoIPTest: create renderer object failed");
        return 0;
    }

    AudioVoIPTest *captureTestObj = new(std::nothrow) AudioVoIPTest();
    if (!captureTestObj) {
        MEDIA_ERR_LOG("AudioVoIPTest: create capturer object failed");
        delete renderTestObj;
        renderTestObj = nullptr;
        return 0;
    }

    std::thread renderThread(&AudioVoIPTest::TestPlayback, renderTestObj,
                             argv[AudioTestConstants::ARGS_INDEX_RENDERER_TEST_PATH]);

    std::thread captureThread(&AudioVoIPTest::TestRecording, captureTestObj,
                              argv[AudioTestConstants::ARGS_INDEX_CAPTURER_TEST_PATH]);

    renderThread.join();
    captureThread.join();

    delete renderTestObj;
    renderTestObj = nullptr;
    delete captureTestObj;
    captureTestObj = nullptr;

    return 0;
}
