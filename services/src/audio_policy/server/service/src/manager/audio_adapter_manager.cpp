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

#include <memory>
#include <unistd.h>

#include "audio_errors.h"
#include "media_log.h"

#include "audio_adapter_manager.h"

namespace OHOS {
namespace AudioStandard {
bool AudioAdapterManager::Init()
{
    return true;
}

bool AudioAdapterManager::ConnectServiceAdapter()
{
    std::unique_ptr<AudioAdapterManager> audioAdapterManager(this);
    std::unique_ptr<PolicyCallbackImpl> policyCallbackImpl = std::make_unique<PolicyCallbackImpl>(audioAdapterManager);
    mAudioServiceAdapter = AudioServiceAdapter::CreateAudioAdapter(std::move(policyCallbackImpl));
    bool result = mAudioServiceAdapter->Connect();
    if (!result) {
        MEDIA_ERR_LOG("[AudioAdapterManager] Error in connecting audio adapter");
        return false;
    }

    return true;
}


void AudioAdapterManager::InitKVStore()
{
    bool isFirstBoot = false;
    InitAudioPolicyKvStore(isFirstBoot);
    InitVolumeMap(isFirstBoot);
    InitRingerMode(isFirstBoot);
}

void AudioAdapterManager::Deinit(void)
{
    return  mAudioServiceAdapter->Disconnect();
}

int32_t AudioAdapterManager::SetAudioSessionCallback(AudioSessionCallback *callback)
{
    if (callback == nullptr) {
        MEDIA_ERR_LOG("[AudioAdapterManager] SetAudioSessionCallback callback == nullptr");
        return ERR_INVALID_PARAM;
    }

    sessionCallback_ = callback;
    return SUCCESS;
}

int32_t AudioAdapterManager::SetStreamVolume(AudioStreamType streamType, float volume)
{
    // Incase if KvStore didnot connect during  bootup
    if (mAudioPolicyKvStore == nullptr) {
        bool isFirstBoot = false;
        InitAudioPolicyKvStore(isFirstBoot);
    }

    AudioStreamType streamForVolumeMap = GetStreamForVolumeMap(streamType);
    mVolumeMap[streamForVolumeMap] = volume;
    WriteVolumeToKvStore(streamType, volume);

    return mAudioServiceAdapter->SetVolume(streamType, volume);
}

float AudioAdapterManager::GetStreamVolume(AudioStreamType streamType)
{
    AudioStreamType streamForVolumeMap = GetStreamForVolumeMap(streamType);

    return mVolumeMap[streamForVolumeMap];
}

int32_t AudioAdapterManager::SetStreamMute(AudioStreamType streamType, bool mute)
{
    bool result =  mAudioServiceAdapter->SetMute(streamType, mute);
    return result;
}

bool AudioAdapterManager::GetStreamMute(AudioStreamType streamType)
{
    bool result =  mAudioServiceAdapter->IsMute(streamType);
    return result;
}

bool AudioAdapterManager::IsStreamActive(AudioStreamType streamType)
{
    bool result = mAudioServiceAdapter->IsStreamActive(streamType);
    return result;
}

int32_t AudioAdapterManager::SuspendAudioDevice(std::string &portName, bool isSuspend)
{
    return mAudioServiceAdapter->SuspendAudioDevice(portName, isSuspend);
}

int32_t AudioAdapterManager::SetDeviceActive(AudioIOHandle ioHandle, InternalDeviceType deviceType,
    std::string name, bool active)
{
    switch (deviceType) {
        case InternalDeviceType::DEVICE_TYPE_SPEAKER:
        case InternalDeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_USB_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_SCO: {
            MEDIA_INFO_LOG("SetDefaultSink %{public}d", deviceType);
            return mAudioServiceAdapter->SetDefaultSink(name);
        }
        case InternalDeviceType::DEVICE_TYPE_MIC: {
            MEDIA_INFO_LOG("SetDefaultSource %{public}d", deviceType);
            return mAudioServiceAdapter->SetDefaultSource(name);
        }
        default:
            break;
    }
    return SUCCESS;
}

int32_t AudioAdapterManager::SetRingerMode(AudioRingerMode ringerMode)
{
    mRingerMode = ringerMode;

    // Incase if KvStore didnot connect during  bootup
    if (mAudioPolicyKvStore == nullptr) {
        bool isFirstBoot = false;
        InitAudioPolicyKvStore(isFirstBoot);
    }

    WriteRingerModeToKvStore(ringerMode);
    return SUCCESS;
}

AudioRingerMode AudioAdapterManager::GetRingerMode()
{
    return mRingerMode;
}

AudioIOHandle AudioAdapterManager::OpenAudioPort(const AudioModuleInfo &audioModuleInfo)
{
    std::string moduleArgs = GetModuleArgs(audioModuleInfo);
    MEDIA_INFO_LOG("[Adapter load-module] %{public}s %{public}s", audioModuleInfo.lib.c_str(), moduleArgs.c_str());
    if (audioModuleInfo.lib == PIPE_SOURCE || audioModuleInfo.lib == PIPE_SINK) {
        if (!audioModuleInfo.fileName.empty() && access(audioModuleInfo.fileName.c_str(), F_OK) == 0) {
            int32_t ret = std::remove(audioModuleInfo.fileName.c_str());
            if (ret) {
                MEDIA_ERR_LOG("[AudioAdapterManager] Error Removing file: %{public}s Failed! ret val: %{public}d",
                    audioModuleInfo.fileName.c_str(), ret);
                return ERR_OPERATION_FAILED;
            }
        } else {
            MEDIA_ERR_LOG("[AudioAdapterManager] Error audioModuleInfo.fileName is null! or file not exists");
            return ERR_OPERATION_FAILED;
        }
    }

    CHECK_AND_RETURN_RET_LOG(mAudioServiceAdapter != nullptr, ERR_OPERATION_FAILED, "ServiceAdapter is null");
    return mAudioServiceAdapter->OpenAudioPort(audioModuleInfo.lib, moduleArgs.c_str());
}

int32_t AudioAdapterManager::CloseAudioPort(AudioIOHandle ioHandle)
{
    CHECK_AND_RETURN_RET_LOG(mAudioServiceAdapter != nullptr, ERR_OPERATION_FAILED, "ServiceAdapter is null");
    return mAudioServiceAdapter->CloseAudioPort(ioHandle);
}

void UpdateCommonArgs(const AudioModuleInfo &audioModuleInfo, std::string &args)
{
    if (!audioModuleInfo.rate.empty()) {
        args = "rate=";
        args.append(audioModuleInfo.rate);
    }

    if (!audioModuleInfo.channels.empty()) {
        args.append(" channels=");
        args.append(audioModuleInfo.channels);
    }

    if (!audioModuleInfo.bufferSize.empty()) {
        args.append(" buffer_size=");
        args.append(audioModuleInfo.bufferSize);
    }

    if (!audioModuleInfo.format.empty()) {
        args.append(" format=");
        args.append(audioModuleInfo.format);
        MEDIA_INFO_LOG("[PolicyManager] format: %{public}s", args.c_str());
    }
}

// Private Members
std::string AudioAdapterManager::GetModuleArgs(const AudioModuleInfo &audioModuleInfo)
{
    std::string args;

    if (audioModuleInfo.lib == HDI_SINK) {
        UpdateCommonArgs(audioModuleInfo, args);
        if (!audioModuleInfo.adapterName.empty()) {
            args.append(" sink_name=");
            args.append(audioModuleInfo.name);
        }

        if (!audioModuleInfo.className.empty()) {
            args.append(" device_class=");
            args.append(audioModuleInfo.className);
        }
    } else if (audioModuleInfo.lib == HDI_SOURCE) {
        UpdateCommonArgs(audioModuleInfo, args);
        if (!audioModuleInfo.adapterName.empty()) {
            args.append(" source_name=");
            args.append(audioModuleInfo.name);
        }
    } else if (audioModuleInfo.lib == PIPE_SINK) {
        if (!audioModuleInfo.fileName.empty()) {
            args = "file=";
            args.append(audioModuleInfo.fileName);
        }
    } else if (audioModuleInfo.lib == PIPE_SOURCE) {
        if (!audioModuleInfo.fileName.empty()) {
            args = "file=";
            args.append(audioModuleInfo.fileName);
        }
    }

    return args;
}

std::string AudioAdapterManager::GetStreamNameByStreamType(AudioStreamType streamType)
{
    switch (streamType) {
        case STREAM_MUSIC:
            return "music";
        case STREAM_RING:
            return "ring";
        case STREAM_SYSTEM:
            return "system";
        case STREAM_NOTIFICATION:
            return "notification";
        case STREAM_ALARM:
            return "alarm";
        case STREAM_DTMF:
            return "dtmf";
        case STREAM_VOICE_CALL:
            return "voice_call";
        case STREAM_VOICE_ASSISTANT:
            return "voice_assistant";
        default:
            return "";
    }
}

AudioStreamType AudioAdapterManager::GetStreamIDByType(std::string streamType)
{
    AudioStreamType stream = STREAM_MUSIC;

    if (!streamType.compare(std::string("music")))
        stream = STREAM_MUSIC;
    else if (!streamType.compare(std::string("ring")))
        stream = STREAM_RING;
    else if (!streamType.compare(std::string("voice_call")))
        stream = STREAM_VOICE_CALL;
    else if (!streamType.compare(std::string("system")))
        stream = STREAM_SYSTEM;
    else if (!streamType.compare(std::string("notification")))
        stream = STREAM_NOTIFICATION;
    else if (!streamType.compare(std::string("alarm")))
        stream = STREAM_ALARM;
    else if (!streamType.compare(std::string("voice_assistant")))
        stream = STREAM_VOICE_ASSISTANT;

    return stream;
}

AudioStreamType AudioAdapterManager::GetStreamForVolumeMap(AudioStreamType streamType)
{
    switch (streamType) {
        case STREAM_MUSIC:
            return STREAM_MUSIC;
        case STREAM_NOTIFICATION:
        case STREAM_DTMF:
        case STREAM_SYSTEM:
        case STREAM_RING:
            return STREAM_RING;
        case STREAM_ALARM:
            return STREAM_ALARM;
        case STREAM_VOICE_CALL:
            return STREAM_VOICE_CALL;
        case STREAM_VOICE_ASSISTANT:
            return STREAM_VOICE_ASSISTANT;
        default:
            return STREAM_MUSIC;
    }
}

bool AudioAdapterManager::InitAudioPolicyKvStore(bool& isFirstBoot)
{
    DistributedKvDataManager manager;
    Options options;

    options.createIfMissing = false;
    options.encrypt = false;
    options.autoSync = true;
    options.kvStoreType = KvStoreType::SINGLE_VERSION;

    AppId appId;
    appId.appId = "policymanager";
    StoreId storeId;
    storeId.storeId = "audiopolicy";
    Status status = Status::SUCCESS;

    // open and initialize kvstore instance.
    if (mAudioPolicyKvStore == nullptr) {
        uint32_t retries = 0;

        do {
            status = manager.GetSingleKvStore(options, appId, storeId, mAudioPolicyKvStore);
            if (status == Status::STORE_NOT_FOUND) {
                MEDIA_ERR_LOG("[AudioAdapterManager] InitAudioPolicyKvStore: STORE_NOT_FOUND!");
            }

            if ((status == Status::SUCCESS) || (status == Status::STORE_NOT_FOUND)) {
                break;
            } else {
                MEDIA_ERR_LOG("[AudioAdapterManager] InitAudioPolicyKvStore: Kvstore Connect failed! Retrying.");
                retries++;
                usleep(KVSTORE_CONNECT_RETRY_DELAY_TIME);
            }
        } while (retries <= KVSTORE_CONNECT_RETRY_COUNT);
    }

    if (mAudioPolicyKvStore == nullptr) {
        if (status == Status::STORE_NOT_FOUND) {
            MEDIA_INFO_LOG("[AudioAdapterManager] First Boot: Create AudioPolicyKvStore");
            options.createIfMissing = true;
            // [create and] open and initialize kvstore instance.
            status = manager.GetSingleKvStore(options, appId, storeId, mAudioPolicyKvStore);
            if (status == Status::SUCCESS) {
                isFirstBoot = true;
            } else {
                MEDIA_ERR_LOG("[AudioAdapterManager] Create AudioPolicyKvStore Failed!");
            }
        }
    }

    if (mAudioPolicyKvStore == nullptr) {
        MEDIA_ERR_LOG("[AudioAdapterManager] InitAudioPolicyKvStore: Failed!");
        return false;
    }

    return true;
}

void AudioAdapterManager::InitVolumeMap(bool isFirstBoot)
{
    if (isFirstBoot == true) {
        WriteVolumeToKvStore(STREAM_MUSIC, MAX_VOLUME);
        WriteVolumeToKvStore(STREAM_RING, MAX_VOLUME);
        WriteVolumeToKvStore(STREAM_VOICE_CALL, MAX_VOLUME);
        WriteVolumeToKvStore(STREAM_VOICE_ASSISTANT, MAX_VOLUME);
        MEDIA_INFO_LOG("[AudioAdapterManager] Wrote default stream volumes to KvStore");
    } else {
        LoadVolumeMap();
    }
    return;
}

void AudioAdapterManager::InitRingerMode(bool isFirstBoot)
{
    if (mAudioPolicyKvStore == nullptr) {
        MEDIA_ERR_LOG("[AudioAdapterManager] mAudioPolicyKvStore is null!");
        return;
    }

    if (isFirstBoot == true) {
        mRingerMode = RINGER_MODE_NORMAL;
        WriteRingerModeToKvStore(RINGER_MODE_NORMAL);
        MEDIA_INFO_LOG("[AudioAdapterManager] Wrote default ringer mode to KvStore");
    } else {
        LoadRingerMode();
    }
}

bool AudioAdapterManager::LoadVolumeFromKvStore(AudioStreamType streamType)
{
    Key key;
    Value value;

    switch (streamType) {
        case STREAM_MUSIC:
            key = "music";
            break;
        case STREAM_RING:
            key = "ring";
            break;
        case STREAM_VOICE_CALL:
            key = "voice_call";
            break;
        case STREAM_VOICE_ASSISTANT:
            key = "voice_assistant";
            break;
        default:
            return false;
    }

    Status status = mAudioPolicyKvStore->Get(key, value);
    if (status == Status::SUCCESS) {
        float volume = TransferByteArrayToType<float>(value.Data());
        mVolumeMap[streamType] = volume;
        MEDIA_DEBUG_LOG("[AudioAdapterManager] volume from kvStore %{public}f for streamType:%{public}d",
                        volume, streamType);
        return true;
    }

    return false;
}

bool AudioAdapterManager::LoadVolumeMap(void)
{
    if (mAudioPolicyKvStore == nullptr) {
        MEDIA_ERR_LOG("[AudioAdapterManager] LoadVolumeMap: mAudioPolicyKvStore is null!");
        return false;
    }

    if (!LoadVolumeFromKvStore(STREAM_MUSIC))
        MEDIA_ERR_LOG("[AudioAdapterManager] LoadVolumeMap: Couldnot load volume for Music from kvStore!");

    if (!LoadVolumeFromKvStore(STREAM_RING))
        MEDIA_ERR_LOG("[AudioAdapterManager] LoadVolumeMap: Couldnot load volume for Ring from kvStore!");

    if (!LoadVolumeFromKvStore(STREAM_VOICE_CALL))
        MEDIA_ERR_LOG("[AudioAdapterManager] LoadVolumeMap: Couldnot load volume for voice_call from kvStore!");

    if (!LoadVolumeFromKvStore(STREAM_VOICE_ASSISTANT))
        MEDIA_ERR_LOG("[AudioAdapterManager] LoadVolumeMap: Couldnot load volume for voice_assistant from kvStore!");

    return true;
}

bool AudioAdapterManager::LoadRingerMode(void)
{
    if (mAudioPolicyKvStore == nullptr) {
        MEDIA_ERR_LOG("[AudioAdapterManager] LoadRingerMap: mAudioPolicyKvStore is null!");
        return false;
    }

    // get ringer mode value from kvstore.
    Key key = "ringermode";
    Value value;
    Status status = mAudioPolicyKvStore->Get(key, value);
    if (status == Status::SUCCESS) {
        mRingerMode = static_cast<AudioRingerMode>(TransferByteArrayToType<int>(value.Data()));
        MEDIA_DEBUG_LOG("[AudioAdapterManager] Ringer Mode from kvStore %{public}d", mRingerMode);
    }

    return true;
}

void AudioAdapterManager::WriteVolumeToKvStore(AudioStreamType streamType, float volume)
{
    if (mAudioPolicyKvStore == nullptr)
        return;

    Key key = GetStreamNameByStreamType(streamType);
    Value value = Value(TransferTypeToByteArray<float>(volume));

    Status status = mAudioPolicyKvStore->Put(key, value);
    if (status == Status::SUCCESS) {
        MEDIA_INFO_LOG("[AudioAdapterManager] volume %{public}f for %{public}s updated in kvStore", volume,
            GetStreamNameByStreamType(streamType).c_str());
    } else {
        MEDIA_ERR_LOG("[AudioAdapterManager] volume %{public}f for %{public}s failed to update in kvStore!", volume,
            GetStreamNameByStreamType(streamType).c_str());
    }

    return;
}

void AudioAdapterManager::WriteRingerModeToKvStore(AudioRingerMode ringerMode)
{
    if (mAudioPolicyKvStore == nullptr)
        return;

    Key key = "ringermode";
    Value value = Value(TransferTypeToByteArray<int>(ringerMode));

    Status status = mAudioPolicyKvStore->Put(key, value);
    if (status == Status::SUCCESS) {
        MEDIA_INFO_LOG("[AudioAdapterManager] Wrote RingerMode:%d to kvStore", ringerMode);
    } else {
        MEDIA_ERR_LOG("[AudioAdapterManager] Writing RingerMode:%d to kvStore failed!", ringerMode);
    }

    return;
}
} // namespace AudioStandard
} // namespace OHOS
