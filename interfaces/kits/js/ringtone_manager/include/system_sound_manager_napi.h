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

#ifndef SYSTEM_SOUND_MNGR_NAPI_H_
#define SYSTEM_SOUND_MNGR_NAPI_H_

#include "iringtone_sound_manager.h"
#include "ringtone_player_napi.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "napi_base_context.h"
#include "ability.h"

namespace OHOS {
namespace AudioStandard {
static const std::string SYSTEM_SND_MNGR_NAPI_CLASS_NAME = "SystemSoundManager";

static const std::map<std::string, RingtoneType> ringtoneTypeMap = {
    {"RINGTONE_TYPE_DEFAULT", RINGTONE_TYPE_DEFAULT},
    {"RINGTONE_TYPE_MULTISIM", RINGTONE_TYPE_MULTISIM}
};

class SystemSoundManagerNapi {
public:
    static napi_value Init(napi_env env, napi_value exports);

    SystemSoundManagerNapi();
    ~SystemSoundManagerNapi();

private:
    static void Destructor(napi_env env, void* nativeObject, void* finalize_hint);
    static napi_value Construct(napi_env env, napi_callback_info info);
    static napi_value GetSystemSoundManager(napi_env env, napi_callback_info info);
    static napi_value SetSystemRingtoneUri(napi_env env, napi_callback_info info);
    static napi_value GetSystemRingtoneUri(napi_env env, napi_callback_info info);
    static napi_value GetSystemRingtonePlayer(napi_env env, napi_callback_info info);
    static napi_value SetSystemNotificationUri(napi_env env, napi_callback_info info);
    static napi_value GetSystemNotificationUri(napi_env env, napi_callback_info info);
    static napi_value SetSystemAlarmUri(napi_env env, napi_callback_info info);
    static napi_value GetSystemAlarmUri(napi_env env, napi_callback_info info);
    static napi_status AddNamedProperty(napi_env env, napi_value object, const std::string name, int32_t enumValue);
    static napi_value CreateRingtoneTypeObject(napi_env env);
    static std::shared_ptr<AbilityRuntime::Context> GetAbilityContext(napi_env env, napi_value contextArg);

    static napi_ref ringtoneType_;
    static napi_ref sConstructor_;

    napi_env env_;

    std::shared_ptr<IRingtoneSoundManager> sysSoundMgrClient_ = nullptr;
};

struct SystemSoundManagerAsyncContext {
    napi_env env;
    napi_async_work work;
    napi_deferred deferred;
    napi_ref callbackRef;
    bool status;
    SystemSoundManagerNapi *objectInfo;
    std::shared_ptr<AbilityRuntime::Context> abilityContext_;
    std::string uri;
    std::shared_ptr<IRingtonePlayer> ringtonePlayer;
    int32_t ringtoneType;
};
} // namespace AudioStandard
} // namespace OHOS
#endif /* SYSTEM_SOUND_MNGR_NAPI_H_ */