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
   * Obtains an SoundPlayer instance.
   * @sysCap SystemCapability.Multimedia.Audio
   * @devices
   */
  function createSoundPlayer(): SoundPlayer;
  /**
   * Obtains an ToneDescriptor instance.
   * @sysCap SystemCapability.Multimedia.Audio
   * @devices
   */
  function createToneDescriptor(tone: ToneType): ToneDescriptor;
  /**
   * Obtains an SoundEffectBuilder instance.
   * @sysCap SystemCapability.Multimedia.Audio
   * @devices
   */
  function getSoundEffectBuilder(): SoundEffectBuilder;

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
   * 呼叫状态枚举.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum CallState {
    /**
     * 空闲状态
     */
    IDLE = 0,
    /**
     * 打电话状态
     */
    IN_CALL = 1,
    /**
     * 处于voip状态
     */
    IN_VOIP = 2,
    /**
     * 铃声状态
     */
    RINGTONE = 3,
  }
  /**
   * 音频编码方式枚举.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum AudioEncodingFormat {
    /**
     * 默认
     */
    ENCODING_DEFAULT = 0,
    /**
     * 无效
     */
    ENCODING_INVALID = 1,
    /**
     * mp3
     */
    ENCODING_MP3 = 2,
    /**
     * pcm16bit
     */
    ENCODING_PCM_16BIT = 3,
    /**
     * pcm8bit
     */
    ENCODING_PCM_8BIT = 4,
    /**
     * pcmfloat
     */
    ENCODING_PCM_FLOAT = 5,
  }
  /**
   * 音频内容枚举.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum ContentType {
    /**
     * 电影
     */
    CONTENT_TYPE_MOVIE = 0,
    /**
     * 音乐
     */
    CONTENT_TYPE_MUSIC = 1,
    /**
     * 可视化
     */
    CONTENT_TYPE_SONIFICATION = 2,
    /**
     * 演讲
     */
    CONTENT_TYPE_SPEECH = 3,
    /**
     * 未知
     */
    CONTENT_TYPE_UNKNOWN = 4,
  }

  enum InterruptType {
    /**
     * 静音
     */
    INTERRUPT_HINT_DUCK = 0,
    /**
     * 空
     */
    INTERRUPT_HINT_NONE = 1,
    /**
     * 暂停
     */
    INTERRUPT_HINT_PAUSE = 2,
    /**
     * 重新开始
     */
    INTERRUPT_HINT_RESUME = 3,
    /**
     * 停止
     */
    INTERRUPT_HINT_STOP = 4,
    /**
     * 不静音
     */
    INTERRUPT_HINT_UNDUCK = 5,
    /**
     * 开始
     */
    INTERRUPT_TYPE_BEGIN = 6,
    /**
     * 结束
     */
    INTERRUPT_TYPE_END = 7,
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
     * 获取主输出帧数，回调方式返回帧数。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    getMasterOutputFrameCount(callback: AsyncCallback<number>): void;
    /**
     * 获取主输出帧数，promise方式返回帧数。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    getMasterOutputFrameCount(): Promise<number>;
    /**
     * 获取主输出采样率，回调方式返回采样率。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    getMasterOutputSampleRate(callback: AsyncCallback<number>): void;
    /**
     * 获取主输出采样率，promise方式返回采样率。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    getMasterOutputSampleRate(): Promise<number>;
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
     * 判断主设备是否静音，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    isMasterMute(callback: AsyncCallback<void>): void; //不倾向与实现
    /**
     * 判断主设备是否静音，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    isMasterMute(): Promise<void>; //不倾向与实现
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
     * 设置主设备静音，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setMasterMute(mute: boolean, callback: AsyncCallback<void>): void; //不倾向与实现
    /**
     * 设置主设备静音，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setMasterMute(mute: boolean): Promise<void>; //不倾向与实现
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
     * 连接蓝牙设备，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    connectBluetoothSco(callback: AsyncCallback<void>): void;
    /**
     * 连接蓝牙设备，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    connectBluetoothSco(): Promise<void>;
    /**
     * 断开蓝牙设备，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    disconnectBluetoothSco(callback: AsyncCallback<void>): void;
    /**
     * 断开蓝牙设备，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    disconnectBluetoothSco(): Promise<void>;
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
    /**
     * 获取打电话状态，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    getCallState(callback: AsyncCallback<CallState>): void;
    /**
     * 获取打电话状态，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    getCallState(): Promise<CallState>;
    /**
     * 设置打电话状态，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setCallState(callState: CallState, callback: AsyncCallback<void>): void;
    /**
     * 设置打电话状态，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setCallState(callState: CallState): Promise<void>;
    /**
     * 激活音频中断。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    activateAudioInterrupt(interrupt: AudioInterrupt): void;
    /**
     * 不激活音频中断。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    deactivateAudioInterrupt(interrupt: AudioInterrupt): void;
    /**
     * 监听音频中断。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'interrupt', callback: Callback<InterruptType>): void;
    /**
     * 监听错误消息。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'error', callback: ErrorCallback): void;
    /**
     * 监听设备变化。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'deviceChange', callback: Callback<AudioDeviceDescriptor>): void;
  }

  /**
   * Describes an audio device.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface AudioDeviceDescriptor {
    /**
     * id
     * @devices
     */
    id: number;
    /**
     * 名字
     * @devices
     */
    name: string;
    /**
     * 地址
     * @devices
     */
    address: string;
    /**
     * 采样率
     * @devices
     */
    sampleRate: Array<number>;
    /**
     * 通道数
     * @devices
     */
    channelCounts: Array<Readonly<number>>;
    /**
     * 通道的mask序列
     * @devices
     */
    channelIndexMasks: Array<number>;
    /**
     * 通道的mask
     * @devices
     */
    channelMasks: Array<number>;
    /**
     * 哈希值
     * @devices
     */
    hashCode: number;
    /**
     * 编码方式
     * @devices
     */
    encodeFormat: Array<AudioEncodingFormat>;
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
   * 音频通道的类型
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum ChannelMask {
    /**
     * mono
     */
    CHANNEL_IN_MONO = 0,
    /**
     * stereo
     */
    CHANNEL_IN_STEREO = 1,
    /**
     * 无效
     */
    CHANNEL_INVALID = 2,
  }
  /**
   * 音频中断。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface AudioInterrupt {
    /**
     * 音频流类型
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    streamType: AudioVolumeType;
    /**
     * 音频内容
     * @devices
     */
    contentType: ContentType;
    /**
     * 暂停时是否静音
     * @devices
     */
    pauseWhenDucked: boolean;
    /**
     * 中断类型
     * @devices
     */
    streamFlag: InterruptType;
    /**
     * 音频通道的mask
     * @devices
     */
    chanelMask: ChannelMask;
    /**
     * 音频编码方式
     * @devices
     */
    encodingFormat: AudioEncodingFormat;
  }

  /**
   * A queue of AudioDeviceDescriptor, which is read-only.
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  type AudioDeviceDescriptors = Array<Readonly<AudioDeviceDescriptor>>;
  /**
   * 短音类型。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum SoundType {
    /**
     * 点击
     */
    KEY_CLICK = 0,
    /**
     * 删除
     */
    KEYPRESS_DELETE = 1,
    /**
     * 无效
     */
    KEYPRESS_INVALID = 2,
    /**
     * 返回
     */
    KEYPRESS_RETURN = 3,
    /**
     * 空格键
     */
    KEYPRESS_SPACEBAR = 4,
    /**
     * 标准
     */
    KEYPRESS_STANDARD = 5,
    /**
     * 下
     */
    NAVIGATION_DOWN = 6,
    /**
     * 左
     */
    NAVIGATION_LEFT = 7,
    /**
     * 右
     */
    NAVIGATION_RIGHT = 8,
    /**
     * 上
     */
    NAVIGATION_UP = 9,
  }
  /**
   * 短音播放器。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface SoundPlayer {
    /**
     * 读取短音，返回id，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    loadSound(path: string, callback: AsyncCallback<number>): void;
    /**
     * 读取短音，返回id，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    loadSound(path: string): Promise<number>;
    /**
     * 通过tone读取短音，返回id，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    loadSoundByTone(tone: ToneType, durationMs: number, callback: AsyncCallback<number>): void;
    /**
     * 通过tone读取短音，返回id，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    loadSoundByTone(tone: ToneType, durationMs: number): Promise<number>;
    /**
     * 通过流类型读取短音，返回id，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    loadSoundByStream(audioStream: AudioVolumeType, volume: number, callback: AsyncCallback<number>);
    /**
     * 通过流类型读取短音，返回id，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    loadSoundByStream(audioStream: AudioVolumeType, volume: number): Promise<number>;
    /**
     * 删除短音。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    unloadSound(soundId: number);
    /**
     * 播放短音，回调方式返回id。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    play(soundId: number, options: SoundOptions, callback: AsyncCallback<number>): void;
    /**
     * 播放短音，回调方式返回id。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    play(soundId: number, callback: AsyncCallback<number>): void;
    /**
     * 播放短音，promise方式返回id。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    play(soundId: number, options?: SoundOptions): Promise<number>;
    /**
     * 暂停短音。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    pause(taskId: number): void;
    /**
     * 暂停所有短音。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    pauseAll(): void;
    /**
     * 重新开始短音。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    resume(taskId: number): void;
    /**
     * 重新开始所有短音。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    resumeAll(): void;
    /**
     * 停止短音。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    stop(taskId: number): void;
    /**
     * 设置参数。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    setOptions(taskId: number, options: SoundOptions): void;
    /**
     * 释放。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    release(): void;
    /**
     * 监听暂停消息。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'pause', callback: Callback<number>): void;
    /**
     * 监听暂停所有消息。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'pauseAll', callback: Callback<void>): void;
    /**
     * 监听重新开始消息。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'resume', callback: Callback<number>): void;
    /**
     * 监听重新开始所有的消息。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'resumeAll', callback: Callback<void>): void;
    /**
     * 监听暂停的消息。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'stop', callback: Callback<number>): void;
    /**
     * 监听参数变化的消息。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'optionsChange', callback: Callback<number>): void;
    /**
     * 监听完成的消息。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'complete', callback: Callback<number>): void;
    /**
     * 监听错误消息。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'error', callback: ErrorCallback): void;
    /**
     * 监听释放的消息。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'release', callback: Callback<void>): void;
  }
  /**
   * 短音参数。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface SoundOptions {
    /**
     * 循环次数
     * @devices
     */
    loopNumber: number;
    /**
     * 速率
     * @devices
     */
    speed: number;
    /**
     * 音量
     * @devices
     */
    volumes: SoundVolumes;
    /**
     * 优先级
     * @devices
     */
    priority: number;
  }
  /**
   * 短音音量
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface SoundVolumes {
    /**
     * 左声道
     * @devices
     */
    left: number;
    /**
     * 右声道
     * @devices
     */
    right: number;
  }
  /**
   * tone类型
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum ToneType {
    /**
     * DTMF_0
     */
    DTMF_0 = 0,
    /**
     * DTMF_1
     */
    DTMF_1 = 1,
    /**
     * DTMF_2
     */
    DTMF_2 = 2,
    /**
     * DTMF_3
     */
    DTMF_3 = 3,
    /**
     * DTMF_4
     */
    DTMF_4 = 4,
    /**
     * DTMF_5
     */
    DTMF_5 = 5,
    /**
     * DTMF_6
     */
    DTMF_6 = 6,
    /**
     * DTMF_7
     */
    DTMF_7 = 7,
    /**
     * DTMF_8
     */
    DTMF_8 = 8,
    /**
     * DTMF_9
     */
    DTMF_9 = 9,
    /**
     * DTMF_A
     */
    DTMF_A = 10,
    /**
     * DTMF_B
     */
    DTMF_B = 11,
    /**
     * DTMF_C
     */
    DTMF_C = 12,
    /**
     * DTMF_D
     */
    DTMF_D = 13,
    /**
     * DTMF_P
     */
    DTMF_P = 14,
    /**
     * DTMF_S
     */
    DTMF_S = 15,
    /**
     * PROP_PROMPT
     */
    PROP_PROMPT = 16,
    /**
     * SUP_CALL_WAITING
     */
    SUP_CALL_WAITING = 17,
  }
  /**
   * tone描述
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface ToneDescriptor {
    /**
     * tone类型
     * @devices
     */
    toneType: ToneType;
    /**
     * 高频
     * @devices
     */
    highFrequency: number;
    /**
     * 低频
     * @devices
     */
    lowFrequency: number;
  }
  /**
   * 声音特效模式。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum SoundEffectMode {
    /**
     * 辅助
     */
    SOUND_EFFECT_MODE_AUXILIARY = 0,
    /**
     * 插入
     */
    SOUND_EFFECT_MODE_INSERT = 1,
    /**
     * 待加工
     */
    SOUND_EFFECT_MODE_PRE_PROCESSING = 2,
  }
  /**
   * 声音特效类型。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum SoundEffectType {
    /**
     * AE
     */
    SOUND_EFFECT_TYPE_AE = 0,
    /**
     * EC
     */
    SOUND_EFFECT_TYPE_EC = 1,
    /**
     * GC
     */
    SOUND_EFFECT_TYPE_GC = 2,
    /**
     * 无效
     */
    SOUND_EFFECT_TYPE_INVALID = 3,
    /**
     * NS
     */
    SOUND_EFFECT_TYPE_NS = 4,
  }
  /**
   * 声音特效衍生类型。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  enum SoundEffectDerivative {
    /**
     * GAIN_CONTRAL_EFFECT
     */
    GAIN_CONTRAL_EFFECT = 0,
    /**
     * ECHO_CANCELER_EFFECT
     */
    ECHO_CANCELER_EFFECT = 1,
    /**
     * NOISE_SUPPRESSOR_EFFECT
     */
    NOISE_SUPPRESSOR_EFFECT = 2,
  }
  /**
   * 声音特效创建器。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface SoundEffectBuilder {
    /**
     * 获取可创建的声音特效，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    acquireEffects(callback: AsyncCallback<Array<EffectInfo>>): void;
    /**
     * 获取可创建的声音特效，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    acquireEffects(): Promise<Array<EffectInfo>>;
    /**
     * 创建声音特效，回调方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    create(options: EffectOptions, callback: AsyncCallback<SoundEffect>): void;
    /**
     * 创建声音特效，promise方式返回。
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    create(options: EffectOptions): Promise<SoundEffect>;
  }
  /**
   * 声音特效。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface SoundEffect {
    /**
     * 特效音模式
     * @devices
     */
    mode: SoundEffectMode;
    /**
     * 名字
     * @devices
     */
    name: string;
    /**
     * 类型
     * @devices
     */
    type: SoundEffectType;
    /**
     * 衍生类型
     * @devices
     */
    deriveType: SoundEffectDerivative;
    /**
     * uid
     * @devices
     */
    uid: string;
    /**
     * 激活
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    active(): void;
    /**
     * 停止
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    deactive(): void;
    /**
     * 释放
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    release(): void;
    /**
     * 监听激活的消息
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'active', callback: Callback<void>): void;
    /**
     * 监听停止的消息
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'deactive', callback: Callback<void>): void;
    /**
     * 监听释放的消息
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'release', callback: Callback<void>): void;
    /**
     * 监听错误消息
     * @sysCap SystemCapability.Multimedia.Audio
     * @devices
     */
    on(type: 'error', callback: ErrorCallback): void;
  }
  /**
   * 声音特效信息。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface EffectInfo {
    /**
     * 衍生类型
     * @devices
     */
    deriveType: SoundEffectDerivative;
    /**
     * 是否可获取
     * @devices
     */
    isAvailable: boolean;
  }
  /**
   * 声音特效选项。
   * @devices
   * @sysCap SystemCapability.Multimedia.Audio
   */
  interface EffectOptions {
    /**
     * 衍生类型
     * @devices
     */
    deriveType: SoundEffectDerivative
    /**
     * 音频播放器
     * @devices
     */
    audioPlayer?: AudioPlayer;
    /**
     * 视频播放器
     * @devices
     */
    videoPlayer?: VideoPlayer;
    /**
     * 包名
     * @devices
     */
    pkgName: string;
  }
}

export default audio;