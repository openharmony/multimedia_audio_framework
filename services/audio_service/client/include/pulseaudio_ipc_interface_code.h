/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#ifndef I_AUDIO_SERVER_INTERFACE_H
#define I_AUDIO_SERVER_INTERFACE_H

#include "audio_info.h"

/* SAID: 3001 */
namespace OHOS {
namespace AudioStandard {
    enum class AudioServerInterfaceCode {
        GET_AUDIO_PARAMETER,
        SET_AUDIO_PARAMETER,
        GET_EXTRA_AUDIO_PARAMETERS,
        SET_EXTRA_AUDIO_PARAMETERS,
        SET_MICROPHONE_MUTE,
        SET_AUDIO_SCENE,
        UPDATE_ROUTE_REQ,
        GET_TRANSACTION_ID,
        SET_PARAMETER_CALLBACK,
        GET_REMOTE_AUDIO_PARAMETER,
        SET_REMOTE_AUDIO_PARAMETER,
        NOTIFY_DEVICE_INFO,
        CHECK_REMOTE_DEVICE_STATE,
        SET_VOICE_VOLUME,
        SET_AUDIO_MONO_STATE,
        SET_AUDIO_BALANCE_VALUE,
        CREATE_AUDIOPROCESS,
        LOAD_AUDIO_EFFECT_LIBRARIES,
        REQUEST_THREAD_PRIORITY,
        CREATE_AUDIO_EFFECT_CHAIN_MANAGER,
        SET_OUTPUT_DEVICE_SINK,
        CREATE_PLAYBACK_CAPTURER_MANAGER,
        SET_SUPPORT_STREAM_USAGE,
        REGISET_POLICY_PROVIDER,
        SET_WAKEUP_CLOSE_CALLBACK,
        SET_CAPTURE_SILENT_STATE,
        UPDATE_SPATIALIZATION_STATE,
        OFFLOAD_SET_VOLUME,
        OFFLOAD_DRAIN,
        OFFLOAD_GET_PRESENTATION_POSITION,
        OFFLOAD_SET_BUFFER_SIZE,
        NOTIFY_STREAM_VOLUME_CHANGED,
        GET_CAPTURE_PRESENTATION_POSITION,
        GET_RENDER_PRESENTATION_POSITION,
        AUDIO_SERVER_CODE_MAX = GET_RENDER_PRESENTATION_POSITION,
    };
} // namespace AudioStandard
} // namespace OHOS

#endif // I_AUDIO_SERVER_INTERFACE_H
