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

#ifndef AUDIO_RINGERMODE_CALLBACK_NAPI_H_
#define AUDIO_RINGERMODE_CALLBACK_NAPI_H_

#include "audio_common_napi.h"
#include "audio_manager_napi.h"
#include "audio_system_manager.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
class AudioRingerModeCallbackNapi : public AudioRingerModeCallback {
public:
    explicit AudioRingerModeCallbackNapi(napi_env env);
    virtual ~AudioRingerModeCallbackNapi();
    void SaveCallbackReference(const std::string &callbackName, napi_value callback);
    void OnRingerModeUpdated(const AudioRingerMode &ringerMode) override;

private:
    struct AudioRingerModeJsCallback {
        std::shared_ptr<AutoRef> callback = nullptr;
        std::string callbackName = "unknown";
        AudioRingerMode ringerMode = RINGER_MODE_NORMAL;
    };

    void OnJsCallbackRingerMode(std::unique_ptr<AudioRingerModeJsCallback> &jsCb);

    std::mutex mutex_;
    napi_env env_ = nullptr;
    std::shared_ptr<AutoRef> ringerModeCallback_ = nullptr;
};
}  // namespace AudioStandard
}  // namespace OHOS
#endif // AUDIO_RINGERMODE_CALLBACK_NAPI_H_
