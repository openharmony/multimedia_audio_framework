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

#include "audio_interrupt_manager_napi.h"

#include "audio_common_napi.h"
#include "audio_micstatechange_callback_napi.h"
#include "audio_errors.h"
#include "audio_log.h"
#include "hilog/log.h"
#include "napi_base_context.h"

using namespace std;
using OHOS::HiviewDFX::HiLog;
using OHOS::HiviewDFX::HiLogLabel;

namespace OHOS {
namespace AudioStandard {
static __thread napi_ref g_interruptManagerConstructor = nullptr;

namespace {
    const int ARGS_TWO = 2;
    const int PARAM0 = 0;
    constexpr HiLogLabel LABEL = {LOG_CORE, LOG_DOMAIN, "AudioInterruptManagerNapi"};
}

struct AudioInterruptManagerAsyncContext {
    napi_env env;
    napi_async_work work;
    napi_deferred deferred;
    napi_ref callbackRef = nullptr;
    int32_t deviceFlag;
    bool bArgTransFlag = true;
    int32_t status = SUCCESS;
    int32_t groupId;
    std::string networkId;
    AudioInterruptManagerNapi *objectInfo;
};

AudioInterruptManagerNapi::AudioInterruptManagerNapi()
    : audioSystemMngr_(nullptr), env_(nullptr) {}

AudioInterruptManagerNapi::~AudioInterruptManagerNapi() = default;

void AudioInterruptManagerNapi::Destructor(napi_env env, void *nativeObject, void *finalize_hint)
{
    if (nativeObject != nullptr) {
        auto obj = static_cast<AudioInterruptManagerNapi *>(nativeObject);
        delete obj;
        obj = nullptr;
    }
}

napi_value AudioInterruptManagerNapi::Construct(napi_env env, napi_callback_info info)
{
    AUDIO_INFO_LOG("AudioInterruptManagerNapi::Construct");
    napi_status status;
    napi_value result = nullptr;
    napi_get_undefined(env, &result);

    size_t argc = ARGS_TWO;
    napi_value argv[ARGS_TWO] = {0};
    napi_value thisVar = nullptr;
    void *data = nullptr;
    napi_get_cb_info(env, info, &argc, argv, &thisVar, &data);
    unique_ptr<AudioInterruptManagerNapi> audioInterruptManagerNapi = make_unique<AudioInterruptManagerNapi>();
    CHECK_AND_RETURN_RET_LOG(audioInterruptManagerNapi != nullptr, result, "No memory");

    audioInterruptManagerNapi->audioSystemMngr_ = AudioSystemManager::GetInstance();
        
    audioInterruptManagerNapi->env_ = env;

    status = napi_wrap(env, thisVar, static_cast<void*>(audioInterruptManagerNapi.get()),
        AudioInterruptManagerNapi::Destructor, nullptr, nullptr);
    if (status == napi_ok) {
        audioInterruptManagerNapi.release();
        return thisVar;
    }

    HiLog::Error(LABEL, "Failed in AudioInterruptManager::Construct()!");
    return result;
}

napi_value AudioInterruptManagerNapi::CreateInterruptManagerWrapper(napi_env env)
{
    napi_status status;
    napi_value result = nullptr;
    napi_value constructor;

    status = napi_get_reference_value(env, g_interruptManagerConstructor, &constructor);
    if (status == napi_ok) {
        status = napi_new_instance(env, constructor, 0, nullptr, &result);
        if (status == napi_ok) {
            return result;
        }
    }
    HiLog::Error(LABEL, "Failed in AudioInterruptManagerNapi::CreateInterruptManagerWrapper!");
    napi_get_undefined(env, &result);

    return result;
}

napi_value AudioInterruptManagerNapi::Init(napi_env env, napi_value exports)
{
    AUDIO_INFO_LOG("AudioInterruptManagerNapi::Init");
    napi_status status;
    napi_value constructor;
    napi_value result = nullptr;
    const int32_t refCount = 1;
    napi_get_undefined(env, &result);

    napi_property_descriptor audio_interrupt_manager_properties[] = {

    };

    status = napi_define_class(env, AUDIO_INTERRUPT_MANAGER_NAPI_CLASS_NAME.c_str(), NAPI_AUTO_LENGTH, Construct,
        nullptr, sizeof(audio_interrupt_manager_properties) / sizeof(audio_interrupt_manager_properties[PARAM0]),
        audio_interrupt_manager_properties, &constructor);
    if (status != napi_ok) {
        return result;
    }
    status = napi_create_reference(env, constructor, refCount, &g_interruptManagerConstructor);
    if (status == napi_ok) {
        status = napi_set_named_property(env, exports, AUDIO_INTERRUPT_MANAGER_NAPI_CLASS_NAME.c_str(), constructor);
        if (status == napi_ok) {
            return exports;
        }
    }

    HiLog::Error(LABEL, "Failure in AudioInterruptManagerNapi::Init()");
    return result;
}

} // namespace AudioStandard
} // namespace OHOS
