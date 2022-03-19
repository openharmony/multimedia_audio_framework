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

#include <cstdlib>
#include "media_log.h"

#include "xml_parser.h"

namespace OHOS {
namespace AudioStandard {
bool XMLParser::LoadConfiguration()
{
    mDoc = xmlReadFile(CONFIG_FILE, NULL, 0);
    if (mDoc == NULL) {
        MEDIA_ERR_LOG("xmlReadFile Failed");
        return false;
    }

    return true;
}

bool XMLParser::Parse()
{
    xmlNode *root = xmlDocGetRootElement(mDoc);
    if (root == NULL) {
        MEDIA_ERR_LOG("xmlDocGetRootElement Failed");
        return false;
    }

    if (!ParseInternal(*root)) {
        return false;
    }

    return true;
}

void XMLParser::Destroy()
{
    if (mDoc != NULL) {
        xmlFreeDoc(mDoc);
    }

    return;
}

bool XMLParser::ParseInternal(xmlNode &node)
{
    xmlNode *currNode = &node;
    for (; currNode; currNode = currNode->next) {
        if (XML_ELEMENT_NODE == currNode->type) {
            switch (GetNodeNameAsInt(*currNode)) {
                case DEVICE_CLASS:
                    ParseDeviceClass(*currNode);
                    break;
                case AUDIO_INTERRUPT_ENABLE:
                    ParseAudioInterrupt(*currNode);
                    break;
                default:
                    ParseInternal(*(currNode->children));
                    break;
            }
        }
    }

    mPortObserver.OnXmlParsingCompleted(xmlParsedDataMap_);
    return true;
}

void XMLParser::ParseDeviceClass(xmlNode &node)
{
    xmlNode *modulesNode = NULL;
    modulesNode = node.xmlChildrenNode;

    std::string className = ExtractPropertyValue("name", node);
    if (className.empty()) {
        MEDIA_ERR_LOG("No name provided for the device class %{public}s", node.name);
        return;
    }

    deviceClassType_ = GetDeviceClassType(className);
    xmlParsedDataMap_[deviceClassType_] = {};

    while (modulesNode != NULL) {
        if (modulesNode->type == XML_ELEMENT_NODE) {
            ParseModules(*modulesNode, className);
        }
        modulesNode = modulesNode->next;
    }
}

void XMLParser::ParseModules(xmlNode &node, std::string &className)
{
    xmlNode *moduleNode = NULL;
    std::list<AudioModuleInfo> moduleList = {};
    moduleNode = node.xmlChildrenNode;

    while (moduleNode != NULL) {
        if (moduleNode->type == XML_ELEMENT_NODE) {
            AudioModuleInfo moduleInfo = {};
            moduleInfo.className = className;
            moduleInfo.name = ExtractPropertyValue("name", *moduleNode);
            moduleInfo.lib = ExtractPropertyValue("lib", *moduleNode);
            moduleInfo.role = ExtractPropertyValue("role", *moduleNode);
            moduleInfo.rate = ExtractPropertyValue("rate", *moduleNode);
            moduleInfo.format = ExtractPropertyValue("format", *moduleNode);
            moduleInfo.channels = ExtractPropertyValue("channels", *moduleNode);
            moduleInfo.bufferSize = ExtractPropertyValue("buffer_size", *moduleNode);
            moduleInfo.fileName = ExtractPropertyValue("file", *moduleNode);
            moduleInfo.ports = {};

            ParsePorts(*moduleNode, moduleInfo);
            moduleList.push_back(moduleInfo);
        }
        moduleNode = moduleNode->next;
    }

    xmlParsedDataMap_[deviceClassType_] = moduleList;
}

void XMLParser::ParsePorts(xmlNode &node, AudioModuleInfo &moduleInfo)
{
    xmlNode *portsNode = NULL;
    portsNode = node.xmlChildrenNode;

    while (portsNode != NULL) {
        if (portsNode->type == XML_ELEMENT_NODE) {
            ParsePort(*portsNode, moduleInfo);
        }
        portsNode = portsNode->next;
    }
}

void XMLParser::ParsePort(xmlNode &node, AudioModuleInfo &moduleInfo)
{
    xmlNode *portNode = NULL;
    std::list<AudioModuleInfo> portInfoList = {};
    portNode = node.xmlChildrenNode;

    while (portNode != NULL) {
        if (portNode->type == XML_ELEMENT_NODE) {
            moduleInfo.adapterName = ExtractPropertyValue("adapter_name", *portNode);
            moduleInfo.id = ExtractPropertyValue("id", *portNode);

            // if some parameter is not configured inside <Port>, take data from moduleinfo
            std::string value = ExtractPropertyValue("rate", *portNode);
            if (!value.empty()) {
                moduleInfo.rate = value;
            }

            value = ExtractPropertyValue("format", *portNode);
            if (!value.empty()) {
                moduleInfo.format = value;
            }

            value = ExtractPropertyValue("channels", *portNode);
            if (!value.empty()) {
                moduleInfo.channels = value;
            }

            value = ExtractPropertyValue("buffer_size", *portNode);
            if (!value.empty()) {
                moduleInfo.bufferSize = value;
            }

            value = ExtractPropertyValue("file", *portNode);
            if (!value.empty()) {
                moduleInfo.fileName = value;
            }

            portInfoList.push_back(moduleInfo);
        }
        portNode = portNode->next;
    }

    moduleInfo.ports.assign(portInfoList.begin(), portInfoList.end());
}

NodeName XMLParser::GetNodeNameAsInt(xmlNode &node)
{
    if (!xmlStrcmp(node.name, reinterpret_cast<const xmlChar*>("deviceclass"))) {
        return DEVICE_CLASS;
    } else  if (!xmlStrcmp(node.name, reinterpret_cast<const xmlChar*>("AudioInterruptEnable"))) {
        return AUDIO_INTERRUPT_ENABLE;
    } else {
        return UNKNOWN;
    }
}

void XMLParser::ParseAudioInterrupt(xmlNode &node)
{
    xmlNode *child = node.children;
    xmlChar *enableFlag = xmlNodeGetContent(child);

    if (!xmlStrcmp(enableFlag, reinterpret_cast<const xmlChar*>("true"))) {
        mPortObserver.OnAudioInterruptEnable(true);
    } else {
        mPortObserver.OnAudioInterruptEnable(false);
    }

    xmlFree(enableFlag);
}

ClassType XMLParser::GetDeviceClassType(const std::string &deviceClass)
{
    if (deviceClass == PRIMARY_CLASS)
        return ClassType::TYPE_PRIMARY;
    else if (deviceClass == A2DP_CLASS)
        return ClassType::TYPE_A2DP;
    else if (deviceClass == USB_CLASS)
        return ClassType::TYPE_USB;
    else
        return ClassType::TYPE_INVALID;
}

std::string XMLParser::ExtractPropertyValue(const std::string &propName, xmlNode &node)
{
    std::string propValue = "";
    xmlChar *tempValue = nullptr;

    if (xmlHasProp(&node, reinterpret_cast<const xmlChar*>(propName.c_str()))) {
        tempValue = xmlGetProp(&node, reinterpret_cast<const xmlChar*>(propName.c_str()));
    }

    if (tempValue != nullptr) {
        propValue = reinterpret_cast<const char*>(tempValue);
        xmlFree(tempValue);
    }

    return propValue;
}
} // namespace AudioStandard
} // namespace OHOS
