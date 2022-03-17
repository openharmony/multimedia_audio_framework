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

#include <chrono>
#include <string>
#include <vector>

#include "audio_capturer.h"
#include "media_log.h"

using namespace std;
using namespace std::chrono;
using namespace OHOS;
using namespace OHOS::AudioStandard;
int32_t bufferMsec = 0;
namespace AudioTestConstants {
    constexpr int32_t FIRST_ARG_IDX = 1;
    constexpr int32_t SECOND_ARG_IDX = 2;
    constexpr int32_t THIRD_ARG_IDX = 3;
    constexpr int32_t FOURTH_ARG_IDX = 4;
    constexpr int32_t NUM_BASE = 10;
    constexpr int32_t PAUSE_BUFFER_POSITION = 128;
    constexpr int32_t PAUSE_READ_TIME_SECONDS = 2;
    constexpr int32_t SUCCESS = 0;
}

class AudioCapturerCallbackTestImpl : public AudioCapturerCallback {
public:
    void OnStateChange(const CapturerState state) override
    {
        MEDIA_DEBUG_LOG("AudioCapturerCallbackTestImpl:: OnStateChange");

        switch (state) {
            case CAPTURER_PREPARED:
                MEDIA_DEBUG_LOG("AudioCapturerCallbackTestImpl: OnStateChange CAPTURER_PREPARED");
                break;
            case CAPTURER_RUNNING:
                MEDIA_DEBUG_LOG("AudioCapturerCallbackTestImpl: OnStateChange CAPTURER_RUNNING");
                break;
            case CAPTURER_STOPPED:
                MEDIA_DEBUG_LOG("AudioCapturerCallbackTestImpl: OnStateChange CAPTURER_STOPPED");
                break;
            case CAPTURER_RELEASED:
                MEDIA_DEBUG_LOG("AudioCapturerCallbackTestImpl: OnStateChange CAPTURER_RELEASED");
                break;
            default:
                MEDIA_ERR_LOG("AudioCapturerCallbackTestImpl: OnStateChange NOT A VALID state");
                break;
        }
    }
};

class AudioCapturerTest {
public:
    void CheckSupportedParams() const
    {
        vector<AudioSampleFormat> supportedFormatList = AudioCapturer::GetSupportedFormats();
        MEDIA_INFO_LOG("Supported formats:");
        for (auto i = supportedFormatList.begin(); i != supportedFormatList.end(); ++i) {
            MEDIA_INFO_LOG("Format %{public}d", *i);
        }

        vector<AudioChannel> supportedChannelList = AudioCapturer::GetSupportedChannels();
        MEDIA_INFO_LOG("Supported channels:");
        for (auto i = supportedChannelList.begin(); i != supportedChannelList.end(); ++i) {
            MEDIA_INFO_LOG("channel %{public}d", *i);
        }

        vector<AudioEncodingType> supportedEncodingTypes
                                    = AudioCapturer::GetSupportedEncodingTypes();
        MEDIA_INFO_LOG("Supported encoding types:");
        for (auto i = supportedEncodingTypes.begin(); i != supportedEncodingTypes.end(); ++i) {
            MEDIA_INFO_LOG("encoding type %{public}d", *i);
        }

        vector<AudioSamplingRate> supportedSamplingRates = AudioCapturer::GetSupportedSamplingRates();
        MEDIA_INFO_LOG("Supported sampling rates:");
        for (auto i = supportedSamplingRates.begin(); i != supportedSamplingRates.end(); ++i) {
            MEDIA_INFO_LOG("sampling rate %{public}d", *i);
        }
    }

    bool InitCapture(const unique_ptr<AudioCapturer> &audioCapturer) const
    {
        MEDIA_INFO_LOG("Starting Stream");
        if (!audioCapturer->Start()) {
            MEDIA_ERR_LOG("Start stream failed");
            audioCapturer->Release();
            return false;
        }
        MEDIA_INFO_LOG("Capturing started");

        MEDIA_INFO_LOG("Get Audio parameters:");
        AudioCapturerParams getCapturerParams;
        if (audioCapturer->GetParams(getCapturerParams) == AudioTestConstants::SUCCESS) {
            MEDIA_INFO_LOG("Get Audio format: %{public}d", getCapturerParams.audioSampleFormat);
            MEDIA_INFO_LOG("Get Audio sampling rate: %{public}d", getCapturerParams.samplingRate);
            MEDIA_INFO_LOG("Get Audio channels: %{public}d", getCapturerParams.audioChannel);
        }

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
            MEDIA_ERR_LOG("AudioCapturerTest: Failed to allocate buffer");
            return false;
        }
        MEDIA_INFO_LOG("AudioPerf Capturer First Frame Read, BUFFER_LEN = %{public}zu", bufferLen);
        size_t size = 1;
        size_t numBuffersToCapture = 256;
        while (numBuffersToCapture) {
            size_t bytesRead = 0;
            while (bytesRead < bufferLen) {
                auto start = high_resolution_clock::now();
                int32_t len = audioCapturer->Read(*(buffer.get() + bytesRead), bufferLen - bytesRead, isBlocking);
                auto stop = high_resolution_clock::now();
                auto duration = duration_cast<microseconds>(stop - start);
                MEDIA_INFO_LOG("AudioPerf Capturer Read in microseconds TimeTaken =%{public}lld",
                    (long long)duration.count());
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
            if ((numBuffersToCapture == AudioTestConstants::PAUSE_BUFFER_POSITION)
                && (audioCapturer->Stop())) {
                MEDIA_INFO_LOG("Audio capture stopped for %{public}d seconds",
                               AudioTestConstants::PAUSE_READ_TIME_SECONDS);
                sleep(AudioTestConstants::PAUSE_READ_TIME_SECONDS);
                if (!audioCapturer->Start()) {
                    MEDIA_ERR_LOG("resume stream failed");
                    audioCapturer->Release();
                    break;
                }
                MEDIA_INFO_LOG("AudioPerf Capturer Read after stop and start");
            }
        }

        return true;
    }

    bool TestRecording(int32_t samplingRate, bool isBlocking, string filePath) const
    {
        MEDIA_INFO_LOG("TestCapture start ");
        AudioCapturerOptions capturerOptions;
        capturerOptions.streamInfo.samplingRate = static_cast<AudioSamplingRate>(samplingRate);
        capturerOptions.streamInfo.encoding = AudioEncodingType::ENCODING_PCM;
        capturerOptions.streamInfo.format = AudioSampleFormat::SAMPLE_S16LE;
        capturerOptions.streamInfo.channels = AudioChannel::STEREO;
        capturerOptions.capturerInfo.sourceType = SourceType::SOURCE_TYPE_MIC;
        capturerOptions.capturerInfo.capturerFlags = 0;

        unique_ptr<AudioCapturer> audioCapturer = AudioCapturer::Create(capturerOptions);

        int32_t ret = 0;
        shared_ptr<AudioCapturerCallback> cb1 = make_shared<AudioCapturerCallbackTestImpl>();
        ret = audioCapturer->SetCapturerCallback(cb1);
        if (ret) {
            MEDIA_ERR_LOG("AudioCapturerTest: SetCapturerCallback failed %{public}d", ret);
            return false;
        }

        CheckSupportedParams();
        MEDIA_ERR_LOG("AudioCaptruerTest: buffermsec = %{public}d", bufferMsec);
        int32_t status = audioCapturer->SetBufferDuration(bufferMsec);
        if (status) {
            MEDIA_ERR_LOG("Failed to set buffer duration");
        }

        if (!InitCapture(audioCapturer)) {
            MEDIA_ERR_LOG("Initialize capturer failed");
            return false;
        }

        MEDIA_INFO_LOG("Is blocking read: %{public}s", isBlocking ? "true" : "false");
        FILE *pFile = fopen(filePath.c_str(), "wb");
        if (pFile == nullptr) {
            MEDIA_INFO_LOG("AudioCapturerTest: Unable to open file");
            return false;
        }

        if (!StartCapture(audioCapturer, isBlocking, pFile)) {
            MEDIA_ERR_LOG("Start capturer failed");
            fclose(pFile);
            return false;
        }

        fflush(pFile);
        if (!audioCapturer->Flush()) {
            MEDIA_ERR_LOG("AudioCapturerTest: flush failed");
        }

        if (!audioCapturer->Stop()) {
            MEDIA_ERR_LOG("AudioCapturerTest: Stop failed");
        }

        if (!audioCapturer->Release()) {
            MEDIA_ERR_LOG("AudioCapturerTest: Release failed");
        }
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
    MEDIA_INFO_LOG("argv[1]=%{public}s", argv[AudioTestConstants::FIRST_ARG_IDX]);
    MEDIA_INFO_LOG("argv[2]=%{public}s", argv[AudioTestConstants::SECOND_ARG_IDX]);
    MEDIA_INFO_LOG("argv[3]=%{public}s", argv[AudioTestConstants::THIRD_ARG_IDX]);

    int32_t samplingRate = atoi(argv[AudioTestConstants::SECOND_ARG_IDX]);
    bool isBlocking = (atoi(argv[AudioTestConstants::THIRD_ARG_IDX]) == 1);
    string filePath = argv[AudioTestConstants::FIRST_ARG_IDX];
    if (argc > AudioTestConstants::FOURTH_ARG_IDX) {
        bufferMsec = strtol(argv[AudioTestConstants::FOURTH_ARG_IDX], nullptr, AudioTestConstants::NUM_BASE);
    }
    AudioCapturerTest testObj;
    bool ret = testObj.TestRecording(samplingRate, isBlocking, filePath);

    return ret;
}
