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
#ifndef AUDIO_INFO_H
#define AUDIO_INFO_H

#ifdef __MUSL__
#include <stdint.h>
#endif // __MUSL__

#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <unistd.h>

namespace OHOS {
namespace AudioStandard {
constexpr int32_t MAX_NUM_STREAMS = 3;
constexpr int32_t RENDERER_STREAM_USAGE_SHIFT = 16;
constexpr int32_t MINIMUM_BUFFER_SIZE_MSEC = 5;
constexpr int32_t MAXIMUM_BUFFER_SIZE_MSEC = 20;
constexpr int32_t MIN_SERVICE_COUNT = 2;
constexpr int32_t ROOT_UID = 0;
constexpr int32_t INVALID_UID = -1;
constexpr int32_t NETWORK_ID_SIZE = 80;
constexpr int32_t DEFAULT_VOLUME_GROUP_ID = 1;
constexpr int32_t DEFAULT_VOLUME_INTERRUPT_ID = 1;
const std::string MICROPHONE_PERMISSION = "ohos.permission.MICROPHONE";
const std::string MANAGE_AUDIO_CONFIG = "ohos.permission.MANAGE_AUDIO_CONFIG";
const std::string MODIFY_AUDIO_SETTINGS_PERMISSION = "ohos.permission.MODIFY_AUDIO_SETTINGS";
const std::string ACCESS_NOTIFICATION_POLICY_PERMISSION = "ohos.permission.ACCESS_NOTIFICATION_POLICY";
const std::string USE_BLUETOOTH_PERMISSION = "ohos.permission.USE_BLUETOOTH";
const std::string LOCAL_NETWORK_ID = "LocalDevice";

// Maximun number of sine waves in a tone segment
constexpr uint32_t TONEINFO_MAX_WAVES = 3;

// Maximun number of segments in a tone descriptor
constexpr uint32_t TONEINFO_MAX_SEGMENTS = 12;
constexpr uint32_t TONEINFO_INF = 0xFFFFFFFF;
class ToneSegment {
public:
    uint32_t duration;
    uint16_t waveFreq[TONEINFO_MAX_WAVES+1];
    uint16_t loopCnt;
    uint16_t loopIndx;
};

class ToneInfo {
public:
    ToneSegment segments[TONEINFO_MAX_SEGMENTS+1];
    uint32_t segmentCnt;
    uint32_t repeatCnt;
    uint32_t repeatSegment;
    ToneInfo() {}
};

enum DeviceFlag {
    /**
     * Device flag none.
     */
    NONE_DEVICES_FLAG = 0,
    /**
     * Indicates all output audio devices.
     */
    OUTPUT_DEVICES_FLAG = 1,
    /**
     * Indicates all input audio devices.
     */
    INPUT_DEVICES_FLAG = 2,
    /**
     * Indicates all audio devices.
     */
    ALL_DEVICES_FLAG = 3,
    /**
     * Indicates all distributed output audio devices.
     */
    DISTRIBUTED_OUTPUT_DEVICES_FLAG = 4,
    /**
     * Indicates all distributed input audio devices.
     */
    DISTRIBUTED_INPUT_DEVICES_FLAG = 8,
    /**
     * Indicates all distributed audio devices.
     */
    ALL_DISTRIBUTED_DEVICES_FLAG = 12,
    /**
     * Indicates all local and distributed audio devices.
     */
    ALL_L_D_DEVICES_FLAG = 15,
    /**
     * Device flag max count.
     */
    DEVICE_FLAG_MAX
};

enum DeviceRole {
    /**
     * Device role none.
     */
    DEVICE_ROLE_NONE = -1,
    /**
     * Input device role.
     */
    INPUT_DEVICE = 1,
    /**
     * Output device role.
     */
    OUTPUT_DEVICE = 2,
    /**
     * Device role max count.
     */
    DEVICE_ROLE_MAX
};

enum DeviceType {
    /**
     * Indicates device type none.
     */
    DEVICE_TYPE_NONE = -1,
    /**
     * Indicates invalid device
     */
    DEVICE_TYPE_INVALID = 0,
    /**
     * Indicates a built-in earpiece device
     */
    DEVICE_TYPE_EARPIECE = 1,
    /**
     * Indicates a speaker built in a device.
     */
    DEVICE_TYPE_SPEAKER = 2,
    /**
     * Indicates a headset, which is the combination of a pair of headphones and a microphone.
     */
    DEVICE_TYPE_WIRED_HEADSET = 3,
    /**
     * Indicates a pair of wired headphones.
     */
    DEVICE_TYPE_WIRED_HEADPHONES = 4,
    /**
     * Indicates a Bluetooth device used for telephony.
     */
    DEVICE_TYPE_BLUETOOTH_SCO = 7,
    /**
     * Indicates a Bluetooth device supporting the Advanced Audio Distribution Profile (A2DP).
     */
    DEVICE_TYPE_BLUETOOTH_A2DP = 8,
    /**
     * Indicates a microphone built in a device.
     */
    DEVICE_TYPE_MIC = 15,
    /**
     * Indicates a microphone built in a device.
     */
    DEVICE_TYPE_USB_HEADSET = 22,
    /**
     * Indicates a debug sink device
     */
    DEVICE_TYPE_FILE_SINK = 50,
    /**
     * Indicates a debug source device
     */
    DEVICE_TYPE_FILE_SOURCE = 51,
    /**
     * Indicates default device
     */
    DEVICE_TYPE_DEFAULT = 1000,
    /**
     * Indicates device type max count.
     */
    DEVICE_TYPE_MAX
};

enum ConnectType {
    /**
     * Group connect type of local device
     */
    CONNECT_TYPE_LOCAL = 0,
    /**
     * Group connect type of distributed device
     */
    CONNECT_TYPE_DISTRIBUTED
};

enum ActiveDeviceType {
    ACTIVE_DEVICE_TYPE_NONE = -1,
    SPEAKER = 2,
    BLUETOOTH_SCO = 7,
    FILE_SINK_DEVICE = 50,
    ACTIVE_DEVICE_TYPE_MAX
};

enum AudioStreamType {
    /**
     * Indicates audio streams default.
     */
    STREAM_DEFAULT = -1,
    /**
     * Indicates audio streams of voices in calls.
     */
    STREAM_VOICE_CALL = 0,
    /**
     * Indicates audio streams for music playback.
     */
    STREAM_MUSIC = 1,
    /**
     * Indicates audio streams for ringtones.
     */
    STREAM_RING = 2,
    /**
     * Indicates audio streams media.
     */
    STREAM_MEDIA = 3,
    /**
     * Indicates Audio streams for voice assistant
     */
    STREAM_VOICE_ASSISTANT = 4,
    /**
     * Indicates audio streams for system sounds.
     */
    STREAM_SYSTEM = 5,
    /**
     * Indicates audio streams for alarms.
     */
    STREAM_ALARM = 6,
    /**
     * Indicates audio streams for notifications.
     */
    STREAM_NOTIFICATION = 7,
    /**
     * Indicates audio streams for voice calls routed through a connected Bluetooth device.
     */
    STREAM_BLUETOOTH_SCO = 8,
    /**
     * Indicates audio streams for enforced audible.
     */
    STREAM_ENFORCED_AUDIBLE = 9,
    /**
     * Indicates audio streams for dual-tone multi-frequency (DTMF) tones.
     */
    STREAM_DTMF = 10,
    /**
     * Indicates audio streams exclusively transmitted through the speaker (text-to-speech) of a device.
     */
    STREAM_TTS =  11,
    /**
     * Indicates audio streams used for prompts in terms of accessibility.
     */
    STREAM_ACCESSIBILITY = 12,
    /**
     * Indicates special scene used for recording.
     */
    STREAM_RECORDING = 13,
    /**
     * Indicates audio streams used for only one volume bar of a device.
     */
    STREAM_ALL = 100
};

typedef AudioStreamType AudioVolumeType;

enum FocusType {
    /**
     * Recording type.
     */
    FOCUS_TYPE_RECORDING = 0,
};

enum API_VERSION {
    API_7 = 7,
    API_8 = 8,
    API_9 = 9
};

enum AudioErrors {
    /**
     * Common errors.
     */
    ERROR_INVALID_PARAM = 6800101,
    ERROR_NO_MEMORY     = 6800102,
    ERROR_ILLEGAL_STATE = 6800103,
    ERROR_UNSUPPORTED   = 6800104,
    ERROR_TIMEOUT       = 6800105,
    /**
     * Audio specific errors.
     */
    ERROR_STREAM_LIMIT  = 6800201,
    /**
     * Default error.
     */
    ERROR_SYSTEM        = 6800301
};

enum CommunicationDeviceType {
    /**
     * Speaker.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio.Communication
     */
    COMMUNICATION_SPEAKER = 2
};

enum InterruptMode {
    SHARE_MODE = 0,
    INDEPENDENT_MODE = 1
};

enum AudioEncodingType {
    ENCODING_PCM = 0,
    ENCODING_INVALID = -1
};

// Ringer Mode
enum AudioRingerMode {
    RINGER_MODE_NORMAL = 0,
    RINGER_MODE_SILENT = 1,
    RINGER_MODE_VIBRATE = 2
};

// format
enum AudioSampleFormat {
    SAMPLE_U8 = 0,
    SAMPLE_S16LE = 1,
    SAMPLE_S24LE = 2,
    SAMPLE_S32LE = 3,
    SAMPLE_F32LE = 4,
    INVALID_WIDTH = -1
};

// channel
enum AudioChannel {
    MONO = 1,
    STEREO = 2,
    CHANNEL_3 = 3,
    CHANNEL_4 = 4,
    CHANNEL_5 = 5,
    CHANNEL_6 = 6,
    CHANNEL_7 = 7,
    CHANNEL_8 = 8
};


/**
 * Enumerates the audio interrupt request type.
 */
enum InterruptRequestType {
    INTERRUPT_REQUEST_TYPE_DEFAULT = 0,
};

/**
 * Enumerates audio interrupt request result type.
 */
enum InterruptRequestResultType {
    INTERRUPT_REQUEST_GRANT = 0,
    INTERRUPT_REQUEST_REJECT = 1
};

// sampling rate
enum AudioSamplingRate {
    SAMPLE_RATE_8000 = 8000,
    SAMPLE_RATE_11025 = 11025,
    SAMPLE_RATE_12000 = 12000,
    SAMPLE_RATE_16000 = 16000,
    SAMPLE_RATE_22050 = 22050,
    SAMPLE_RATE_24000 = 24000,
    SAMPLE_RATE_32000 = 32000,
    SAMPLE_RATE_44100 = 44100,
    SAMPLE_RATE_48000 = 48000,
    SAMPLE_RATE_64000 = 64000,
    SAMPLE_RATE_96000 = 96000
};

typedef enum {
    /** Invalid audio source */
    AUDIO_SOURCE_INVALID = -1,
    /** Default audio source */
    AUDIO_SOURCE_DEFAULT = 0,
    /** Microphone */
    AUDIO_MIC = 1,
    /** Uplink voice */
    AUDIO_VOICE_UPLINK = 2,
    /** Downlink voice */
    AUDIO_VOICE_DOWNLINK = 3,
    /** Voice call */
    AUDIO_VOICE_CALL = 4,
    /** Camcorder */
    AUDIO_CAMCORDER = 5,
    /** Voice recognition */
    AUDIO_VOICE_RECOGNITION = 6,
    /** Voice communication */
    AUDIO_VOICE_COMMUNICATION = 7,
    /** Remote submix */
    AUDIO_REMOTE_SUBMIX = 8,
    /** Unprocessed audio */
    AUDIO_UNPROCESSED = 9,
    /** Voice performance */
    AUDIO_VOICE_PERFORMANCE = 10,
    /** Echo reference */
    AUDIO_ECHO_REFERENCE = 1997,
    /** Radio tuner */
    AUDIO_RADIO_TUNER = 1998,
    /** Hotword */
    AUDIO_HOTWORD = 1999,
    /** Extended remote submix */
    AUDIO_REMOTE_SUBMIX_EXTEND = 10007,
} AudioSourceType;

struct AudioStreamParams {
    uint32_t samplingRate;
    uint8_t encoding;
    uint8_t format;
    uint8_t channels;
};

/**
* Enumerates the audio content type.
*/
enum ContentType {
    CONTENT_TYPE_UNKNOWN = 0,
    CONTENT_TYPE_SPEECH = 1,
    CONTENT_TYPE_MUSIC = 2,
    CONTENT_TYPE_MOVIE = 3,
    CONTENT_TYPE_SONIFICATION = 4,
    CONTENT_TYPE_RINGTONE = 5
};

/**
* Enumerates the stream usage.
*/
enum StreamUsage {
    STREAM_USAGE_UNKNOWN = 0,
    STREAM_USAGE_MEDIA = 1,
    STREAM_USAGE_VOICE_COMMUNICATION = 2,
    STREAM_USAGE_VOICE_ASSISTANT = 3,
    STREAM_USAGE_NOTIFICATION_RINGTONE = 6
};

/**
* Enumerates the capturer source type
*/
enum SourceType {
    SOURCE_TYPE_INVALID = -1,
    SOURCE_TYPE_MIC,
    SOURCE_TYPE_VOICE_RECOGNITION = 1,
    SOURCE_TYPE_VOICE_COMMUNICATION = 7
};

/**
* Enumerates the renderer playback speed.
*/
enum AudioRendererRate {
    RENDER_RATE_NORMAL = 0,
    RENDER_RATE_DOUBLE = 1,
    RENDER_RATE_HALF = 2,
};

enum InterruptType {
    INTERRUPT_TYPE_BEGIN = 1,
    INTERRUPT_TYPE_END = 2,
};

enum InterruptHint {
    INTERRUPT_HINT_NONE = 0,
    INTERRUPT_HINT_RESUME,
    INTERRUPT_HINT_PAUSE,
    INTERRUPT_HINT_STOP,
    INTERRUPT_HINT_DUCK,
    INTERRUPT_HINT_UNDUCK
};

enum InterruptForceType {
    /**
     * Force type, system change audio state.
     */
    INTERRUPT_FORCE = 0,
    /**
     * Share type, application change audio state.
     */
    INTERRUPT_SHARE
};

enum ActionTarget {
    CURRENT = 0,
    INCOMING,
    BOTH
};

struct InterruptEvent {
    /**
     * Interrupt event type, begin or end
     */
    InterruptType eventType;
    /**
     * Interrupt force type, force or share
     */
    InterruptForceType forceType;
    /**
     * Interrupt hint type. In force type, the audio state already changed,
     * but in share mode, only provide a hint for application to decide.
     */
    InterruptHint hintType;
};

// Used internally only by AudioFramework
struct InterruptEventInternal {
    InterruptType eventType;
    InterruptForceType forceType;
    InterruptHint hintType;
    float duckVolume;
};

struct AudioFocusEntry {
    InterruptForceType forceType;
    InterruptHint hintType;
    ActionTarget actionOn;
    bool isReject;
};

struct AudioInterrupt {
    StreamUsage streamUsage;
    ContentType contentType;
    AudioStreamType streamType;
    uint32_t sessionID;
    bool pauseWhenDucked;
};

struct VolumeEvent {
    AudioStreamType volumeType;
    int32_t volume;
    bool updateUi;
    int32_t volumeGroupId;
    std::string networkId;
};

struct AudioParameters {
    AudioSampleFormat format;
    AudioChannel channels;
    AudioSamplingRate samplingRate;
    AudioEncodingType encoding;
    ContentType contentType;
    StreamUsage usage;
    DeviceRole deviceRole;
    DeviceType deviceType;
};

struct AudioStreamInfo {
    AudioSamplingRate samplingRate;
    AudioEncodingType encoding;
    AudioSampleFormat format;
    AudioChannel channels;
};

struct AudioRendererInfo {
    ContentType contentType = CONTENT_TYPE_UNKNOWN;
    StreamUsage streamUsage = STREAM_USAGE_UNKNOWN;
    int32_t rendererFlags = 0;
};

struct AudioCapturerInfo {
    SourceType sourceType = SOURCE_TYPE_INVALID;
    int32_t capturerFlags = 0;
};

struct AudioRendererDesc {
    ContentType contentType = CONTENT_TYPE_UNKNOWN;
    StreamUsage streamUsage = STREAM_USAGE_UNKNOWN;
};

struct AudioRendererOptions {
    AudioStreamInfo streamInfo;
    AudioRendererInfo rendererInfo;
};

struct MicStateChangeEvent {
    bool mute;
};

enum DeviceChangeType {
    CONNECT = 0,
    DISCONNECT = 1,
};

enum AudioScene {
    /**
     * Default audio scene
     */
    AUDIO_SCENE_DEFAULT,
    /**
     * Ringing audio scene
     * Only available for system api.
     */
    AUDIO_SCENE_RINGING,
    /**
     * Phone call audio scene
     * Only available for system api.
     */
    AUDIO_SCENE_PHONE_CALL,
    /**
     * Voice chat audio scene
     */
    AUDIO_SCENE_PHONE_CHAT,
};

struct AudioCapturerOptions {
    AudioStreamInfo streamInfo;
    AudioCapturerInfo capturerInfo;
};

struct AppInfo {
    int32_t appUid { INVALID_UID };
    uint32_t appTokenId { 0 };
    int32_t appPid { 0 };
};

// Supported audio parameters for both renderer and capturer
const std::vector<AudioSampleFormat> AUDIO_SUPPORTED_FORMATS {
    SAMPLE_U8,
    SAMPLE_S16LE,
    SAMPLE_S24LE,
    SAMPLE_S32LE
};

const std::vector<AudioChannel> RENDERER_SUPPORTED_CHANNELS {
    MONO,
    STEREO,
    CHANNEL_3,
    CHANNEL_4,
    CHANNEL_5,
    CHANNEL_6,
    CHANNEL_7,
    CHANNEL_8
};

const std::vector<AudioChannel> CAPTURER_SUPPORTED_CHANNELS {
    MONO,
    STEREO,
    CHANNEL_3,
    CHANNEL_4,
    CHANNEL_5,
    CHANNEL_6
};

const std::vector<AudioEncodingType> AUDIO_SUPPORTED_ENCODING_TYPES {
    ENCODING_PCM
};

const std::vector<AudioSamplingRate> AUDIO_SUPPORTED_SAMPLING_RATES {
    SAMPLE_RATE_8000,
    SAMPLE_RATE_11025,
    SAMPLE_RATE_12000,
    SAMPLE_RATE_16000,
    SAMPLE_RATE_22050,
    SAMPLE_RATE_24000,
    SAMPLE_RATE_32000,
    SAMPLE_RATE_44100,
    SAMPLE_RATE_48000,
    SAMPLE_RATE_64000,
    SAMPLE_RATE_96000
};

struct BufferDesc {
    uint8_t* buffer;
    size_t bufLength;
    size_t dataLength;
};

struct BufferQueueState {
    uint32_t numBuffers;
    uint32_t currentIndex;
};

enum AudioRenderMode {
    RENDER_MODE_NORMAL,
    RENDER_MODE_CALLBACK
};

enum AudioCaptureMode {
    CAPTURE_MODE_NORMAL,
    CAPTURE_MODE_CALLBACK
};

struct SinkInfo {
    uint32_t sinkId; // sink id
    std::string sinkName;
    std::string adapterName;
};

struct SinkInput {
    int32_t streamId;
    AudioStreamType streamType;

    // add for routing stream.
    int32_t uid; // client uid
    int32_t pid; // client pid
    uint32_t paStreamId; // streamId
    uint32_t deviceSinkId; // sink id
    std::string sinkName; // sink name
    int32_t statusMark; // mark the router status
    uint64_t startTime; // when this router is created
};

struct SourceOutput {
    int32_t streamId;
    AudioStreamType streamType;

    // add for routing stream.
    int32_t uid; // client uid
    int32_t pid; // client pid
    uint32_t paStreamId; // streamId
    uint32_t deviceSourceId; // sink id
    int32_t statusMark; // mark the router status
    uint64_t startTime; // when this router is created
};

typedef uint32_t AudioIOHandle;

static inline bool FLOAT_COMPARE_EQ(const float& x, const float& y)
{
    return (std::abs((x) - (y)) <= (std::numeric_limits<float>::epsilon()));
}

// Below APIs are added to handle compilation error in call manager
// Once call manager adapt to new interrupt APIs, this will be removed
enum InterruptActionType {
    TYPE_ACTIVATED = 0,
    TYPE_INTERRUPT = 1
};

struct InterruptAction {
    InterruptActionType actionType;
    InterruptType interruptType;
    InterruptHint interruptHint;
    bool activated;
};

enum AudioServiceIndex {
    HDI_SERVICE_INDEX = 0,
    AUDIO_SERVICE_INDEX
};

/**
 * @brief Enumerates the rendering states of the current device.
 */
enum RendererState {
    /** INVALID state */
    RENDERER_INVALID = -1,
    /** Create New Renderer instance */
    RENDERER_NEW,
    /** Reneder Prepared state */
    RENDERER_PREPARED,
    /** Rendere Running state */
    RENDERER_RUNNING,
    /** Renderer Stopped state */
    RENDERER_STOPPED,
    /** Renderer Released state */
    RENDERER_RELEASED,
    /** Renderer Paused state */
    RENDERER_PAUSED
};

/**
 * @brief Enumerates the capturing states of the current device.
 */
enum CapturerState {
    /** Capturer INVALID state */
    CAPTURER_INVALID = -1,
    /** Create new capturer instance */
    CAPTURER_NEW,
    /** Capturer Prepared state */
    CAPTURER_PREPARED,
    /** Capturer Running state */
    CAPTURER_RUNNING,
    /** Capturer Stopped state */
    CAPTURER_STOPPED,
    /** Capturer Released state */
    CAPTURER_RELEASED,
    /** Capturer Paused state */
    CAPTURER_PAUSED
};

enum AudioMode {
    AUDIO_MODE_PLAYBACK,
    AUDIO_MODE_RECORD
};

struct DeviceInfo {
    DeviceType deviceType;
    DeviceRole deviceRole;
    int32_t deviceId;
    int32_t channelMasks;
    std::string deviceName;
    std::string macAddress;
    AudioStreamInfo audioStreamInfo;
    std::string networkId;
};

enum StreamSetState {
    STREAM_PAUSE,
    STREAM_RESUME
};

struct StreamSetStateEventInternal {
    StreamSetState streamSetState;
    AudioStreamType audioStreamType;
};

struct AudioRendererChangeInfo {
    int32_t clientUID;
    int32_t sessionId;
    AudioRendererInfo rendererInfo;
    RendererState rendererState;
    DeviceInfo outputDeviceInfo;
};

struct AudioCapturerChangeInfo {
    int32_t clientUID;
    int32_t sessionId;
    AudioCapturerInfo capturerInfo;
    CapturerState capturerState;
    DeviceInfo inputDeviceInfo;
};

struct AudioStreamChangeInfo {
    AudioRendererChangeInfo audioRendererChangeInfo;
    AudioCapturerChangeInfo audioCapturerChangeInfo;
};

enum AudioPin {
    AUDIO_PIN_NONE = 0,
    AUDIO_PIN_OUT_SPEAKER = 1,
    AUDIO_PIN_OUT_HEADSET = 2,
    AUDIO_PIN_OUT_LINEOUT = 4,
    AUDIO_PIN_OUT_HDMI = 8,
    AUDIO_PIN_OUT_USB = 16,
    AUDIO_PIN_OUT_USB_EXT = 32,
    AUDIO_PIN_OUT_DAUDIO_DEFAULT = 64,
    AUDIO_PIN_IN_MIC = 134217729,
    AUDIO_PIN_IN_HS_MIC = 134217730,
    AUDIO_PIN_IN_LINEIN = 134217732,
    AUDIO_PIN_IN_USB_EXT = 134217736,
    AUDIO_PIN_IN_DAUDIO_DEFAULT = 134217744,
};

enum AudioParamKey {
    NONE = 0,
    VOLUME = 1,
    INTERRUPT = 2,
    RENDER_STATE = 5,
    PARAM_KEY_LOWPOWER = 1000,
};

struct DStatusInfo {
    char networkId[NETWORK_ID_SIZE];
    AudioPin hdiPin = AUDIO_PIN_NONE;
    int32_t mappingVolumeId = 0;
    int32_t mappingInterruptId = 0;
    int32_t deviceId;
    int32_t channelMasks;
    std::string deviceName = "";
    bool isConnected = false;
    std::string macAddress;
    AudioStreamInfo streamInfo = {};
    ConnectType connectType = CONNECT_TYPE_LOCAL;
};

struct AudioRendererDataInfo {
    uint8_t *buffer;
    size_t flag;
};

enum AudioPermissionState {
    AUDIO_PERMISSION_START = 0,
    AUDIO_PERMISSION_STOP = 1,
};

enum StateChangeCmdType {
    CMD_FROM_CLIENT = 0,
    CMD_FROM_SYSTEM = 1
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_INFO_H
