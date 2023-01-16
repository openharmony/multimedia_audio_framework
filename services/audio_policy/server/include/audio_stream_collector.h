/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef AUDIO_STREAM_COLLECTOR_H
#define AUDIO_STREAM_COLLECTOR_H

#include "audio_info.h"
#include "audio_stream_event_dispatcher.h"

namespace OHOS {
namespace AudioStandard {
class AudioStreamCollector {
public:
    static AudioStreamCollector& GetAudioStreamCollector()
    {
        static AudioStreamCollector audioStreamCollector;
        return audioStreamCollector;
    }

    AudioStreamCollector();
    ~AudioStreamCollector();
    int32_t RegisterAudioRendererEventListener(int32_t clientUID, const sptr<IRemoteObject> &object,
        bool hasBTPermission);
    int32_t UnregisterAudioRendererEventListener(int32_t clientUID);
    int32_t RegisterAudioCapturerEventListener(int32_t clientUID, const sptr<IRemoteObject> &object,
        bool hasBTPermission);
    int32_t UnregisterAudioCapturerEventListener(int32_t clientUID);
    int32_t RegisterTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo,
        const sptr<IRemoteObject> &object);
    int32_t UpdateTracker(AudioMode &mode, AudioStreamChangeInfo &streamChangeInfo);
    int32_t UpdateTracker(const AudioMode &mode, DeviceInfo &deviceInfo);
    int32_t GetCurrentRendererChangeInfos(vector<unique_ptr<AudioRendererChangeInfo>> &rendererChangeInfos);
    int32_t GetCurrentCapturerChangeInfos(vector<unique_ptr<AudioCapturerChangeInfo>> &capturerChangeInfos);
    void RegisteredTrackerClientDied(int32_t uid);
    void RegisteredStreamListenerClientDied(int32_t uid);
    int32_t UpdateStreamState(int32_t clientUid, StreamSetStateEventInternal &streamSetStateEventInternal);
    int32_t SetLowPowerVolume(int32_t streamId, float volume);
    float GetLowPowerVolume(int32_t streamId);
    float GetSingleStreamVolume(int32_t streamId);
    bool GetAndCompareStreamType(AudioStreamType requiredType, AudioRendererInfo rendererInfo);
private:
    AudioStreamEventDispatcher &mDispatcherService;
    std::mutex streamsInfoMutex_;
    std::map<std::pair<int32_t, int32_t>, int32_t> rendererStatequeue_;
    std::map<std::pair<int32_t, int32_t>, int32_t> capturerStatequeue_;
    std::vector<std::unique_ptr<AudioRendererChangeInfo>> audioRendererChangeInfos_;
    std::vector<std::unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos_;
    std::unordered_map<int32_t, std::shared_ptr<AudioClientTracker>> clientTracker_;
    static const std::map<std::pair<ContentType, StreamUsage>, AudioStreamType> streamTypeMap_;
    static std::map<std::pair<ContentType, StreamUsage>, AudioStreamType> CreateStreamMap();
    int32_t AddRendererStream(AudioStreamChangeInfo &streamChangeInfo);
    int32_t AddCapturerStream(AudioStreamChangeInfo &streamChangeInfo);
    int32_t UpdateRendererStream(AudioStreamChangeInfo &streamChangeInfo);
    int32_t UpdateCapturerStream(AudioStreamChangeInfo &streamChangeInfo);
    int32_t UpdateRendererDeviceInfo(DeviceInfo &outputDeviceInfo);
    int32_t UpdateCapturerDeviceInfo(DeviceInfo &inputDeviceInfo);
};
} // namespace AudioStandard
} // namespace OHOS
#endif
