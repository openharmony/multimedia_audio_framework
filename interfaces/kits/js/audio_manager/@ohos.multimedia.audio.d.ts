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
import {VideoPlayer, AudioPlayer} from './@ohos.multimedia.media'
import Context from './@ohos.ability';
/**
 * @name audio
 * @since 7
 * @syscap SystemCapability.Multimedia.Audio
 * @import import audio from '@ohos.multimedia.audio';
 * @permission
 */
declare namespace audio {

  /**
   * Obtains an AudioManager instance.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  function getAudioManager(): AudioManager;

  /**
   * Creates a AudioCapturer instance.
   * @param options All options used for audio capturer.
   * @return AudioCapturer instance.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   */
  function createAudioCapturer(options: AudioCapturerOptions, callback: AsyncCallback<AudioCapturer>): void;
  function createAudioCapturer(options: AudioCapturerOptions): Promise<AudioCapturer>;

  /**
   * Creates a AudioRenderer instance.
   * @param options All options used for audio renderer.
   * @return AudioRenderer instance.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   */
  function createAudioRenderer(options: AudioRendererOptions, callback: AsyncCallback<AudioRenderer>): void;
  function createAudioRenderer(options: AudioRendererOptions): Promise<AudioRenderer>;

  /**
   * Enumerates the rendering states of the current device.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
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
   * Enumerates audio stream types.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum AudioVolumeType {
    /**
     * Audio streams for voice calls
     * @since 8
     */
    VOICE_CALL = 0,
    /**
     * Audio streams for ring tones
     * @since 7
     */
    RINGTONE = 2,
    /**
     * Audio streams for media purpose
     * @since 7
     */
    MEDIA = 3,
    /**
     * Audio stream for voice assistant
     * @since 8
     */
    VOICE_ASSISTANT = 9,
  }

  /**
   * Enumerates audio device flags.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum DeviceFlag {
    /**
     * Output devices
     * @since 7
     */
    OUTPUT_DEVICES_FLAG = 1,
    /**
     * Input devices
     * @since 7
     */
    INPUT_DEVICES_FLAG = 2,
    /**
     * All devices
     * @since 7
     */
    ALL_DEVICES_FLAG = 3,
  }

  /**
   * Enumerates device roles.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum DeviceRole {
    /**
     * Input role
     * @since 7
     */
    INPUT_DEVICE = 1,
    /**
     * Output role
     * @since 7
     */
    OUTPUT_DEVICE = 2,
  }

  /**
   * Enumerates device types.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum DeviceType {
    /**
     * Invalid device
     * @since 7
     */
    INVALID = 0,
    /**
     * Speaker
     * @since 7
     */
    SPEAKER = 2,
    /**
     * Wired headset
     * @since 7
     */
    WIRED_HEADSET = 3,
    /**
     * Bluetooth device using the synchronous connection oriented link (SCO)
     * @since 7
     */
    BLUETOOTH_SCO = 7,
    /**
     * Bluetooth device using advanced audio distribution profile (A2DP)
     * @since 7
     */
    BLUETOOTH_A2DP = 8,
    /**
     * Microphone
     * @since 7
     */
    MIC = 15,
    /**
     * USB audio headset.
     * @since 7
     */
    USB_HEADSET = 22
  }

  /**
   * Enumerates Active device types.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
   enum ActiveDeviceType {
    /**
     * Speaker
     * @since 7
     */
    SPEAKER = 2,
    /**
     * Bluetooth device using the synchronous connection oriented link (SCO)
     * @since 7
     */
    BLUETOOTH_SCO = 7,
  }

  /**
   * Enumerates Audio Ringer modes
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum AudioRingMode {
    /**
     * Silent mode
     * @since 7
     */
    RINGER_MODE_SILENT = 0,
    /**
     * Vibration mode
     * @since 7
     */
    RINGER_MODE_VIBRATE,
   /**
     * Normal mode
     * @since 7
     */
    RINGER_MODE_NORMAL,
  }

  /**
   * Enumerates the sample format.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
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
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum AudioChannel {
    CHANNEL_1 = 0x1 << 0,
    CHANNEL_2 = 0x1 << 1
  }

  /**
   * Enumerates the audio sampling rate.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
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
   * @syscap SystemCapability.Multimedia.Audio
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
    ENCODING_TYPE_RAW = 0
  }

  /**
   * Enumerates the audio content type.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum ContentType {
    /**
     * Unknown content.
     * @since 7
     */
    CONTENT_TYPE_UNKNOWN = 0,
    /**
     * Speech content.
     * @since 7
     */
    CONTENT_TYPE_SPEECH = 1,
    /**
     * Music content.
     * @since 7
     */
    CONTENT_TYPE_MUSIC = 2,
    /**
     * Movie content.
     * @since 7
     */
    CONTENT_TYPE_MOVIE = 3,
    /**
     * Notification content.
     * @since 7
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
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum StreamUsage {
    /**
     * Unkown usage.
     * @since 7
     */
    STREAM_USAGE_UNKNOWN = 0,
    /**
     * Media usage.
     * @since 7
     */
    STREAM_USAGE_MEDIA = 1,
    /**
     * Voice communication usage.
     * @since 7
     */
    STREAM_USAGE_VOICE_COMMUNICATION = 2,
    /**
     * Notification or ringtone usage.
     * @since 7
     */
    STREAM_USAGE_NOTIFICATION_RINGTONE = 3
  }

  /**
   * Interface for audio stream info
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface AudioStreamInfo {
    /**
     * Audio sampling rate
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    samplingRate: AudioSamplingRate;
    /**
     * Audio channels
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    channels: AudioChannel;
    /**
     * Audio sample format
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    sampleFormat: AudioSampleFormat;
    /**
     * Audio encoding type
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    encodingType: AudioEncodingType;
  }

  /**
   * Interface for audio renderer info
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface AudioRendererInfo {
    /**
     * Audio content type
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    content: ContentType;
    /**
     * Audio stream usage
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    usage: StreamUsage;
    /**
     * Audio renderer flags
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    rendererFlags: number;
  }

  /**
   * Interface for audio renderer options
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface AudioRendererOptions {
    /**
     * Audio stream info
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    streamInfo: AudioStreamInfo;
    /**
     * Audio renderer info
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    rendererInfo: AudioRendererInfo;
  }

  /**
   * Enum for audio renderer rate
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
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
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum InterruptType {
    /**
     * An audio interruption event starts.
     * @since 7
     */
    INTERRUPT_TYPE_BEGIN = 1,

    /**
     * An audio interruption event ends.
     * @since 7
     */
    INTERRUPT_TYPE_END = 2
  }

  /**
   * Enumerates the types of hints for audio interruption.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum InterruptHint {
    /**
     * Audio no interrupt.
     * @since 8
     */
    INTERRUPT_HINT_NONE = 0,
    /**
     * Audio resumed.
     * @since 7
     */
    INTERRUPT_HINT_RESUME = 1,

    /**
     * Audio paused.
     * @since 7
     */
    INTERRUPT_HINT_PAUSE = 2,

    /**
     * Audio stopped.
     * @since 7
     */
    INTERRUPT_HINT_STOP = 3,

    /**
     * Audio ducking. (In ducking, the audio volume is reduced, but not silenced.)
     * @since 7
     */
    INTERRUPT_HINT_DUCK = 4,

    /**
     * Audio unducking.
     * @since 8
     */
    INTERRUPT_HINT_UNDUCK = 5,
  }

  /**
   * Interrupt force type.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum InterruptForceType {
    /**
     * Force type, system change audio state.
     * @since 8
     */
    INTERRUPT_FORCE = 0,
    /**
     * Share type, application change audio state.
     * @since 8
     */
    INTERRUPT_SHARE
  }

  /**
   * Interrupt events
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface InterruptEvent {
    /**
     * Interrupt event type, begin or end
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    eventType: InterruptType;

    /**
     * Interrupt force type, force or share
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    forceType: InterruptForceType;

    /**
     * Interrupt hint type. In force type, the audio state already changed,
     * but in share mode, only provide a hint for application to decide.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    hintType: InterruptHint;
  }

  /**
   * Enumerates device change types.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  enum DeviceChangeType {
    /**
     * Device connection.
     * @since 7
     */
    CONNECT = 0,

    /**
     * Device disconnection.
     * @since 7
     */
    DISCONNECT = 1,
  }

  /**
   * Enumerates audio scenes.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
  */
  enum AudioScene {
    /**
     * Default audio scene
     * @since 8
     */
    AUDIO_SCENE_DEFAULT = 0,
    /**
     * Ringing audio scene
     * Only available for system api.
     * @since 8
     */
    AUDIO_SCENE_RINGING,
    /**
     * Phone call audio scene
     * Only available for system api.
     * @since 8
     */
    AUDIO_SCENE_PHONE_CALL,
    /**
     * Voice chat audio scene
     * @since 8
     */
    AUDIO_SCENE_VOICE_CHAT
  }

  /**
   * Manages audio volume and audio device information.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface AudioManager {
    /**
     * Sets volume for a stream. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    setVolume(audioType: AudioVolumeType, volume: number, callback: AsyncCallback<void>): void;
    /**
     * Sets volume for a stream. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    setVolume(audioType: AudioVolumeType, volume: number): Promise<void>;
    /**
     * Obtains volume of a stream. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getVolume(audioType: AudioVolumeType, callback: AsyncCallback<number>): void;
    /**
     * Obtains the volume of a stream. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getVolume(audioType: AudioVolumeType): Promise<number>;
    /**
     * Obtains the minimum volume allowed for a stream. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getMinVolume(audioType: AudioVolumeType, callback: AsyncCallback<number>): void;
    /**
     * Obtains the minimum volume allowed for a stream. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getMinVolume(audioType: AudioVolumeType): Promise<number>;
    /**
     * Obtains the maximum volume allowed for a stream. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getMaxVolume(audioType: AudioVolumeType, callback: AsyncCallback<number>): void;
    /**
     * Obtains the maximum volume allowed for a stream. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getMaxVolume(audioType: AudioVolumeType): Promise<number>;
    /**
     * Obtains the audio devices of a specified flag. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getDevices(deviceFlag: DeviceFlag, callback: AsyncCallback<AudioDeviceDescriptors>): void;
    /**
     * Obtains the audio devices with a specified flag. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getDevices(deviceFlag: DeviceFlag): Promise<AudioDeviceDescriptors>;
    /**
     * Sets the stream to mute. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    mute(audioType: AudioVolumeType, mute: boolean, callback: AsyncCallback<void>): void;
    /**
     * Sets the stream to mute. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    mute(audioType: AudioVolumeType, mute: boolean): Promise<void>;
    /**
     * Checks whether the stream is muted. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    isMute(audioType: AudioVolumeType, callback: AsyncCallback<boolean>): void;
    /**
     * Checks whether the stream is muted. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    isMute(audioType: AudioVolumeType): Promise<boolean>;
    /**
     * Checks whether the stream is active. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    isActive(audioType: AudioVolumeType, callback: AsyncCallback<boolean>): void;
    /**
     * Checks whether the stream is active. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    isActive(audioType: AudioVolumeType): Promise<boolean>;
    /**
     * Mute/Unmutes the microphone. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    setMicrophoneMute(mute: boolean, callback: AsyncCallback<void>): void;
    /**
     * Mute/Unmutes the microphone. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    setMicrophoneMute(mute: boolean): Promise<void>;
    /**
     * Checks whether the microphone is muted. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    isMicrophoneMute(callback: AsyncCallback<boolean>): void;
    /**
     * Checks whether the microphone is muted. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    isMicrophoneMute(): Promise<boolean>;
    /**
     * Sets the ringer mode. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    setRingerMode(mode: AudioRingMode, callback: AsyncCallback<void>): void;
    /**
     * Sets the ringer mode. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    setRingerMode(mode: AudioRingMode): Promise<void>;
    /**
     * Gets the ringer mode. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getRingerMode(callback: AsyncCallback<AudioRingMode>): void;
    /**
     * Gets the ringer mode. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getRingerMode(): Promise<AudioRingMode>;
    /**
     * Sets the audio parameter. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    setAudioParameter(key: string, value: string, callback: AsyncCallback<void>): void;
    /**
     * Sets the audio parameter. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    setAudioParameter(key: string, value: string): Promise<void>;
    /**
     * Gets the audio parameter. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getAudioParameter(key: string, callback: AsyncCallback<string>): void;
    /**
     * Gets the audio parameter. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    getAudioParameter(key: string): Promise<string>;
    /**
     * Activates the device. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    setDeviceActive(deviceType: ActiveDeviceType, active: boolean, callback: AsyncCallback<void>): void;
    /**
     * Activates the device. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    setDeviceActive(deviceType: ActiveDeviceType, active: boolean): Promise<void>;
    /**
     * Checks whether the device is active. This method uses an asynchronous callback to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    isDeviceActive(deviceType: ActiveDeviceType, callback: AsyncCallback<boolean>): void;
    /**
     * Checks whether the device is active. This method uses a promise to return the execution result.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    isDeviceActive(deviceType: ActiveDeviceType): Promise<boolean>;
    /**
     * Subscribes volume change event callback, only for system
     * @return VolumeEvent callback.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @systemapi
     */
    on(type: 'volumeChange', callback: Callback<VolumeEvent>): void;
    /**
     * Monitors ringer mode change
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @systemapi
     */
    on(type: 'ringerModeChange', callback: Callback<AudioRingMode>): void;
    /**
     * Sets the audio scene mode to change audio strategy.
     * This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    setAudioScene(scene: AudioScene, callback: AsyncCallback<void> ): void;
    /**
     * Sets the audio scene mode to change audio strategy. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    setAudioScene(scene: AudioScene): Promise<void>;
    /**
     * Obtains the system audio scene mode. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getAudioScene(callback: AsyncCallback<AudioScene> ): void;
    /**
     * Obtains the system audio scene mode. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getAudioScene(): Promise<AudioScene>;
    /**
    * Monitors device changes
    * @since 7
    * @syscap SystemCapability.Multimedia.Audio
    */
    on(type: 'deviceChange', callback: Callback<DeviceChangeAction>): void;
  }

  /**
   * Describes an audio device.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface AudioDeviceDescriptor {
    /**
     * Audio device role
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    readonly deviceRole: DeviceRole;
    /**
     * Audio device type
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    readonly deviceType: DeviceType;
  }

  /**
   * A queue of AudioDeviceDescriptor, which is read-only.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  type AudioDeviceDescriptors = Array<Readonly<AudioDeviceDescriptor>>;

  /**
   * Audio volume event
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   * @systemapi
   */
  interface VolumeEvent {
    /**
     * volumeType of current stream
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    volumeType: AudioVolumeType;
    /**
     * volume level
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    volume: number;
    /**
     * updateUi show volume change in Ui
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    updateUi: boolean;
  }

  /**
   * Describes the device change type and device information.
   * @since 7
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface DeviceChangeAction {
    /**
     * Device change type.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    type: DeviceChangeType;

    /**
     * Device information.
     * @since 7
     * @syscap SystemCapability.Multimedia.Audio
     */
    deviceDescriptors: AudioDeviceDescriptors;
  }

  /**
   * Provides functions for applications for audio playback.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface AudioRenderer {
    /**
     * Gets audio state.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    readonly state: AudioState;
    /**
     * Gets audio renderer info.
     * @return AudioRendererInfo value
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getRendererInfo(callback: AsyncCallback<AudioRendererInfo>): void;
    getRendererInfo(): Promise<AudioRendererInfo>;
    /**
     * Gets audio stream info.
     * @return AudioStreamInfo value
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getStreamInfo(callback: AsyncCallback<AudioStreamInfo>): void;
    getStreamInfo(): Promise<AudioStreamInfo>;
    /**
     * Starts audio rendering. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    start(callback: AsyncCallback<void>): void;
    /**
     * Starts audio rendering. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    start(): Promise<void>;
    /**
     * Render audio data. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    write(buffer: ArrayBuffer, callback: AsyncCallback<number>): void;
    /**
     * Render audio data. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    write(buffer: ArrayBuffer): Promise<number>;
    /**
     * Obtains the current timestamp. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getAudioTime(callback: AsyncCallback<number>): void;
    /**
     * Obtains the current timestamp. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getAudioTime(): Promise<number>;
    /**
     * Drain renderer buffer. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    drain(callback: AsyncCallback<void>): void;
    /**
     * Drain renderer buffer. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    drain(): Promise<void>;
    /**
     * Pauses audio rendering. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    pause(callback: AsyncCallback<void>): void;
    /**
     * Pauses audio rendering. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    pause(): Promise<void>;
    /**
     * Stops audio rendering. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    stop(callback: AsyncCallback<void>): void;
    /**
     * Stops audio rendering. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    stop(): Promise<void>;
    /**
     * Releases resources. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    release(callback: AsyncCallback<void>): void;
    /**
     * Releases resources. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    release(): Promise<void>;
    /**
     * Obtains a reasonable minimum buffer size for renderer, however, the renderer can
     * accept other read sizes as well. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getBufferSize(callback: AsyncCallback<number>): void;
    /**
     * Obtains a reasonable minimum buffer size for renderer, however, the renderer can
     * accept other read sizes as well. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getBufferSize(): Promise<number>;
    /**
     * Set the render rate. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    setRenderRate(rate: AudioRendererRate, callback: AsyncCallback<void>): void;
    /**
     * Set the render rate. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    setRenderRate(rate: AudioRendererRate): Promise<void>;
    /**
     * Obtains the current render rate. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getRenderRate(callback: AsyncCallback<AudioRendererRate>): void;
    /**
     * Obtains the current render rate. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getRenderRate(): Promise<AudioRendererRate>;
    /**
     * Subscribes interrupt event callback.
     * @param type Event type.
     * @return InterruptEvent callback.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    on(type: 'interrupt', callback: Callback<InterruptEvent>): void;
    /**
     * Subscribes mark reach event callback.
     * @param type Event type.
     * @param frame Mark reach frame count.
     * @return Mark reach event callback.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @initial
     */
    on(type: "markReach", frame: number, callback: (position: number) => {}): void;
    /**
     * Unsubscribes mark reach event callback.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @initial
     */
    off(type: "markReach"): void;
    /**
     * Subscribes period reach event callback.
     * @param type Event type.
     * @param frame Period reach frame count.
     * @return Period reach event callback.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @initial
     */
    on(type: "periodReach", frame: number, callback: (position: number) => {}): void;
    /**
     * Unsubscribes period reach event callback.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @initial
     */
    off(type: "periodReach"): void;
    /**
     * Subscribes audio state change event callback.
     * @param type Event type.
     * @param callback Callback used to listen for the audio state change event.
     * @return AudioState
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @initial
     */
    on(type: "stateChange", callback: Callback<AudioState>): void;
  }

  /**
   * Enum for source type.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
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
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface AudioCapturerInfo {
    /**
     * Audio source type
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    source: SourceType;
    /**
     * Audio capturer flags
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    capturerFlags: number;
  }

  /**
   * Interface for audio capturer options.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface AudioCapturerOptions {
    /**
     * Audio stream info.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    streamInfo: AudioStreamInfo;
    /**
     * Audio capturer info.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    capturerInfo: AudioCapturerInfo;
  }

  /**
   * Provides functions for applications to manage audio capturing.
   * @since 8
   * @syscap SystemCapability.Multimedia.Audio
   */
  interface AudioCapturer {
    /**
     * Gets capture state.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    readonly state: AudioState;
    /**
     * Gets audio capturer info.
     * @return AudioCapturerInfo value
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getCapturerInfo(callback: AsyncCallback<AudioCapturerInfo>): void;
    getCapturerInfo(): Promise<AudioCapturerInfo>;

    /**
     * Gets audio stream info.
     * @return AudioStreamInfo value
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getStreamInfo(callback: AsyncCallback<AudioStreamInfo>): void;
    getStreamInfo(): Promise<AudioStreamInfo>;

    /**
     * Starts audio capturing. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    start(callback: AsyncCallback<void>): void;
    /**
     * Starts audio capturing. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    start(): Promise<void>;

    /**
     * Capture audio data. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    read(size: number, isBlockingRead: boolean, callback: AsyncCallback<ArrayBuffer>): void;
    /**
     * Capture audio data. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    read(size: number, isBlockingRead: boolean): Promise<ArrayBuffer>;

    /**
     * Obtains the current timestamp. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getAudioTime(callback: AsyncCallback<number>): void;
    /**
     * Obtains the current timestamp. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getAudioTime(): Promise<number>;

    /**
     * Stops audio capturing. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    stop(callback: AsyncCallback<void>): void;
    /**
     * Stops audio capturing. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    stop(): Promise<void>;

    /**
     * Releases a capture resources. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    release(callback: AsyncCallback<void>): void;
    /**
     * Releases a capture resources. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    release(): Promise<void>;

    /**
     * Obtains a reasonable minimum buffer size for capturer, however, the capturer can
     * accept other read sizes as well. This method uses an asynchronous callback to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getBufferSize(callback: AsyncCallback<number>): void;
    /**
     * Obtains a reasonable minimum buffer size for capturer, however, the capturer can
     * accept other read sizes as well. This method uses a promise to return the execution result.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     */
    getBufferSize(): Promise<number>;

    /**
     * Subscribes mark reach event callback.
     * @param type Event type.
     * @param frame Mark reach frame count.
     * @return Mark reach event callback.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @initial
     */
    on(type: "markReach", frame: number, callback: (position: number) => {}): void;
    /**
     * Unsubscribes mark reach event callback.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @initial
     */
    off(type: "markReach"): void;

    /**
     * Subscribes period reach event callback.
     * @param type Event type.
     * @param frame Period reach frame count.
     * @return Period reach event callback.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @initial
     */
    on(type: "periodReach", frame: number, callback: (position: number) => {}): void;
    /**
     * Unsubscribes period reach event callback.
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @initial
     */
    off(type: "periodReach"): void;
    /**
     * Subscribes audio state change event callback.
     * @param type Event type.
     * @param callback Callback used to listen for the audio state change event.
     * @return AudioState
     * @since 8
     * @syscap SystemCapability.Multimedia.Audio
     * @initial
     */
    on(type: "stateChange", callback: Callback<AudioState>): void;

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

  interface RingtonePlayer {
    /**
     * Gets render state of ringtone.
     * @syscap SystemCapability.Multimedia.Audio
     */
    readonly state: AudioState;

    /**
     * Gets the title of ringtone.
     * @since 1.0
     */
    getTitle(callback: AsyncCallback<string>): void;
    getTitle(): Promise<string>;

    /**
     * Gets audio renderer info.
     * @return AudioRendererInfo value
     * @since 1.0
     */
    getAudioRendererInfo(callback: AsyncCallback<AudioRendererInfo>): void;
    getAudioRendererInfo(): Promise<AudioRendererInfo>;

    /**
     * Sets ringtone parameters.
     * @param option Set RingtoneOption for ringtone like volume & loop
     * @param callback Callback object to be passed along with request
     * @since 1.0
     * @version 1.0
     */
    configure(option: RingtoneOptions, callback: AsyncCallback<void>): void;
    configure(option: RingtoneOptions): Promise<void>;
    /**
     * Starts playing ringtone.
     * @param callback Callback object to be passed along with request
     * @since 1.0
     * @version 1.0
     */
    start(callback: AsyncCallback<void>): void;
    start(): Promise<void>;
    /**
     * Stops playing ringtone.
     * @param callback Callback object to be passed along with request
     * @since 1.0
     * @version 1.0
     */
    stop(callback: AsyncCallback<void>): void;
    stop(): Promise<void>;
    /**
     * Release ringtone player resource.
     * @param callback Callback object to be passed along with request
     * @since 1.0
     * @version 1.0
     */
    release(callback: AsyncCallback<void>): void;
    release(): Promise<void>;
  }
  function getSystemSoundManager(): SystemSoundManager;
  interface SystemSoundManager {
    /**
     * Sets the ringtone uri.
     * @param context Indicates the Context object on OHOS
     * @param uri Indicated which uri to be set for the tone type
     * @param type Indicats the type of the tone
     * @param callback Callback object to be passed along with request
     * @since 1.0
     * @version 1.0
     */
    setSystemRingtoneUri(context: Context, uri: string, type: RingtoneType, callback: AsyncCallback<void>): void;
    setSystemRingtoneUri(context: Context, uri: string, type: RingtoneType): Promise<void>;
    /**
     * Sets the ringtone uri.
     * @param context Indicates the Context object on OHOS
     * @param type Indicats the type of the tone
     * @param callback Callback object to be passed along with request
     * @return Returns uri of the ringtone
     * @since 1.0
     * @version 1.0
     */
    getSystemRingtoneUri(context: Context, type: RingtoneType, callback: AsyncCallback<string>): void;
    getSystemRingtoneUri(context: Context, type: RingtoneType): Promise<string>;
    /**
     * Gets the ringtone player.
     * @param context Indicates the Context object on OHOS
     * @param type Indicats the type of the tone
     * @param callback Callback object to be passed along with request
     * @return Returns ringtone player object
     * @since 1.0
     * @version 1.0
     */
    getSystemRingtonePlayer(context: Context, type: RingtoneType, callback: AsyncCallback<RingtonePlayer>): void;
    getSystemRingtonePlayer(context: Context, type: RingtoneType): Promise<RingtonePlayer>;
    /**
     * Sets the notification uri.
     * @param context Indicates the Context object on OHOS
     * @param uri Indicats the uri of the notification
     * @param callback Callback object to be passed along with request
     * @since 1.0
     * @version 1.0
     */
    setSystemNotificationUri(context: Context, uri: string, callback: AsyncCallback<void>): void;
    setSystemNotificationUri(context: Context, uri: string): Promise<void>;
    /**
     * Gets the notification uri.
     * @param context Indicates the Context object on OHOS
     * @param callback Callback object to be passed along with request
     * @return Returns the uri of the notification
     * @since 1.0
     * @version 1.0
     */
    getSystemNotificationUri(context: Context, callback: AsyncCallback<string>): void;
    getSystemNotificationUri(context: Context): Promise<string>;
    /**
     * Sets the alarm uri.
     * @param context Indicates the Context object on OHOS
     * @param uri Indicats the uri of the alarm
     * @param callback Callback object to be passed along with request
     * @since 1.0
     * @version 1.0
     */
    setSystemAlarmUri(context: Context, uri: string, callback: AsyncCallback<void>): void;
    setSystemAlarmUri(context: Context, uri: string): Promise<void>;
    /**
     * Gets the alarm uri.
     * @param context Indicates the Context object on OHOS
     * @param callback Callback object to be passed along with request
     * @return Returns the uri of the alarm
     * @since 1.0
     * @version 1.0
     */
    getSystemAlarmUri(context: Context, callback: AsyncCallback<string>): void;
    getSystemAlarmUri(context: Context): Promise<string>;
  }
}

export default audio;
