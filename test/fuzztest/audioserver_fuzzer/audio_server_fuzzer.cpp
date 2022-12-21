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

#include "audio_server.h"
#include "message_parcel.h"
using namespace std;

namespace OHOS {
namespace AudioStandard {
constexpr int32_t OFFSET = 4;
const std::u16string FORMMGR_INTERFACE_TOKEN = u"IStandardAudioService";
const int32_t SYSTEM_ABILITY_ID = 3001;
const bool RUN_ON_CREATE = false;
const int32_t LIMITSIZE = 4;
const int32_t SHIFT_LEFT_8 = 8;
const int32_t SHIFT_LEFT_16 = 16;
const int32_t SHIFT_LEFT_24 = 24;

uint32_t Convert2Uint32(const uint8_t *ptr)
{
    if (ptr == nullptr) {
        return 0;
    }
    /* Move the 0th digit to the left by 24 bits, the 1st digit to the left by 16 bits,
       the 2nd digit to the left by 8 bits, and the 3rd digit not to the left */
    return (ptr[0] << SHIFT_LEFT_24) | (ptr[1] << SHIFT_LEFT_16) | (ptr[2] << SHIFT_LEFT_8) | (ptr[3]);
}

void AudioServerFuzzTest(const uint8_t *rawData, size_t size)
{
    if (rawData == nullptr || size < LIMITSIZE) {
        return;
    }
    uint32_t code = Convert2Uint32(rawData);
    rawData = rawData + OFFSET;
    size = size - OFFSET;
    
    MessageParcel data;
    data.WriteInterfaceToken(FORMMGR_INTERFACE_TOKEN);
    data.WriteBuffer(rawData, size);
    data.RewindRead(0);
    MessageParcel reply;
    MessageOption option;

    std::shared_ptr<AudioServer> AudioServerPtr =
        std::make_shared<AudioServer>(SYSTEM_ABILITY_ID, RUN_ON_CREATE);
    AudioServerPtr->OnRemoteRequest(code, data, reply, option);
}
} // namespace AudioStandard
} // namesapce OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Run your code on data */
    OHOS::AudioStandard::AudioServerFuzzTest(data, size);
    return 0;
}