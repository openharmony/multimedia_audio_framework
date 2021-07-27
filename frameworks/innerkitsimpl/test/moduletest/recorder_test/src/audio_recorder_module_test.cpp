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

#include "audio_recorder_module_test.h"


#include "audio_info.h"
#include "audio_recorder.h"

using namespace std;
using namespace testing::ext;

namespace OHOS {
namespace AudioStandard {
namespace {
    const string AUDIO_RECORD_FILE1 = "/data/audioreordtest_blocking.pcm";
    const string AUDIO_RECORD_FILE2 = "/data/audioreordtest_nonblocking.pcm";
    const string AUDIO_RECORD_FILE3 = "/data/audioreordtest_out.pcm";
    const int32_t READ_BUFFERS_COUNT = 128;
    const int32_t VALUE_ZERO = 0;
    const int32_t SUCCESS = 0;
} // namespace

void AudioRecorderModuleTest::SetUpTestCase(void) {}
void AudioRecorderModuleTest::TearDownTestCase(void) {}
void AudioRecorderModuleTest::SetUp(void) {}
void AudioRecorderModuleTest::TearDown(void) {}

int32_t AudioRecorderModuleTest::InitializeRecorder(unique_ptr<AudioRecorder> &audioRecorder)
{
    AudioRecorderParams recorderParams;
    recorderParams.audioSampleFormat = SAMPLE_S16LE;
    recorderParams.samplingRate = SAMPLE_RATE_44100;
    recorderParams.audioChannel = STEREO;
    recorderParams.audioEncoding = ENCODING_PCM;

    return audioRecorder->SetParams(recorderParams);
}

/**
* @tc.name  : Test GetSupportedFormats API
* @tc.number: Auido_Recorder_GetSupportedFormats_001
* @tc.desc  : test GetSupportedFormats interface, Returns supported Formats on success.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_GetSupportedFormats_001, TestSize.Level1)
{
    vector<AudioSampleFormat> supportedFormatList = AudioRecorder::GetSupportedFormats();
    EXPECT_EQ(AUDIO_SUPPORTED_FORMATS.size(), supportedFormatList.size());
}

/**
* @tc.name  : Test GetSupportedChannels API
* @tc.number: Auido_Recorder_GetSupportedChannels_001
* @tc.desc  : test GetSupportedChannels interface, Returns supported Channels on success.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_GetSupportedChannels_001, TestSize.Level1)
{
    vector<AudioChannel> supportedChannelList = AudioRecorder::GetSupportedChannels();
    EXPECT_EQ(AUDIO_SUPPORTED_CHANNELS.size(), supportedChannelList.size());
}

/**
* @tc.name  : Test GetSupportedEncodingTypes API
* @tc.number: Auido_Recorder_GetSupportedEncodingTypes_001
* @tc.desc  : test GetSupportedEncodingTypes interface, Returns supported Encoding types on success.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_GetSupportedEncodingTypes_001, TestSize.Level1)
{
    vector<AudioEncodingType> supportedEncodingTypes
                                        = AudioRecorder::GetSupportedEncodingTypes();
    EXPECT_EQ(AUDIO_SUPPORTED_ENCODING_TYPES.size(), supportedEncodingTypes.size());
}

/**
* @tc.name  : Test GetSupportedSamplingRates API
* @tc.number: Auido_Recorder_GetSupportedSamplingRates_001
* @tc.desc  : test GetSupportedSamplingRates interface, Returns supported Sampling rates on success.
*/
HWTEST(AudioRecoderModuleTest, Auido_Recorder_GetSupportedSamplingRates_001, TestSize.Level1)
{
    vector<AudioSamplingRate> supportedSamplingRates = AudioRecorder::GetSupportedSamplingRates();
    EXPECT_EQ(AUDIO_SUPPORTED_SAMPLING_RATES.size(), supportedSamplingRates.size());
}

/**
* @tc.name  : Test Create API via legal input.
* @tc.number: Auido_Recorder_Create_001
* @tc.desc  : test Create interface, Returns audioRecorder instance if create is Successful.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_Create_001, TestSize.Level1)
{
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRecorder);
}

/**
* @tc.name  : Test SetParams API via legal input.
* @tc.number: Auido_Recorder_SetParams_001
* @tc.desc  : test SetParams interface, Returns 0 {SUCCESS} if the setting is successful.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_SetParams_001, TestSize.Level1)
{
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    ASSERT_NE(nullptr, audioRecorder);

    AudioRecorderParams recorderParams;
    recorderParams.audioSampleFormat = SAMPLE_S16LE;
    recorderParams.samplingRate = SAMPLE_RATE_44100;
    recorderParams.audioChannel = STEREO;
    recorderParams.audioEncoding = ENCODING_PCM;

    int32_t ret = audioRecorder->SetParams(recorderParams);
    EXPECT_EQ(SUCCESS, ret);
    audioRecorder->Release();
}

/**
* @tc.name  : Test GetParams API via legal input.
* @tc.number: Auido_Recorder_GetParams_001
* @tc.desc  : test GetParams interface, Returns 0 {SUCCESS} if the getting is successful.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_GetParams_001, TestSize.Level1)
{
    int32_t ret = -1;
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    ASSERT_NE(nullptr, audioRecorder);

    AudioRecorderParams recorderParams;
    recorderParams.audioSampleFormat = SAMPLE_S16LE;
    recorderParams.samplingRate = SAMPLE_RATE_44100;
    recorderParams.audioChannel = STEREO;
    recorderParams.audioEncoding = ENCODING_PCM;
    ret = audioRecorder->SetParams(recorderParams);
    EXPECT_EQ(SUCCESS, ret);

    AudioRecorderParams getRecorderParams;
    ret = audioRecorder->GetParams(getRecorderParams);
    EXPECT_EQ(SUCCESS, ret);
    EXPECT_EQ(recorderParams.audioSampleFormat, getRecorderParams.audioSampleFormat);
    EXPECT_EQ(recorderParams.samplingRate, getRecorderParams.samplingRate);
    EXPECT_EQ(recorderParams.audioChannel, getRecorderParams.audioChannel);
    EXPECT_EQ(recorderParams.audioEncoding, getRecorderParams.audioEncoding);

    audioRecorder->Release();
}

/**
* @tc.name  : Test GetBufferSize API via legal input.
* @tc.number: Auido_Recorder_GetBufferSize_001
* @tc.desc  : test GetBufferSize interface, Returns 0 {SUCCESS} if the getting is successful.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_GetBufferSize_001, TestSize.Level1)
{
    int32_t ret = -1;
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRecorder);

    ret = AudioRecorderModuleTest::InitializeRecorder(audioRecorder);
    EXPECT_EQ(SUCCESS, ret);
    size_t bufferLen;
    ret = audioRecorder->GetBufferSize(bufferLen);
    EXPECT_EQ(SUCCESS, ret);

    audioRecorder->Release();
}

/**
* @tc.name  : Test GetFrameCount API via legal input.
* @tc.number: Auido_Recorder_GetFrameCount_001
* @tc.desc  : test GetFrameCount interface, Returns 0 {SUCCESS} if the getting is successful.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_GetFrameCount_001, TestSize.Level1)
{
    int32_t ret = -1;
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRecorder);

    ret = AudioRecorderModuleTest::InitializeRecorder(audioRecorder);
    EXPECT_EQ(SUCCESS, ret);

    uint32_t frameCount;
    ret = audioRecorder->GetFrameCount(frameCount);
    EXPECT_EQ(SUCCESS, ret);

    audioRecorder->Release();
}

/**
* @tc.name  : Test Start API.
* @tc.number: Audio_Recorder_Start_001
* @tc.desc  : test Start interface, Returns true if start is successful.
*/
HWTEST(AudioRecorderModuleTest, Audio_Recorder_Start_001, TestSize.Level1)
{
    int32_t ret = -1;
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRecorder);

    ret = AudioRecorderModuleTest::InitializeRecorder(audioRecorder);
    EXPECT_EQ(SUCCESS, ret);

    bool isStarted = audioRecorder->Start();
    EXPECT_EQ(true, isStarted);

    audioRecorder->Release();
}

/**
* @tc.name  : Test Read API via isBlockingRead = true.
* @tc.number: Audio_Recorder_Read_001
* @tc.desc  : test GetFrameCount interface, Returns number of bytes read if the read is successful.
*/
HWTEST(AudioRecorderModuleTest, Audio_Recorder_Read_001, TestSize.Level1)
{
    int32_t ret = -1;
    bool isBlockingRead = true;
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRecorder);

    ret = AudioRecorderModuleTest::InitializeRecorder(audioRecorder);
    EXPECT_EQ(SUCCESS, ret);

    bool isStarted = audioRecorder->Start();
    EXPECT_EQ(true, isStarted);

    size_t bufferLen;
    ret = audioRecorder->GetBufferSize(bufferLen);
    EXPECT_EQ(SUCCESS, ret);

    uint8_t* buffer = (uint8_t *) malloc(bufferLen);
    ASSERT_NE(nullptr, buffer);
    FILE *recFile = fopen(AUDIO_RECORD_FILE1.c_str(), "wb");
    ASSERT_NE(nullptr, recFile);

    size_t size = 1;
    int32_t bytesRead = 0;
    int32_t numBuffersToRecord = READ_BUFFERS_COUNT;

    while (numBuffersToRecord) {
        bytesRead = audioRecorder->Read(*buffer, bufferLen, isBlockingRead);
        if (bytesRead < 0) {
            break;
        } else if (bytesRead > 0) {
            fwrite(buffer, size, bytesRead, recFile);
            numBuffersToRecord--;
        }
    }

    audioRecorder->Flush();
    audioRecorder->Stop();
    audioRecorder->Release();

    free(buffer);
    fclose(recFile);
}

/**
* @tc.name  : Test Read API via isBlockingRead = false.
* @tc.number: Audio_Recorder_Read_002
* @tc.desc  : test GetFrameCount interface, Returns number of bytes read if the read is successful.
*/
HWTEST(AudioRecorderModuleTest, Audio_Recorder_Read_002, TestSize.Level1)
{
    int32_t ret = -1;
    bool isBlockingRead = false;
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRecorder);

    ret = AudioRecorderModuleTest::InitializeRecorder(audioRecorder);
    EXPECT_EQ(SUCCESS, ret);

    bool isStarted = audioRecorder->Start();
    EXPECT_EQ(true, isStarted);

    size_t bufferLen;
    ret = audioRecorder->GetBufferSize(bufferLen);
    EXPECT_EQ(SUCCESS, ret);

    uint8_t* buffer = (uint8_t *) malloc(bufferLen);
    ASSERT_NE(nullptr, buffer);
    FILE *recFile = fopen(AUDIO_RECORD_FILE2.c_str(), "wb");
    ASSERT_NE(nullptr, recFile);

    size_t size = 1;
    int32_t bytesRead = 0;
    int32_t numBuffersToRecord = READ_BUFFERS_COUNT;

    while (numBuffersToRecord) {
        bytesRead = audioRecorder->Read(*buffer, bufferLen, isBlockingRead);
        if (bytesRead < 0) {
            break;
        } else if (bytesRead > 0) {
            fwrite(buffer, size, bytesRead, recFile);
            numBuffersToRecord--;
        }
    }

    audioRecorder->Flush();
    audioRecorder->Stop();
    audioRecorder->Release();

    free(buffer);
    fclose(recFile);
}

/**
* @tc.name  : Test GetAudioTime API via legal input.
* @tc.number: Auido_Recorder_GetAudioTime_001
* @tc.desc  : test GetAudioTime interface, Returns true if the getting is successful.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_GetAudioTime_001, TestSize.Level1)
{
    int32_t ret = -1;
    bool isBlockingRead = true;
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRecorder);

    ret = AudioRecorderModuleTest::InitializeRecorder(audioRecorder);
    EXPECT_EQ(SUCCESS, ret);

    bool isStarted = audioRecorder->Start();
    EXPECT_EQ(true, isStarted);

    size_t bufferLen;
    ret = audioRecorder->GetBufferSize(bufferLen);
    EXPECT_EQ(SUCCESS, ret);

    uint8_t* buffer = (uint8_t *) malloc(bufferLen);
    ASSERT_NE(nullptr, buffer);
    FILE *recFile = fopen(AUDIO_RECORD_FILE3.c_str(), "wb");
    ASSERT_NE(nullptr, recFile);

    size_t size = 1;
    int32_t bytesRead = 0;
    int32_t numBuffersToRecord = 5;

    while (numBuffersToRecord) {
        bytesRead = audioRecorder->Read(*buffer, bufferLen, isBlockingRead);
        if (bytesRead < 0) {
            break;
        } else if (bytesRead > 0) {
            fwrite(buffer, size, bytesRead, recFile);
            numBuffersToRecord--;
        }
    }

    Timestamp timeStamp;
    bool getAudioTime = audioRecorder->GetAudioTime(timeStamp, Timestamp::Timestampbase::MONOTONIC);
    EXPECT_EQ(true, getAudioTime);
    EXPECT_GE(timeStamp.time.tv_sec, (const long)VALUE_ZERO);
    EXPECT_GE(timeStamp.time.tv_nsec, (const long)VALUE_ZERO);

    audioRecorder->Flush();
    audioRecorder->Stop();
    audioRecorder->Release();

    free(buffer);
    fclose(recFile);
}

/**
* @tc.name  : Test Flush API.
* @tc.number: Auido_Recorder_Flush_001
* @tc.desc  : test Flush interface, Returns true if the flush is successful.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_Flush_001, TestSize.Level1)
{
    int32_t ret = -1;
    bool isBlockingRead = true;
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRecorder);

    ret = AudioRecorderModuleTest::InitializeRecorder(audioRecorder);
    EXPECT_EQ(SUCCESS, ret);

    bool isStarted = audioRecorder->Start();
    EXPECT_EQ(true, isStarted);

    size_t bufferLen;
    ret = audioRecorder->GetBufferSize(bufferLen);
    EXPECT_EQ(SUCCESS, ret);

    uint8_t* buffer = (uint8_t *) malloc(bufferLen);
    ASSERT_NE(nullptr, buffer);
    FILE *recFile = fopen(AUDIO_RECORD_FILE3.c_str(), "wb");
    ASSERT_NE(nullptr, recFile);

    size_t size = 1;
    int32_t bytesRead = 0;
    int32_t numBuffersToRecord = 5;

    while (numBuffersToRecord) {
        bytesRead = audioRecorder->Read(*buffer, bufferLen, isBlockingRead);
        if (bytesRead < 0) {
            break;
        } else if (bytesRead > 0) {
            fwrite(buffer, size, bytesRead, recFile);
            numBuffersToRecord--;
        }
    }

    bool isFlushed = audioRecorder->Flush();
    EXPECT_EQ(true, isFlushed);

    audioRecorder->Stop();
    audioRecorder->Release();

    free(buffer);
    fclose(recFile);
}

/**
* @tc.name  : Test Stop API.
* @tc.number: Auido_Recorder_Stop_001
* @tc.desc  : test Stop interface, Returns true if the stop is successful.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_Stop_001, TestSize.Level1)
{
    int32_t ret = -1;
    bool isBlockingRead = true;
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRecorder);

    ret = AudioRecorderModuleTest::InitializeRecorder(audioRecorder);
    EXPECT_EQ(SUCCESS, ret);

    bool isStarted = audioRecorder->Start();
    EXPECT_EQ(true, isStarted);

    size_t bufferLen;
    ret = audioRecorder->GetBufferSize(bufferLen);
    EXPECT_EQ(SUCCESS, ret);

    uint8_t* buffer = (uint8_t *) malloc(bufferLen);
    ASSERT_NE(nullptr, buffer);
    FILE *recFile = fopen(AUDIO_RECORD_FILE3.c_str(), "wb");
    ASSERT_NE(nullptr, recFile);

    size_t size = 1;
    int32_t bytesRead = 0;
    int32_t numBuffersToRecord = 5;

    while (numBuffersToRecord) {
        bytesRead = audioRecorder->Read(*buffer, bufferLen, isBlockingRead);
        if (bytesRead < 0) {
            break;
        } else if (bytesRead > 0) {
            fwrite(buffer, size, bytesRead, recFile);
            numBuffersToRecord--;
        }
    }

    audioRecorder->Flush();

    bool isStopped = audioRecorder->Stop();
    EXPECT_EQ(true, isStopped);

    audioRecorder->Release();

    free(buffer);
    fclose(recFile);
}

/**
* @tc.name  : Test Release API.
* @tc.number: Auido_Recorder_Release_001
* @tc.desc  : test Release interface, Returns true if the release is successful.
*/
HWTEST(AudioRecorderModuleTest, Auido_Recorder_Release_001, TestSize.Level1)
{
    int32_t ret = -1;
    bool isBlockingRead = true;
    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    EXPECT_NE(nullptr, audioRecorder);

    ret = AudioRecorderModuleTest::InitializeRecorder(audioRecorder);
    EXPECT_EQ(SUCCESS, ret);

    bool isStarted = audioRecorder->Start();
    EXPECT_EQ(true, isStarted);

    size_t bufferLen;
    ret = audioRecorder->GetBufferSize(bufferLen);
    EXPECT_EQ(SUCCESS, ret);

    uint8_t* buffer = (uint8_t *) malloc(bufferLen);
    ASSERT_NE(nullptr, buffer);
    FILE *recFile = fopen(AUDIO_RECORD_FILE3.c_str(), "wb");
    ASSERT_NE(nullptr, recFile);

    size_t size = 1;
    int32_t bytesRead = 0;
    int32_t numBuffersToRecord = 5;

    while (numBuffersToRecord) {
        bytesRead = audioRecorder->Read(*buffer, bufferLen, isBlockingRead);
        if (bytesRead < 0) {
            break;
        } else if (bytesRead > 0) {
            fwrite(buffer, size, bytesRead, recFile);
            numBuffersToRecord--;
        }
    }

    audioRecorder->Flush();
    audioRecorder->Stop();

    bool isReleased = audioRecorder->Release();
    EXPECT_EQ(true, isReleased);

    free(buffer);
    fclose(recFile);
}

/**
* @tc.name  : Test GetStatus API.
* @tc.number: Auido_Recorder_GetStatus_001
* @tc.desc  : test GetStatus interface, Returns correct state on success.
*/
HWTEST(AudioRecorderModuleTest, Audio_Recorder_GetStatus_001, TestSize.Level1)
{
    int32_t ret = -1;
    RecorderState state = RECORDER_INVALID;

    unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(STREAM_MUSIC);
    ASSERT_NE(nullptr, audioRecorder);
    state = audioRecorder->GetStatus();
    EXPECT_EQ(RECORDER_NEW, state);

    AudioRecorderParams recorderParams;
    recorderParams.audioSampleFormat = SAMPLE_S16LE;
    recorderParams.samplingRate = SAMPLE_RATE_44100;
    recorderParams.audioChannel = STEREO;
    recorderParams.audioEncoding = ENCODING_PCM;
    ret = audioRecorder->SetParams(recorderParams);
    EXPECT_EQ(SUCCESS, ret);
    state = audioRecorder->GetStatus();
    EXPECT_EQ(RECORDER_PREPARED, state);

    bool isStarted = audioRecorder->Start();
    EXPECT_EQ(true, isStarted);
    state = audioRecorder->GetStatus();
    EXPECT_EQ(RECORDER_RUNNING, state);

    bool isStopped = audioRecorder->Stop();
    EXPECT_EQ(true, isStopped);
    state = audioRecorder->GetStatus();
    EXPECT_EQ(RECORDER_STOPPED, state);

    bool isReleased = audioRecorder->Release();
    EXPECT_EQ(true, isReleased);
    state = audioRecorder->GetStatus();
    EXPECT_EQ(RECORDER_RELEASED, state);
}
} // namespace AudioStandard
} // namespace OHOS
