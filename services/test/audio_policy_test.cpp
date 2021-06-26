/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unistd.h>

#include "audio_errors.h"
#include "audio_system_manager.h"
#include "media_log.h"

using namespace std;
using namespace OHOS;
using namespace OHOS::AudioStandard;

namespace AudioPolicyTest {
    const int FIRST_ARG = 1;
    const int SECOND_ARG = 2;
    const int OPT_ARG_BASE = 10;
    const int OPT_SHORT_LEN = 3;
    const float VOLUME = 0.5f;
}

static void PrintUsage(void)
{
    cout << "NAME" << endl << endl;
    cout << "\taudio_policy_test - Audio Policy Test " << endl << endl;
    cout << "SYNOPSIS" << endl << endl;
    cout << "\t#include <audio_system_manager.h>" << endl << endl;
    cout << "\t./audio_playback_test [OPTIONS]..." << endl << endl;
    cout << "DESCRIPTION" << endl << endl;
    cout << "\tControls audio volume, audio routing, audio mute" << endl << endl;
    cout << "-V\n\tSets Volume for streams, -S to setStream" << endl << endl;
    cout << "-v\n\tGets Volume for streams, -S to setStream" << endl << endl;
    cout << "-S\n\tSet stream type" << endl << endl;
    cout << "\tSupported Streams are" << endl << endl;
    cout << "\t4\tMUSIC" << endl << endl;
    cout << "\t3\tRING" << endl << endl;
    cout << "-D\n\tSets Device Active" << endl << endl;
    cout << "\tSupported Devices are" << endl << endl;
    cout << "\t0\tSPEAKER" << endl << endl;
    cout << "\t3\tBLUETOOTH_A2DP" << endl << endl;
    cout << "\t4\tMIC" << endl << endl;
    cout << "-d\n\tGets Device Active" << endl << endl;
    cout << "-M\n\tSets Mute for streams, -S to setStream" << endl << endl;
    cout << "-m\n\tGets Mute for streams, -S to setStream" << endl << endl;
    cout << "-R\n\tSets RingerMode" << endl << endl;
    cout << "-r\n\tGets RingerMode status" << endl << endl;
    cout << "-s\n\tGet Stream Status" << endl << endl;
    cout << "AUTHOR" << endl << endl;
    cout << "\tWritten by Sajeesh Sidharthan and Anurup M" << endl << endl;
}

int main(int argc, char* argv[])
{
    int32_t result;
    float volume = AudioPolicyTest::VOLUME;
    int device = -1;
    int active = -1;
    int mute = -1;
    bool muteStatus = false;
    bool devActiveStatus = false;
    int opt = 0;
    int ringMode = 0;
    int streamType = static_cast<int32_t>(AudioSystemManager::AudioVolumeType::STREAM_MUSIC);

    if (((argc >= AudioPolicyTest::SECOND_ARG) && !strcmp(argv[AudioPolicyTest::FIRST_ARG], "--help")) ||
        (argc == AudioPolicyTest::FIRST_ARG)) {
        PrintUsage();
        return ERR_INVALID_PARAM;
    }

    AudioSystemManager *audioSystemMgr = AudioSystemManager::GetInstance();

    while ((opt = getopt(argc, argv, ":V:S:D:M:R:d:s:vmr")) != -1) {
        switch (opt) {
            case 'V':
                volume = strtof(optarg, nullptr);
                cout << "Set Volume : " << volume << endl;
                result = audioSystemMgr->SetVolume(static_cast<AudioSystemManager::AudioVolumeType>(streamType),
                    volume);
                cout << "Set Volume Result: " << result << endl;
                break;
            case 'v':
                volume = audioSystemMgr->GetVolume(static_cast<AudioSystemManager::AudioVolumeType>(streamType));
                cout << "Get Volume : " << volume << endl;
                break;
            case 'M':
                mute = strtol(optarg, nullptr, AudioPolicyTest::OPT_ARG_BASE);
                cout << "Set Mute : " << mute << endl;
                result = audioSystemMgr->SetMute(static_cast<AudioSystemManager::AudioVolumeType>(streamType),
                    (mute) ? true : false);
                cout << "Set Mute Result: " << result << endl;
                break;
            case 'm':
                muteStatus = audioSystemMgr->IsStreamMute(
                    static_cast<AudioSystemManager::AudioVolumeType>(streamType));
                cout << "Get Mute : " << muteStatus << endl;
                break;
            case 'S':
                streamType = strtol(optarg, nullptr, AudioPolicyTest::OPT_ARG_BASE);
                cout << "Set Stream : " << streamType << endl;
                break;
            case 's':
                streamType = strtol(optarg, nullptr, AudioPolicyTest::OPT_ARG_BASE);
                cout << "Stream Active: " << audioSystemMgr->IsStreamActive(
                    static_cast<AudioSystemManager::AudioVolumeType>(streamType)) << endl;
                break;
            case 'D':
                device = strtol(optarg, nullptr, AudioPolicyTest::OPT_ARG_BASE);
                cout << "Set Device : " << device << endl;

                if (optind < argc && *argv[optind] != '-') {
                    active = strtol(argv[optind], nullptr, AudioPolicyTest::OPT_ARG_BASE);
                    optind++;
                }
                cout << "Active : " << active << endl << endl;

                result = audioSystemMgr->SetDeviceActive(AudioDeviceDescriptor::DeviceType(device),
                    (active) ? true : false);
                cout << "Set DeviceActive Result: " << result << endl;
                break;
            case 'd':
                device = strtol(optarg, nullptr, AudioPolicyTest::OPT_ARG_BASE);
                devActiveStatus = audioSystemMgr->IsDeviceActive(AudioDeviceDescriptor::DeviceType(device));
                cout << "GetDevice Active : " << devActiveStatus << endl;
                break;
            case 'R':
                ringMode = strtol(optarg, nullptr, AudioPolicyTest::OPT_ARG_BASE);
                cout << "Set Ringer Mode : " << ringMode << endl;
                audioSystemMgr->SetRingerMode(static_cast<AudioRingerMode>(ringMode));
                break;
            case 'r':
                ringMode = static_cast<int32_t>(audioSystemMgr->GetRingerMode());
                cout << "Get Ringer Mode : " << ringMode << endl;
                break;
            case ':':
                char option[AudioPolicyTest::OPT_SHORT_LEN];
                cout << "option ";
                snprintf(option, AudioPolicyTest::OPT_SHORT_LEN, "-%c", optopt);
                cout << option << " needs a value" << endl << endl;
                PrintUsage();
                break;
            case '?': {
                char option[AudioPolicyTest::OPT_SHORT_LEN];
                snprintf(option, AudioPolicyTest::OPT_SHORT_LEN, "-%c", optopt);
                cout << "unknown option: " << option << endl << endl;
                PrintUsage();
                break;
            }
            default:
                break;
        }
    }

    cout<<"Exit from test app"<< endl;
    return 0;
}
