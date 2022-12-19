/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef RINGTONE_PLAYER_NAPI_H_
#define RINGTONE_PLAYER_NAPI_H_

#include <map>

#include "iringtone_player.h"
#include "ringtone_options_napi.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
static const std::string RINGTONE_PLAYER_NAPI_CLASS_NAME = "RingtonePlayer";

class RingtonePlayerNapi {
public:
    static napi_value Init(napi_env env, napi_value exports);
    static napi_value GetRingtonePlayerInstance(napi_env env, std::shared_ptr<IRingtonePlayer> &iRingtonePlayer);

    RingtonePlayerNapi();
    ~RingtonePlayerNapi();

private:
    static void RingtonePlayerNapiDestructor(napi_env env, void* nativeObject, void* finalize_hint);
    static napi_value RingtonePlayerNapiConstructor(napi_env env, napi_callback_info info);
    static napi_value GetTitle(napi_env env, napi_callback_info info);
    static napi_value GetAudioRendererInfo(napi_env env, napi_callback_info info);
    static napi_value Configure(napi_env env, napi_callback_info info);
    static napi_value Start(napi_env env, napi_callback_info info);
    static napi_value Stop(napi_env env, napi_callback_info info);
    static napi_value Release(napi_env env, napi_callback_info info);
    static napi_value GetAudioState(napi_env env, napi_callback_info info);
    static napi_status AddNamedProperty(napi_env env, napi_value object, const std::string name, int32_t enumValue);

    napi_env env_;

    std::shared_ptr<IRingtonePlayer> iRingtonePlayer;

    static napi_ref sConstructor_;
    static std::shared_ptr<IRingtonePlayer> sIRingtonePlayer_;
};

struct RingtonePlayerAsyncContext {
    napi_env env;
    napi_async_work work;
    napi_deferred deferred;
    napi_ref callbackRef;
    bool status;
    RingtonePlayerNapi *objectInfo;
    ContentType contentType;
    StreamUsage streamUsage;
    int32_t rendererFlags;
    float volume;
    bool loop;
    std::string title;
    RingtoneState *ringtoneState;
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* RINGTONE_PLAYER_NAPI_H_ */