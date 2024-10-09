/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#ifndef I_AUDIO_ENGINE_H
#define I_AUDIO_ENGINE_H

#include "audio_device_info.h"
namespace OHOS {
namespace AudioStandard {
class IAudioEngine {
public:
    IAudioEngine() = default;
    virtual ~IAudioEngine() = default;
    virtual int32_t Init(const DeviceInfo &type, bool isVoip) = 0;
    virtual int32_t Start() = 0;
    virtual int32_t Stop() = 0;
    virtual int32_t Pause() = 0;
    virtual int32_t Flush() = 0;
    virtual bool IsPlaybackEngineRunning() const noexcept = 0;
};
} // namespace AudioStandard
} // namespace OHOS
#endif
