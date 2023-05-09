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

#include <cinttypes>
#include <csignal>
#include <fstream>
#include <sstream>

#include "audio_capturer_source.h"
#include "audio_errors.h"
#include "audio_renderer_sink.h"
#include "iservice_registry.h"
#include "audio_log.h"
#include "system_ability_definition.h"
#include "audio_manager_listener_proxy.h"
#include "bluetooth_renderer_sink_intf.h"

#include "audio_server.h"

extern "C" {
#include "renderer_sink_adapter.h"
}

#define PA
#ifdef PA
extern "C" {
    extern int ohos_pa_main(int argc, char *argv[]);
}
#endif

using namespace std;

namespace OHOS {
namespace AudioStandard {
std::map<std::string, std::string> AudioServer::audioParameters;
const string DEFAULT_COOKIE_PATH = "/data/data/.pulse_dir/state/cookie";

REGISTER_SYSTEM_ABILITY_BY_ID(AudioServer, AUDIO_DISTRIBUTED_SERVICE_ID, true)

#ifdef PA
constexpr int PA_ARG_COUNT = 1;

void *AudioServer::paDaemonThread(void *arg)
{
    /* Load the mandatory pulseaudio modules at start */
    char *argv[] = {
        (char*)"pulseaudio",
    };

    AUDIO_INFO_LOG("Calling ohos_pa_main\n");
    ohos_pa_main(PA_ARG_COUNT, argv);
    AUDIO_INFO_LOG("Exiting ohos_pa_main\n");
    exit(-1);
}
#endif

AudioServer::AudioServer(int32_t systemAbilityId, bool runOnCreate)
    : SystemAbility(systemAbilityId, runOnCreate)
{}

void AudioServer::OnDump()
{}

void AudioServer::OnStart()
{
    AUDIO_DEBUG_LOG("AudioService OnStart");
    bool res = Publish(this);
    if (res) {
        AUDIO_DEBUG_LOG("AudioService OnStart res=%{public}d", res);
    }
    AddSystemAbilityListener(AUDIO_POLICY_SERVICE_ID);
#ifdef PA
    int32_t ret = pthread_create(&m_paDaemonThread, nullptr, AudioServer::paDaemonThread, nullptr);
    if (ret != 0) {
        AUDIO_ERR_LOG("pthread_create failed %d", ret);
    }
    AUDIO_INFO_LOG("Created paDaemonThread\n");
#endif
}

void AudioServer::OnAddSystemAbility(int32_t systemAbilityId, const std::string& deviceId)
{
    AUDIO_DEBUG_LOG("OnAddSystemAbility systemAbilityId:%{public}d", systemAbilityId);
    switch (systemAbilityId) {
        case AUDIO_POLICY_SERVICE_ID:
            AUDIO_INFO_LOG("OnAddSystemAbility input service start");
            RegisterPolicyServerDeathRecipient();
            break;
        default:
            AUDIO_ERR_LOG("OnAddSystemAbility unhandled sysabilityId:%{public}d", systemAbilityId);
            break;
    }
}

void AudioServer::OnStop()
{
    AUDIO_DEBUG_LOG("AudioService OnStop");
}

void AudioServer::SetAudioParameter(const std::string &key, const std::string &value)
{
    AUDIO_DEBUG_LOG("server: set audio parameter");
    if (!VerifyClientPermission(MODIFY_AUDIO_SETTINGS_PERMISSION)) {
        AUDIO_ERR_LOG("SetAudioParameter: MODIFY_AUDIO_SETTINGS permission denied");
        return;
    }

    AudioServer::audioParameters[key] = value;
#ifdef PRODUCT_M40
    AudioRendererSink* audioRendererSinkInstance = AudioRendererSink::GetInstance();
    if (audioRendererSinkInstance == nullptr) {
        AUDIO_ERR_LOG("has no valid sink");
        return;
    }
    AudioParamKey parmKey = AudioParamKey::NONE;
    if (key == "AUDIO_EXT_PARAM_KEY_LOWPOWER") {
        parmKey = AudioParamKey::PARAM_KEY_LOWPOWER;
    } else {
        AUDIO_ERR_LOG("SetAudioParameter: key %{publbic}s is invalid for hdi interface", key.c_str());
        return;
    }
    audioRendererSinkInstance->SetAudioParameter(parmKey, "", value);
#endif
}

void AudioServer::SetAudioParameter(const std::string& networkId, const AudioParamKey key, const std::string& condition,
    const std::string& value)
{
    RemoteAudioRendererSink* audioRendererSinkInstance = RemoteAudioRendererSink::GetInstance(networkId.c_str());
    if (audioRendererSinkInstance == nullptr) {
        AUDIO_ERR_LOG("has no valid sink");
        return;
    }

    audioRendererSinkInstance->SetAudioParameter(key, condition, value);
}

const std::string AudioServer::GetAudioParameter(const std::string &key)
{
    AUDIO_DEBUG_LOG("server: get audio parameter");
#ifdef PRODUCT_M40
    AudioRendererSink* audioRendererSinkInstance = AudioRendererSink::GetInstance();
    if (audioRendererSinkInstance != nullptr) {
        AudioParamKey parmKey = AudioParamKey::NONE;
        if (key == "AUDIO_EXT_PARAM_KEY_LOWPOWER") {
            parmKey = AudioParamKey::PARAM_KEY_LOWPOWER;
            return audioRendererSinkInstance->GetAudioParameter(AudioParamKey(parmKey), "");
        }
    }
#endif
     if (AudioServer::audioParameters.count(key)) {
         return AudioServer::audioParameters[key];
     } else {
         return "";
     }
}

const std::string AudioServer::GetAudioParameter(const std::string& networkId, const AudioParamKey key,
    const std::string& condition)
{
    RemoteAudioRendererSink* audioRendererSinkInstance = RemoteAudioRendererSink::GetInstance(networkId.c_str());
    if (audioRendererSinkInstance == nullptr) {
        AUDIO_ERR_LOG("has no valid sink");
        return "";
    }
    return audioRendererSinkInstance->GetAudioParameter(key, condition);
}

const char *AudioServer::RetrieveCookie(int32_t &size)
{
    char *cookieInfo = nullptr;
    size = 0;
    std::ifstream cookieFile(DEFAULT_COOKIE_PATH, std::ifstream::binary);
    if (!cookieFile) {
        return cookieInfo;
    }

    cookieFile.seekg (0, cookieFile.end);
    size = cookieFile.tellg();
    cookieFile.seekg (0, cookieFile.beg);

    if ((size > 0) && (size < PATH_MAX)) {
        cookieInfo = (char *)malloc(size * sizeof(char));
        if (cookieInfo == nullptr) {
            AUDIO_ERR_LOG("AudioServer::RetrieveCookie: No memory");
            cookieFile.close();
            return cookieInfo;
        }
        AUDIO_DEBUG_LOG("Reading: %{public}d characters...", size);
        cookieFile.read(cookieInfo, size);
    }
    cookieFile.close();
    return cookieInfo;
}

uint64_t AudioServer::GetTransactionId(DeviceType deviceType, DeviceRole deviceRole)
{
    uint64_t transactionId = 0;
    AUDIO_INFO_LOG("GetTransactionId in: device type: %{public}d, device role: %{public}d", deviceType, deviceRole);
    if (deviceRole != INPUT_DEVICE && deviceRole != OUTPUT_DEVICE) {
        AUDIO_ERR_LOG("GetTransactionId: error device role");
        return ERR_INVALID_PARAM;
    }
    if (deviceRole == OUTPUT_DEVICE) {
        struct RendererSinkAdapter *sinkAdapter;
        int32_t ret = SUCCESS;
        if (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) {
            ret = LoadSinkAdapter("a2dp", "LocalDevice", &sinkAdapter);
        } else {
            ret = LoadSinkAdapter("primary", "LocalDevice", &sinkAdapter);
        }

        if (ret) {
            AUDIO_ERR_LOG("Load adapter failed");
            return transactionId;
        }

        sinkAdapter->RendererSinkGetTransactionId(&transactionId);
        UnLoadSinkAdapter(sinkAdapter);
    } else if (deviceRole == INPUT_DEVICE) {
        AudioCapturerSource *audioCapturerSourceInstance = AudioCapturerSource::GetInstance();
        if (audioCapturerSourceInstance) {
            transactionId = audioCapturerSourceInstance->GetTransactionId();
        }
    }

    AUDIO_INFO_LOG("Transaction Id: %{public}" PRIu64, transactionId);
    return transactionId;
}

int32_t AudioServer::GetMaxVolume(AudioVolumeType volumeType)
{
    AUDIO_DEBUG_LOG("GetMaxVolume server");
    return MAX_VOLUME;
}

int32_t AudioServer::GetMinVolume(AudioVolumeType volumeType)
{
    AUDIO_DEBUG_LOG("GetMinVolume server");
    if (volumeType == STREAM_VOICE_ASSISTANT || volumeType == STREAM_VOICE_CALL) {
        return (MIN_VOLUME + 1);
    }
    return MIN_VOLUME;
}

int32_t AudioServer::SetMicrophoneMute(bool isMute)
{
    int32_t audio_policy_server_id = 1041;
    int32_t audio_policy_server_Uid = 1005;
    if (IPCSkeleton::GetCallingUid() != audio_policy_server_id
        && IPCSkeleton::GetCallingUid() != audio_policy_server_Uid) {
        return ERR_PERMISSION_DENIED;
    }
    AudioCapturerSource *audioCapturerSourceInstance = AudioCapturerSource::GetInstance();

    if (!audioCapturerSourceInstance->capturerInited_) {
            AUDIO_INFO_LOG("Capturer is not initialized. Set the flag mute state flag");
            AudioCapturerSource::micMuteState_ = isMute;
            return 0;
    }

    return audioCapturerSourceInstance->SetMute(isMute);
}

bool AudioServer::IsMicrophoneMute()
{
    int32_t audio_policy_server_id = 1041;
    int32_t audio_policy_server_Uid = 1005;
    if (IPCSkeleton::GetCallingUid() != audio_policy_server_id
        && IPCSkeleton::GetCallingUid() != audio_policy_server_Uid) {
        return false;
    }
    AudioCapturerSource *audioCapturerSourceInstance = AudioCapturerSource::GetInstance();
    bool isMute = false;

    if (!audioCapturerSourceInstance->capturerInited_) {
        AUDIO_INFO_LOG("Capturer is not initialized. Get the mic mute state flag value!");
        return AudioCapturerSource::micMuteState_;
    }

    if (audioCapturerSourceInstance->GetMute(isMute)) {
        AUDIO_ERR_LOG("GetMute status in capturer returned Error !");
    }

    return isMute;
}

int32_t AudioServer::SetVoiceVolume(float volume)
{
    AudioRendererSink *audioRendererSinkInstance = AudioRendererSink::GetInstance();

    if (audioRendererSinkInstance == nullptr) {
        AUDIO_WARNING_LOG("Renderer is null.");
    } else {
        return audioRendererSinkInstance->SetVoiceVolume(volume);
    }
    return ERROR;
}

int32_t AudioServer::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    AudioCapturerSource *audioCapturerSourceInstance = AudioCapturerSource::GetInstance();
    AudioRendererSink *audioRendererSinkInstance = AudioRendererSink::GetInstance();

    if (!audioCapturerSourceInstance->capturerInited_) {
        AUDIO_WARNING_LOG("Capturer is not initialized.");
    } else {
        audioCapturerSourceInstance->SetAudioScene(audioScene, activeDevice);
    }

    if (!audioRendererSinkInstance->rendererInited_) {
        AUDIO_WARNING_LOG("Renderer is not initialized.");
    } else {
        audioRendererSinkInstance->SetAudioScene(audioScene, activeDevice);
    }

    audioScene_ = audioScene;

    return SUCCESS;
}

int32_t AudioServer::UpdateActiveDeviceRoute(DeviceType type, DeviceFlag flag)
{
    AUDIO_INFO_LOG("UpdateActiveDeviceRoute deviceType: %{public}d, flag: %{public}d", type, flag);
    AudioCapturerSource *audioCapturerSourceInstance = AudioCapturerSource::GetInstance();
    AudioRendererSink *audioRendererSinkInstance = AudioRendererSink::GetInstance();

    switch (flag) {
        case DeviceFlag::INPUT_DEVICES_FLAG: {
            if (audioScene_ != AUDIO_SCENE_DEFAULT) {
                audioCapturerSourceInstance->SetAudioScene(audioScene_, type);
            } else {
                audioCapturerSourceInstance->SetInputRoute(type);
            }
            break;
        }
        case DeviceFlag::OUTPUT_DEVICES_FLAG: {
            if (audioScene_ != AUDIO_SCENE_DEFAULT) {
                audioRendererSinkInstance->SetAudioScene(audioScene_, type);
            } else {
                audioRendererSinkInstance->SetOutputRoute(type);
            }
            break;
        }
        case DeviceFlag::ALL_DEVICES_FLAG: {
            if (audioScene_ != AUDIO_SCENE_DEFAULT) {
                SetAudioScene(audioScene_, type);
            } else {
                audioCapturerSourceInstance->SetInputRoute(type);
                audioRendererSinkInstance->SetOutputRoute(type);
            }
            break;
        }
        default:
            break;
    }

    return SUCCESS;
}

void AudioServer::SetAudioMonoState(bool audioMono)
{
    AUDIO_INFO_LOG("AudioServer::SetAudioMonoState: audioMono = %{public}s", audioMono? "true": "false");

    // Set mono for audio_renderer_sink(primary sink)
    AudioRendererSink *audioRendererSinkInstance = AudioRendererSink::GetInstance();
    if (!audioRendererSinkInstance->rendererInited_) {
        AUDIO_WARNING_LOG("Renderer is not initialized.");
    } else {
        audioRendererSinkInstance->SetAudioMonoState(audioMono);
    }

    // Set mono for bluetooth_renderer_sink
    BluetoothRendererSinkSetAudioMonoState(audioMono);
}

void AudioServer::SetAudioBalanceValue(float audioBalance)
{
    AUDIO_INFO_LOG("AudioServer::SetAudioBalanceValue: audioBalance = %{public}f", audioBalance);

    // Set balance for audio_renderer_sink(primary sink)
    AudioRendererSink *audioRendererSinkInstance = AudioRendererSink::GetInstance();
    if (!audioRendererSinkInstance->rendererInited_) {
        AUDIO_WARNING_LOG("Renderer is not initialized.");
    } else {
        audioRendererSinkInstance->SetAudioBalanceValue(audioBalance);
    }

    // Set balance for bluetooth_renderer_sink
    BluetoothRendererSinkSetAudioBalanceValue(audioBalance);
}

void AudioServer::NotifyDeviceInfo(std::string networkId, bool connected)
{
    AUDIO_INFO_LOG("notify device info: networkId(%{public}s), connected(%{public}d)", networkId.c_str(), connected);
    RemoteAudioRendererSink* audioRendererSinkInstance = RemoteAudioRendererSink::GetInstance(networkId.c_str());
    if (audioRendererSinkInstance != nullptr && connected) {
        audioRendererSinkInstance->RegisterParameterCallback(this);
    }
}

int32_t AudioServer::CheckRemoteDeviceState(std::string networkId, DeviceRole deviceRole, bool isStartDevice)
{
    AUDIO_INFO_LOG("CheckRemoteDeviceState: device[%{public}s] deviceRole[%{public}d] isStartDevice[%{public}s]",
        networkId.c_str(), static_cast<int32_t>(deviceRole), (isStartDevice ? "true" : "false"));
    RemoteAudioRendererSink* audioRendererSinkInstance = RemoteAudioRendererSink::GetInstance(networkId.c_str());
    if (audioRendererSinkInstance == nullptr || !audioRendererSinkInstance->rendererInited_) {
        return ERR_ILLEGAL_STATE;
    }
    int32_t ret = SUCCESS;
    if (isStartDevice) {
        ret = audioRendererSinkInstance->Start();
    }
    return ret;
}

void AudioServer::OnAudioParameterChange(std::string netWorkId, const AudioParamKey key, const std::string& condition,
    const std::string value)
{
    std::lock_guard<std::mutex> lockSet(setParameterCallbackMutex_);
    AUDIO_INFO_LOG("OnAudioParameterChange Callback from networkId: %s", netWorkId.c_str());

    if (callback_ != nullptr) {
        callback_->OnAudioParameterChange(netWorkId, key, condition, value);
    }
}

int32_t AudioServer::SetParameterCallback(const sptr<IRemoteObject>& object)
{
    std::lock_guard<std::mutex> lockSet(setParameterCallbackMutex_);
    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_INVALID_PARAM, "AudioServer:set listener object is nullptr");

    sptr<IStandardAudioServerManagerListener> listener = iface_cast<IStandardAudioServerManagerListener>(object);

    CHECK_AND_RETURN_RET_LOG(listener != nullptr, ERR_INVALID_PARAM, "AudioServer: listener obj cast failed");

    std::shared_ptr<AudioParameterCallback> callback = std::make_shared<AudioManagerListenerCallback>(listener);
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer: failed to  create cb obj");

    callback_ = callback;
    AUDIO_INFO_LOG("AudioServer:: SetParameterCallback  done");

    return SUCCESS;
}

bool AudioServer::VerifyClientPermission(const std::string &permissionName)
{
    auto callerUid = IPCSkeleton::GetCallingUid();
    AUDIO_INFO_LOG("AudioServer: ==[%{public}s] [uid:%{public}d]==", permissionName.c_str(), callerUid);

    // Root users should be whitelisted
    if (callerUid == ROOT_UID) {
        AUDIO_INFO_LOG("Root user. Permission GRANTED!!!");
        return true;
    }

    Security::AccessToken::AccessTokenID clientTokenId = IPCSkeleton::GetCallingTokenID();
    int res = Security::AccessToken::AccessTokenKit::VerifyAccessToken(clientTokenId, permissionName);
    if (res != Security::AccessToken::PermissionState::PERMISSION_GRANTED) {
        AUDIO_ERR_LOG("Permission denied [tid:%{public}d]", clientTokenId);
        return false;
    }

    return true;
}

std::vector<sptr<AudioDeviceDescriptor>> AudioServer::GetDevices(DeviceFlag deviceFlag)
{
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptor = {};
    return audioDeviceDescriptor;
}

void AudioServer::AudioServerDied(pid_t pid)
{
    AUDIO_INFO_LOG("Policy server died: restart pulse audio");
    _Exit(0);
}

void AudioServer::RegisterPolicyServerDeathRecipient()
{
    AUDIO_INFO_LOG("Register policy server death recipient");
    pid_t pid = IPCSkeleton::GetCallingPid();
    sptr<AudioServerDeathRecipient> deathRecipient_ = new(std::nothrow) AudioServerDeathRecipient(pid);
    if (deathRecipient_ != nullptr) {
        auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
        CHECK_AND_RETURN_LOG(samgr != nullptr, "Failed to obtain system ability manager");
        sptr<IRemoteObject> object = samgr->GetSystemAbility(OHOS::AUDIO_POLICY_SERVICE_ID);
        CHECK_AND_RETURN_LOG(object != nullptr, "Policy service unavailable");
        deathRecipient_->SetNotifyCb(std::bind(&AudioServer::AudioServerDied, this, std::placeholders::_1));
        bool result = object->AddDeathRecipient(deathRecipient_);
        if (!result) {
            AUDIO_ERR_LOG("Failed to add deathRecipient");
        }
    }
}
} // namespace AudioStandard
} // namespace OHOS
