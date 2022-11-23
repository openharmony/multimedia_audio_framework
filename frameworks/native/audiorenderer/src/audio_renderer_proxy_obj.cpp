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

#include "audio_renderer_proxy_obj.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
void AudioRendererProxyObj::SaveRendererObj(const AudioRenderer *rendererObj)
{
    renderer = rendererObj;
}

void AudioRendererProxyObj::PausedStreamImpl(const StreamSetStateEventInternal &streamSetStateEventInternal)
{
    renderer->Pause(CMD_FROM_SYSTEM);
}

void AudioRendererProxyObj::ResumeStreamImpl(const StreamSetStateEventInternal &streamSetStateEventInternal)
{
    renderer->Start(CMD_FROM_SYSTEM);
}

void AudioRendererProxyObj::SetLowPowerVolumeImpl(float volume)
{
    renderer->SetLowPowerVolume(volume);
}

void AudioRendererProxyObj::GetLowPowerVolumeImpl(float &volume)
{
    volume = renderer->GetLowPowerVolume();
}

void AudioRendererProxyObj::GetSingleStreamVolumeImpl(float &volume)
{
    volume = renderer->GetSingleStreamVolume();
}
} // namespace AudioStandard
} // namespace OHOS
