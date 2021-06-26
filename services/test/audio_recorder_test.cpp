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

#include "audio_recorder.h"
#include "media_log.h"

using namespace std;
using namespace OHOS;
using namespace OHOS::AudioStandard;

namespace AudioTestConstants {
    constexpr int32_t SECOND_ARG_IDX = 2;
    constexpr int32_t THIRD_ARG_IDX = 3;
    constexpr int32_t PAUSE_BUFFER_POSITION = 512;
    constexpr int32_t PAUSE_READ_TIME_SECONDS = 2;
    constexpr int32_t SUCCESS = 0;
}

class StreamRecorderTest {
public:
    bool TestRecord(int argc, char *argv[]) const
    {
        MEDIA_INFO_LOG("TestCapture start ");

        unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(AudioStreamType::STREAM_MUSIC);
        vector<AudioSampleFormat> supportedFormatList = audioRecorder->GetSupportedFormats();
        MEDIA_INFO_LOG("Supported formats:");
        for (auto i = supportedFormatList.begin(); i != supportedFormatList.end(); ++i) {
            MEDIA_INFO_LOG("Format %{public}d", *i);
        }

        vector<AudioChannel> supportedChannelList = audioRecorder->GetSupportedChannels();
        MEDIA_INFO_LOG("Supported channels:");
        for (auto i = supportedChannelList.begin(); i != supportedChannelList.end(); ++i) {
            MEDIA_INFO_LOG("channel %{public}d", *i);
        }

        vector<AudioEncodingType> supportedEncodingTypes
                                    = audioRecorder->GetSupportedEncodingTypes();
        MEDIA_INFO_LOG("Supported encoding types:");
        for (auto i = supportedEncodingTypes.begin(); i != supportedEncodingTypes.end(); ++i) {
            MEDIA_INFO_LOG("encoding type %{public}d", *i);
        }

        vector<AudioSamplingRate> supportedSamplingRates = audioRecorder->GetSupportedSamplingRates();
        MEDIA_INFO_LOG("Supported sampling rates:");
        for (auto i = supportedSamplingRates.begin(); i != supportedSamplingRates.end(); ++i) {
            MEDIA_INFO_LOG("sampling rate %{public}d", *i);
        }
        AudioRecorderParams recorderParams;
        recorderParams.audioSampleFormat = SAMPLE_S16LE;
        recorderParams.samplingRate =  static_cast<AudioSamplingRate>(atoi(argv[AudioTestConstants::SECOND_ARG_IDX]));
        recorderParams.audioChannel = AudioChannel::STEREO;
        recorderParams.audioEncoding = ENCODING_PCM;
        if (audioRecorder->SetParams(recorderParams) != AudioTestConstants::SUCCESS) {
            MEDIA_ERR_LOG("Set audio stream parameters failed");
            audioRecorder->Release();
            return false;
        }
        MEDIA_INFO_LOG("Record stream created");

        MEDIA_INFO_LOG("Starting Stream");
        if (!audioRecorder->Start()) {
            MEDIA_ERR_LOG("Start stream failed");
            audioRecorder->Release();
            return false;
        }
        MEDIA_INFO_LOG("Recording started");

        MEDIA_INFO_LOG("Get Audio parameters:");
        AudioRecorderParams getRecorderParams;
        if (audioRecorder->GetParams(getRecorderParams) == AudioTestConstants::SUCCESS) {
            MEDIA_INFO_LOG("Get Audio format: %{public}d", getRecorderParams.audioSampleFormat);
            MEDIA_INFO_LOG("Get Audio sampling rate: %{public}d", getRecorderParams.samplingRate);
            MEDIA_INFO_LOG("Get Audio channels: %{public}d", getRecorderParams.audioChannel);
        }

        size_t bufferLen;
        if (audioRecorder->GetBufferSize(bufferLen) < 0) {
            MEDIA_ERR_LOG(" GetMinimumBufferSize failed");
            return false;
        }
        MEDIA_INFO_LOG("Buffer size: %{public}d", bufferLen);

        uint32_t frameCount;
        if (audioRecorder->GetFrameCount(frameCount) < 0) {
            MEDIA_ERR_LOG(" GetMinimumFrameCount failed");
            return false;
        }
        MEDIA_INFO_LOG("Frame count: %{public}d", frameCount);

        bool isBlocking = (atoi(argv[AudioTestConstants::THIRD_ARG_IDX]) == 1);
        MEDIA_INFO_LOG("Is blocking read: %{public}s", isBlocking ? "true" : "false");

        uint8_t* buffer = nullptr;
        buffer = (uint8_t *) malloc(bufferLen);
        FILE *pFile = fopen(argv[AudioTestConstants::SECOND_ARG_IDX - 1], "wb");

        size_t size = 1;
        size_t numBuffersToRecord = 1024;
        int32_t bytesRead = 0;
        while (numBuffersToRecord) {
            bytesRead = audioRecorder->Read(*buffer, bufferLen, isBlocking);
            MEDIA_INFO_LOG("Bytes read: %{public}d", bytesRead);
            if (bytesRead < 0) {
                break;
            }
            if (bytesRead > 0) {
                fwrite(buffer, size, bytesRead, pFile);
                numBuffersToRecord--;
                MEDIA_INFO_LOG("Number of buffers to record: %{public}d", numBuffersToRecord);
                if ((numBuffersToRecord == AudioTestConstants::PAUSE_BUFFER_POSITION)
                    && (audioRecorder->Stop())) {
                    MEDIA_INFO_LOG("Audio record stopped for 2 seconds");
                    sleep(AudioTestConstants::PAUSE_READ_TIME_SECONDS);
                    MEDIA_INFO_LOG("Audio record resume");
                    if (!audioRecorder->Start()) {
                        MEDIA_ERR_LOG("resume stream failed");
                        audioRecorder->Release();
                        return false;
                    }
                }
            }
        }

        Timestamp timestamp;
        if (audioRecorder->GetAudioTime(timestamp, Timestamp::Timestampbase::MONOTONIC)) {
            MEDIA_INFO_LOG("Timestamp seconds: %{public}ld", timestamp.time.tv_sec);
            MEDIA_INFO_LOG("Timestamp nanoseconds: %{public}ld", timestamp.time.tv_nsec);
        }

        fflush(pFile);
        if(!audioRecorder->Flush()) {
            MEDIA_ERR_LOG("AudioRecorderTest: flush failed");
        }

        if(!audioRecorder->Stop()) {
            MEDIA_ERR_LOG("AudioRecorderTest: Stop failed");
        }

        if(!audioRecorder->Release()) {
            MEDIA_ERR_LOG("AudioRecorderTest: Release failed");
        }
        free(buffer);
        fclose(pFile);
        MEDIA_INFO_LOG("TestCapture end");

        return true;
    }
};

int main(int argc, char *argv[])
{
    MEDIA_INFO_LOG("capture test in");

    if ((argv == nullptr) || (argc <= AudioTestConstants::THIRD_ARG_IDX)) {
        MEDIA_ERR_LOG("argv is null");
        return 0;
    }

    MEDIA_INFO_LOG("argc=%d", argc);
    MEDIA_INFO_LOG("argv[1]=%{public}s", argv[1]);
    MEDIA_INFO_LOG("argv[2]=%{public}s", argv[AudioTestConstants::SECOND_ARG_IDX]);
    MEDIA_INFO_LOG("argv[3]=%{public}s", argv[AudioTestConstants::THIRD_ARG_IDX]);

    StreamRecorderTest testObj;
    bool ret = testObj.TestRecord(argc, argv);

    return ret;
}
