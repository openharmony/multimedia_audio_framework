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

#ifndef AUDIO_COMMON_NAPI_H_
#define AUDIO_COMMON_NAPI_H_

#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
namespace {
    const std::string INTERRUPT_CALLBACK_NAME = "interrupt";
    const std::string STATE_CHANGE_CALLBACK_NAME = "stateChange";
}

class AudioCommonNapi {
public:
    AudioCommonNapi() = delete;
    ~AudioCommonNapi() = delete;
    static std::string GetStringArgument(napi_env env, napi_value value);
};

struct AutoRef {
    AutoRef(napi_env env, napi_ref cb)
        : env_(env), cb_(cb)
    {
    }
    ~AutoRef()
    {
        if (env_ != nullptr && cb_ != nullptr) {
            (void)napi_delete_reference(env_, cb_);
        }
    }
    napi_env env_;
    napi_ref cb_;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_COMMON_NAPI_H_
