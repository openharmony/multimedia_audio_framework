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
import image from './@ohos.multimedia.image';

/**
 * @name session
 * @sysCap SystemCapability.Multimedia.Session
 * @import import audio from '@ohos.multimedia.session';
 */
declare namespace session {

  function getSystemSessionManager(context: Context): SystemSessionManager;

  interface SystemSessionManager {
    listAllSessionDescriptors(callback: AsyncCallback<Array<SessionDescriptor>>): void;
    listAllSessionDescriptors(): Promise<Array<SessionDescriptor>>;

    findSessionDescriptiorByTag(tag: string, callback: AsyncCallback<SessionDescriptor>): void;
    findSessionDescriptiorByTag(tag: string): Promise<SessionDescriptor>;

    createSessionController(sessionId: number, context: Context, callback: AsyncCallback<SessionController>): void;
    createSessionController(sessionId: number, context: Context): Promise<SessionController>;
  }

  interface SessionDescriptor {
    // Unique id for session
    readonly sessionId: number;

    readonly tag: string;

    readonly bundleName: string;

    readonly active: boolean;
  }

  function createSession(context: Context, tag: string, callback: AsyncCallback<Session>): void;
  function createSession(context: Context, tag: string): Promise<Session>;

  interface Session {
    updateActiveState(active: boolean, callback: AsyncCallback<void>): void;
    updateActiveState(active: boolean): Promise<void>;

    updateAVPlaybackState(state: AVPlaybackState, callback: AsyncCallback<void>): void;
    updateAVPlaybackState(state: AVPlaybackState): Promise<void>;

    updateAVMetadata(meta: AVMetadata, callback: AsyncCallback<void>): void;
    updateAVMetadata(meta: AVMetadata): Promise<void>;

    on(type: 'permissionCheck', callback: (pid: number, uid: number, bundleName: string) => {}): void;

    on(type: 'play', callback: () => {}): void;

    on(type: 'pause', callback: () => {}): void;

    on(type: 'stop', callback: () => {}): void;

    on(type: 'playNext', callback: () => {}): void;

    on(type: 'playPrevious', callback: () => {}): void;

    on(type: 'seek', callback: Callback<number>): void;

    on(type: 'setSpeed', callback: Callback<number>): void;

    release(callback: AsyncCallback<void>): void;
    release(): Promise<void>;
  }

  interface SessionController {
    getAVPlaybackState(callback: AsyncCallback<AVPlaybackState>): void;
    getAVPlaybackState(): Promise<AVPlaybackState>;

    getAVMetadata(callback: AsyncCallback<AVMetadata>): void;
    getAVMetadata(): Promise<AVMetadata>;

    sendCommandToPlay(callback: AsyncCallback<void>): void;
    sendCommandToPlay(): Promise<void>;

    sendCommandTolPause(callback: AsyncCallback<void>): void;
    sendCommandTolPause(): Promise<void>;

    sendCommandToStop(callback: AsyncCallback<void>): void;
    sendCommandToStop(): Promise<void>;

    sendCommandToPlayNext(callback: AsyncCallback<void>): void;
    sendCommandToPlayNext(): Promise<void>;

    sendCommandToPlayPrevious(callback: AsyncCallback<void>): void;
    sendCommandToPlayPrevious(): Promise<void>;

    sendCommandToPlay(timeMs: number, callback: AsyncCallback<void>): void;
    sendCommandToSeek(timeMs: number): Promise<void>;

    sendCommandToSetSpeed(speed: number, callback: AsyncCallback<void>): void;
    sendCommandToSetSpeed(speed: number): Promise<void>;

    on(type: 'sessionRelease', callback: () => {}): void;

    on(type: 'playbackStateUpdate', callback: (state: AVPlaybackState) => {}): void;

    on(type: 'metadataUpdate', callback: (meta: AVMetadata) => {}): void;

    release(callback: AsyncCallback<void>): void;
    release(): Promise<void>;
  }

  interface AVPlaybackState {

    readonly state: PlaybackState;

    readonly currentTime: number;
  }

  enum PlaybackState {
    /**
     * Invalid state.
     * @since 8
     */
    PLAYBACK_STATE_INVALID = -1,
    /**
     * Create New instance state.
     * @since 8
     */
    PLAYBACK_STATE_NEW,
    /**
     * Prepared state.
     * @since 8
     */
    PLAYBACK_STATE_PREPARED,
    /**
     * Running state.
     * @since 8
     */
    PLAYBACK_STATE_PLAYING,
    /**
     * Paused state.
     * @since 8
     */
    PLAYBACK_STATE_PAUSED,
    /**
     * Stopped state.
     * @since 8
     */
    PLAYBACK_STATE_STOPPED,
    /**
     * Released state.
     * @since 8
     */
    PLAYBACK_STATE_RELEASED
  }

  enum AVMetadataKey {
    METADATA_KEY_TITLE = "title",
    METADATA_KEY_SUBTITLE = "subtitle",
    METADATA_KEY_ARTIST = "artist",
    METADATA_KEY_DURATION = "duration",
    METADATA_KEY_DATE = "date",
    METADATA_KEY_DISPLAY_ICON = "display_icon"
  }

  interface AVMetadata {
    setNumberValue(key: AVMetadataKey, value: number, callback: AsyncCallback<void>): void;
    setNumberValue(key: AVMetadataKey, value: number): Promise<void>;

    setStringValue(key: AVMetadataKey, value: string, callback: AsyncCallback<void>): void;
    setStringValue(key: AVMetadataKey, value: string): Promise<void>;

    setPixelMapValue(key: AVMetadataKey, value: image.PixelMap, callback: AsyncCallback<void>): void;
    setPixelMapValue(key: AVMetadataKey, value: image.PixelMap): Promise<void>;
  }
}

export default session;