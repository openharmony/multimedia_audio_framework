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
#include "audio_log.h"
#include "parameter.h"

#include "audio_adapter_manager.h"

using namespace std;

namespace OHOS {
namespace AudioStandard {
bool AudioAdapterManager::Init()
{
    char testMode[10] = {0}; // 10 for system parameter usage
    auto res = GetParameter("debug.audio_service.testmodeon", "0", testMode, sizeof(testMode));
    if (res == 1 && testMode[0] == '1') {
        AUDIO_DEBUG_LOG("testMode on");
        testModeOn_ = true;
    }

    char currentVolumeValue[3] = {0};
    auto ret = GetParameter("persist.multimedia.audio.mediavolume", "15",
        currentVolumeValue, sizeof(currentVolumeValue));
    if (ret > 0) {
        int32_t valueNumber = atoi(currentVolumeValue);
        mVolumeMap[STREAM_MUSIC] = AudioGroupManager::MapVolumeToHDI(valueNumber);
        AUDIO_INFO_LOG("[AudioAdapterManager] Get music volume to map success %{public}f", mVolumeMap[STREAM_MUSIC]);
    } else {
        AUDIO_ERR_LOG("[AudioAdapterManager] Get volume parameter failed %{public}d", ret);
    }

    return true;
}

bool AudioAdapterManager::ConnectServiceAdapter()
{
    std::shared_ptr<AudioAdapterManager> audioAdapterManager(this);
    std::unique_ptr<PolicyCallbackImpl> policyCallbackImpl = std::make_unique<PolicyCallbackImpl>(audioAdapterManager);
    mAudioServiceAdapter = AudioServiceAdapter::CreateAudioAdapter(std::move(policyCallbackImpl));
    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] Error in audio adapter initialization");
        return false;
    }

    bool result = mAudioServiceAdapter->Connect();
    if (!result) {
        AUDIO_ERR_LOG("[AudioAdapterManager] Error in connecting audio adapter");
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
    InitMuteStatusMap(isFirstBoot);
}

void AudioAdapterManager::Deinit(void)
{
    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        return;
    }

    return  mAudioServiceAdapter->Disconnect();
}

int32_t AudioAdapterManager::SetAudioSessionCallback(AudioSessionCallback *callback)
{
    if (callback == nullptr) {
        AUDIO_ERR_LOG("[AudioAdapterManager] SetAudioSessionCallback callback == nullptr");
        return ERR_INVALID_PARAM;
    }

    sessionCallback_ = callback;
    return SUCCESS;
}

int32_t AudioAdapterManager::SetStreamVolume(AudioStreamType streamType, float volume)
{
    if (FLOAT_COMPARE_EQ(volume, 0.0f) &&
        (streamType == STREAM_VOICE_ASSISTANT || streamType == STREAM_VOICE_CALL)) {
        // these types can not set to mute, but don't return error
        AUDIO_ERR_LOG("SetStreamVolume this type can not set mute");
        return SUCCESS;
    }

    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        return ERR_OPERATION_FAILED;
    }

    // In case if KvStore didnot connect during  bootup
    if (mAudioPolicyKvStore == nullptr) {
        bool isFirstBoot = false;
        InitAudioPolicyKvStore(isFirstBoot);
    }

    if (GetStreamMute(streamType) && volume > 0.0f) {
        SetStreamMute(streamType, 0);
    }
    AudioStreamType streamForVolumeMap = GetStreamForVolumeMap(streamType);
    mVolumeMap[streamForVolumeMap] = volume;
    WriteVolumeToKvStore(currentActiveDevice_, streamType, volume);

    // Set the power on default volume to the database
    if (streamType == STREAM_MUSIC) {
        int32_t maxMediaVolume = 15; // The max volume is 15;
        int32_t volumeInt = (int) round(volume * maxMediaVolume);
        AUDIO_INFO_LOG("[AudioAdapterManager] Start set volume value to %{public}d", volumeInt);
        int ret = SetParameter("persist.multimedia.audio.mediavolume", std::to_string(volumeInt).c_str());
        if (ret == 0) {
            AUDIO_INFO_LOG("[AudioAdapterManager] Save media volume success %{public}d", volumeInt);
        } else {
            AUDIO_ERR_LOG("[AudioAdapterManager] Save media volume failed, result %{public}d", ret);
        }
    }
    
    return mAudioServiceAdapter->SetVolume(streamType, volume);
}

float AudioAdapterManager::GetStreamVolume(AudioStreamType streamType)
{
    AudioStreamType streamForVolumeMap = GetStreamForVolumeMap(streamType);

    return mVolumeMap[streamForVolumeMap];
}

int32_t AudioAdapterManager::SetStreamMute(AudioStreamType streamType, bool mute)
{
    if (mute &&
        (streamType == STREAM_VOICE_ASSISTANT || streamType == STREAM_VOICE_CALL)) {
        // these types can not set to mute, but don't return error
        AUDIO_ERR_LOG("SetStreamMute: this type can not set mute");
        return SUCCESS;
    }

    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        return ERR_OPERATION_FAILED;
    }
    AudioStreamType streamForVolumeMap = GetStreamForVolumeMap(streamType);
    mMuteStatusMap[streamForVolumeMap] = mute;
    WriteMuteStatusToKvStore(currentActiveDevice_, streamType, mute);
    return mAudioServiceAdapter->SetMute(streamType, mute);
}

int32_t AudioAdapterManager::SetSourceOutputStreamMute(int32_t uid, bool setMute)
{
    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        return ERR_OPERATION_FAILED;
    }
    return mAudioServiceAdapter->SetSourceOutputMute(uid, setMute);
}

bool AudioAdapterManager::GetStreamMute(AudioStreamType streamType)
{
    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        return false;
    }
    AudioStreamType streamForVolumeMap = GetStreamForVolumeMap(streamType);
    return mMuteStatusMap[streamForVolumeMap];
}

bool AudioAdapterManager::IsStreamActive(AudioStreamType streamType)
{
    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        return false;
    }

    return mAudioServiceAdapter->IsStreamActive(streamType);
}

vector<SinkInput> AudioAdapterManager::GetAllSinkInputs()
{
    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        vector<SinkInput> sinkInputList;
        return sinkInputList;
    }

    return mAudioServiceAdapter->GetAllSinkInputs();
}

vector<SourceOutput> AudioAdapterManager::GetAllSourceOutputs()
{
    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        vector<SourceOutput> sourceOutputList;
        return sourceOutputList;
    }

    return mAudioServiceAdapter->GetAllSourceOutputs();
}

int32_t AudioAdapterManager::SuspendAudioDevice(std::string &portName, bool isSuspend)
{
    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        return ERR_OPERATION_FAILED;
    }

    return mAudioServiceAdapter->SuspendAudioDevice(portName, isSuspend);
}

int32_t AudioAdapterManager::SelectDevice(DeviceRole deviceRole, InternalDeviceType deviceType, std::string name)
{
    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        return ERR_OPERATION_FAILED;
    }
    switch (deviceRole) {
        case DeviceRole::INPUT_DEVICE:
            return mAudioServiceAdapter->SetDefaultSource(name);
        case DeviceRole::OUTPUT_DEVICE: {
            SetVolumeForSwitchDevice(deviceType);
            AUDIO_INFO_LOG("SetDefaultSink %{public}d", deviceType);
            return mAudioServiceAdapter->SetDefaultSink(name);
        }
        default:
            AUDIO_ERR_LOG("[AudioAdapterManager] error deviceRole %{public}d", deviceRole);
            return ERR_OPERATION_FAILED;
    }
    return SUCCESS;
}

int32_t AudioAdapterManager::SetDeviceActive(AudioIOHandle ioHandle, InternalDeviceType deviceType,
    std::string name, bool active)
{
    if (!mAudioServiceAdapter) {
        AUDIO_ERR_LOG("[AudioAdapterManager] audio adapter null");
        return ERR_OPERATION_FAILED;
    }

    switch (deviceType) {
        case InternalDeviceType::DEVICE_TYPE_SPEAKER:
        case InternalDeviceType::DEVICE_TYPE_FILE_SINK:
        case InternalDeviceType::DEVICE_TYPE_WIRED_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_USB_HEADSET:
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_A2DP:
        case InternalDeviceType::DEVICE_TYPE_BLUETOOTH_SCO: {
            SetVolumeForSwitchDevice(deviceType);
            AUDIO_INFO_LOG("SetDefaultSink %{public}d", deviceType);
            return mAudioServiceAdapter->SetDefaultSink(name);
        }
        case InternalDeviceType::DEVICE_TYPE_FILE_SOURCE:
        case InternalDeviceType::DEVICE_TYPE_MIC: {
            AUDIO_INFO_LOG("SetDefaultSource %{public}d", deviceType);
            return mAudioServiceAdapter->SetDefaultSource(name);
        }
        default:
            break;
    }
    return SUCCESS;
}

void AudioAdapterManager::SetVolumeForSwitchDevice(InternalDeviceType deviceType)
{
    if (mAudioPolicyKvStore == nullptr) {
        AUDIO_ERR_LOG("[AudioAdapterManager] mAudioPolicyKvStore is null!");
        return;
    }
    currentActiveDevice_ = deviceType;
    LoadVolumeMap();
    std::vector<AudioStreamType> streamTypeList = {
        STREAM_MUSIC,
        STREAM_RING,
        STREAM_VOICE_CALL,
        STREAM_VOICE_ASSISTANT
    };
    auto iter = streamTypeList.begin();
    while (iter != streamTypeList.end()) {
        Key key = GetStreamNameByStreamType(deviceType, *iter);
        Value value = Value(TransferTypeToByteArray<float>(0));
        Status status = mAudioPolicyKvStore->Get(key, value);
        if (status == SUCCESS) {
            float volume = TransferByteArrayToType<float>(value.Data());
            SetStreamVolume(*iter, volume);
        }
        iter++;
    }
}

int32_t AudioAdapterManager::MoveSinkInputByIndexOrName(uint32_t sinkInputId, uint32_t sinkIndex, std::string sinkName)
{
    return mAudioServiceAdapter->MoveSinkInputByIndexOrName(sinkInputId, sinkIndex, sinkName);
}

int32_t AudioAdapterManager::MoveSourceOutputByIndexOrName(uint32_t sourceOutputId, uint32_t sourceIndex,
    std::string sourceName)
{
    return mAudioServiceAdapter->MoveSourceOutputByIndexOrName(sourceOutputId, sourceIndex, sourceName);
}

int32_t AudioAdapterManager::SetRingerMode(AudioRingerMode ringerMode)
{
    mRingerMode = ringerMode;

    // In case if KvStore didnot connect during  bootup
    if (mAudioPolicyKvStore == nullptr) {
        bool isFirstBoot = false;
        InitAudioPolicyKvStore(isFirstBoot);
    }

    WriteRingerModeToKvStore(ringerMode);
    return SUCCESS;
}

AudioRingerMode AudioAdapterManager::GetRingerMode() const
{
    return mRingerMode;
}

AudioIOHandle AudioAdapterManager::OpenAudioPort(const AudioModuleInfo &audioModuleInfo)
{
    std::string moduleArgs = GetModuleArgs(audioModuleInfo);
    AUDIO_INFO_LOG("[Adapter load-module] %{public}s %{public}s", audioModuleInfo.lib.c_str(), moduleArgs.c_str());

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
        AUDIO_INFO_LOG("[PolicyManager] format: %{public}s", args.c_str());
    }

    if (!audioModuleInfo.fixedLatency.empty()) {
        args.append(" fixed_latency=");
        args.append(audioModuleInfo.fixedLatency);
    }

    if (!audioModuleInfo.renderInIdleState.empty()) {
        args.append(" render_in_idle_state=");
        args.append(audioModuleInfo.renderInIdleState);
    }

    if (!audioModuleInfo.OpenMicSpeaker.empty()) {
        args.append(" open_mic_speaker=");
        args.append(audioModuleInfo.OpenMicSpeaker);
    }
}

// Private Members
std::string AudioAdapterManager::GetModuleArgs(const AudioModuleInfo &audioModuleInfo) const
{
    std::string args;

    if (audioModuleInfo.lib == HDI_SINK) {
        UpdateCommonArgs(audioModuleInfo, args);
        if (!audioModuleInfo.name.empty()) {
            args.append(" sink_name=");
            args.append(audioModuleInfo.name);
        }

        if (!audioModuleInfo.adapterName.empty()) {
            args.append(" adapter_name=");
            args.append(audioModuleInfo.adapterName);
        }

        if (!audioModuleInfo.className.empty()) {
            args.append(" device_class=");
            args.append(audioModuleInfo.className);
        }

        if (!audioModuleInfo.fileName.empty()) {
            args.append(" file_path=");
            args.append(audioModuleInfo.fileName);
        }
        if (!audioModuleInfo.sinkLatency.empty()) {
            args.append(" sink_latency=");
            args.append(audioModuleInfo.sinkLatency);
        }
        if (testModeOn_) {
            args.append(" test_mode_on=");
            args.append("1");
        }
        if (!audioModuleInfo.networkId.empty()) {
            args.append(" network_id=");
            args.append(audioModuleInfo.networkId);
        } else {
            args.append(" network_id=LocalDevice");
        }

        if (!audioModuleInfo.deviceType.empty()) {
            args.append(" device_type=");
            args.append(audioModuleInfo.deviceType);
        }
    } else if (audioModuleInfo.lib == HDI_SOURCE) {
        UpdateCommonArgs(audioModuleInfo, args);
        if (!audioModuleInfo.name.empty()) {
            args.append(" source_name=");
            args.append(audioModuleInfo.name);
        }

        if (!audioModuleInfo.adapterName.empty()) {
            args.append(" adapter_name=");
            args.append(audioModuleInfo.adapterName);
        }

        if (!audioModuleInfo.className.empty()) {
            args.append(" device_class=");
            args.append(audioModuleInfo.className);
        }

        if (!audioModuleInfo.fileName.empty()) {
            args.append(" file_path=");
            args.append(audioModuleInfo.fileName);
        }

        if (!audioModuleInfo.networkId.empty()) {
            args.append(" network_id=");
            args.append(audioModuleInfo.networkId);
        } else {
            args.append(" network_id=LocalDevice");
        }

        if (!audioModuleInfo.deviceType.empty()) {
            args.append(" device_type=");
            args.append(audioModuleInfo.deviceType);
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

std::string AudioAdapterManager::GetStreamNameByStreamType(DeviceType deviceType, AudioStreamType streamType)
{
    std::string type;
    switch (deviceType) {
        case DEVICE_TYPE_SPEAKER:
            type = "primary";
            break;
        case DEVICE_TYPE_BLUETOOTH_A2DP:
            type = "bluetooth";
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
            type = "wired";
            break;
        case DEVICE_TYPE_USB_HEADSET:
            type = "usb";
            break;
        default:
            AUDIO_ERR_LOG("[AudioAdapterManager::GetStreamNameByStreamType] deivice %{public}d is not supported for kv"
                " store", deviceType);
            return "";
    }

    switch (streamType) {
        case STREAM_MUSIC:
            return type + "music";
        case STREAM_RING:
            return type + "ring";
        case STREAM_SYSTEM:
            return type + "system";
        case STREAM_NOTIFICATION:
            return type + "notification";
        case STREAM_ALARM:
            return type + "alarm";
        case STREAM_DTMF:
            return type + "dtmf";
        case STREAM_VOICE_CALL:
            return type + "voice_call";
        case STREAM_VOICE_ASSISTANT:
            return type + "voice_assistant";
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

    AppId appId;
    appId.appId = "audio_policy_manager";

    options.securityLevel = S1;
    options.createIfMissing = true;
    options.encrypt = false;
    options.autoSync = false;
    options.kvStoreType = KvStoreType::SINGLE_VERSION;
    options.area = EL1;
    options.baseDir = std::string("/data/service/el1/public/database/") + appId.appId;

    StoreId storeId;
    storeId.storeId = "audiopolicy";
    Status status = Status::SUCCESS;

    // open and initialize kvstore instance.
    if (mAudioPolicyKvStore == nullptr) {
        uint32_t retries = 0;

        do {
            status = manager.GetSingleKvStore(options, appId, storeId, mAudioPolicyKvStore);
            if (status == Status::STORE_NOT_FOUND) {
                AUDIO_ERR_LOG("[AudioAdapterManager] InitAudioPolicyKvStore: STORE_NOT_FOUND!");
            }

            if ((status == Status::SUCCESS) || (status == Status::STORE_NOT_FOUND)) {
                break;
            } else {
                AUDIO_ERR_LOG("[AudioAdapterManager] InitAudioPolicyKvStore: Kvstore Connect failed! Retrying.");
                retries++;
                usleep(KVSTORE_CONNECT_RETRY_DELAY_TIME);
            }
        } while (retries <= KVSTORE_CONNECT_RETRY_COUNT);
    }

    if (mAudioPolicyKvStore == nullptr) {
        if (status == Status::STORE_NOT_FOUND) {
            AUDIO_INFO_LOG("[AudioAdapterManager] First Boot: Create AudioPolicyKvStore");
            options.createIfMissing = true;
            // [create and] open and initialize kvstore instance.
            status = manager.GetSingleKvStore(options, appId, storeId, mAudioPolicyKvStore);
            if (status == Status::SUCCESS) {
                isFirstBoot = true;
            } else {
                AUDIO_ERR_LOG("[AudioAdapterManager] Create AudioPolicyKvStore Failed!");
            }
        }
    }

    if (mAudioPolicyKvStore == nullptr) {
        AUDIO_ERR_LOG("[AudioAdapterManager] InitAudioPolicyKvStore: Failed!");
        return false;
    }

    return true;
}

void AudioAdapterManager::InitVolumeMap(bool isFirstBoot)
{
    if (isFirstBoot == true) {
        auto iter = deviceList_.begin();
        while (iter != deviceList_.end()) {
            WriteVolumeToKvStore(*iter, STREAM_MUSIC, MAX_VOLUME);
            WriteVolumeToKvStore(*iter, STREAM_RING, MAX_VOLUME);
            WriteVolumeToKvStore(*iter, STREAM_VOICE_CALL, MAX_VOLUME);
            WriteVolumeToKvStore(*iter, STREAM_VOICE_ASSISTANT, MAX_VOLUME);
            iter++;
        }
        AUDIO_INFO_LOG("[AudioAdapterManager] Wrote default stream volumes to KvStore");
    } else {
        LoadVolumeMap();
    }
}

void AudioAdapterManager::InitRingerMode(bool isFirstBoot)
{
    if (mAudioPolicyKvStore == nullptr) {
        AUDIO_ERR_LOG("[AudioAdapterManager] mAudioPolicyKvStore is null!");
        return;
    }

    if (isFirstBoot == true) {
        mRingerMode = RINGER_MODE_NORMAL;
        WriteRingerModeToKvStore(RINGER_MODE_NORMAL);
        AUDIO_INFO_LOG("[AudioAdapterManager] Wrote default ringer mode to KvStore");
    } else {
        LoadRingerMode();
    }
}

bool AudioAdapterManager::LoadVolumeFromKvStore(DeviceType deviceType, AudioStreamType streamType)
{
    Key key;
    Value value;
    std::string type;

    switch (deviceType) {
        case DEVICE_TYPE_SPEAKER:
            type = "primary";
            break;
        case DEVICE_TYPE_BLUETOOTH_A2DP:
            type = "bluetooth";
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
            type = "wired";
            break;
        case DEVICE_TYPE_USB_HEADSET:
            type = "usb";
            break;
        default:
            AUDIO_ERR_LOG("[AudioAdapterManager::LoadVolumeFromKvStore] deivice %{public}d is not supported for kv"
                "store", deviceType);
            return false;
    }

    switch (streamType) {
        case STREAM_MUSIC:
            key = type + "music";
            break;
        case STREAM_RING:
            key = type + "ring";
            break;
        case STREAM_VOICE_CALL:
            key = type + "voice_call";
            break;
        case STREAM_VOICE_ASSISTANT:
            key = type + "voice_assistant";
            break;
        default:
            return false;
    }

    Status status = mAudioPolicyKvStore->Get(key, value);
    if (status == Status::SUCCESS) {
        float volume = TransferByteArrayToType<float>(value.Data());
        mVolumeMap[streamType] = volume;
        AUDIO_DEBUG_LOG("[AudioAdapterManager] volume from kvStore %{public}f for streamType:%{public}d",
                        volume, streamType);
        return true;
    }

    return false;
}

bool AudioAdapterManager::LoadVolumeMap(void)
{
    if (mAudioPolicyKvStore == nullptr) {
        AUDIO_ERR_LOG("[AudioAdapterManager] LoadVolumeMap: mAudioPolicyKvStore is null!");
        return false;
    }

    if (!LoadVolumeFromKvStore(currentActiveDevice_, STREAM_MUSIC))
        AUDIO_ERR_LOG("[AudioAdapterManager] LoadVolumeMap: Couldnot load volume for Music from kvStore!");

    if (!LoadVolumeFromKvStore(currentActiveDevice_, STREAM_RING))
        AUDIO_ERR_LOG("[AudioAdapterManager] LoadVolumeMap: Couldnot load volume for Ring from kvStore!");

    if (!LoadVolumeFromKvStore(currentActiveDevice_, STREAM_VOICE_CALL))
        AUDIO_ERR_LOG("[AudioAdapterManager] LoadVolumeMap: Couldnot load volume for voice_call from kvStore!");

    if (!LoadVolumeFromKvStore(currentActiveDevice_, STREAM_VOICE_ASSISTANT))
        AUDIO_ERR_LOG("[AudioAdapterManager] LoadVolumeMap: Couldnot load volume for voice_assistant from kvStore!");

    return true;
}

bool AudioAdapterManager::LoadRingerMode(void)
{
    if (mAudioPolicyKvStore == nullptr) {
        AUDIO_ERR_LOG("[AudioAdapterManager] LoadRingerMap: mAudioPolicyKvStore is null!");
        return false;
    }

    // get ringer mode value from kvstore.
    Key key = "ringermode";
    Value value;
    Status status = mAudioPolicyKvStore->Get(key, value);
    if (status == Status::SUCCESS) {
        mRingerMode = static_cast<AudioRingerMode>(TransferByteArrayToType<int>(value.Data()));
        AUDIO_DEBUG_LOG("[AudioAdapterManager] Ringer Mode from kvStore %{public}d", mRingerMode);
    }

    return true;
}

void AudioAdapterManager::WriteVolumeToKvStore(DeviceType type, AudioStreamType streamType, float volume)
{
    if (mAudioPolicyKvStore == nullptr)
        return;

    Key key = GetStreamNameByStreamType(type, streamType);
    Value value = Value(TransferTypeToByteArray<float>(volume));

    Status status = mAudioPolicyKvStore->Put(key, value);
    if (status == Status::SUCCESS) {
        AUDIO_INFO_LOG("[AudioAdapterManager] volume %{public}f for %{public}s updated in kvStore", volume,
            GetStreamNameByStreamType(type, streamType).c_str());
    } else {
        AUDIO_ERR_LOG("[AudioAdapterManager] volume %{public}f for %{public}s failed to update in kvStore!", volume,
            GetStreamNameByStreamType(type, streamType).c_str());
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
        AUDIO_INFO_LOG("[AudioAdapterManager] Wrote RingerMode:%d to kvStore", ringerMode);
    } else {
        AUDIO_ERR_LOG("[AudioAdapterManager] Writing RingerMode:%d to kvStore failed!", ringerMode);
    }

    return;
}

void AudioAdapterManager::InitMuteStatusMap(bool isFirstBoot)
{
    if (isFirstBoot == true) {
        auto iter = deviceList_.begin();
        while (iter != deviceList_.end()) {
            WriteMuteStatusToKvStore(*iter, STREAM_MUSIC, 0);
            WriteMuteStatusToKvStore(*iter, STREAM_RING, 0);
            WriteMuteStatusToKvStore(*iter, STREAM_VOICE_CALL, 0);
            WriteMuteStatusToKvStore(*iter, STREAM_VOICE_ASSISTANT, 0);
            iter++;
        }
        AUDIO_INFO_LOG("[AudioAdapterManager] Wrote default mute status to KvStore");
    } else {
        LoadMuteStatusMap();
    }
}


bool AudioAdapterManager::LoadMuteStatusMap(void)
{
    if (mAudioPolicyKvStore == nullptr) {
        AUDIO_ERR_LOG("[AudioAdapterManager] LoadMuteStatusMap: mAudioPolicyKvStore is null!");
        return false;
    }

    if (!LoadMuteStatusFromKvStore(STREAM_MUSIC))
        AUDIO_ERR_LOG("[AudioAdapterManager] Could not load mute status for MUSIC from kvStore!");

    if (!LoadMuteStatusFromKvStore(STREAM_RING))
        AUDIO_ERR_LOG("[AudioAdapterManager] Could not load mute status for RING from kvStore!");

    if (!LoadMuteStatusFromKvStore(STREAM_VOICE_CALL))
        AUDIO_ERR_LOG("[AudioAdapterManager] Could not load mute status for VOICE_CALL from kvStore!");

    if (!LoadMuteStatusFromKvStore(STREAM_VOICE_ASSISTANT))
        AUDIO_ERR_LOG("[AudioAdapterManager] Could not load mute status for VOICE_ASSISTANT from kvStore!");

    return true;
}

bool AudioAdapterManager::LoadMuteStatusFromKvStore(AudioStreamType streamType)
{
    Value value;

    switch (streamType) {
        case STREAM_MUSIC:
            break;
        case STREAM_RING:
            break;
        case STREAM_VOICE_CALL:
            break;
        case STREAM_VOICE_ASSISTANT:
            break;
        default:
            return false;
    }
    Key key = GetStreamTypeKeyForMute(DeviceType::DEVICE_TYPE_SPEAKER, streamType);
    Status status = mAudioPolicyKvStore->Get(key, value);
    if (status == Status::SUCCESS) {
        int volumeStatus = TransferByteArrayToType<int>(value.Data());
        mMuteStatusMap[streamType] = volumeStatus;
        return true;
    }

    return false;
}

std::string AudioAdapterManager::GetStreamTypeKeyForMute(DeviceType deviceType, AudioStreamType streamType)
{
    std::string type = "";
    switch (deviceType) {
        case DEVICE_TYPE_SPEAKER:
            type = "primary";
            break;
        case DEVICE_TYPE_BLUETOOTH_A2DP:
            type = "bluetooth";
            break;
        case DEVICE_TYPE_WIRED_HEADSET:
            type = "wired";
            break;
        case DEVICE_TYPE_USB_HEADSET:
            type = "usb";
            break;
        default:
            AUDIO_ERR_LOG("[AudioAdapterManager::GetStreamTypeKeyForMute] deivice %{public}d is not supported for kv"
                "store", deviceType);
            return "";
    }

    switch (streamType) {
        case STREAM_MUSIC:
            return type + "music_mute_status";
        case STREAM_RING:
            return type + "ring_mute_status";
        case STREAM_SYSTEM:
            return type + "system_mute_status";
        case STREAM_NOTIFICATION:
            return type + "notification_mute_status";
        case STREAM_ALARM:
            return type + "alarm_mute_status";
        case STREAM_DTMF:
            return type + "dtmf_mute_status";
        case STREAM_VOICE_CALL:
            return type + "voice_call_mute_status";
        case STREAM_VOICE_ASSISTANT:
            return type + "voice_assistant_mute_status";
        default:
            return "";
    }
}

void AudioAdapterManager::WriteMuteStatusToKvStore(DeviceType deviceType, AudioStreamType streamType,
    bool muteStatus)
{
    if (mAudioPolicyKvStore == nullptr) {
        return;
    }

    Key key = GetStreamTypeKeyForMute(deviceType, streamType);
    Value value = Value(TransferTypeToByteArray<int>(muteStatus));

    Status status = mAudioPolicyKvStore->Put(key, value);
    if (status == Status::SUCCESS) {
        AUDIO_INFO_LOG("[AudioAdapterManager] muteStatus %{public}d for %{public}s updated to kvStore", muteStatus,
            GetStreamNameByStreamType(deviceType, streamType).c_str());
    } else {
        AUDIO_INFO_LOG("[AudioAdapterManager] muteStatus %{public}d for %{public}s update to kvStore failed",
            muteStatus, GetStreamNameByStreamType(deviceType, streamType).c_str());
    }

    return;
}
} // namespace AudioStandard
} // namespace OHOS
