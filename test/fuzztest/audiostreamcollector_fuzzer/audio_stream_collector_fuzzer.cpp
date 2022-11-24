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

#include <iostream>
#include <cstddef>
#include <cstdint>
#include "audio_info.h"
#include "audio_stream_collector.h"
#include "message_parcel.h"
using namespace std;

namespace OHOS {
namespace AudioStandard {
const std::u16string FORMMGR_INTERFACE_TOKEN = u"IAudioPolicy";
const int32_t LIMITSIZE = 4;

void AudioStreamCollectorFuzzTest(const uint8_t *rawData, size_t size)
{
    if (rawData == nullptr || size < LIMITSIZE) {
        return;
    }

    MessageParcel data;
    data.WriteInterfaceToken(FORMMGR_INTERFACE_TOKEN);
    data.WriteBuffer(rawData, size);
    data.RewindRead(0);
    
    sptr<IRemoteObject> object = data.ReadRemoteObject();
    int32_t clientUID = *reinterpret_cast<const int32_t *>(rawData);
    bool hasBTPermission = *reinterpret_cast<const bool *>(rawData);
    AudioStreamCollector::GetAudioStreamCollector()
        .RegisterAudioRendererEventListener(clientUID, object, hasBTPermission);
    AudioStreamCollector::GetAudioStreamCollector().UnregisterAudioRendererEventListener(clientUID);
    AudioStreamCollector::GetAudioStreamCollector()
        .RegisterAudioCapturerEventListener(clientUID, object, hasBTPermission);
    AudioStreamCollector::GetAudioStreamCollector().UnregisterAudioCapturerEventListener(clientUID);

    int32_t uid = *reinterpret_cast<const int32_t *>(rawData);
    AudioStreamCollector::GetAudioStreamCollector().RegisteredTrackerClientDied(uid);
    AudioStreamCollector::GetAudioStreamCollector().RegisteredStreamListenerClientDied(uid);

    int32_t clientUid = *reinterpret_cast<const int32_t *>(rawData);
    StreamSetStateEventInternal streamSetStateEventInternal = {};
    streamSetStateEventInternal.streamSetState = *reinterpret_cast<const StreamSetState *>(rawData);
    streamSetStateEventInternal.audioStreamType = *reinterpret_cast<const AudioStreamType *>(rawData);
    AudioStreamCollector::GetAudioStreamCollector().
        UpdateStreamState(clientUid, streamSetStateEventInternal);

    int32_t streamId = *reinterpret_cast<const int32_t *>(rawData);
    float volume = *reinterpret_cast<const float *>(rawData);
    AudioStreamCollector::GetAudioStreamCollector().SetLowPowerVolume(streamId, volume);
    AudioStreamCollector::GetAudioStreamCollector().GetLowPowerVolume(streamId);
    AudioStreamCollector::GetAudioStreamCollector().GetSingleStreamVolume(streamId);
}
} // namespace AudioStandard
} // namesapce OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    OHOS::AudioStandard::AudioStreamCollectorFuzzTest(data, size);
    return 0;
}
