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
#include "audio_focus_parser.h"

namespace OHOS {
namespace AudioStandard {
AudioFocusParser::AudioFocusParser()
{
    MEDIA_INFO_LOG("AudioFocusParser ctor");

    // Initialize stream map with string vs AudioStreamType
    streamMap = {
        {"STREAM_RING", STREAM_RING},
        {"STREAM_MUSIC", STREAM_MUSIC},
        {"STREAM_VOICE_CALL", STREAM_VOICE_CALL},
    };

    // Initialize action map with string vs InterruptActionType
    actionMap = {
        {"DUCK", INTERRUPT_HINT_DUCK},
        {"PAUSE", INTERRUPT_HINT_PAUSE},
        {"REJECT", INTERRUPT_HINT_NONE},
        {"STOP", INTERRUPT_HINT_STOP}
    };

    // Initialize target map with string vs InterruptActionTarget
    targetMap = {
        {"new", INCOMING},
        {"existing", CURRENT},
        {"both", BOTH},
    };

    forceMap = {
        {"true", INTERRUPT_FORCE},
        {"false", INTERRUPT_SHARE},
    };
}

AudioFocusParser::~AudioFocusParser()
{
    streamMap.clear();
    actionMap.clear();
    targetMap.clear();
    forceMap.clear();
}

void AudioFocusParser::LoadDefaultConfig(AudioFocusEntry &focusTable)
{
}

int32_t AudioFocusParser::LoadConfig(AudioFocusEntry &focusTable)
{
    xmlDoc *doc = NULL;
    xmlNode *rootElement = NULL;

    pIntrAction = &focusTable;

    if ((doc = xmlReadFile(AUDIO_FOCUS_CONFIG_FILE, NULL, 0)) == NULL) {
        MEDIA_ERR_LOG("error: could not parse file %s", AUDIO_FOCUS_CONFIG_FILE);
        LoadDefaultConfig(focusTable);
        return ERROR;
    }

    rootElement = xmlDocGetRootElement(doc);
    xmlNode *currNode = rootElement;
    if (!xmlStrcmp(currNode->name, reinterpret_cast<const xmlChar*>("audio_focus_policy"))) {
        if ((currNode->children) && (currNode->children->next)) {
            currNode = currNode->children->next;
            ParseStreams(currNode);
        } else {
            MEDIA_ERR_LOG("empty focus policy in : %s", AUDIO_FOCUS_CONFIG_FILE);
            return SUCCESS;
        }
    } else {
        MEDIA_ERR_LOG("Missing tag - focus_policy in : %s", AUDIO_FOCUS_CONFIG_FILE);
        return ERROR;
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
    return SUCCESS;
}

void AudioFocusParser::ParseFocusTable(xmlNode *node, char *curStream)
{
    xmlNode *currNode = node;
    while (currNode) {
        if (currNode->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(currNode->name, reinterpret_cast<const xmlChar*>("focus_table"))) {
                MEDIA_INFO_LOG("node type: Element, name: %s", currNode->name);
                xmlNode *sNode = currNode->children;
                while (sNode) {
                    if (sNode->type == XML_ELEMENT_NODE) {
                        if (!xmlStrcmp(sNode->name, reinterpret_cast<const xmlChar*>("deny"))) {
                            ParseRejectedStreams(sNode->children, curStream);
                        } else {
                            ParseAllowedStreams(sNode->children, curStream);
                        }
                    }
                    sNode = sNode->next;
                }
            }
        }
        currNode = currNode->next;
    }
}

void AudioFocusParser::ParseStreams(xmlNode *node)
{
    xmlNode *currNode = node;
    while (currNode) {
        if (currNode->type == XML_ELEMENT_NODE) {
            char *sType = reinterpret_cast<char*>(xmlGetProp(currNode,
                reinterpret_cast<xmlChar*>(const_cast<char*>("value"))));
            std::map<std::string, AudioStreamType>::iterator it = streamMap.find(sType);
            if (it != streamMap.end()) {
                MEDIA_INFO_LOG("stream type: %{public}s",  sType);
                ParseFocusTable(currNode->children, sType);
            }
            xmlFree(sType);
        }
        currNode = currNode->next;
    }
}

void AudioFocusParser::ParseRejectedStreams(xmlNode *node, char *curStream)
{
    xmlNode *currNode = node;

    while (currNode) {
        if (currNode->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(currNode->name, reinterpret_cast<const xmlChar*>("stream_type"))) {
                char *newStream = reinterpret_cast<char*>(xmlGetProp(currNode,
                    reinterpret_cast<xmlChar*>(const_cast<char*>("value"))));

                std::map<std::string, AudioStreamType>::iterator it1 = streamMap.find(newStream);
                if (it1 != streamMap.end()) {
                    AudioFocusEntry *pAction = pIntrAction + (streamMap[curStream] * MAX_NUM_STREAMS) +
                        streamMap[newStream];
                    pAction->actionOn = INCOMING;
                    pAction->hintType = INTERRUPT_HINT_NONE;
                    pAction->forceType = INTERRUPT_FORCE;
                    pAction->isReject = true;

                    MEDIA_INFO_LOG("current stream: %s, incoming stream: %s", curStream, newStream);
                    MEDIA_INFO_LOG("actionOn: %d, hintType: %d, forceType: %d isReject: %d", pAction->actionOn,
                                   pAction->hintType, pAction->forceType, pAction->isReject);
                }
                xmlFree(newStream);
            }
        }
        currNode = currNode->next;
    }
}

void AudioFocusParser::ParseAllowedStreams(xmlNode *node, char *curStream)
{
    xmlNode *currNode = node;

    while (currNode) {
        if (currNode->type == XML_ELEMENT_NODE) {
            if (!xmlStrcmp(currNode->name, reinterpret_cast<const xmlChar*>("stream_type"))) {
                char *newStream = reinterpret_cast<char*>(xmlGetProp(currNode,
                    reinterpret_cast<xmlChar*>(const_cast<char*>("value"))));
                char *aType = reinterpret_cast<char*>(xmlGetProp(currNode,
                    reinterpret_cast<xmlChar*>(const_cast<char*>("action_type"))));
                char *aTarget = reinterpret_cast<char*>(xmlGetProp(currNode,
                    reinterpret_cast<xmlChar*>(const_cast<char*>("action_on"))));
                char *isForced = reinterpret_cast<char*>(xmlGetProp(currNode,
                    reinterpret_cast<xmlChar*>(const_cast<char*>("is_forced"))));

                std::map<std::string, AudioStreamType>::iterator it1 = streamMap.find(newStream);
                std::map<std::string, ActionTarget>::iterator it2 = targetMap.find(aTarget);
                std::map<std::string, InterruptHint>::iterator it3 = actionMap.find(aType);
                std::map<std::string, InterruptForceType>::iterator it4 = forceMap.find(isForced);
                if ((it1 != streamMap.end()) && (it2 != targetMap.end()) && (it3 != actionMap.end()) &&
                    (it4 != forceMap.end())) {
                    AudioFocusEntry *pAction = pIntrAction + (streamMap[curStream] * MAX_NUM_STREAMS)  +
                        streamMap[newStream];
                    pAction->actionOn = targetMap[aTarget];
                    pAction->hintType = actionMap[aType];
                    pAction->forceType = forceMap[isForced];
                    pAction->isReject = false;

                    MEDIA_INFO_LOG("current stream: %s, incoming stream: %s", curStream, newStream);
                    MEDIA_INFO_LOG("actionOn: %d, hintType: %d, forceType: %d isReject: %d", pAction->actionOn,
                                   pAction->hintType, pAction->forceType, pAction->isReject);
                }
                xmlFree(newStream);
                xmlFree(aType);
                xmlFree(aTarget);
                xmlFree(isForced);
            }
        }
        currNode = currNode->next;
    }
}
}
}
