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

#ifndef AUDIO_VOLUME_KEY_EVENT_NAPI_CALLBACK_H_
#define AUDIO_VOLUME_KEY_EVENT_NAPI_CALLBACK_H_

#include "audio_manager_napi.h"
#include "audio_system_manager.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"

namespace OHOS {
namespace AudioStandard {
struct VolumeKeyEventAutoRef {
    VolumeKeyEventAutoRef(napi_env env, napi_ref cb)
        : env_(env), cb_(cb)
    {
    }
    ~VolumeKeyEventAutoRef()
    {
        if (env_ != nullptr && cb_ != nullptr) {
            (void)napi_delete_reference(env_, cb_);
        }
    }
    napi_env env_;
    napi_ref cb_;
};

class AudioVolumeKeyEventNapi : public VolumeKeyEventCallback {
public:
    explicit AudioVolumeKeyEventNapi(napi_env env);
    virtual ~AudioVolumeKeyEventNapi();

    /**
     * @brief OnAudioVolumeKeyEvent will be executed when hard volume key is pressed up/down
     *
     * @param streamType the stream type for which volume must be updated.
     * @param volumeLevel the volume level to be updated.
     * @param isUpdateUi whether to update volume level in UI.
    **/
    void OnVolumeKeyEvent(AudioStreamType streamType, int volumeLevel, bool isUpdateUi) override;
    void SaveCallbackReference(const std::string &callbackName, napi_value callback);

private:
    struct AudioVolumeKeyEventJsCallback {
        std::shared_ptr<VolumeKeyEventAutoRef> callback = nullptr;
        std::string callbackName = "unknown";
        VolumeEvent volumeEvent;
    };

    void OnJsCallbackVolumeEvent(std::unique_ptr<AudioVolumeKeyEventJsCallback> &jsCb);

    std::shared_ptr<VolumeKeyEventAutoRef> audioVolumeKeyEventJsCallback_ = nullptr;
    std::mutex mutex_;
    napi_env env_;

    static napi_ref sConstructor_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_VOLUME_KEY_EVENT_NAPI_CALLBACK_H_
