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

#ifndef IRINGTONE_SOUND_MANAGER_H
#define IRINGTONE_SOUND_MANAGER_H

#include <cstdint>
#include <memory>
#include <string>

#include "iringtone_player.h"
#include "foundation/aafwk/standard/frameworks/kits/appkit/native/ability_runtime/context/context.h"

namespace OHOS {
namespace AudioStandard {
enum RingtoneType {
    RINGTONE_TYPE_DEFAULT = 0,
    RINGTONE_TYPE_MULTISIM
};

class IRingtoneSoundManager {
public:
    virtual ~IRingtoneSoundManager() = default;

    /**
     * @brief Returns the ringtone player instance
     *
     * @param context Indicates the Context object on OHOS.
     * @param type Indicates the type of tone for which player instance has to be returned
     * @return Returns IRingtonePlayer
     * @since 1.0
     * @version 1.0
     */
    virtual std::shared_ptr<IRingtonePlayer> GetRingtonePlayer(const std::shared_ptr<AbilityRuntime::Context> &context,
        RingtoneType type) = 0;

    /**
     * @brief API used for setting the system ringtone uri
     *
     * @param ctx Indicates the Context object on OHOS.
     * @param uri Indicates which uri to be set for the tone type
     * @return Returns IRingtonePlayer
     * @since 1.0
     * @version 1.0
     */
    virtual int32_t SetSystemRingtoneUri(const std::shared_ptr<AbilityRuntime::Context> &ctx, const std::string &uri,
        RingtoneType type) = 0;

    /**
     * @brief Returns the current ringtone uri
     *
     * @param context Indicates the Context object on OHOS.
     * @return Returns the system ringtone uri
     * @since 1.0
     * @version 1.0
     */
    virtual std::string GetSystemRingtoneUri(const std::shared_ptr<AbilityRuntime::Context> &context,
        RingtoneType type) = 0;

    /**
     * @brief Returns the current notification uri
     *
     * @param context Indicates the Context object on OHOS.
     * @return Returns the system notification uri
     * @since 1.0
     * @version 1.0
     */
    virtual std::string GetSystemNotificationUri(const std::shared_ptr<AbilityRuntime::Context> &context)= 0;

    /**
     * @brief Returns the current system uri
     *
     * @param context Indicates the Context object on OHOS.
     * @return Returns system alarm uri
     * @since 1.0
     * @version 1.0
     */
    virtual std::string GetSystemAlarmUri(const std::shared_ptr<AbilityRuntime::Context> &context) = 0;

    /**
     * @brief API used for setting the notification uri
     *
     * @param context Indicates the Context object on OHOS.
     * @param uri indicates which uri to be set for notification
     * @since 1.0
     * @version 1.0
     */
    virtual int32_t SetSystemNotificationUri(const std::shared_ptr<AbilityRuntime::Context> &context,
        const std::string &uri) = 0;

    /**
     * @brief API used for setting the Alarm uri
     *
     * @param ctx Indicates the Context object on OHOS.
     * @param uri indicates which uri to be set for alarm
     * @return Returns IRingtonePlayer
     * @since 1.0
     * @version 1.0
     */
    virtual int32_t SetSystemAlarmUri(const std::shared_ptr<AbilityRuntime::Context> &ctx, const std::string &uri) = 0;
};

class __attribute__((visibility("default"))) RingtoneFactory {
public:
    static std::unique_ptr<IRingtoneSoundManager> CreateRingtoneManager();

private:
    RingtoneFactory() = default;
    ~RingtoneFactory() = default;
};
} // AudioStandard
} // OHOS
#endif // IRINGTONE_SOUND_MANAGER_H
