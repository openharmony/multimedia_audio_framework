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
#ifndef AUDIO_FOCUS_PARSER_H
#define AUDIO_FOCUS_PARSER_H

#include<map>
#include<string>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "audio_errors.h"
#include "audio_info.h"
#include "media_log.h"

namespace OHOS {
namespace AudioStandard {
static constexpr char AUDIO_FOCUS_CONFIG_FILE[] = "/etc/audio/audio_interrupt_policy_config.xml";

class AudioFocusParser {
public:
    AudioFocusParser();
    virtual ~AudioFocusParser();
    int32_t LoadConfig(AudioFocusEntry &focusTable);

private:
    std::map<std::string, InterruptHint> actionMap;
    std::map<std::string, AudioStreamType> streamMap;
    std::map<std::string, ActionTarget> targetMap;
    std::map<std::string, InterruptForceType> forceMap;

    AudioFocusEntry* pIntrAction;

    void LoadDefaultConfig(AudioFocusEntry &focusTable);
    void ParseFocusTable(xmlNode *node, char *curStream);
    void ParseStreams(xmlNode *node);
    void ParseAllowedStreams(xmlNode *node, char *curStream);
    void ParseRejectedStreams(xmlNode *node, char *curStream);
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_FOCUS_PARSER_H
