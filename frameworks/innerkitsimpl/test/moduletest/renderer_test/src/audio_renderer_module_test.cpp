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

#include "audio_renderer_module_test.h"

#include "audio_renderer.h"

using namespace std;
using namespace OHOS::AudioStandard;
using namespace testing::ext;

namespace RendererModuleTestConstants {
    const char *AUDIO_TEST_FILE_PATH = "/data/test_44100_2.wav";
    const unsigned int ZERO = 0;
    // Writing only 500 KB of data for test
    const uint64_t WRITE_SIZE_LIMIT = 500 * 1024;
}

void AudioRendererModuleTest::SetUpTestCase(void) {}
void AudioRendererModuleTest::TearDownTestCase(void) {}
void AudioRendererModuleTest::SetUp(void) {}
void AudioRendererModuleTest::TearDown(void) {}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Get supported formats
 */
HWTEST(AudioRendererModuleTest, audio_renderer_get_supported_formats_001, TestSize.Level1)
{
    vector<AudioSampleFormat> supportedFormatList = AudioRenderer::GetSupportedFormats();
    EXPECT_EQ(AUDIO_SUPPORTED_FORMATS.size(), supportedFormatList.size());
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Get supported channels
 */
HWTEST(AudioRendererModuleTest, audio_renderer_get_supported_channels_001, TestSize.Level1)
{
    vector<AudioChannel> supportedChannelList = AudioRenderer::GetSupportedChannels();
    EXPECT_EQ(AUDIO_SUPPORTED_CHANNELS.size(), supportedChannelList.size());
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Get supported channels
 */
HWTEST(AudioRendererModuleTest, audio_renderer_get_supported_encoding_types_001, TestSize.Level1)
{
    vector<AudioEncodingType> supportedEncodingTypes
                                        = AudioRenderer::GetSupportedEncodingTypes();
    EXPECT_EQ(AUDIO_SUPPORTED_ENCODING_TYPES.size(), supportedEncodingTypes.size());
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Get supported sampling rates
 */
HWTEST(AudioRendererModuleTest, audio_renderer_get_supported_sampling_rates_001, TestSize.Level1)
{
    vector<AudioSamplingRate> supportedSamplingRates = AudioRenderer::GetSupportedSamplingRates();
    EXPECT_EQ(AUDIO_SUPPORTED_SAMPLING_RATES.size(), supportedSamplingRates.size());
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Create renderer instance
 */
HWTEST(AudioRendererModuleTest, audio_renderer_create_001, TestSize.Level1)
{
    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRenderer);
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Set renderer parameters
 */
HWTEST(AudioRendererModuleTest, audio_renderer_set_params_001, TestSize.Level1)
{
    int32_t ret = 0;
    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = AudioSampleFormat::SAMPLE_S16LE;
    rendererParams.sampleRate = AudioSamplingRate::SAMPLE_RATE_44100;
    rendererParams.channelCount = AudioChannel::STEREO;
    rendererParams.encodingType = AudioEncodingType::ENCODING_PCM;

    ret = audioRenderer->SetParams(rendererParams);
    EXPECT_EQ(0, ret);
    audioRenderer->Release();
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Get renderer parameters
 */
HWTEST(AudioRendererModuleTest, audio_renderer_get_params_001, TestSize.Level1)
{
    int32_t ret = 0;
    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = AudioSampleFormat::SAMPLE_S16LE;
    rendererParams.sampleRate = AudioSamplingRate::SAMPLE_RATE_44100;
    rendererParams.channelCount = AudioChannel::STEREO;
    rendererParams.encodingType = AudioEncodingType::ENCODING_PCM;

    ret = audioRenderer->SetParams(rendererParams);
    EXPECT_EQ(0, ret);

    AudioRendererParams paRendererParams;
    ret = audioRenderer->GetParams(paRendererParams);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(paRendererParams.sampleFormat, rendererParams.sampleFormat);
    EXPECT_EQ(paRendererParams.sampleRate, rendererParams.sampleRate);
    EXPECT_EQ(paRendererParams.channelCount, rendererParams.channelCount);
    EXPECT_EQ(paRendererParams.encodingType, rendererParams.encodingType);

    audioRenderer->Release();
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Get buffer size
 */
HWTEST(AudioRendererModuleTest, audio_renderer_get_buffer_size_001, TestSize.Level1)
{
    int32_t ret = 0;
    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = AudioSampleFormat::SAMPLE_S16LE;
    rendererParams.sampleRate = AudioSamplingRate::SAMPLE_RATE_44100;
    rendererParams.channelCount = AudioChannel::STEREO;
    rendererParams.encodingType = AudioEncodingType::ENCODING_PCM;

    ret = audioRenderer->SetParams(rendererParams);
    EXPECT_EQ(0, ret);
    size_t bufferLen;
    ret = audioRenderer->GetBufferSize(bufferLen);
    EXPECT_EQ(0, ret);

    audioRenderer->Release();
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Get frame count
 */
HWTEST(AudioRendererModuleTest, audio_renderer_get_frame_count_001, TestSize.Level1)
{
    int32_t ret = 0;
    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = AudioSampleFormat::SAMPLE_S16LE;
    rendererParams.sampleRate = AudioSamplingRate::SAMPLE_RATE_44100;
    rendererParams.channelCount = AudioChannel::STEREO;
    rendererParams.encodingType = AudioEncodingType::ENCODING_PCM;

    ret = audioRenderer->SetParams(rendererParams);
    EXPECT_EQ(0, ret);
    uint32_t frameCount;
    ret = audioRenderer->GetFrameCount(frameCount);
    EXPECT_EQ(0, ret);

    audioRenderer->Release();
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Start audio renderer
 */
HWTEST(AudioRendererModuleTest, audio_renderer_start_001, TestSize.Level1)
{
    int32_t ret = 0;
    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = AudioSampleFormat::SAMPLE_S16LE;
    rendererParams.sampleRate = AudioSamplingRate::SAMPLE_RATE_44100;
    rendererParams.channelCount = AudioChannel::STEREO;
    rendererParams.encodingType = AudioEncodingType::ENCODING_PCM;

    ret = audioRenderer->SetParams(rendererParams);
    EXPECT_EQ(0, ret);
    bool isStarted = audioRenderer->Start();
    EXPECT_EQ(true, isStarted);

    audioRenderer->Release();
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: write audio renderer
 */
HWTEST(AudioRendererModuleTest, audio_renderer_write_001, TestSize.Level1)
{
    int32_t ret = 0;

    FILE *wavFile = fopen(RendererModuleTestConstants::AUDIO_TEST_FILE_PATH, "rb");
    EXPECT_NE(nullptr, wavFile);

    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = AudioSampleFormat::SAMPLE_S16LE;
    rendererParams.sampleRate = AudioSamplingRate::SAMPLE_RATE_44100;
    rendererParams.channelCount = AudioChannel::STEREO;
    rendererParams.encodingType = AudioEncodingType::ENCODING_PCM;

    ret = audioRenderer->SetParams(rendererParams);
    EXPECT_EQ(0, ret);
    bool isStarted = audioRenderer->Start();
    EXPECT_EQ(true, isStarted);
    size_t bufferLen;
    ret = audioRenderer->GetBufferSize(bufferLen);
    EXPECT_EQ(0, ret);

    uint8_t *buffer = nullptr;
    int32_t n = 2;
    buffer = (uint8_t *) malloc(n * bufferLen);
    size_t bytesToWrite = 0;
    size_t bytesWritten = 0;
    size_t minBytes = 4;
    size_t tempBytesWritten = 0;
    size_t totalBytesWritten = 0;
    while (!feof(wavFile)) {
        bytesToWrite = fread(buffer, 1, bufferLen, wavFile);
        bytesWritten = 0;
        while ((bytesWritten < bytesToWrite) && ((bytesToWrite - bytesWritten) > minBytes)) {
            tempBytesWritten = audioRenderer->Write(buffer + bytesWritten,
                                                 bytesToWrite - bytesWritten);
            EXPECT_GE(tempBytesWritten, RendererModuleTestConstants::ZERO);
            bytesWritten += tempBytesWritten;
        }
        totalBytesWritten += bytesWritten;
        if (totalBytesWritten >= RendererModuleTestConstants::WRITE_SIZE_LIMIT) {
            break;
        }
    }

    audioRenderer->Drain();
    audioRenderer->Release();

    free(buffer);
    fclose(wavFile);
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Get audio time
 */
HWTEST(AudioRendererModuleTest, audio_renderer_get_audio_time_001, TestSize.Level1)
{
    int32_t ret = 0;

    FILE *wavFile = fopen(RendererModuleTestConstants::AUDIO_TEST_FILE_PATH, "rb");
    EXPECT_NE(nullptr, wavFile);

    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = AudioSampleFormat::SAMPLE_S16LE;
    rendererParams.sampleRate = AudioSamplingRate::SAMPLE_RATE_44100;
    rendererParams.channelCount = AudioChannel::STEREO;
    rendererParams.encodingType = AudioEncodingType::ENCODING_PCM;

    ret = audioRenderer->SetParams(rendererParams);
    EXPECT_EQ(0, ret);
    bool isStarted = audioRenderer->Start();
    EXPECT_EQ(true, isStarted);
    size_t bufferLen;
    ret = audioRenderer->GetBufferSize(bufferLen);
    EXPECT_EQ(0, ret);

    uint8_t *buffer = nullptr;
    int32_t n = 2;
    buffer = (uint8_t *) malloc(n * bufferLen);
    size_t bytesToWrite = 0;
    size_t bytesWritten = 0;
    size_t minBytes = 4;
    size_t tempBytesWritten = 0;
    size_t totalBytesWritten = 0;
    while (!feof(wavFile)) {
        bytesToWrite = fread(buffer, 1, bufferLen, wavFile);
        bytesWritten = 0;
        while ((bytesWritten < bytesToWrite) && ((bytesToWrite - bytesWritten) > minBytes)) {
            tempBytesWritten = audioRenderer->Write(buffer + bytesWritten,
                                                 bytesToWrite - bytesWritten);
            EXPECT_GE(tempBytesWritten, RendererModuleTestConstants::ZERO);
            bytesWritten += tempBytesWritten;
        }
        totalBytesWritten += bytesWritten;
        if (totalBytesWritten >= RendererModuleTestConstants::WRITE_SIZE_LIMIT) {
            break;
        }
    }

    audioRenderer->Drain();

    Timestamp timeStamp;
    bool getAudioTime = audioRenderer->GetAudioTime(timeStamp, Timestamp::Timestampbase::MONOTONIC);
    EXPECT_EQ(true, getAudioTime);
    EXPECT_GE(timeStamp.time.tv_sec, (const long)RendererModuleTestConstants::ZERO);
    EXPECT_GE(timeStamp.time.tv_nsec, (const long)RendererModuleTestConstants::ZERO);

    audioRenderer->Release();

    free(buffer);
    fclose(wavFile);
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Get latency
 */
HWTEST(AudioRendererModuleTest, audio_renderer_get_latency_001, TestSize.Level1)
{
    int32_t ret = 0;

    FILE *wavFile = fopen(RendererModuleTestConstants::AUDIO_TEST_FILE_PATH, "rb");
    EXPECT_NE(nullptr, wavFile);

    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = AudioSampleFormat::SAMPLE_S16LE;
    rendererParams.sampleRate = AudioSamplingRate::SAMPLE_RATE_44100;
    rendererParams.channelCount = AudioChannel::STEREO;
    rendererParams.encodingType = AudioEncodingType::ENCODING_PCM;

    ret = audioRenderer->SetParams(rendererParams);
    EXPECT_EQ(0, ret);
    bool isStarted = audioRenderer->Start();
    EXPECT_EQ(true, isStarted);
    size_t bufferLen;
    ret = audioRenderer->GetBufferSize(bufferLen);
    EXPECT_EQ(0, ret);

    uint8_t *buffer = nullptr;
    int32_t n = 2;
    buffer = (uint8_t *) malloc(n * bufferLen);
    size_t bytesToWrite = 0;
    size_t bytesWritten = 0;
    size_t minBytes = 4;
    size_t tempBytesWritten = 0;
    size_t totalBytesWritten = 0;
    while (!feof(wavFile)) {
        bytesToWrite = fread(buffer, 1, bufferLen, wavFile);
        bytesWritten = 0;
        uint64_t latency;
        ret = audioRenderer->GetLatency(latency);
        EXPECT_EQ(0, ret);
        while ((bytesWritten < bytesToWrite) && ((bytesToWrite - bytesWritten) > minBytes)) {
            tempBytesWritten = audioRenderer->Write(buffer + bytesWritten,
                                                 bytesToWrite - bytesWritten);
            EXPECT_GE(tempBytesWritten, RendererModuleTestConstants::ZERO);
            bytesWritten += tempBytesWritten;
        }
        totalBytesWritten += bytesWritten;
        if (totalBytesWritten >= RendererModuleTestConstants::WRITE_SIZE_LIMIT) {
            break;
        }
    }

    audioRenderer->Drain();
    audioRenderer->Release();

    free(buffer);
    fclose(wavFile);
}


/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Stop playback
 */
HWTEST(AudioRendererModuleTest, audio_renderer_stop_001, TestSize.Level1)
{
    int32_t ret = 0;

    FILE *wavFile = fopen(RendererModuleTestConstants::AUDIO_TEST_FILE_PATH, "rb");
    EXPECT_NE(nullptr, wavFile);

    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = AudioSampleFormat::SAMPLE_S16LE;
    rendererParams.sampleRate = AudioSamplingRate::SAMPLE_RATE_44100;
    rendererParams.channelCount = AudioChannel::STEREO;
    rendererParams.encodingType = AudioEncodingType::ENCODING_PCM;

    ret = audioRenderer->SetParams(rendererParams);
    EXPECT_EQ(0, ret);
    bool isStarted = audioRenderer->Start();
    EXPECT_EQ(true, isStarted);
    size_t bufferLen;
    ret = audioRenderer->GetBufferSize(bufferLen);
    EXPECT_EQ(0, ret);

    uint8_t *buffer = nullptr;
    int32_t n = 2;
    buffer = (uint8_t *) malloc(n * bufferLen);
    size_t bytesToWrite = 0;
    size_t bytesWritten = 0;
    size_t minBytes = 4;
    size_t tempBytesWritten = 0;
    size_t totalBytesWritten = 0;
    while (!feof(wavFile)) {
        bytesToWrite = fread(buffer, 1, bufferLen, wavFile);
        bytesWritten = 0;
        while ((bytesWritten < bytesToWrite) && ((bytesToWrite - bytesWritten) > minBytes)) {
            tempBytesWritten = audioRenderer->Write(buffer + bytesWritten,
                                                 bytesToWrite - bytesWritten);
            EXPECT_GE(tempBytesWritten, RendererModuleTestConstants::ZERO);
            bytesWritten += tempBytesWritten;
        }
        totalBytesWritten += bytesWritten;
        if (totalBytesWritten >= RendererModuleTestConstants::WRITE_SIZE_LIMIT) {
            break;
        }
    }

    bool isStopped = audioRenderer->Stop();
    EXPECT_EQ(true, isStopped);

    audioRenderer->Drain();
    audioRenderer->Release();

    free(buffer);
    fclose(wavFile);
}

/*
 * Feature: Audio render
 * Function: Playback
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Check states
 */
HWTEST(AudioRendererModuleTest, audio_renderer_check_status_001, TestSize.Level1)
{
    int32_t ret = 0;

    FILE *wavFile = fopen(RendererModuleTestConstants::AUDIO_TEST_FILE_PATH, "rb");
    EXPECT_NE(nullptr, wavFile);

    unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(AudioStreamType::STREAM_MUSIC);
    RendererState state = audioRenderer->GetStatus();
    EXPECT_EQ(RendererState::RENDERER_NEW, state);

    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = AudioSampleFormat::SAMPLE_S16LE;
    rendererParams.sampleRate = AudioSamplingRate::SAMPLE_RATE_44100;
    rendererParams.channelCount = AudioChannel::STEREO;
    rendererParams.encodingType = AudioEncodingType::ENCODING_PCM;

    ret = audioRenderer->SetParams(rendererParams);
    EXPECT_EQ(0, ret);
    state = audioRenderer->GetStatus();
    EXPECT_EQ(RendererState::RENDERER_PREPARED, state);

    bool isStarted = audioRenderer->Start();
    EXPECT_EQ(true, isStarted);
    state = audioRenderer->GetStatus();
    EXPECT_EQ(RendererState::RENDERER_RUNNING, state);

    bool isStopped = audioRenderer->Stop();
    EXPECT_EQ(true, isStopped);
    state = audioRenderer->GetStatus();
    EXPECT_EQ(RendererState::RENDERER_STOPPED, state);

    bool isReleased = audioRenderer->Release();
    EXPECT_EQ(true, isReleased);
    state = audioRenderer->GetStatus();
    EXPECT_EQ(RendererState::RENDERER_RELEASED, state);
}