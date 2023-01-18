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

#include "audio_interrupt_test.h"
#include "media_log.h"
#include "pcm2wav.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
void AudioInterruptTest::OnStateChange(const RendererState state)
{
    MEDIA_DEBUG_LOG("AudioInterruptTest:: OnStateChange");

    switch (state) {
        case RENDERER_PREPARED:
            MEDIA_DEBUG_LOG("AudioInterruptTest: OnStateChange RENDERER_PREPARED");
            break;
        case RENDERER_RUNNING:
            MEDIA_DEBUG_LOG("AudioInterruptTest: OnStateChange RENDERER_RUNNING");
            break;
        case RENDERER_STOPPED:
            MEDIA_DEBUG_LOG("AudioInterruptTest: OnStateChange RENDERER_STOPPED");
            break;
        case RENDERER_PAUSED:
            MEDIA_DEBUG_LOG("AudioInterruptTest: OnStateChange RENDERER_PAUSED");
            break;
        case RENDERER_RELEASED:
            MEDIA_DEBUG_LOG("AudioInterruptTest: OnStateChange RENDERER_RELEASED");
            break;
        default:
            MEDIA_ERR_LOG("AudioInterruptTest: OnStateChange NOT A VALID state");
            break;
    }
}

void AudioInterruptTest::OnInterrupt(const InterruptEvent &interruptEvent)
{
    MEDIA_DEBUG_LOG("AudioInterruptTest:  OnInterrupt");
    MEDIA_DEBUG_LOG("AudioInterruptTest: interrupt hintType: %{public}d", interruptEvent.hintType);

    if (interruptEvent.forceType == INTERRUPT_FORCE) {
        switch (interruptEvent.hintType) {
            case INTERRUPT_HINT_PAUSE:
                MEDIA_DEBUG_LOG("AudioInterruptTest: ForcePaused Pause Writing");
                isRenderPaused_ = true;
                break;
            case INTERRUPT_HINT_STOP:
                MEDIA_DEBUG_LOG("AudioInterruptTest: ForceStopped Stop Writing");
                isRenderStopped_ = true;
                break;
            case INTERRUPT_HINT_DUCK:
                MEDIA_INFO_LOG("AudioInterruptTest: force INTERRUPT_HINT_DUCK received");
                break;
            case INTERRUPT_HINT_UNDUCK:
                MEDIA_INFO_LOG("AudioInterruptTest: force INTERRUPT_HINT_UNDUCK received");
                break;
            default:
                MEDIA_ERR_LOG("AudioInterruptTest: OnInterrupt NOT A VALID force HINT");
                break;
        }
    } else if  (interruptEvent.forceType == INTERRUPT_SHARE) {
        switch (interruptEvent.hintType) {
            case INTERRUPT_HINT_PAUSE:
                MEDIA_DEBUG_LOG("AudioInterruptTest: SharePause Hint received, Do pause if required");
                break;
            case INTERRUPT_HINT_RESUME:
                MEDIA_DEBUG_LOG("AudioInterruptTest: Do ShareResume");
                if (audioRenderer_ == nullptr) {
                    MEDIA_DEBUG_LOG("AudioInterruptTest: OnInterrupt audioRenderer_ nullptr return");
                    return;
                }
                if (audioRenderer_->Start()) {
                    MEDIA_DEBUG_LOG("AudioInterruptTest: Resume Success");
                    isRenderPaused_ = false;
                } else {
                    MEDIA_DEBUG_LOG("AudioInterruptTest: Resume Failed");
                }
                break;
            default:
                MEDIA_ERR_LOG("AudioInterruptTest: OnInterrupt default share hint case");
                break;
        }
    }
}

bool AudioInterruptTest::GetBufferLen(size_t &bufferLen) const
{
    if (audioRenderer_->GetBufferSize(bufferLen)) {
        return false;
    }
    MEDIA_DEBUG_LOG("minimum buffer length: %{public}zu", bufferLen);

    return true;
}

void AudioInterruptTest::WriteBuffer()
{
    size_t bufferLen = 0;
    if (!GetBufferLen(bufferLen)) {
        isRenderingCompleted_ = true;
        return;
    }

    int32_t n = 2;
    auto buffer = std::make_unique<uint8_t[]>(n * bufferLen);
    if (buffer == nullptr) {
        MEDIA_ERR_LOG("AudioInterruptTest: Failed to allocate buffer");
        isRenderingCompleted_ = true;
        return;
    }

    size_t bytesToWrite = 0;
    size_t bytesWritten = 0;
    size_t minBytes = 4;
    while (true) {
        while (!feof(wavFile_) &&
               !isRenderPaused_ && !isRenderStopped_ && !isStopInProgress_) {
            bytesToWrite = fread(buffer.get(), 1, bufferLen, wavFile_);
            bytesWritten = 0;
            MEDIA_INFO_LOG("AudioInterruptTest: Bytes to write: %{public}zu", bytesToWrite);

            while ((bytesWritten < bytesToWrite) && ((bytesToWrite - bytesWritten) > minBytes)) {
                int32_t retBytes = audioRenderer_->Write(buffer.get() + bytesWritten,
                                                         bytesToWrite - bytesWritten);
                MEDIA_INFO_LOG("AudioInterruptTest: Bytes written: %{public}zu", bytesWritten);
                if (retBytes < 0) {
                    if (audioRenderer_->GetStatus() == RENDERER_PAUSED) {
                        isRenderPaused_ = true;
                        int32_t seekPos = bytesWritten - bytesToWrite;
                        if (fseek(wavFile_, seekPos, SEEK_CUR)) {
                            MEDIA_INFO_LOG("AudioInterruptTest: fseek failed");
                        }
                        MEDIA_INFO_LOG("AudioInterruptTest: fseek success");
                    }
                    break;
                }
                bytesWritten += retBytes;
            }
        }

        if (feof(wavFile_)) {
            MEDIA_INFO_LOG("AudioInterruptTest: EOF set isRenderingCompleted_ true ");
            isRenderingCompleted_ = true;
            break;
        }

        if (isRenderStopped_) {
            MEDIA_INFO_LOG("AudioInterruptTest: Renderer stopping, complete writing ");
            break;
        }
    }
}

bool AudioInterruptTest::StartRender()
{
    MEDIA_INFO_LOG("AudioInterruptTest: Starting renderer");
    if (!audioRenderer_->Start()) {
        MEDIA_ERR_LOG("AudioInterruptTest: Start failed");
        if (!audioRenderer_->Release()) {
            MEDIA_ERR_LOG("AudioInterruptTest: Release failed");
        }
        isRenderingCompleted_ = true;
        return false;
    }
    MEDIA_INFO_LOG("AudioInterruptTest: Playback started");
    return true;
}

bool AudioInterruptTest::InitRender()
{
    wav_hdr wavHeader;
    size_t headerSize = sizeof(wav_hdr);
    size_t bytesRead = fread(&wavHeader, 1, headerSize, wavFile_);
    if (bytesRead != headerSize) {
        MEDIA_ERR_LOG("AudioInterruptTest: File header reading error");
        return false;
    }

    AudioRendererOptions rendererOptions = {};
    rendererOptions.streamInfo.encoding = AudioEncodingType::ENCODING_PCM;
    rendererOptions.streamInfo.samplingRate = static_cast<AudioSamplingRate>(wavHeader.SamplesPerSec);
    rendererOptions.streamInfo.format = static_cast<AudioSampleFormat>(wavHeader.bitsPerSample);
    rendererOptions.streamInfo.channels = static_cast<AudioChannel>(wavHeader.NumOfChan);
    rendererOptions.rendererInfo.contentType = contentType_;
    rendererOptions.rendererInfo.streamUsage = streamUsage_;
    rendererOptions.rendererInfo.rendererFlags = 0;

    audioRenderer_ = AudioRenderer::Create(rendererOptions);
    if (audioRenderer_ == nullptr) {
        MEDIA_INFO_LOG("AudioInterruptTest: Renderer create failed");
        return false;
    }
    MEDIA_INFO_LOG("AudioInterruptTest: Playback renderer created");

    return true;
}

int32_t AudioInterruptTest::TestPlayback()
{
    MEDIA_INFO_LOG("AudioInterruptTest: TestPlayback start ");
    if (!InitRender()) {
        fclose(wavFile_);
        return -1;
    }

    std::shared_ptr<AudioRendererCallback> audioRendererCB = GetPtr();
    int32_t ret = audioRenderer_->SetRendererCallback(audioRendererCB);
    MEDIA_DEBUG_LOG("AudioInterruptTest: SetRendererCallback result : %{public}d", ret);
    if (ret) {
        MEDIA_ERR_LOG("AudioInterruptTest: SetRendererCallback failed : %{public}d", ret);
        fclose(wavFile_);
        return -1;
    }

    if (!StartRender()) {
        fclose(wavFile_);
        return -1;
    }

    writeThread_ = std::make_unique<std::thread>(&AudioInterruptTest::WriteBuffer, this);
    writeThread_->join();

    if (audioRenderer_->GetStatus() == RENDERER_RUNNING) {
        MEDIA_DEBUG_LOG("AudioInterruptTest: StopRender");
        if (audioRenderer_->Stop()) {
            MEDIA_ERR_LOG("AudioInterruptTest: Stop Success");
        } else {
            MEDIA_DEBUG_LOG("AudioInterruptTest: Stop Failed");
        }
    }

    if (!audioRenderer_->Release()) {
        MEDIA_ERR_LOG("AudioInterruptTest: Release failed");
    }

    fclose(wavFile_);
    wavFile_ = nullptr;

    MEDIA_INFO_LOG("AudioInterruptTest: TestPlayback end");

    return 0;
}

void AudioInterruptTest::SaveStreamInfo(ContentType contentType, StreamUsage streamUsage)
{
    contentType_ = contentType;
    streamUsage_ = streamUsage;
}
} // namespace AudioStandard
} // namespace OHOS

using namespace OHOS;
using namespace OHOS::AudioStandard;

int main(int argc, char *argv[])
{
    MEDIA_INFO_LOG("AudioInterruptTest: Render test in");
    constexpr int32_t minNumOfArgs = 2;
    constexpr int32_t argIndexTwo = 2;
    constexpr int32_t argIndexThree = 3;

    if (argv == nullptr) {
        MEDIA_ERR_LOG("AudioInterruptTest: argv is null");
        return 0;
    }

    if (argc < minNumOfArgs || argc == minNumOfArgs + 1) {
        MEDIA_ERR_LOG("AudioInterruptTest: incorrect argc. Enter either 2 or 4 args");
        return 0;
    }

    MEDIA_INFO_LOG("AudioInterruptTest: argc=%d", argc);
    MEDIA_INFO_LOG("AudioInterruptTest: argv[1]=%{public}s", argv[1]);

    int numBase = 10;
    char *inputPath = argv[1];
    char path[PATH_MAX + 1] = {0x00};
    if ((strlen(inputPath) > PATH_MAX) || (realpath(inputPath, path) == nullptr)) {
        MEDIA_ERR_LOG("AudioInterruptTest: Invalid input filepath");
        return -1;
    }
    MEDIA_INFO_LOG("AudioInterruptTest: path = %{public}s", path);

    auto audioInterruptTest = std::make_shared<AudioInterruptTest>();

    audioInterruptTest->wavFile_ = fopen(path, "rb");
    if (audioInterruptTest->wavFile_ == nullptr) {
        MEDIA_INFO_LOG("AudioInterruptTest: Unable to open wave file");
        return -1;
    }

    if (argc > minNumOfArgs + 1) { // argc = 4
        ContentType contentType = static_cast<ContentType>(strtol(argv[argIndexTwo], NULL, numBase));
        StreamUsage streamUsage = static_cast<StreamUsage>(strtol(argv[argIndexThree], NULL, numBase));
        audioInterruptTest->SaveStreamInfo(contentType, streamUsage);
    }

    int32_t ret = audioInterruptTest->TestPlayback();

    return ret;
}
