# Audio<a name="EN-US_TOPIC_0000001146901937"></a>

  - [Introduction](#introduction)
    - [Basic Concepts](#basic-concepts)
  - [Directory Structure](#directory-structure)
  - [Usage Guidelines](#usage-guidelines)
    - [Audio Playback](#audio-playback)
    - [Audio Recording](#audio-recording)
    - [Audio Management](#audio-management)
  - [Repositories Involved](#repositories-involved)

## Introduction<a name="introduction"></a>
The **audio\_standard** repository is used to implement audio-related features, including audio playback, recording, volume management and device management.

**Figure  1**  Position in the subsystem architecture<a name="fig483116248288"></a>  


![](figures/en-us_image_0000001152315135.png)

### Basic Concepts<a name="basic-concepts"></a>

-   **Sampling**

Sampling is a process to obtain discrete-time signals by extracting samples from analog signals in a continuous time domain at a specific interval.

-   **Sampling rate**

Sampling rate is the number of samples extracted from a continuous signal per second to form a discrete signal. It is measured in Hz. Generally, human hearing range is from 20 Hz to 20 kHz. Common audio sampling rates include 8 kHz, 11.025 kHz, 22.05 kHz, 16 kHz, 37.8 kHz, 44.1 kHz, 48 kHz, and 96 kHz.

-   **Channel**

Channels refer to different spatial positions where independent audio signals are recorded or played. The number of channels is the number of audio sources used during audio recording, or the number of speakers used for audio playback.

-   **Audio frame**

Audio data is in stream form. For the convenience of audio algorithm processing and transmission, it is generally agreed that a data amount in a unit of 2.5 to 60 milliseconds is one audio frame. This unit is called sampling time, and its length is specific to codecs and the application requirements.

-   **PCM**

Pulse code modulation \(PCM\) is a method used to digitally represent sampled analog signals. It converts continuous-time analog signals into discrete-time digital signal samples.

## Directory Structure<a name="directory-structure"></a>

The structure of the repository directory is as follows:

```
/foundation/multimedia/audio_standard  # Audio code
├── frameworks                         # Framework code
│   ├── innerkitsimpl                  # Internal Native API Implementation.
|   |                                    Pulseaudio, libsndfile build configuration and pulseaudio-hdi modules
│   └── kitsimpl                       # External JS API Implementation
├── interfaces                         # Interfaces
│   ├── innerkits                      # Internal Native APIs
│   └── kits                           # External JS APIs
├── sa_profile                         # Service configuration profile
├── services                           # Service code
├── LICENSE                            # License file
└── ohos.build                         # Build file
```

## Usage Guidelines<a name="usage-guidelines"></a>
### Audio Playback<a name="audio-playback"></a>
You can use APIs provided in this repository to convert audio data into audible analog signals, play the audio signals using output devices, and manage playback tasks. The following steps describe how to use  **AudioRenderer**  to develop the audio playback function:
1. Use **Create** API with required stream type to get **AudioRenderer** instance.
    ```
    AudioStreamType streamType = STREAM_MUSIC; // example stream type
    std::unique_ptr<AudioRenderer> audioRenderer = AudioRenderer::Create(streamType);
    ```
2. (Optional) Static APIs **GetSupportedFormats**(), **GetSupportedChannels**(), **GetSupportedEncodingTypes**(), **GetSupportedSamplingRates**() can be used to get the supported values of the params.
3. To Prepare the device, call **SetParams** on the instance.
    ```
    AudioRendererParams rendererParams;
    rendererParams.sampleFormat = SAMPLE_S16LE;
    rendererParams.sampleRate = SAMPLE_RATE_44100;
    rendererParams.channelCount = STEREO;
    rendererParams.encodingType = ENCODING_PCM;

    audioRenderer->SetParams(rendererParams);
    ```
4. (Optional) use audioRenderer->**GetParams**(rendererParams) to validate SetParams
5. Call **audioRenderer->Start()** function on the AudioRenderer instance to start the playback task.
6. Get the buffer length to be written, using **GetBufferSize** API .
    ```
    audioRenderer->GetBufferSize(bufferLen);
    ```
7. Read the audio data to be played from the source(for example, an audio file) and transfer it into the bytes stream. Call the **Write** function repeatedly to write the render data.
    ```
    bytesToWrite = fread(buffer, 1, bufferLen, wavFile);
    while ((bytesWritten < bytesToWrite) && ((bytesToWrite - bytesWritten) > minBytes)) {
        bytesWritten += audioRenderer->Write(buffer + bytesWritten, bytesToWrite - bytesWritten);
        if (bytesWritten < 0)
            break;
    }
    ```
8. Call audioRenderer->**Drain**() to drain the playback stream.

9. Call audioRenderer->**Stop()** function to Stop rendering.
10. After the playback task is complete, call the audioRenderer->**Release**() function on the AudioRenderer instance to release the resources.

Provided the basic playback usecase above. Please refer [**audio_renderer.h**](https://gitee.com/openharmony/multimedia_audio_standard/blob/master/interfaces/innerkits/native/audiorenderer/include/audio_renderer.h) and [**audio_info.h**](https://gitee.com/openharmony/multimedia_audio_standard/blob/master/interfaces/innerkits/native/audiocommon/include/audio_info.h) for more APIs.


### Audio Recording<a name="audio-recording"></a>
You can use the APIs provided in this repository for your application to record voices using input devices, convert the voices into audio data, and manage recording tasks. The following steps describe how to use AudioReorder to develop the audio recording function:

1. Use **Create** API with required stream type to get **AudioRecorder** instance.
    ```
    AudioStreamType streamType = STREAM_MUSIC;
    std::unique_ptr<AudioRecorder> audioRecorder = AudioRecorder::Create(streamType);
    ```
2. (Optional) Static APIs **GetSupportedFormats**(), **GetSupportedChannels**(), **GetSupportedEncodingTypes**(), **GetSupportedSamplingRates()** can be used to get the supported values of the params.
3. To Prepare the device, call **SetParams** on the instance.
    ```
    AudioRecorderParams recorderParams;
    recorderParams.sampleFormat = SAMPLE_S16LE;
    recorderParams.sampleRate = SAMPLE_RATE_44100;
    recorderParams.channelCount = STEREO;
    recorderParams.encodingType = ENCODING_PCM;

    audioRecorder->SetParams(recorderParams);
    ```
4. (Optional) use audioRecorder->**GetParams**(recorderParams) to validate SetParams()
5. Call audioRenderer->**Start**() function on the AudioRecorder instance to start the recording task.
6. Get the buffer length to be read, using **GetBufferSize** API. 
    ```
    audioRecorder->GetBufferSize(bufferLen);
    ```
7. Read the recorded audio data and convert it to a byte stream. Call the read function repeatedly to read data untill you want to stop recording
    ```
    bytesRead = audioRecorder->Read(*buffer, bufferLen, isBlocking); // set isBlocking = true/false for blocking/non-blocking read
    while (numBuffersToRecord) {
        bytesRead = audioRecorder->Read(*buffer, bufferLen, isBlockingRead);
        if (bytesRead < 0) {
            break;
        } else if (bytesRead > 0) {
            fwrite(buffer, size, bytesRead, recFile); // example shows writes the recored data into a file
            numBuffersToRecord--;
        }
    }
    ```
8. (Optional) Call audioRecorder->**Flush**() to flush the record buffer of this stream.
9. Call the audioRecorder->**Stop**() function on the AudioRecorder instance to stop the recording.
10. After the recording task is complete, call the audioRecorder->**Release**() function on the AudioRecorder instance to release resources.

Provided the basic recording usecase above. Please refer [**audio_recorder.h**](https://gitee.com/openharmony/multimedia_audio_standard/blob/master/interfaces/innerkits/native/audiorecorder/include/audio_recorder.h) and [**audio_info.h**](https://gitee.com/openharmony/multimedia_audio_standard/blob/master/interfaces/innerkits/native/audiocommon/include/audio_info.h) for more APIs.

### Audio Management<a name="audio-management"></a>

JS apps can use the APIs provided by audio manager to control the volume and the device.\
Please refer [**audio-management.md**](https://gitee.com/openharmony/docs/blob/master/en/application-dev/js-reference/audio-management.md) for JS usage of audio volume and device management.

## Repositories Involved<a name="repositories-involved"></a>

[multimedia\_audio\_standard](https://gitee.com/openharmony/multimedia_audio_standard)\
[multimedia\_media\_standard](https://gitee.com/openharmony/multimedia_media_standard)
