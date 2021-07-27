/*
* Copyright (C) 2021 Huawei Device Co., Ltd.
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
import {VideoPlayer, AudioPlayer} from '@ohos.Multimedia.media'
/**
 * @name audio
 * @since 6
 * @sysCap SystemCapability.Multimedia.Audio
 * @import import audio from '@ohos.Multimedia.audio';
 * @permission
 */
declare namespace audio {

  /**
   * Obtains an AudioManager instance.
   * @sysCap SystemCapability.Multimedia.Audio
   * @devices
   */
  function getAudioManager(): AudioManager;

  /**
   * Enumerates audio stream types.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum AudioVolumeType {
    /**
     * Audio streams for media purpose
     */
    MEDIA = 1,
    /**
     * Audio streams for ring tones
     */
    RINGTONE = 2,
  }

  /**
   * Enumerates audio device flags.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum DeviceFlag {
    /**
     * Output devices
     */
    OUTPUT_DEVICES_FLAG = 1,
    /**
     * Input devices
     */
    INPUT_DEVICES_FLAG = 2,
    /**
     * All devices
     */
    ALL_DEVICES_FLAG = 3,
  }
  /**
   * Enumerates device roles.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum DeviceRole {
    /**
     * Input role
     */
    INPUT_DEVICE = 1,
    /**
     * Output role
     */
    OUTPUT_DEVICE = 2,
  }
  /**
   * Enumerates device types.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum DeviceType {
    /**
     * Invalid device
     */
    INVALID = 0,
    /**
     * Speaker
     */
    SPEAKER = 1,
    /**
     * Wired headset
     */
    WIRED_HEADSET = 2,
    /**
     * Bluetooth device using the synchronous connection oriented link (SCO)
     */
    BLUETOOTH_SCO = 3,
    /**
     * Bluetooth device using advanced audio distribution profile (A2DP)
     */
    BLUETOOTH_A2DP = 4,
    /**
     * Microphone
     */
    MIC = 5,
  }
  /**
   * 音频铃声枚举.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum AudioRingMode {
    /**
     * 正常铃声模式
     */
    RINGER_MODE_NORMAL = 0,
    /**
     * 静音铃声模式
     */
    RINGER_MODE_SILENT = 1,
    /**
     * 振动铃声模式
     */
    RINGER_MODE_VIBRATE = 2,
  }

  /**
   * Manages audio volume and audio device information.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface AudioManager {
    /**
     * Sets volume for a stream. This method uses an asynchronous callback to return the execution result.
     * @devices
     * @sysCap SystemCapability.Multimedia.Audio
     */
    setVolume(audioType: AudioVolumeType, volume: number,callback: AsyncCallback<void>): void;
    /**
     * Sets volume for a stream. This method uses a promise to return the execution result.
     * @devices
     * @sysCap SystemCapability.Multimedia.Audio
     */
    setVolume(audioType: AudioVolumeType, volume: number): Promise<void>;
    /**
     * Obtains volume of a stream. This method uses an asynchronous callback to return the execution result.
     * @devices
     * @sysCap SystemCapability.Multimedia.Audio
     */
    getVolume(audioType: AudioVolumeType, callback: AsyncCallback<number>): void;
    /**
     * Obtains the volume of a stream. This method uses a promise to return the execution result.
     * @devices
     * @sysCap SystemCapability.Multimedia.Audio
     */
    getVolume(audioType: AudioVolumeType): Promise<number>;
    /**
     * Obtains the minimum volume allowed for a stream. This method uses an asynchronous callback to return the execution result.
     * @devices
     * @sysCap SystemCapability.Multimedia.Audio
     */
    getMinVolume(audioType: AudioVolumeType, callback: AsyncCallback<number>): void
    /**
     * Obtains the minimum volume allowed for a stream. This method uses a promise to return the execution result.
     * @devices
     * @sysCap SystemCapability.Multimedia.Audio
     */
    getMinVolume(audioType: AudioVolumeType): Promise<number>;
    /**
     * Obtains the maximum volume allowed for a stream. This method uses an asynchronous callback to return the execution result.
     * @devices
     * @sysCap SystemCapability.Multimedia.Audio
     */
    getMaxVolume(audioType: AudioVolumeType, callback: AsyncCallback<number>): void
    /**
     * Obtains the maximum volume allowed for a stream. This method uses a promise to return the execution result.
     * @devices
     * @sysCap SystemCapability.Multimedia.Audio
     */
    getMaxVolume(audioType: AudioVolumeType): Promise<number>;
    /**
     * Obtains the audio devices of a specified flag. This method uses an asynchronous callback to return the execution result.
     * @devices
     * @sysCap SystemCapability.Multimedia.Audio
     */
    getDevices(deviceFlag: DeviceFlag, callback: AsyncCallback<AudioDeviceDescriptors>): void
    /**
     * Obtains the audio devices with a specified flag. This method uses a promise to return the execution result.
     * @devices
     * @sysCap SystemCapability.Multimedia.Audio
     */
    getDevices(deviceFlag: DeviceFlag): Promise<AudioDeviceDescriptors>;
    /**
     * 回调方式获取铃声模式。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    getRingerMode(callback: AsyncCallback<AudioRingMode>): void;
    /**
     * promise方式获取铃声模式。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    getRingerMode(): Promise<AudioRingMode>;
    /**
     * 设置铃声模式，回调方式返回是否成功。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setRingerMode(mode: AudioRingMode, callback: AsyncCallback<void>): void;
    /**
     * 设置铃声模式，promise方式返回是否成功。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setRingerMode(mode: AudioRingMode): Promise<void>;
    /**
     * 判断流是否静音，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    isStreamMute(volumeType: AudioVolumeType, callback: AsyncCallback<boolean>): void;
    /**
     * 判断流是否静音，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    isStreamMute(volumeType: AudioVolumeType): Promise<boolean>;
    /**
     * 判断流是否激活，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    isStreamActive(volumeType: AudioVolumeType, callback: AsyncCallback<boolean>): void;
    /**
     * 判断流是否激活，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    isStreamActive(volumeType: AudioVolumeType): Promise<boolean>;
    /**
     * 判断麦克风是否静音，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    isMicrophoneMute(callback: AsyncCallback<boolean>): void;
    /**
     * 判断麦克风是否静音，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    isMicrophoneMute(): Promise<boolean>;
    /**
     * 设置流静音，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
	 setStreamMute(volumeType: AudioVolumeType, mute: boolean, callback: AsyncCallback<void>) : void;
    /**
     * 设置流静音，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setStreamMute(volumeType: AudioVolumeType, mute: boolean): Promise<void>;
    /**
     * 设置麦克风是否静音，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setMicrophoneMute(isMute: boolean, callback: AsyncCallback<void>): void;
    /**
     * 设置麦克风是否静音，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setMicrophoneMute(isMute: boolean): Promise<void>;
    /**
     * 设备是否激活，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    isDeviceActive(deviceType: DeviceType, callback: AsyncCallback<boolean>): void;
    /**
     * 设备是否激活，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    isDeviceActive(deviceType: DeviceType): Promise<boolean>;
    /**
     * 设置设备激活，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setDeviceActive(deviceType: DeviceType, active: boolean, callback: AsyncCallback<boolean>): void;
    /**
     * 设置设备激活，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setDeviceActive(deviceType: DeviceType, active: boolean): Promise<boolean>;
    /**
     * 获取音频参数，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    getAudioParameter(key: string, callback: AsyncCallback<string>): void;
    /**
     * 获取音频参数，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    getAudioParameter(key: string): Promise<string>;
    /**
     * 设置音频参数，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setAudioParameter(key: string, value: string, callback: AsyncCallback<void>): void;
    /**
     * 设置音频参数，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setAudioParameter(key: string, value: string): Promise<void>;
  }

  /**
   * Describes an audio device.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface AudioDeviceDescriptor {
    /**
     * Audio device role
     * @devices
     */
    readonly deviceRole: DeviceRole;
    /**
     * Audio device type
     * @devices
     */
    readonly deviceType: DeviceType;
  }

  /**
   * A queue of AudioDeviceDescriptor, which is read-only.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  type AudioDeviceDescriptors = Array<Readonly<AudioDeviceDescriptor>>;
}

export default audio;