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

#include "audio_opensles_unit_test.h"

using namespace std;
using namespace testing::ext;

namespace OHOS {
namespace AudioStandard {
namespace {
    const char *AUDIORENDER_TEST_FILE_PATH = "/data/test_44100_2.wav";
    FILE *wavFile_;
    wav_hdr wavHeader_;
    SLObjectItf engineObject_;
    SLObjectItf outputMixObject_;
    SLPlayItf playItf_;
    SLVolumeItf volumeItf_;
    SLOHBufferQueueItf bufferQueueItf_;
    SLObjectItf pcmPlayerObject_;
    SLEngineItf engineEngine_;
} // namespace

static void BuqqerQueueCallback (SLOHBufferQueueItf bufferQueueItf, void *pContext, SLuint32 size)
{
    FILE *wavFile = (FILE *)pContext;
    if (!feof(wavFile)) {
        SLuint8 *buffer = nullptr;
        SLuint32 pSize = 0;
        (*bufferQueueItf)->GetBuffer(bufferQueueItf, &buffer, pSize);
        fread(buffer, 1, size, wavFile);
        (*bufferQueueItf)->Enqueue(bufferQueueItf, buffer, size);
    }
    return;
}

void AudioOpenslesUnitTest::SetUpTestCase(void) { }

void AudioOpenslesUnitTest::TearDownTestCase(void) { }

void AudioOpenslesUnitTest::SetUp(void) { }

void AudioOpenslesUnitTest::TearDown(void) { }

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_CreateEngine_001, TestSize.Level0)
{
    SLresult result = slCreateEngine(&engineObject_, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_CreateEngine_002, TestSize.Level0)
{
    SLresult result = (*engineObject_)->Realize(engineObject_, SL_BOOLEAN_FALSE);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_CreateEngine_003, TestSize.Level0)
{
    SLresult result = (*engineObject_)->GetInterface(engineObject_, SL_IID_ENGINE, &engineEngine_);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_CreateOutputMix_001, TestSize.Level0)
{
    SLresult result = (*engineEngine_)->CreateOutputMix(engineEngine_, &outputMixObject_, 0, nullptr, nullptr);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_CreateOutputMix_002, TestSize.Level0)
{
    SLresult result = (*outputMixObject_)->Realize(outputMixObject_, SL_BOOLEAN_FALSE);;
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_CreateAudioPlayer_001, TestSize.Level0)
{
    wavFile_ = fopen(AUDIORENDER_TEST_FILE_PATH, "rb");
    if (wavFile_ == nullptr) {
        MEDIA_INFO_LOG("AudioRendererTest: Unable to open wave file");
    }
    size_t headerSize = sizeof(wav_hdr);
    fread(&wavHeader_, 1, headerSize, wavFile_);

    SLDataLocator_OutputMix slOutputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject_};
    SLDataSink slSink = {&slOutputMix, nullptr};
    SLDataLocator_BufferQueue slBufferQueue = {
        SL_DATALOCATOR_BUFFERQUEUE,
        0
    };
    SLDataFormat_PCM pcmFormat = {
        SL_DATAFORMAT_PCM,
        wavHeader_.NumOfChan,
        wavHeader_.SamplesPerSec * 1000,
        wavHeader_.bitsPerSample,
        0,
        0,
        0
    };
    SLDataSource slSource = {&slBufferQueue, &pcmFormat};
    SLresult result = (*engineEngine_)->CreateAudioPlayer(engineEngine_, &pcmPlayerObject_, &slSource, &slSink, 0, nullptr, nullptr);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_CreateAudioPlayer_002, TestSize.Level0)
{
    SLresult result = (*pcmPlayerObject_)->Realize(pcmPlayerObject_, SL_BOOLEAN_FALSE);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_CreateAudioPlayer_003, TestSize.Level0)
{
    SLresult result = (*pcmPlayerObject_)->GetInterface(pcmPlayerObject_, SL_IID_PLAY, &playItf_);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_GetVoumeItf_001, TestSize.Level0)
{
    SLresult result = (*pcmPlayerObject_)->GetInterface(pcmPlayerObject_, SL_IID_VOLUME, &volumeItf_);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_GetVoume_001, TestSize.Level0)
{
    SLmillibel level = 0;
    SLresult result = (*volumeItf_)->GetVolumeLevel(volumeItf_, &level);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_GetVoume_002, TestSize.Level0)
{
    SLmillibel level = 0;
    SLresult result = (*volumeItf_)->GetMaxVolumeLevel(volumeItf_, &level);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_SetVoume_001, TestSize.Level0)
{
    SLresult result = (*volumeItf_)->SetVolumeLevel(volumeItf_, 0);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_SetVoume_002, TestSize.Level0)
{
    SLmillibel level = 0;
    (*volumeItf_)->GetMaxVolumeLevel(volumeItf_, &level);
    SLresult result = (*volumeItf_)->SetVolumeLevel(volumeItf_, level);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_CetBufferQueue_001, TestSize.Level0)
{
    SLresult result = (*pcmPlayerObject_)->GetInterface(pcmPlayerObject_, SL_IID_OH_BUFFERQUEUE, &bufferQueueItf_);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_RegisterCallback_001, TestSize.Level0)
{
    SLresult result = (*bufferQueueItf_)->RegisterCallback(bufferQueueItf_, BuqqerQueueCallback, wavFile_);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_SetPlayState_001, TestSize.Level0)
{
    SLresult result = (*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_PLAYING);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_play_001, TestSize.Level0)
{
    if (!feof(wavFile_)) {
        SLuint8* buffer = nullptr;
        SLuint32 size = 0;
        SLresult result = (*bufferQueueItf_)->GetBuffer(bufferQueueItf_, &buffer, size);
        EXPECT_TRUE(result == SL_RESULT_SUCCESS);
        fread(buffer, 1, size, wavFile_);
        result = (*bufferQueueItf_)->Enqueue(bufferQueueItf_, buffer, size);
        EXPECT_TRUE(result == SL_RESULT_SUCCESS);
    }
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_SetPlayState_002, TestSize.Level0)
{
    SLresult result = (*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_PAUSED);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_SetPlayState_003, TestSize.Level0)
{
    SLresult result = (*playItf_)->SetPlayState(playItf_, SL_PLAYSTATE_STOPPED);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_GetPlayState_001, TestSize.Level0)
{
    SLuint32 state;
    SLresult result = (*playItf_)->GetPlayState(playItf_, &state);
    EXPECT_TRUE(result == SL_RESULT_SUCCESS);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_Destroy_001, TestSize.Level0)
{
    (*pcmPlayerObject_)->Destroy(pcmPlayerObject_);
    EXPECT_TRUE(true);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_Destroy_002, TestSize.Level0)
{
    (*engineObject_)->Destroy(engineObject_);
    EXPECT_TRUE(true);
}

HWTEST(AudioOpenslesUnitTest, Audio_Opensles_Destroy_003, TestSize.Level0)
{
    (*outputMixObject_)->Destroy(outputMixObject_);
    EXPECT_TRUE(true);
}

HWTEST(AudioOpenslesUnitTest, Prf_Audio_Opensles_CreateEngine_001, TestSize.Level0)
{
    struct timespec tv1 = {0};
    struct timespec tv2 = {0};
    int64_t performanceTestTimes = 10;
    int64_t usecTimes = 1000000;
    int64_t totalTime = 0;
    for (int32_t i = 0; i < performanceTestTimes; i++) {
        clock_gettime(CLOCK_REALTIME, &tv1);
        slCreateEngine(&engineObject_, 0, nullptr, 0, nullptr, nullptr);
        clock_gettime(CLOCK_REALTIME, &tv2);
        totalTime += tv2.tv_sec * usecTimes + tv2.tv_nsec - (tv1.tv_sec * usecTimes + tv1.tv_nsec);
    }
    int64_t expectTime = 1000000;
    EXPECT_TRUE(totalTime <= expectTime * performanceTestTimes);
}

HWTEST(AudioOpenslesUnitTest, Prf_Audio_Opensles_DestoryEngine_001, TestSize.Level0)
{
    struct timespec tv1 = {0};
    struct timespec tv2 = {0};
    int64_t performanceTestTimes = 10;
    int64_t usecTimes = 1000000;
    int64_t totalTime = 0;
    for (int32_t i = 0; i < performanceTestTimes; i++) {
        engineObject_ = {};
        slCreateEngine(&engineObject_, 0, nullptr, 0, nullptr, nullptr);
        clock_gettime(CLOCK_REALTIME, &tv1);
        (*engineObject_)->Destroy(engineObject_);
        clock_gettime(CLOCK_REALTIME, &tv2);
        totalTime += tv2.tv_sec * usecTimes + tv2.tv_nsec - (tv1.tv_sec * usecTimes + tv1.tv_nsec);
    }
    int64_t expectTime = 1000000;
    EXPECT_TRUE(totalTime <= expectTime * performanceTestTimes);
}

HWTEST(AudioOpenslesUnitTest, Prf_Audio_Opensles_Realize_001, TestSize.Level0)
{
    struct timespec tv1 = {0};
    struct timespec tv2 = {0};
    int64_t performanceTestTimes = 10;
    int64_t usecTimes = 1000000;
    int64_t totalTime = 0;
    engineObject_ = {};
    slCreateEngine(&engineObject_, 0, nullptr, 0, nullptr, nullptr);
    for (int32_t i = 0; i < performanceTestTimes; i++) {
        clock_gettime(CLOCK_REALTIME, &tv1);
        (*engineObject_)->Realize(engineObject_, SL_BOOLEAN_FALSE);
        clock_gettime(CLOCK_REALTIME, &tv2);
        totalTime += tv2.tv_sec * usecTimes + tv2.tv_nsec - (tv1.tv_sec * usecTimes + tv1.tv_nsec);
    }
    int64_t expectTime = 1000000;
    EXPECT_TRUE(totalTime <= expectTime * performanceTestTimes);
}

HWTEST(AudioOpenslesUnitTest, Prf_Audio_Opensles_GetInterface_001, TestSize.Level0)
{
    struct timespec tv1 = {0};
    struct timespec tv2 = {0};
    int64_t performanceTestTimes = 10;
    int64_t usecTimes = 1000000;
    int64_t totalTime = 0;
    for (int32_t i = 0; i < performanceTestTimes; i++) {
        clock_gettime(CLOCK_REALTIME, &tv1);
        (*engineObject_)->GetInterface(engineObject_, SL_IID_ENGINE, &engineEngine_);
        clock_gettime(CLOCK_REALTIME, &tv2);
        totalTime += tv2.tv_sec * usecTimes + tv2.tv_nsec - (tv1.tv_sec * usecTimes + tv1.tv_nsec);
    }
    int64_t expectTime = 1000000;
    EXPECT_TRUE(totalTime <= expectTime * performanceTestTimes);
}

HWTEST(AudioOpenslesUnitTest, Prf_Audio_Opensles_CreateOutputMix_001, TestSize.Level0)
{
    struct timespec tv1 = {0};
    struct timespec tv2 = {0};
    int64_t performanceTestTimes = 10;
    int64_t usecTimes = 1000000;
    int64_t totalTime = 0;
    for (int32_t i = 0; i < performanceTestTimes; i++) {
        clock_gettime(CLOCK_REALTIME, &tv1);
        (*engineEngine_)->CreateOutputMix(engineEngine_, &outputMixObject_, 0, nullptr, nullptr);
        clock_gettime(CLOCK_REALTIME, &tv2);
        totalTime += tv2.tv_sec * usecTimes + tv2.tv_nsec - (tv1.tv_sec * usecTimes + tv1.tv_nsec);
    }
    int64_t expectTime = 1000000;
    EXPECT_TRUE(totalTime <= expectTime * performanceTestTimes);
}

HWTEST(AudioOpenslesUnitTest, Prf_Audio_Opensles_CreateAudioPlayer_001, TestSize.Level0)
{
    wavFile_ = fopen(AUDIORENDER_TEST_FILE_PATH, "rb");
    if (wavFile_ == nullptr) {
        MEDIA_INFO_LOG("AudioRendererTest: Unable to open wave file");
    }
    size_t headerSize = sizeof(wav_hdr);
    fread(&wavHeader_, 1, headerSize, wavFile_);
    SLDataLocator_OutputMix slOutputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject_};
    SLDataSink slSink = {&slOutputMix, nullptr};
    SLDataLocator_BufferQueue slBufferQueue = {
        SL_DATALOCATOR_BUFFERQUEUE,
        0
    };
    SLDataFormat_PCM pcmFormat = {
        SL_DATAFORMAT_PCM,
        wavHeader_.NumOfChan,
        wavHeader_.SamplesPerSec * 1000,
        wavHeader_.bitsPerSample,
        0,
        0,
        0
    };
    SLDataSource slSource = {&slBufferQueue, &pcmFormat};
    struct timespec tv1 = {0};
    struct timespec tv2 = {0};
    int64_t performanceTestTimes = 10;
    int64_t usecTimes = 1000000;
    int64_t totalTime = 0;
    for (int32_t i = 0; i < performanceTestTimes; i++) {
        clock_gettime(CLOCK_REALTIME, &tv1);
        (*engineEngine_)->CreateAudioPlayer(engineEngine_, &pcmPlayerObject_, &slSource, &slSink, 0, nullptr, nullptr);
        clock_gettime(CLOCK_REALTIME, &tv2);
        totalTime += tv2.tv_sec * usecTimes + tv2.tv_nsec - (tv1.tv_sec * usecTimes + tv1.tv_nsec);
    }
    int64_t expectTime = 10000000;
    EXPECT_TRUE(totalTime <= expectTime * performanceTestTimes);
}

HWTEST(AudioOpenslesUnitTest, Prf_Audio_Opensles_GetVolumeLevel_001, TestSize.Level0)
{
    struct timespec tv1 = {0};
    struct timespec tv2 = {0};
    int64_t performanceTestTimes = 10;
    int64_t usecTimes = 1000000;
    int64_t totalTime = 0;
    SLmillibel level = 0;
    (*pcmPlayerObject_)->GetInterface(pcmPlayerObject_, SL_IID_VOLUME, &volumeItf_);
    for (int32_t i = 0; i < performanceTestTimes; i++) {
        clock_gettime(CLOCK_REALTIME, &tv1);
        (*volumeItf_)->GetVolumeLevel(volumeItf_, &level);
        clock_gettime(CLOCK_REALTIME, &tv2);
        totalTime += tv2.tv_sec * usecTimes + tv2.tv_nsec - (tv1.tv_sec * usecTimes + tv1.tv_nsec);
    }
    int64_t expectTime = 10000000;
    EXPECT_TRUE(totalTime <= expectTime * performanceTestTimes);
}

HWTEST(AudioOpenslesUnitTest, Prf_Audio_Opensles_GetMaxVolumeLevel_001, TestSize.Level0)
{
    struct timespec tv1 = {0};
    struct timespec tv2 = {0};
    int64_t performanceTestTimes = 10;
    int64_t usecTimes = 1000000;
    int64_t totalTime = 0;
    SLmillibel level = 0;
    for (int32_t i = 0; i < performanceTestTimes; i++) {
        clock_gettime(CLOCK_REALTIME, &tv1);
        (*volumeItf_)->GetMaxVolumeLevel(volumeItf_, &level);
        clock_gettime(CLOCK_REALTIME, &tv2);
        totalTime += tv2.tv_sec * usecTimes + tv2.tv_nsec - (tv1.tv_sec * usecTimes + tv1.tv_nsec);
    }
    int64_t expectTime = 1000000;
    EXPECT_TRUE(totalTime <= expectTime * performanceTestTimes);
}

HWTEST(AudioOpenslesUnitTest, Prf_Audio_Opensles_SetVolumeLevel_001, TestSize.Level0)
{
    struct timespec tv1 = {0};
    struct timespec tv2 = {0};
    int64_t performanceTestTimes = 10;
    int64_t usecTimes = 1000000;
    int64_t totalTime = 0;
    SLmillibel level = 0;
    for (int32_t i = 0; i < performanceTestTimes; i++) {
        clock_gettime(CLOCK_REALTIME, &tv1);
        (*volumeItf_)->SetVolumeLevel(volumeItf_, level);
        clock_gettime(CLOCK_REALTIME, &tv2);
        totalTime += tv2.tv_sec * usecTimes + tv2.tv_nsec - (tv1.tv_sec * usecTimes + tv1.tv_nsec);
    }
    int64_t expectTime = 1000000;
    EXPECT_TRUE(totalTime <= expectTime * performanceTestTimes);
}
} // namespace AudioStandard
} // namespace OHOS
