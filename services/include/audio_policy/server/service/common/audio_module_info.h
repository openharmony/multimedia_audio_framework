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

#ifndef ST_AUDIO_CONFIG_H
#define ST_AUDIO_CONFIG_H

#include <list>
#include <string>

#include "audio_info.h"
#include "media_log.h"

#include <libxml/tree.h>
#include <libxml/parser.h>

namespace OHOS {
namespace AudioStandard {
static const std::string PRIMARY_CLASS = "primary";
static const std::string A2DP_CLASS = "a2dp";
static const std::string USB_CLASS = "usb";
static const std::string BLUETOOTH_SPEAKER = "Bt_Speaker";
static const std::string PRIMARY_SPEAKER = "Speaker";
static const std::string PRIMARY_MIC = "Built_in_mic";
static const std::string PIPE_SINK = "fifo_output";
static const std::string PIPE_SOURCE = "fifo_input";
static const std::string INTERNAL_PORT = "internal";
static const std::string ROLE_SOURCE = "source";
static const std::string ROLE_SINK = "sink";
static const std::string PORT_NONE = "none";

enum NodeName {
    DEVICE_CLASS,
    MODULES,
    MODULE,
    PORTS,
    PORT,
    AUDIO_INTERRUPT_ENABLE,
    UNKNOWN
};

enum ClassType {
    TYPE_PRIMARY,
    TYPE_A2DP,
    TYPE_USB,
    TYPE_INVALID
};

class AudioModuleInfo {
public:
    AudioModuleInfo() = default;
    virtual ~AudioModuleInfo() = default;

    std::string className;
    std::string name;
    std::string adapterName;
    std::string id;
    std::string lib;
    std::string role;
    std::string rate;
    std::string format;
    std::string channels;
    std::string bufferSize;
    std::string fileName;
    std::list<AudioModuleInfo> ports;
};
} // namespace AudioStandard
} // namespace OHOS

#endif // ST_AUDIO_CONFIG_H
