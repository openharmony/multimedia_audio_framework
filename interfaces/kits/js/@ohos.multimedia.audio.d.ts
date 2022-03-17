/*
* Copyright (c) 2021-2022 Huawei Device Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

import {ErrorCallback, AsyncCallback, Callback} from './basic';
import Context from './@ohos.ability';

/**
 * @name audio
 * @SysCap SystemCapability.Multimedia.Audio
 * @devices phone, tablet, wearable, car
 * @since 7
 */
declare namespace audio {

  /**
   * Gets the AudioManager instance.
   * @param context Current application context.
   * @return AudioManager instance.
   * @since 8
   */
  function getAudioManager(context?: Context): AudioManager;

  /**
   * the type of audio stream
   * @since 7
   */
  enum AudioVolumeType {
    /**
     * the ringtone stream
     * @since 7
     */
    VOICE_CALL = 0,
    /**
     * the ringtone stream
     * @since 7
     */
    RINGTONE = 2,
    /**
     * the media stream
     * @since 7
     */
    MEDIA = 3,
  }

  /**
   * the flag type of device
   * @since 7
   */
  enum DeviceFlag {
    /**
     * the device flag of output
     * @since 7
     */
    OUTPUT_DEVICES_FLAG = 1,
    /**
     * the device flag of input
     * @since 7
     */
    INPUT_DEVICES_FLAG = 2,
    /**
     * the device flag of all devices
     * @since 7
     */
    ALL_DEVICES_FLAG = 3,
  }
  /**
   * the role of device
   */
  enum DeviceRole {
    /**
     * the role of input devices
     * @since 7
     */
    INPUT_DEVICE = 1,
    /**
     * the role of output devices
     * @since 7
     */
    OUTPUT_DEVICE = 2,
  }
  /**
   * the type of device
   * @since 7
   */
  enum DeviceType {
    /**
     * invalid
     * @since 7
     */
    INVALID = 0,
    /**
     * speaker
     * @since 7
     */
    SPEAKER = 2,
    /**
     * wired headset
     * @since 7
     */
    WIRED_HEADSET = 3,
    /**
     * bluetooth sco
     * @since 7
     */
    BLUETOOTH_SCO = 7,
    /**
     * bluetooth a2dp
     * @since 7
     */
    BLUETOOTH_A2DP = 8,
    /**
     * mic
     * @since 7
     */
    MIC = 15,
  }

  /**
   * Enumerates device types.
   * @devices phone, tablet, tv, wearable, car
   * @since 7
   * @SysCap SystemCapability.Multimedia.Audio
   */
  enum ActiveDeviceType {
    /**
     * Speaker.
     */
    SPEAKER = 2,

    /**
     * Bluetooth.
     */
    BLUETOOTH_SCO = 7,
  }

  /**
   * Enumerates ringer modes.
   * @devices phone, tablet, tv, car
   * @since 7
   * @SysCap SystemCapability.Multimedia.Audio
   */
  enum AudioRingMode {
    /**
     * Silent mode.
     */
    RINGER_MODE_SILENT = 0,

    /**
     * Vibration mode.
     */
    RINGER_MODE_VIBRATE = 1,

    /**
     * Normal mode.
     */
    RINGER_MODE_NORMAL = 2,
  }

  /**
   * Enumerates device change types.
   * @devices phone, tablet, tv, wearable, car
   * @since 7
   * @SysCap SystemCapability.Multimedia.Audio
   */
  enum DeviceChangeType {
    /**
     * Device connection.
     */
    CONNECT = 0,

    /**
     * Device disconnection.
     */
    DISCONNECT = 1,
  }

  /**
   * Describes the device change type and device information.
   */
  interface DeviceChangeAction {
    /**
     * Device change type.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     */
    type: DeviceChangeType;

    /**
     * Device information.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     */
    deviceDescriptors: AudioDeviceDescriptors;
  }

  /**
   * the audiomanager of the audio
   * @since 7
   */
  interface AudioManager {
    /**
     * set the volume of the audiovolumetype
     * @since 7
     */
    setVolume(audioType: AudioVolumeType,volume: number, callback: AsyncCallback<void>): void;
    /**
     * set the volume of the audiovolumetype
     * @since 7
     */
    setVolume(audioType: AudioVolumeType,volume: number): Promise<void>;
    /**
     * get the volume of the audiovolumetype
     * @since 7
     */
    getVolume(audioType: AudioVolumeType, callback: AsyncCallback<number>): void;
    /**
     * get the volume of the audiovolumetype
     * @since 7
     */
    getVolume(audioType: AudioVolumeType): Promise<number>;
    /**
     * get the min volume of the audiovolumetype
     * @since 7
     */
    getMinVolume(audioType: AudioVolumeType, callback: AsyncCallback<number>): void
    /**
     * get the min volume of the audiovolumetype
     * @since 7
     */
    getMinVolume(audioType: AudioVolumeType): Promise<number>;
    /**
     * get the max volume of the audiovolumetype
     * @since 7
     */
    getMaxVolume(audioType: AudioVolumeType, callback: AsyncCallback<number>): void
    /**
     * get the max volume of the audiovolumetype
     * @since 7
     */
    getMaxVolume(audioType: AudioVolumeType): Promise<number>;
    /**
     * Determines whether audio streams of a given type are muted.
     * @devices phone, tablet, tv, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param volumeType Audio stream type.
     * @param callback Callback used to return the mute status of the stream. The value true means that the stream
     * is muted,and false means the opposite.
     */
    isMute(volumeType: AudioVolumeType, callback: AsyncCallback<boolean>): void;
    /**
     * Checks whether a stream is muted. This method uses an asynchronous callback to return the query result.
     * @devices phone, tablet, tv, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param volumeType Audio stream type.
     * @return return A Promise instance used to return the mute status of the stream. The value true means that the
     * stream is muted, and false means the opposite.
     */
    isMute(volumeType: AudioVolumeType): Promise<boolean>;

    /**
     * Checks whether a stream is muted. This method uses a promise to return the query result.
     * @devices phone, tablet, tv, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param volumeType Audio stream type.
     * @param mute Whether to mute the stream. The value true meas to mute the stream, and false means the opposite.
     * @param callback Callback used to return the execution result.
     */
    mute(volumeType: AudioVolumeType, mute: boolean, callback: AsyncCallback<void>): void;
    /**
     * Mutes or unmutes a stream. This method uses a promise to return the execution result.
     * @devices phone, tablet, tv, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param volumeType Audio stream type.
     * @param mute Whether to mute the stream. The value true meas to mute the stream, and false means the opposite.
     * @return return A Promise instance used to return the execution result.
     */
    mute(volumeType: AudioVolumeType, mute: boolean): Promise<void>;
    /**
     * Checks whether a stream is active, that is, whether a stream is being played rather than being stopped or paused.
     * This method uses an asynchronous callback to return the query result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param volumeType Audio stream type.
     * @param callback Callback used to return the stream activation status. The value true means that the stream is
     * active,and false means the opposite.
     */
    isActive(volumeType: AudioVolumeType, callback: AsyncCallback<boolean>): void;
    /**
     * Checks whether a stream is active, that is, whether a stream is being played rather than being stopped or paused.
     * This method uses a promise to return the query result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param volumeType Audio stream type.
     * @return return A Promise instance used to return the stream activation status. The value true means that the
     * stream is active,and false means the opposite.
     */
    isActive(volumeType: AudioVolumeType): Promise<boolean>;
    /**
     * Checks whether the microphone is muted. This method uses an asynchronous callback to return the query result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param callback Callback used to return the mute status of the microphone. The value true means that
     * the microphone is muted, and false means the opposite.
     * @permission "ohos.permission.MICROPHONE"
     */
    isMicrophoneMute(callback: AsyncCallback<boolean>): void;
    /**
     * Checks whether the microphone is muted. This method uses a promise to return the query result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @return return A Promise instance used to return the mute status of the microphone. The value true means
     * that the microphone is muted, and false means the opposite.
     * @permission "ohos.permission.MICROPHONE"
     */
    isMicrophoneMute(): Promise<boolean>;
    /**
     * Mutes or unmutes the microphone. This method uses an asynchronous callback to return the execution result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param mute Mute status to set. The value true means to mute the microphone, and false means the opposite.
     * @param callback Callback used to return the execution result.
     * @permission "ohos.permission.MICROPHONE"
     */
    setMicrophoneMute(mute: boolean, callback: AsyncCallback<void>): void;
    /**
     * Mutes or unmutes the microphone. This method uses a promise to return the execution result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param mute Mute status to set. The value true means to mute the microphone, and false means the opposite.
     * @return return A Promise instance used to return the execution result.
     * @permission "ohos.permission.MICROPHONE"
     */
    setMicrophoneMute(mute: boolean): Promise<void>;
    /**
     * get the device list of the audio devices by the audio flag
     * @since 7
     */
    getDevices(deviceFlag: DeviceFlag, callback: AsyncCallback<AudioDeviceDescriptors>): void;
    /**
     * get the device list of the audio devices by the audio flag
     * @since 7
     */
    getDevices(deviceFlag: DeviceFlag): Promise<AudioDeviceDescriptors>;
    /**
     * Checks whether a type of device is active. This method uses an asynchronous callback to return the
     * query result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param deviceType Audio device type.
     * @param callback Callback used to return the active status of the device.
     */
    isDeviceActive(deviceType: ActiveDeviceType, callback: AsyncCallback<boolean>): void;
    /**
     * Checks whether a type of device is active. This method uses a promise to return the query result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param deviceType Audio device type.
     * @return return A Promise instance used to return the active status of the device.
     */
    isDeviceActive(deviceType: ActiveDeviceType): Promise<boolean>;
    /**
     * Sets a type of device to the active state. This method uses an asynchronous callback to return the
     * execution result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param deviceType Audio device type.
     * @param active Active status to set. The value true means to set the device to the active status,
     * and false means the opposite.
     * @param callback Callback used to return the execution result.
     */
    setDeviceActive(deviceType: ActiveDeviceType, active: boolean, callback: AsyncCallback<void>): void;
    /**
     * Sets a type of device to the active state. This method uses a promise to return the execution result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
      * @SysCap SystemCapability.Multimedia.Audio
     * @param deviceType Audio device type.
     * @param active Active status to set. The value true means to set the device to the active status,
     * and false means the opposite
     * @return return A Promise instance used to return the execution result.
     */
    setDeviceActive(deviceType: ActiveDeviceType, active: boolean): Promise<void>;
    /**
     * Sets an audio parameter. This method uses an asynchronous callback to return the execution result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param key Key of the audio parameter to set.
     * @param value Value of the audio parameter to set.
     * @param callback Callback used to return the execution result.
     * @permission "ohos.permission.MODIFY_AUDIO_SETTINGS"
     */
    setAudioParameter(key: string, value: string, callback: AsyncCallback<void>): void;
    /**
     * Sets an audio parameter. This method uses a promise to return the execution result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param key Key of the audio parameter to set.
     * @param value Value of the audio parameter to set.
     * @return return A Promise instance used to return the execution result.
     * @permission "ohos.permission.MODIFY_AUDIO_SETTINGS"
     */
    setAudioParameter(key: string, value: string): Promise<void>;
    /**
     * Obtains the value of an audio parameter. This method uses an asynchronous callback to return the query result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param key Key of the audio parameter whose value is to be obtained.
     * @param callback Callback used to return the value of the audio parameter.
     */
    getAudioParameter(key: string, callback: AsyncCallback<string>): void;
    /**
     * Obtains the value of an audio parameter. This method uses a promise to return the query result.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param key Key of the audio parameter whose value is to be obtained.
     * @return return A Promise instance used to return the value of the audio parameter.
     */
    getAudioParameter(key: string): Promise<string>;
    /**
     * Sets the ringer mode. This method uses an asynchronous callback to return the execution result.
     * @devices phone, tablet, tv, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param mode Ringer mode to set.
     * @param callback Callback used to return the execution result.
     * @permission "ohos.permission.ACCESS_NOTIFICATION_POLICY"
     */
    setRingerMode(mode: AudioRingMode, callback: AsyncCallback<void>): void;
    /**
     * Sets the ringer mode. This method uses a promise to return the execution result.
     * @devices phone, tablet, tv, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param mode Ringer mode to set.
     * @return return A Promise instance used to return the execution result.
     * @permission "ohos.permission.ACCESS_NOTIFICATION_POLICY"
     */
    setRingerMode(mode: AudioRingMode): Promise<void>;
    /**
     * Obtains the ringer mode. This method uses an asynchronous callback to return the query result.
     * @devices phone, tablet, tv, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param callback Callback used to return the ringer mode.
     */
    getRingerMode(callback: AsyncCallback<AudioRingMode>): void;
    /**
     * Obtains the ringer mode. This method uses a promise to return the query result.
     * @devices phone, tablet, tv, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @return return A Promise instance used to return the ringer mode.
     */
    getRingerMode(): Promise<AudioRingMode>;
    /**
     * Listens for device change events.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param type Type of the event to listen for. Only the deviceChange event is supported.
     * This event is triggered when a device change is detected.
     * @param callback Callback invoked for the device change event.
     */
    on(type: 'deviceChange', callback: Callback<DeviceChangeAction>): void;
    /**
     * Cancels the listening of device change events.
     * @devices phone, tablet, tv, wearable, car
     * @since 7
     * @SysCap SystemCapability.Multimedia.Audio
     * @param type Type of the event for which the listening is to be canceled. Only the deviceChange event is supported.
     * @param callback Callback invoked for the device change event.
     */
    off(type: 'deviceChange', callback?: Callback<DeviceChangeAction>): void;
    /**
     * Set audio scene mode to change audio strategy.
     * @param scene audio scene mode to set.
     * @since 8
     */
    setAudioScene(scene: AudioScene, callback: AsyncCallback<void>): void;
    setAudioScene(scene: AudioScene): Promise<void>;

    /**
     * Get system audio scene mode.
     * @return Current audio scene mode.
     * @since 8
     */
    getAudioScene(callback: AsyncCallback<AudioScene>): void;
    getAudioScene(): Promise<AudioScene>;

    /**
     * Subscribes volume change event callback, only for system
     * @param type Event type.
     * @return Volume value callback.
     * @since 8
     */
    on(type: 'volumeChange', volumeType: AudioVolumeType, callback: Callback<number>): void;

    /**
     * Subscribes volume change event callback, only for system
     * @param type Event type.
     * @return Volume value callback.
     * @since 8
     */
    on(type: 'ringerModeChange', callback: Callback<AudioRingMode>): void;
  }

  /**
   * Enum for audio scene.
   * @since 8
   */
  enum AudioScene {
    /**
     * Default audio scene.
     * @since 8
     */
    AUDIO_SCENE_DEFAULT,
    /**
     * Ringing audio scene.
     * Only available for system api.
     * @since 8
     */
    AUDIO_SCENE_RINGING,
    /**
     * Phone call audio scene.
     * Only available for system api.
     * @since 8
     */
    AUDIO_SCENE_PHONE_CALL,
    /**
     * Voice chat audio scene.
     * @since 8
     */
    AUDIO_SCENE_VOICE_CHAT
  }

  /**
   * the Descriptor of the device
   * @since 7
   */
  interface AudioDeviceDescriptor {
    /**
     * the role of device
     * @since 7
     */
    readonly deviceRole: DeviceRole;
    /**
     * the type of device
     * @since 7
     */
    readonly deviceType: DeviceType;
  }

  /**
   * the Descriptor list of the devices
   * @since 7
   */
  type AudioDeviceDescriptors = Array<Readonly<AudioDeviceDescriptor>>;

  /**
   * Creates a AudioRenderer instance.
   * @param options All options used for audio renderer.
   * @return AudioRenderer instance.
   * @since 8
   */
  function createAudioRenderer(options: AudioRendererOptions, callback: AsyncCallback<AudioRenderer>): void;
  function createAudioRenderer(options: AudioRendererOptions): Promise<AudioRenderer>;

  /**
   * Creates a AudioCapturer instance.
   * @param options All options used for audio capturer.
   * @return AudioCapturer instance.
   * @since 8
   */
  function createAudioCapturer(options: AudioCapturerOptions, callback: AsyncCallback<AudioCapturer>): void;
  function createAudioCapturer(options: AudioCapturerOptions): Promise<AudioCapturer>;

  /**
   * Enumerates the rendering states of the current device.
   * @since 8
   */
  enum AudioState {
    /**
     * Invalid state.
     * @since 8
     */
    STATE_INVALID = -1,
    /**
     * Create New instance state.
     * @since 8
     */
    STATE_NEW,
    /**
     * Prepared state.
     * @since 8
     */
    STATE_PREPARED,
    /**
     * Running state.
     * @since 8
     */
    STATE_RUNNING,
    /**
     * Stopped state.
     * @since 8
     */
    STATE_STOPPED,
    /**
     * Released state.
     * @since 8
     */
    STATE_RELEASED,
    /**
     * Paused state.
     * @since 8
     */
    STATE_PAUSED
  }
  /**
   * Enumerates the sample format.
   * @since 8
   */
  enum AudioSampleFormat {
    SAMPLE_FORMAT_INVALID = -1,
    SAMPLE_FORMAT_U8 = 0,
    SAMPLE_FORMAT_S16LE = 1,
    SAMPLE_FORMAT_S24LE = 2,
    SAMPLE_FORMAT_S32LE = 3,
  }

  /**
   * Enumerates the audio channel.
   * @since 8
   */
  enum AudioChannel {
    CHANNEL_1 = 0x1 << 0,
    CHANNEL_2 = 0x1 << 1,
    CHANNEL_3 = 0x1 << 2,
    CHANNEL_4 = 0x1 << 3,
    CHANNEL_6 = 0x1 << 4,
    CHANNEL_7 = 0x1 << 5,
    CHANNEL_8 = 0x1 << 6
  }

  /**
   * Enumerates the audio sampling rate.
   * @since 8
   */
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
  }

  /**
   * Enumerates the audio encoding type.
   * @since 8
   */
  enum AudioEncodingType {
    /**
     * Invalid type.
     * @since 8
     */
    ENCODING_TYPE_INVALID = -1,
    /**
     * Raw pcm type.
     * @since 8
     */
    ENCODING_TYPE_RAW = 0,
    /**
     * Mp3 encoding type.
     * @since 8
     */
    ENCODING_TYPE_MP3 = 1,
  }

  /**
   * Enumerates the audio content type.
   * @since 8
   */
  enum ContentType {
    /**
     * Unknown content.
     * @since 8
     */
    CONTENT_TYPE_UNKNOWN = 0,
    /**
     * Speech content.
     * @since 8
     */
    CONTENT_TYPE_SPEECH = 1,
    /**
     * Music content.
     * @since 8
     */
    CONTENT_TYPE_MUSIC = 2,
    /**
     * Movie content.
     * @since 8
     */
    CONTENT_TYPE_MOVIE = 3,
    /**
     * Notification content.
     * @since 8
     */
    CONTENT_TYPE_SONIFICATION = 4,
    /**
     * Ringtone content.
     * @since 8
     */
    CONTENT_TYPE_RINGTONE = 5,
  }

  /**
   * Enumerates the stream usage.
   */
  enum StreamUsage {
    /**
     * Unkown usage.
     * @since 8
     */
    STREAM_USAGE_UNKNOWN = 0,
    /**
     * Media usage.
     * @since 8
     */
    STREAM_USAGE_MEDIA = 1,
    /**
     * Voice communication usage.
     * @since 8
     */
    STREAM_USAGE_VOICE_COMMUNICATION = 2,
    /**
     * Notification or ringtone usage.
     * @since 8
     */
    STREAM_USAGE_NOTIFICATION_RINGTONE = 3,
  }

  /**
   * Interface for audio stream info
   * @since 8
   */
  interface AudioStreamInfo {
    /**
     * Audio sampling rate
     * @since 8
     */
    samplingRate: AudioSamplingRate;
    /**
     * Audio channels
     * @since 8
     */
    channels: AudioChannel;
    /**
     * Audio sample format
     * @since 8
     */
    sampleFormat: AudioSampleFormat;
    /**
     * Audio encoding type
     * @since 8
     */
    encodingType: AudioEncodingType;
  }

  /**
   * Interface for audio renderer info
   * @since 8
   */
  interface AudioRendererInfo {
    /**
     * Audio content type
     * @since 8
     */
    content: ContentType;
    /**
     * Audio stream usage
     * @since 8
     */
    usage: StreamUsage;
    /**
     * Audio renderer flags
     * @since 8
     */
    rendererFlags: number;
  }

  /**
   * Interface for audio renderer options
   * @since 8
   */
  interface AudioRendererOptions {
    /**
     * Audio stream info
     * @since 8
     */
    streamInfo: AudioStreamInfo;
    /**
     * Audio renderer info
     * @since 8
     */
    rendererInfo: AudioRendererInfo;
  }

  /**
   * Enum for audio renderer rate
   * @since 8
   */
  enum AudioRendererRate {
    /**
     * Normal rate
     * @since 8
     */
    RENDER_RATE_NORMAL = 0,
    /**
     * Double rate
     * @since 8
     */
    RENDER_RATE_DOUBLE = 1,
    /**
     * Half rate
     * @since 8
     */
    RENDER_RATE_HALF = 2
  }

  /**
   * Enumerates audio interruption event types.
   * @devices phone, tablet, tv, wearable, car
   * @since 7
   * @SysCap SystemCapability.Multimedia.Audio
   */
  enum InterruptType {
    /**
     * An audio interruption event starts.
     */
    INTERRUPT_TYPE_BEGIN = 1,

    /**
     * An audio interruption event ends.
     */
    INTERRUPT_TYPE_END = 2
  }

  /**
   * Enumerates the types of hints for audio interruption.
   * @devices phone, tablet, tv, wearable, car
   * @since 7
   * @SysCap SystemCapability.Multimedia.Audio
   */
  enum InterruptHint {

    /**
     * Audio resumed.
     */
    INTERRUPT_HINT_RESUME = 1,

    /**
     * Audio paused.
     */
    INTERRUPT_HINT_PAUSE = 2,

    /**
     * Audio stopped.
     */
    INTERRUPT_HINT_STOP = 3,

    /**
     * Audio ducking. (In ducking, the audio volume is reduced, but not silenced.)
     */
    INTERRUPT_HINT_DUCK = 4,

    /**
     * Audio unducking.
     */
    INTERRUPT_HINT_UNDUCK = 5,
  }

  /**
   * Interrupt force type.
   * @since 8
   */
  enum InterruptForceType {
    /**
     * Force type, system change audio state.
     */
    INTERRUPT_FORCE = 0,
    /**
     * Share type, application change audio state.
     */
    INTERRUPT_SHARE
  }

  interface InterruptEvent {
    /**
     * Interrupt event type, begin or end
     */
    eventType: InterruptType;

    /**
     * Interrupt force type, force or share
     */
    forceType: InterruptForceType;

    /**
     * Interrupt hint type. In force type, the audio state already changed,
     * but in share mode, only provide a hint for application to decide.
     */
    hintType: InterruptHint;
  }

  /**
   * Provides functions for applications for audio playback.
   */
  interface AudioRenderer {
    /**
     * Gets audio state.
     * @since 8
     */
    readonly state: AudioState;

    /**
     * Gets unique id for audio stream.
     * @return Audio stream unique id.
     * @since 8
     */
    getAudioStreamUniqueId(callback: AsyncCallback<number>): void;
    getAudioStreamUniqueId(): Promise<number>;

    /**
     * Gets audio renderer info.
     * @return AudioRendererInfo value
     * @since 8
     */
    getRendererInfo(callback: AsyncCallback<AudioRendererInfo>): void;
    getRendererInfo(): Promise<AudioRendererInfo>;

    /**
     * Gets audio stream info.
     * @return AudioStreamInfo value
     * @since 8
     */
    getStreamInfo(callback: AsyncCallback<AudioStreamInfo>): void;
    getStreamInfo(): Promise<AudioStreamInfo>;

    /**
     * Start audio renderer.
     * @since 8
     */
    start(callback: AsyncCallback<void>): void;
    start(): Promise<void>;

    /**
     * Write buffer to audio renderer.
     * @param buffer Data buffer write to audio renderer.
     * @return Length of the written data.
     * @since 8
     */
    write(buffer: ArrayBuffer, callback: AsyncCallback<number>): void;
    write(buffer: ArrayBuffer): Promise<number>;

    /**
     * Gets audio renderer timestamp.
     * @return Audio timestamp.
     * @since 8
     */
    getAudioTime(callback: AsyncCallback<number>): void;
    getAudioTime(): Promise<number>;

    /**
     * Drain audio renderer.
     * @since 8
     */
    drain(callback: AsyncCallback<void>): void;
    drain(): Promise<void>;

    /**
     * Pause audio renderer.
     * @since 8
     */
    pause(callback: AsyncCallback<void>): void;
    pause(): Promise<void>;

    /**
     * Stop audio renderer.
     * @since 8
     */
    stop(callback: AsyncCallback<void>): void;
    stop(): Promise<void>;

    /**
     * Release audio renderer instance.
     * @since 8
     */
    release(callback: AsyncCallback<void>): void;
    release(): Promise<void>;

    /**
     * Gets audio buffer size.
     * @return Audio buffer size for written.
     * @since 8
     */
    getBufferSize(callback: AsyncCallback<number>): void;
    getBufferSize(): Promise<number>;

    /**
     * Sets audio renderer rate.
     * @param rate Audio renderer rate set to audio renderer.
     * @since 8
     */
    setRenderRate(rate: AudioRendererRate, callback: AsyncCallback<void>): void;
    setRenderRate(rate: AudioRendererRate): Promise<void>;

    /**
     * Gets audio renderer rate.
     * @return Current audio renderer rate.
     * @since 8
     */
    getRenderRate(callback: AsyncCallback<AudioRendererRate>): void;
    getRenderRate(): Promise<AudioRendererRate>;

    /**
     * Subscribes mark reach event callback.
     * @param type Event type.
     * @param frame Mark reach frame count.
     * @return Mark reach event callback.
     * @since 8
     */
    on(type: "interrupt", callback: Callback<InterruptEvent>): void;
    /**
     * Unsubscribes mark reach event callback.
     * @since 8
     */
    off(type: "interrupt"): void;

    /**
     * Subscribes mark reach event callback.
     * @param type Event type.
     * @param frame Mark reach frame count.
     * @return Mark reach event callback.
     * @since 8
     */
    on(type: "markReach", frame: number, callback: (position: number) => {}): void;
    /**
     * Unsubscribes mark reach event callback.
     * @since 8
     */
    off(type: "markReach"): void;

    /**
     * Subscribes period reach event callback.
     * @param type Event type.
     * @param frame Period reach frame count.
     * @return Period reach event callback.
     * @since 8
     */
    on(type: "periodReach", frame: number, callback: (position: number) => {}): void;
    /**
     * Unsubscribes period reach event callback.
     * @since 8
     */
    off(type: "periodReach"): void;
  }

  /**
   * Enum for source type.
   * @since 8
   */
  enum SourceType {
    /**
     * Invalid source type.
     * @since 8
     */
    SOURCE_TYPE_INVALID = -1,
    /**
     * Mic source type.
     * @since 8
     */
    SOURCE_TYPE_MIC
  }

  /**
   * Interface for audio capturer info.
   * @since 8
   */
  interface AudioCapturerInfo {
    /**
     * Audio source type
     * @since 8
     */
    source: SourceType;
    /**
     * Audio capturer flags
     * @since 8
     */
    capturerFlags: number;
  }

  /**
   * Interface for audio capturer options.
   * @since 8
   */
  interface AudioCapturerOptions {
    /**
     * Audio stream info.
     * @since 8
     */
    streamInfo: AudioStreamInfo;
    /**
     * Audio capturer info.
     * @since 8
     */
    capturerInfo: AudioCapturerInfo;
  }

  /**
   * Provides functions for applications to manage audio capturing.
   * @since 8
   */
  interface AudioCapturer {
    /**
     * Gets capture state.
     * @since 8
     */
    readonly state: AudioState;

    /**
     * Gets unique id for audio stream.
     * @return Audio stream unique id.
     * @since 8
     */
    getAudioStreamUniqueId(callback: AsyncCallback<number>): void;
    getAudioStreamUniqueId(): Promise<number>;

    /**
     * Gets audio capturer info.
     * @return AudioCapturerInfo value
     * @since 8
     */
    getCapturerInfo(callback: AsyncCallback<AudioCapturerInfo>): void;
    getCapturerInfo(): Promise<AudioCapturerInfo>;

    /**
     * Gets audio stream info.
     * @return AudioStreamInfo value
     * @since 8
     */
    getStreamInfo(callback: AsyncCallback<AudioStreamInfo>): void;
    getStreamInfo(): Promise<AudioStreamInfo>;

    /**
     * Start audio capturer.
     * @since 8
     */
    start(callback: AsyncCallback<void>): void;
    start(): Promise<void>;

    /**
     * Read buffer from audio capturer.
     * @param size Read buffer size.
     * @param isBlockingRead Use blocking read or not.
     * @return buffer Data buffer read from audio capturer.
     * @since 8
     */
    read(size: number, isBlockingRead: boolean, callback: AsyncCallback<ArrayBuffer>): void;
    read(size: number, isBlockingRead: boolean): Promise<ArrayBuffer>;

    /**
     * Gets audio capturer timestamp.
     * @return Audio timestamp.
     * @since 8
     */
    getAudioTime(callback: AsyncCallback<number>): void;
    getAudioTime(): Promise<number>;

    /**
     * Stop audio capturer.
     * @since 8
     */
    stop(callback: AsyncCallback<void>): void;
    stop(): Promise<void>;

    /**
     * Release audio capturer instance.
     * @since 8
     */
    release(callback: AsyncCallback<void>): void;
    release(): Promise<void>;

    /**
     * Gets audio buffer size.
     * @return Audio buffer size for written.
     * @since 8
     */
    getBufferSize(callback: AsyncCallback<number>): void;
    getBufferSize(): Promise<number>;

    /**
     * Subscribes mark reach event callback.
     * @param type Event type.
     * @param frame Mark reach frame count.
     * @return Mark reach event callback.
     * @since 8
     */
     on(type: "markReach", frame: number, callback: (position: number) => {}): void;
     /**
      * Unsubscribes mark reach event callback.
      * @since 8
      */
     off(type: "markReach"): void;

     /**
      * Subscribes period reach event callback.
      * @param type Event type.
      * @param frame Period reach frame count.
      * @return Period reach event callback.
      * @since 8
      */
     on(type: "periodReach", frame: number, callback: (position: number) => {}): void;
     /**
      * Unsubscribes period reach event callback.
      * @since 8
      */
     off(type: "periodReach"): void;
  }

  /**
   * Enum for ringtone type.
   * @since 8
   */
  enum RingtoneType {
    /**
     * Default type.
     * @since 8
     */
    RINGTONE_TYPE_DEFAULT = 0,
    /**
     * Multi-sim type.
     * @since 8
     */
    RINGTONE_TYPE_MULTISIM
  }

  /**
   * Interface for ringtone options.
   * @since 8
   */
  interface RingtoneOptions {
    /**
     * Ringtone volume.
     * @since 8
     */
    volume: number;
    /**
     * Loop value.
     * @since 8
     */
    loop: boolean;
  }

  /**
   * Ringtone player object.
   * @since 8
   */
  interface RingtonePlayer {
    /**
     * Gets player state.
     * @since 8
     */
    readonly state: AudioState;

    /**
     * Gets the title of ringtone.
     * @since 8
     */
    getTitle(callback: AsyncCallback<string>): void;
    getTitle(): Promise<string>;

    /**
     * Gets audio renderer info.
     * @return AudioRendererInfo value
     * @since 8
     */
    getAudioRendererInfo(callback: AsyncCallback<AudioRendererInfo>): void;
    getAudioRendererInfo(): Promise<AudioRendererInfo>;

    /**
     * Configure ringtone options.
     * @param options Ringtone configure options.
     * @since 8
     */
    configure(options: RingtoneOptions, callback: AsyncCallback<void>): void;
    configure(options: RingtoneOptions): Promise<void>;

    /**
     * Starts playing ringtone
     * @since 8
     */
    start(callback: AsyncCallback<void>): void;
    start(): Promise<void>;

    /**
     * Stop playing ringtone
     * @since 8
     */
    stop(callback: AsyncCallback<void>): void;
    stop(): Promise<void>;

    /**
     * Release ringtone player resource
     * @since 8
     */
    release(callback: AsyncCallback<void>): void;
    release(): Promise<void>;
  }

  /**
   * Gets system sound manager for all type sound.
   * @return SystemSoundManager instance.
   * @since 8
   */
  function getSystemSoundManager(): SystemSoundManager;

  /**
   * System sound manager object.
   * @since 8
   */
  interface SystemSoundManager {
    /**
     * Sets the ringtone uri to system.
     * @param context Current application context.
     * @param uri Ringtone uri to set.
     * @param type Ringtone type to set.
     * @since 8
     */
    setSystemRingtoneUri(context: Context, uri: string, type: RingtoneType, callback: AsyncCallback<void>): void;
    setSystemRingtoneUri(context: Context, uri: string, type: RingtoneType): Promise<void>;
    /**
     * Gets the ringtone uri.
     * @param context Current application context.
     * @param type Ringtone type to get.
     * @return Ringtone uri maintained in system.
     * @since 8
     */
    getSystemRingtoneUri(context: Context, type: RingtoneType, callback: AsyncCallback<string>): void;
    getSystemRingtoneUri(context: Context, type: RingtoneType): Promise<string>;
    /**
     * Gets the ringtone player.
     * @param context Current application context.
     * @param type Ringtone type to get.
     * @return Ringtone player maintained in system.
     * @since 8
     */
    getSystemRingtonePlayer(context: Context, type: RingtoneType, callback: AsyncCallback<RingtonePlayer>): void;
    getSystemRingtonePlayer(context: Context, type: RingtoneType): Promise<RingtonePlayer>;

    /**
     * Sets the notification uri to system.
     * @param context Current application context.
     * @param uri Ringtone uri to set.
     * @since 8
     */
    setSystemNotificationUri(context: Context, uri: string, callback: AsyncCallback<void>): void;
    setSystemNotificationUri(context: Context, uri: string): Promise<void>;
    /**
     * Gets the notification uri.
     * @param context Current application context.
     * @return Notification uri maintained in system.
     * @since 8
     */
    getSystemNotificationUri(context: Context, callback: AsyncCallback<string>): void;
    getSystemNotificationUri(context: Context): Promise<string>;

    /**
     * Sets the alarm uri to system.
     * @param context Current application context.
     * @param uri Alarm uri to set.
     * @since 8
     */
    setSystemAlarmUri(context: Context, uri: string, callback: AsyncCallback<void>): void;
    setSystemAlarmUri(context: Context, uri: string): Promise<void>;
    /**
     * Gets the alarm uri.
     * @param context Current application context.
     * @return Alarm uri maintained in system.
     * @since 8
     */
    getSystemAlarmUri(context: Context, callback: AsyncCallback<string>): void;
    getSystemAlarmUri(context: Context): Promise<string>;
  }
}

export default audio;