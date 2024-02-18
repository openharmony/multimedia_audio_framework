/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "audio_server.h"

#include <cinttypes>
#include <csignal>
#include <fstream>
#include <sstream>
#include <thread>
#include <unordered_map>

#include "iservice_registry.h"
#include "system_ability_definition.h"
#include "hisysevent.h"

#include "audio_capturer_source.h"
#include "remote_audio_capturer_source.h"
#include "audio_errors.h"
#include "audio_log.h"
#include "audio_manager_listener_proxy.h"
#include "audio_service.h"
#include "audio_schedule.h"
#include "audio_utils.h"
#include "i_audio_capturer_source.h"
#include "i_audio_renderer_sink.h"
#include "i_standard_audio_server_manager_listener.h"
#include "audio_effect_chain_manager.h"
#include "playback_capturer_manager.h"
#include "policy_handler.h"
#include "config/audio_param_parser.h"

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
std::unordered_map<std::string, std::unordered_map<std::string, std::set<std::string>>> AudioServer::audioParameterKeys;
const string DEFAULT_COOKIE_PATH = "/data/data/.pulse_dir/state/cookie";
const unsigned int TIME_OUT_SECONDS = 10;
const unsigned int SCHEDULE_REPORT_TIME_OUT_SECONDS = 2;
static const std::vector<StreamUsage> STREAMS_NEED_VERIFY_SYSTEM_PERMISSION = {
    STREAM_USAGE_SYSTEM,
    STREAM_USAGE_DTMF,
    STREAM_USAGE_ENFORCED_TONE,
    STREAM_USAGE_ULTRASONIC,
    STREAM_USAGE_VOICE_MODEM_COMMUNICATION
};

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
    : SystemAbility(systemAbilityId, runOnCreate),
      audioEffectServer_(std::make_unique<AudioEffectServer>()) {}

void AudioServer::OnDump() {}

int32_t AudioServer::Dump(int32_t fd, const std::vector<std::u16string> &args)
{
    AUDIO_INFO_LOG("Dump Process Invoked");
    std::stringstream dumpStringStream;
    AudioService::GetInstance()->Dump(dumpStringStream);
    std::string dumpString = dumpStringStream.str();
    return write(fd, dumpString.c_str(), dumpString.size());
}

void AudioServer::OnStart()
{
    audioUid_ = getuid();
    AUDIO_INFO_LOG("OnStart uid:%{public}d", audioUid_);
    bool res = Publish(this);
    if (!res) {
        AUDIO_ERR_LOG("start err");
    }
    AddSystemAbilityListener(AUDIO_POLICY_SERVICE_ID);
#ifdef PA
    int32_t ret = pthread_create(&m_paDaemonThread, nullptr, AudioServer::paDaemonThread, nullptr);
    pthread_setname_np(m_paDaemonThread, "OS_PaDaemon");
    if (ret != 0) {
        AUDIO_ERR_LOG("pthread_create failed %d", ret);
    }
    AUDIO_DEBUG_LOG("Created paDaemonThread\n");
#endif

    RegisterAudioCapturerSourceCallback();

    std::unique_ptr<AudioParamParser> audioParamParser = make_unique<AudioParamParser>();
    CHECK_AND_RETURN_LOG(audioParamParser != nullptr, "Failed to create audio extra parameters parser");
    if (audioParamParser->LoadConfiguration(audioParameterKeys)) {
        AUDIO_INFO_LOG("Audio extra parameters load configuration successfully.");
    }
}

void AudioServer::OnAddSystemAbility(int32_t systemAbilityId, const std::string& deviceId)
{
    AUDIO_DEBUG_LOG("systemAbilityId:%{public}d", systemAbilityId);
    switch (systemAbilityId) {
        case AUDIO_POLICY_SERVICE_ID:
            AUDIO_INFO_LOG("input service start");
            RegisterPolicyServerDeathRecipient();
            break;
        default:
            AUDIO_ERR_LOG("unhandled sysabilityId:%{public}d", systemAbilityId);
            break;
    }
}

void AudioServer::OnStop()
{
    AUDIO_DEBUG_LOG("OnStop");
}

void AudioServer::SetExtraParameters(const std::string& key,
    const std::vector<std::pair<std::string, std::string>>& kvpairs)
{
    bool hasSystemPermission = PermissionUtil::VerifySystemPermission();
    bool hasSettingsPermission = VerifyClientPermission(MODIFY_AUDIO_SETTINGS_PERMISSION);
    CHECK_AND_RETURN_LOG(hasSystemPermission && hasSettingsPermission, "set extra parameters failed: no permission.");

    if (audioParameterKeys.empty()) {
        AUDIO_ERR_LOG("audio extra parameters mainKey and subKey is empty");
    }

    auto mainKeyIt = audioParameterKeys.find(key);
    if (mainKeyIt == audioParameterKeys.end()) {
        return;
    }

    std::unordered_map<std::string, std::set<std::string>> subKeyMap = mainKeyIt->second;
    std::string value;
    for (auto it = kvpairs.begin(); it != kvpairs.end(); it++) {
        auto subKeyIt = subKeyMap.find(it->first);
        if (subKeyIt != subKeyMap.end()) {
            value += it->first + "=" + it->second + ";";
        }
    }
    SetAudioParameter(key, value);
}

void AudioServer::SetAudioParameter(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lockSet(audioParameterMutex_);
    AudioXCollie audioXCollie("AudioServer::SetAudioParameter", TIME_OUT_SECONDS);
    AUDIO_DEBUG_LOG("server: set audio parameter");
    if (key != "AUDIO_EXT_PARAM_KEY_A2DP_OFFLOAD_CONFIG") {
        bool ret = VerifyClientPermission(MODIFY_AUDIO_SETTINGS_PERMISSION);
        CHECK_AND_RETURN_LOG(ret, "MODIFY_AUDIO_SETTINGS permission denied");
    } else {
        int32_t audio_policy_server_id = 1041;
        CHECK_AND_RETURN_LOG(IPCSkeleton::GetCallingUid() == audio_policy_server_id,
            "A2dp offload modify audio settings permission denied");
    }

    AudioServer::audioParameters[key] = value;

    // send it to hal
    AudioParamKey parmKey = AudioParamKey::NONE;
    if (key == "A2dpSuspended") {
        parmKey = AudioParamKey::A2DP_SUSPEND_STATE;
        IAudioRendererSink* bluetoothSinkInstance = IAudioRendererSink::GetInstance("a2dp", "");
        CHECK_AND_RETURN_LOG(bluetoothSinkInstance != nullptr, "has no valid sink");
        std::string renderValue = key + "=" + value + ";";
        bluetoothSinkInstance->SetAudioParameter(parmKey, "", renderValue);
        return;
    }

    IAudioRendererSink* audioRendererSinkInstance = IAudioRendererSink::GetInstance("primary", "");
    CHECK_AND_RETURN_LOG(audioRendererSinkInstance != nullptr, "has no valid sink");

    if (key == "AUDIO_EXT_PARAM_KEY_LOWPOWER") {
        parmKey = AudioParamKey::PARAM_KEY_LOWPOWER;
        HiSysEventWrite(HiviewDFX::HiSysEvent::Domain::AUDIO, "SMARTPA_LOWPOWER",
            HiviewDFX::HiSysEvent::EventType::BEHAVIOR, "STATE", value == "SmartPA_lowpower=on" ? 1 : 0);
    } else if (key == "bt_headset_nrec") {
        parmKey = AudioParamKey::BT_HEADSET_NREC;
    } else if (key == "bt_wbs") {
        parmKey = AudioParamKey::BT_WBS;
    } else if (key == "AUDIO_EXT_PARAM_KEY_A2DP_OFFLOAD_CONFIG") {
        parmKey = AudioParamKey::A2DP_OFFLOAD_STATE;
        std::string value_new = "a2dpOffloadConfig=" + value;
        audioRendererSinkInstance->SetAudioParameter(parmKey, "", value_new);
        return;
    } else if (key == "mmi") {
        parmKey = AudioParamKey::MMI;
    } else if (key == "perf_info") {
        parmKey = AudioParamKey::PERF_INFO;
    } else {
        AUDIO_ERR_LOG("key %{publbic}s is invalid for hdi interface", key.c_str());
        return;
    }
    audioRendererSinkInstance->SetAudioParameter(parmKey, "", value);
}

void AudioServer::SetAudioParameter(const std::string& networkId, const AudioParamKey key, const std::string& condition,
    const std::string& value)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    bool ret = VerifyClientPermission(ACCESS_NOTIFICATION_POLICY_PERMISSION);
    CHECK_AND_RETURN_LOG(callingUid == audioUid_ || ret, "SetAudioParameter refused for %{public}d", callingUid);
    IAudioRendererSink *audioRendererSinkInstance = IAudioRendererSink::GetInstance("remote", networkId.c_str());
    CHECK_AND_RETURN_LOG(audioRendererSinkInstance != nullptr, "has no valid sink");

    audioRendererSinkInstance->SetAudioParameter(key, condition, value);
}

const std::vector<std::pair<std::string, std::string>> AudioServer::GetExtraParameters(const std::string &mainKey,
    const std::vector<std::string> &subKeys)
{
    if (audioParameterKeys.empty()) {
        AUDIO_ERR_LOG("audio extra parameters mainKey and subKey is empty");
    }

    auto mainKeyIt = audioParameterKeys.find(mainKey);
    if (mainKeyIt == audioParameterKeys.end()) {
        return {};
    }

    std::unordered_map<std::string, std::set<std::string>> subKeyMap = mainKeyIt->second;
    std::vector<std::pair<std::string, std::string>> values;
    for (auto it = subKeys.begin(); it != subKeys.end(); it++) {
        auto subKeyIt = subKeyMap.find(*it);
        if (subKeyIt != subKeyMap.end()) {
            std::string value = GetAudioParameter(mainKey + "_" + *it);
            values.emplace_back(std::make_pair(*it, value));
        }
    }
    return values;
}

bool AudioServer::CheckAndPrintStacktrace(const std::string &key)
{
    AUDIO_WARNING_LOG("Start handle forced xcollie event for key %{public}s", key.c_str());
    if (key == "dump_pulseaudio_stacktrace") {
        AudioXCollie audioXCollie("AudioServer::PrintStackTrace", 1); // 1 means XCOLLIE_FLAG_LOG
        sleep(2); // sleep 2 seconds to dump stacktrace
        return true;
    } else if (key == "recovery_audio_server") {
        AudioXCollie audioXCollie("AudioServer::PrintStackTraceAndKill", 1, nullptr, nullptr, 2); // 2 means RECOVERY
        sleep(2); // sleep 2 seconds to dump stacktrace
        return true;
    }
    return false;
}

const std::string AudioServer::GetAudioParameter(const std::string &key)
{
    if (IPCSkeleton::GetCallingUid() == MEDIA_SERVICE_UID && CheckAndPrintStacktrace(key) == true) {
        return "";
    }
    std::lock_guard<std::mutex> lockSet(audioParameterMutex_);
    AudioXCollie audioXCollie("GetAudioParameter", TIME_OUT_SECONDS);
    AUDIO_DEBUG_LOG("server: get audio parameter");
    if (key == "get_usb_info") {
        IAudioRendererSink *usbAudioRendererSinkInstance = IAudioRendererSink::GetInstance("usb", "");
        IAudioCapturerSource *usbAudioCapturerSinkInstance = IAudioCapturerSource::GetInstance("usb", "");
        if (usbAudioRendererSinkInstance != nullptr && usbAudioCapturerSinkInstance != nullptr) {
            std::string usbInfoStr =
                usbAudioRendererSinkInstance->GetAudioParameter(AudioParamKey::USB_DEVICE, "get_usb_info");
            // Preload usb sink and source, make pa load module faster to avoid blocking client write
            usbAudioRendererSinkInstance->Preload(usbInfoStr);
            usbAudioCapturerSinkInstance->Preload(usbInfoStr);
            return usbInfoStr;
        }
    }
    IAudioRendererSink *audioRendererSinkInstance = IAudioRendererSink::GetInstance("primary", "");
    if (audioRendererSinkInstance != nullptr) {
        AudioParamKey parmKey = AudioParamKey::NONE;
        if (key == "AUDIO_EXT_PARAM_KEY_LOWPOWER") {
            parmKey = AudioParamKey::PARAM_KEY_LOWPOWER;
            return audioRendererSinkInstance->GetAudioParameter(AudioParamKey(parmKey), "");
        }
        if (key == "need_change_usb_device") {
            parmKey = AudioParamKey::USB_DEVICE;
            return audioRendererSinkInstance->GetAudioParameter(AudioParamKey(parmKey), "need_change_usb_device");
        }
        if (key == "getSmartPAPOWER" || key == "show_RealTime_ChipModel") {
            return audioRendererSinkInstance->GetAudioParameter(AudioParamKey::NONE, key);
        }

        const std::string mmiPre = "mmi_";
        if (key.size() > mmiPre.size()) {
            if (key.substr(0, mmiPre.size()) == mmiPre) {
                parmKey = AudioParamKey::MMI;
                return audioRendererSinkInstance->GetAudioParameter(AudioParamKey(parmKey),
                    key.substr(mmiPre.size(), key.size() - mmiPre.size()));
            }
        }
    }

    if (AudioServer::audioParameters.count(key)) {
        return AudioServer::audioParameters[key];
    } else {
        return "";
    }
}

const std::string AudioServer::GetAudioParameter(const std::string& networkId, const AudioParamKey key,
    const std::string& condition)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    bool ret = VerifyClientPermission(ACCESS_NOTIFICATION_POLICY_PERMISSION);
    CHECK_AND_RETURN_RET_LOG(callingUid == audioUid_ || ret, "",
        "GetAudioParameter refused for %{public}d", callingUid);
    IAudioRendererSink *audioRendererSinkInstance = IAudioRendererSink::GetInstance("remote", networkId.c_str());
    CHECK_AND_RETURN_RET_LOG(audioRendererSinkInstance != nullptr, "", "has no valid sink");
    return audioRendererSinkInstance->GetAudioParameter(key, condition);
}

uint64_t AudioServer::GetTransactionId(DeviceType deviceType, DeviceRole deviceRole)
{
    uint64_t transactionId = 0;
    AUDIO_DEBUG_LOG("device type: %{public}d, device role: %{public}d", deviceType, deviceRole);
    if (deviceRole != INPUT_DEVICE && deviceRole != OUTPUT_DEVICE) {
        AUDIO_ERR_LOG("AudioServer::GetTransactionId: error device role");
        return ERR_INVALID_PARAM;
    }
    if (deviceRole == INPUT_DEVICE) {
        AudioCapturerSource *audioCapturerSourceInstance;
        if (deviceType == DEVICE_TYPE_USB_ARM_HEADSET) {
            audioCapturerSourceInstance = AudioCapturerSource::GetInstance("usb");
        } else {
            audioCapturerSourceInstance = AudioCapturerSource::GetInstance("primary");
        }
        if (audioCapturerSourceInstance) {
            transactionId = audioCapturerSourceInstance->GetTransactionId();
        }
        AUDIO_INFO_LOG("Transaction Id: %{public}" PRIu64, transactionId);
        return transactionId;
    }

    // deviceRole OUTPUT_DEVICE
    IAudioRendererSink *iRendererInstance = nullptr;
    if (deviceType == DEVICE_TYPE_BLUETOOTH_A2DP) {
        iRendererInstance = IAudioRendererSink::GetInstance("a2dp", "");
    } else if (deviceType == DEVICE_TYPE_USB_ARM_HEADSET) {
        iRendererInstance = IAudioRendererSink::GetInstance("usb", "");
    } else {
        iRendererInstance = IAudioRendererSink::GetInstance("primary", "");
    }

    int32_t ret = ERROR;
    if (iRendererInstance != nullptr) {
        ret = iRendererInstance->GetTransactionId(&transactionId);
    }

    CHECK_AND_RETURN_RET_LOG(!ret, transactionId, "Get transactionId failed.");

    AUDIO_DEBUG_LOG("Transaction Id: %{public}" PRIu64, transactionId);
    return transactionId;
}

bool AudioServer::LoadAudioEffectLibraries(const std::vector<Library> libraries, const std::vector<Effect> effects,
    std::vector<Effect>& successEffectList)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_RET_LOG(callingUid == audioUid_ || callingUid == ROOT_UID, false,
        "LoadAudioEffectLibraries refused for %{public}d", callingUid);
    bool loadSuccess = audioEffectServer_->LoadAudioEffects(libraries, effects, successEffectList);
    if (!loadSuccess) {
        AUDIO_WARNING_LOG("Load audio effect failed, please check log");
    }
    return loadSuccess;
}

bool AudioServer::CreateEffectChainManager(std::vector<EffectChain> &effectChains,
    std::unordered_map<std::string, std::string> &map)
{
    int32_t audio_policy_server_id = 1041;
    if (IPCSkeleton::GetCallingUid() != audio_policy_server_id) {
        return false;
    }
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    audioEffectChainManager->InitAudioEffectChainManager(effectChains, map, audioEffectServer_->GetEffectEntries());
    return true;
}

bool AudioServer::SetOutputDeviceSink(int32_t deviceType, std::string &sinkName)
{
    Trace trace("AudioServer::SetOutputDeviceSink:" + std::to_string(deviceType) + " sink:" + sinkName);
    int32_t audio_policy_server_id = 1041;
    if (IPCSkeleton::GetCallingUid() != audio_policy_server_id) {
        return false;
    }
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    if (audioEffectChainManager->SetOutputDeviceSink(deviceType, sinkName) != SUCCESS) {
        return false;
    }
    return true;
}

int32_t AudioServer::SetMicrophoneMute(bool isMute)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_RET_LOG(callingUid == audioUid_ || callingUid == ROOT_UID,
        ERR_PERMISSION_DENIED, "SetMicrophoneMute refused for %{public}d", callingUid);

    std::vector<IAudioCapturerSource *> allSourcesInstance;
    IAudioCapturerSource::GetAllInstance(allSourcesInstance);
    for (auto it = allSourcesInstance.begin(); it != allSourcesInstance.end(); ++it) {
        (*it)->SetMute(isMute);
    }

    return SUCCESS;
}

int32_t AudioServer::SetVoiceVolume(float volume)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_RET_LOG(callingUid == audioUid_ || callingUid == ROOT_UID,
        ERR_NOT_SUPPORTED, "SetVoiceVolume refused for %{public}d", callingUid);
    IAudioRendererSink *audioRendererSinkInstance = IAudioRendererSink::GetInstance("primary", "");

    if (audioRendererSinkInstance == nullptr) {
        AUDIO_WARNING_LOG("Renderer is null.");
    } else {
        return audioRendererSinkInstance->SetVoiceVolume(volume);
    }
    return ERROR;
}

int32_t AudioServer::OffloadSetVolume(float volume)
{
    IAudioRendererSink *audioRendererSinkInstance = IAudioRendererSink::GetInstance("offload", "");

    if (audioRendererSinkInstance == nullptr) {
        AUDIO_ERR_LOG("Renderer is null.");
        return ERROR;
    }
    return audioRendererSinkInstance->SetVolume(volume, 0);
}

int32_t AudioServer::SetAudioScene(AudioScene audioScene, DeviceType activeDevice)
{
    std::lock_guard<std::mutex> lock(audioSceneMutex_);

    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_RET_LOG(callingUid == audioUid_ || callingUid == ROOT_UID,
        ERR_NOT_SUPPORTED, "UpdateActiveDeviceRoute refused for %{public}d", callingUid);
    AudioXCollie audioXCollie("AudioServer::SetAudioScene", TIME_OUT_SECONDS);
    AudioCapturerSource *audioCapturerSourceInstance;
    IAudioRendererSink *audioRendererSinkInstance;
    if (activeDevice == DEVICE_TYPE_USB_ARM_HEADSET) {
        audioCapturerSourceInstance = AudioCapturerSource::GetInstance("usb");
        audioRendererSinkInstance = IAudioRendererSink::GetInstance("usb", "");
    } else {
        audioCapturerSourceInstance = AudioCapturerSource::GetInstance("primary");
        audioRendererSinkInstance = IAudioRendererSink::GetInstance("primary", "");
    }

    if (audioCapturerSourceInstance == nullptr || !audioCapturerSourceInstance->IsInited()) {
        AUDIO_WARNING_LOG("Capturer is not initialized.");
    } else {
        audioCapturerSourceInstance->SetAudioScene(audioScene, activeDevice);
    }

    if (audioRendererSinkInstance == nullptr || !audioRendererSinkInstance->IsInited()) {
        AUDIO_WARNING_LOG("Renderer is not initialized.");
    } else {
        audioRendererSinkInstance->SetAudioScene(audioScene, activeDevice);
    }

    audioScene_ = audioScene;
    return SUCCESS;
}

int32_t AudioServer::SetIORoute(DeviceType type, DeviceFlag flag)
{
    AUDIO_INFO_LOG("SetIORoute deviceType: %{public}d, flag: %{public}d", type, flag);
    AudioCapturerSource *audioCapturerSourceInstance;
    IAudioRendererSink *audioRendererSinkInstance;
    if (type == DEVICE_TYPE_USB_ARM_HEADSET) {
        audioCapturerSourceInstance = AudioCapturerSource::GetInstance("usb");
        audioRendererSinkInstance = IAudioRendererSink::GetInstance("usb", "");
    } else {
        audioCapturerSourceInstance = AudioCapturerSource::GetInstance("primary");
        audioRendererSinkInstance = IAudioRendererSink::GetInstance("primary", "");
    }
    CHECK_AND_RETURN_RET_LOG(audioCapturerSourceInstance != nullptr && audioRendererSinkInstance != nullptr,
        ERR_INVALID_PARAM, "SetIORoute failed for null instance!");

    std::lock_guard<std::mutex> lock(audioSceneMutex_);
    if (flag == DeviceFlag::INPUT_DEVICES_FLAG) {
        if (audioScene_ != AUDIO_SCENE_DEFAULT) {
            audioCapturerSourceInstance->SetAudioScene(audioScene_, type);
        } else {
            audioCapturerSourceInstance->SetInputRoute(type);
        }
    } else if (flag == DeviceFlag::OUTPUT_DEVICES_FLAG) {
        if (audioScene_ != AUDIO_SCENE_DEFAULT) {
            audioRendererSinkInstance->SetAudioScene(audioScene_, type);
        } else {
            audioRendererSinkInstance->SetOutputRoute(type);
        }
        PolicyHandler::GetInstance().SetActiveOutputDevice(type);
    } else if (flag == DeviceFlag::ALL_DEVICES_FLAG) {
        if (audioScene_ != AUDIO_SCENE_DEFAULT) {
            audioCapturerSourceInstance->SetAudioScene(audioScene_, type);
            audioRendererSinkInstance->SetAudioScene(audioScene_, type);
        } else {
            audioCapturerSourceInstance->SetInputRoute(type);
            audioRendererSinkInstance->SetOutputRoute(type);
        }
        PolicyHandler::GetInstance().SetActiveOutputDevice(type);
    } else {
        AUDIO_ERR_LOG("SetIORoute invalid device flag");
        return ERR_INVALID_PARAM;
    }

    return SUCCESS;
}

int32_t AudioServer::UpdateActiveDeviceRoute(DeviceType type, DeviceFlag flag)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_RET_LOG(callingUid == audioUid_ || callingUid == ROOT_UID,
        ERR_NOT_SUPPORTED, "UpdateActiveDeviceRoute refused for %{public}d", callingUid);

    return SetIORoute(type, flag);
}

void AudioServer::SetAudioMonoState(bool audioMono)
{
    AUDIO_INFO_LOG("audioMono = %{public}s", audioMono? "true": "false");
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_LOG(callingUid == audioUid_ || callingUid == ROOT_UID,
        "NotifyDeviceInfo refused for %{public}d", callingUid);
    // Set mono for audio_renderer_sink (primary)
    IAudioRendererSink *audioRendererSinkInstance = IAudioRendererSink::GetInstance("primary", "");
    if (audioRendererSinkInstance != nullptr) {
        audioRendererSinkInstance->SetAudioMonoState(audioMono);
    } else {
        AUDIO_ERR_LOG("AudioServer::SetAudioBalanceValue: primary = null");
    }

    // Set mono for bluetooth_renderer_sink (a2dp)
    IAudioRendererSink *a2dpIAudioRendererSink = IAudioRendererSink::GetInstance("a2dp", "");
    if (a2dpIAudioRendererSink != nullptr) {
        a2dpIAudioRendererSink->SetAudioMonoState(audioMono);
    } else {
        AUDIO_ERR_LOG("AudioServer::SetAudioBalanceValue: a2dp = null");
    }
}

void AudioServer::SetAudioBalanceValue(float audioBalance)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_LOG(callingUid == audioUid_ || callingUid == ROOT_UID,
        "NotifyDeviceInfo refused for %{public}d", callingUid);
    CHECK_AND_RETURN_LOG(audioBalance >= -1.0f && audioBalance <= 1.0f,
        "audioBalance value %{public}f is out of range [-1.0, 1.0]", audioBalance);
    AUDIO_INFO_LOG("audioBalance = %{public}f", audioBalance);

    // Set balance for audio_renderer_sink (primary)
    IAudioRendererSink *audioRendererSinkInstance = IAudioRendererSink::GetInstance("primary", "");
    if (audioRendererSinkInstance != nullptr) {
        audioRendererSinkInstance->SetAudioBalanceValue(audioBalance);
    } else {
        AUDIO_WARNING_LOG("primary = null");
    }

    // Set balance for bluetooth_renderer_sink (a2dp)
    IAudioRendererSink *a2dpIAudioRendererSink = IAudioRendererSink::GetInstance("a2dp", "");
    if (a2dpIAudioRendererSink != nullptr) {
        a2dpIAudioRendererSink->SetAudioBalanceValue(audioBalance);
    } else {
        AUDIO_WARNING_LOG("a2dp = null");
    }
}

void AudioServer::NotifyDeviceInfo(std::string networkId, bool connected)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_LOG(callingUid == audioUid_ || callingUid == ROOT_UID,
        "NotifyDeviceInfo refused for %{public}d", callingUid);
    AUDIO_INFO_LOG("notify device info: networkId(%{public}s), connected(%{public}d)", networkId.c_str(), connected);
    IAudioRendererSink* audioRendererSinkInstance = IAudioRendererSink::GetInstance("remote", networkId.c_str());
    if (audioRendererSinkInstance != nullptr && connected) {
        audioRendererSinkInstance->RegisterParameterCallback(this);
    }
}

inline bool IsParamEnabled(std::string key, bool &isEnabled)
{
    int32_t policyFlag = 0;
    if (GetSysPara(key.c_str(), policyFlag) && policyFlag == 1) {
        isEnabled = true;
        return true;
    }
    isEnabled = false;
    return false;
}

int32_t AudioServer::RegiestPolicyProvider(const sptr<IRemoteObject> &object)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_RET_LOG(callingUid == audioUid_ || callingUid == ROOT_UID, ERR_NOT_SUPPORTED,
        "RegiestPolicyProvider refused for %{public}d", callingUid);
    sptr<IPolicyProviderIpc> policyProvider = iface_cast<IPolicyProviderIpc>(object);
    CHECK_AND_RETURN_RET_LOG(policyProvider != nullptr, ERR_INVALID_PARAM,
        "policyProvider obj cast failed");
    bool ret = PolicyHandler::GetInstance().ConfigPolicyProvider(policyProvider);
    CHECK_AND_RETURN_RET_LOG(ret, ERR_OPERATION_FAILED, "ConfigPolicyProvider failed!");
    return SUCCESS;
}

sptr<IRemoteObject> AudioServer::CreateAudioProcess(const AudioProcessConfig &config)
{
    bool ret = IsParamEnabled("persist.multimedia.audio.mmap.enable", isGetProcessEnabled_);
    CHECK_AND_RETURN_RET_LOG(ret, nullptr, "CreateAudioProcess is not enabled!");

    // client pid uid check.
    int32_t callerUid = IPCSkeleton::GetCallingUid();
    int32_t callerPid = IPCSkeleton::GetCallingPid();
    AUDIO_DEBUG_LOG("Create process for uid:%{public}d pid:%{public}d", callerUid, callerPid);

    AudioProcessConfig resetConfig(config);
    if (callerUid == MEDIA_SERVICE_UID) {
        AUDIO_INFO_LOG("Create process for media service.");
    } else if (resetConfig.appInfo.appPid != callerPid || resetConfig.appInfo.appUid != callerUid ||
        resetConfig.appInfo.appTokenId != IPCSkeleton::GetCallingTokenID()) {
        AUDIO_INFO_LOG("Use true client appInfo instead.");
        resetConfig.appInfo.appPid = callerPid;
        resetConfig.appInfo.appUid = callerUid;
        resetConfig.appInfo.appTokenId = IPCSkeleton::GetCallingTokenID();
    }

    CHECK_AND_RETURN_RET_LOG(PermissionChecker(resetConfig), nullptr, "Create audio process failed, no permission");

    if ((resetConfig.audioMode == AUDIO_MODE_PLAYBACK && resetConfig.rendererInfo.rendererFlags == 0) ||
        (resetConfig.audioMode == AUDIO_MODE_RECORD && resetConfig.capturerInfo.capturerFlags == 0)) {
        AUDIO_INFO_LOG("Create normal ipc stream.");
        int32_t ret = 0;
        sptr<IpcStreamInServer> ipcStream = AudioService::GetInstance()->GetIpcStream(resetConfig, ret);
        CHECK_AND_RETURN_RET_LOG(ipcStream != nullptr, nullptr, "GetIpcStream failed.");
        sptr<IRemoteObject> remoteObject= ipcStream->AsObject();
        return remoteObject;
    }

    sptr<IAudioProcess> process = AudioService::GetInstance()->GetAudioProcess(resetConfig);
    CHECK_AND_RETURN_RET_LOG(process != nullptr, nullptr, "GetAudioProcess failed.");
    sptr<IRemoteObject> remoteObject= process->AsObject();
    return remoteObject;
}

int32_t AudioServer::CheckRemoteDeviceState(std::string networkId, DeviceRole deviceRole, bool isStartDevice)
{
    AUDIO_INFO_LOG("CheckRemoteDeviceState: device[%{public}s] deviceRole[%{public}d] isStartDevice[%{public}s]",
        networkId.c_str(), static_cast<int32_t>(deviceRole), (isStartDevice ? "true" : "false"));

    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_RET_LOG(callingUid == audioUid_ || callingUid == ROOT_UID,
        ERR_NOT_SUPPORTED, "CheckRemoteDeviceState refused for %{public}d", callingUid);
    CHECK_AND_RETURN_RET(isStartDevice, SUCCESS);

    int32_t ret = SUCCESS;
    switch (deviceRole) {
        case OUTPUT_DEVICE:
            {
                IAudioRendererSink* rendererInstance = IAudioRendererSink::GetInstance("remote", networkId.c_str());
                if (rendererInstance == nullptr || !rendererInstance->IsInited()) {
                    AUDIO_ERR_LOG("Remote renderer[%{public}s] is uninit.", networkId.c_str());
                    return ERR_ILLEGAL_STATE;
                }
                ret = rendererInstance->Start();
                break;
            }
        case INPUT_DEVICE:
            {
                IAudioCapturerSource *capturerInstance = IAudioCapturerSource::GetInstance("remote", networkId.c_str());
                if (capturerInstance == nullptr || !capturerInstance->IsInited()) {
                    AUDIO_ERR_LOG("Remote capturer[%{public}s] is uninit.", networkId.c_str());
                    return ERR_ILLEGAL_STATE;
                }
                ret = capturerInstance->Start();
                break;
            }
        default:
            AUDIO_ERR_LOG("Remote device role %{public}d is not supported.", deviceRole);
            return ERR_NOT_SUPPORTED;
    }
    if (ret != SUCCESS) {
        AUDIO_ERR_LOG("Check remote device[%{public}s] fail, ret %{public}d.", networkId.c_str(), ret);
    }
    return ret;
}

void AudioServer::OnAudioSinkParamChange(const std::string &netWorkId, const AudioParamKey key,
    const std::string &condition, const std::string &value)
{
    std::shared_ptr<AudioParameterCallback> callback = nullptr;
    {
        std::lock_guard<std::mutex> lockSet(audioParamCbMtx_);
        AUDIO_INFO_LOG("OnAudioSinkParamChange Callback from networkId: %s", netWorkId.c_str());
        CHECK_AND_RETURN_LOG(audioParamCb_ != nullptr, "OnAudioSinkParamChange: audio param allback is null.");
        callback = audioParamCb_;
    }
    callback->OnAudioParameterChange(netWorkId, key, condition, value);
}

void AudioServer::OnAudioSourceParamChange(const std::string &netWorkId, const AudioParamKey key,
    const std::string &condition, const std::string &value)
{
    std::shared_ptr<AudioParameterCallback> callback = nullptr;
    {
        std::lock_guard<std::mutex> lockSet(audioParamCbMtx_);
        AUDIO_INFO_LOG("OnAudioSourceParamChange Callback from networkId: %s", netWorkId.c_str());
        CHECK_AND_RETURN_LOG(audioParamCb_ != nullptr, "OnAudioSourceParamChange: audio param allback is null.");
        callback = audioParamCb_;
    }
    callback->OnAudioParameterChange(netWorkId, key, condition, value);
}

void AudioServer::OnWakeupClose()
{
    AUDIO_INFO_LOG("OnWakeupClose Callback start");
    std::shared_ptr<WakeUpSourceCallback> callback = nullptr;
    {
        std::lock_guard<std::mutex> lockSet(setWakeupCloseCallbackMutex_);
        CHECK_AND_RETURN_LOG(wakeupCallback_ != nullptr, "OnWakeupClose callback is nullptr.");
        callback = wakeupCallback_;
    }
    callback->OnWakeupClose();
}

void AudioServer::OnCapturerState(bool isActive)
{
    AUDIO_DEBUG_LOG("OnCapturerState Callback start");
    std::shared_ptr<WakeUpSourceCallback> callback = nullptr;
    {
        std::lock_guard<std::mutex> lockSet(setWakeupCloseCallbackMutex_);
        isAudioCapturerSourcePrimaryStarted_ = isActive;
        CHECK_AND_RETURN_LOG(wakeupCallback_ != nullptr, "OnCapturerState callback is nullptr.");
        callback = wakeupCallback_;
    }
    callback->OnCapturerState(isActive);
}

int32_t AudioServer::SetParameterCallback(const sptr<IRemoteObject>& object)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_RET_LOG(callingUid == audioUid_ || callingUid == ROOT_UID,
        ERR_NOT_SUPPORTED, "SetParameterCallback refused for %{public}d", callingUid);
    std::lock_guard<std::mutex> lock(audioParamCbMtx_);
    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_INVALID_PARAM, "AudioServer:set listener object is nullptr");

    sptr<IStandardAudioServerManagerListener> listener = iface_cast<IStandardAudioServerManagerListener>(object);

    CHECK_AND_RETURN_RET_LOG(listener != nullptr, ERR_INVALID_PARAM, "AudioServer: listener obj cast failed");

    std::shared_ptr<AudioParameterCallback> callback = std::make_shared<AudioManagerListenerCallback>(listener);
    CHECK_AND_RETURN_RET_LOG(callback != nullptr, ERR_INVALID_PARAM, "AudioPolicyServer: failed to  create cb obj");

    audioParamCb_ = callback;
    AUDIO_INFO_LOG("AudioServer:: SetParameterCallback  done");

    return SUCCESS;
}

int32_t AudioServer::SetWakeupSourceCallback(const sptr<IRemoteObject>& object)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_RET_LOG(callingUid == INTELL_VOICE_SERVICR_UID, false,
        "SetWakeupSourceCallback refused for %{public}d", callingUid);

    CHECK_AND_RETURN_RET_LOG(object != nullptr, ERR_INVALID_PARAM,
        "SetWakeupCloseCallback set listener object is nullptr");

    sptr<IStandardAudioServerManagerListener> listener = iface_cast<IStandardAudioServerManagerListener>(object);

    CHECK_AND_RETURN_RET_LOG(listener != nullptr, ERR_INVALID_PARAM,
        "SetWakeupCloseCallback listener obj cast failed");

    std::shared_ptr<WakeUpSourceCallback> wakeupCallback = std::make_shared<AudioManagerListenerCallback>(listener);
    CHECK_AND_RETURN_RET_LOG(wakeupCallback != nullptr, ERR_INVALID_PARAM,
        "SetWakeupCloseCallback failed to create cb obj");

    {
        std::lock_guard<std::mutex> lockSet(setWakeupCloseCallbackMutex_);
        wakeupCallback_ = wakeupCallback;

        wakeupCallback->OnCapturerState(isAudioCapturerSourcePrimaryStarted_);
    }

    AUDIO_INFO_LOG("SetWakeupCloseCallback done");

    return SUCCESS;
}

bool AudioServer::VerifyClientPermission(const std::string &permissionName,
    Security::AccessToken::AccessTokenID tokenId)
{
    auto callerUid = IPCSkeleton::GetCallingUid();
    AUDIO_INFO_LOG("AudioServer: ==[%{public}s] [uid:%{public}d]==", permissionName.c_str(), callerUid);

    // Root users should be whitelisted
    if (callerUid == ROOT_UID) {
        AUDIO_INFO_LOG("Root user. Permission GRANTED!!!");
        return true;
    }
    Security::AccessToken::AccessTokenID clientTokenId = tokenId;
    if (clientTokenId == Security::AccessToken::INVALID_TOKENID) {
        clientTokenId = IPCSkeleton::GetCallingTokenID();
    }
    int res = Security::AccessToken::AccessTokenKit::VerifyAccessToken(clientTokenId, permissionName);
    CHECK_AND_RETURN_RET_LOG(res == Security::AccessToken::PermissionState::PERMISSION_GRANTED,
        false, "Permission denied [tid:%{public}d]", clientTokenId);

    return true;
}

bool AudioServer::PermissionChecker(const AudioProcessConfig &config)
{
    if (config.audioMode == AUDIO_MODE_PLAYBACK) {
        return CheckPlaybackPermission(config.appInfo.appTokenId, config.rendererInfo.streamUsage);
    } else {
        return CheckRecorderPermission(config.appInfo.appTokenId, config.capturerInfo.sourceType);
    }
}

bool AudioServer::CheckPlaybackPermission(Security::AccessToken::AccessTokenID tokenId, const StreamUsage streamUsage)
{
    bool needVerifyPermission = false;
    for (const auto& item : STREAMS_NEED_VERIFY_SYSTEM_PERMISSION) {
        if (streamUsage == item) {
            needVerifyPermission = true;
            break;
        }
    }
    if (needVerifyPermission == false) {
        return true;
    }
    CHECK_AND_RETURN_RET_LOG(PermissionUtil::VerifySystemPermission(), false,
        "Check playback permission failed, no system permission");
    return true;
}

bool AudioServer::CheckRecorderPermission(Security::AccessToken::AccessTokenID tokenId, const SourceType sourceType)
{
    // All record streams should be checked for MICROPHONE_PERMISSION
    bool res = VerifyClientPermission(MICROPHONE_PERMISSION, tokenId);
    CHECK_AND_RETURN_RET_LOG(res, false, "Check record permission failed: No permission.");

    if (sourceType == SOURCE_TYPE_WAKEUP) {
        bool hasSystemPermission = PermissionUtil::VerifySystemPermission();
        bool hasIntelVoicePermission = VerifyClientPermission(MANAGE_INTELLIGENT_VOICE_PERMISSION, tokenId);
        CHECK_AND_RETURN_RET_LOG(hasSystemPermission && hasIntelVoicePermission, false,
            "Create wakeup record stream failed: no permission.");
    }
    return true;
}

int32_t AudioServer::OffloadDrain()
{
    auto *audioRendererSinkInstance = static_cast<IOffloadAudioRendererSink*> (IAudioRendererSink::GetInstance(
        "offload", ""));

    CHECK_AND_RETURN_RET_LOG(audioRendererSinkInstance != nullptr, ERROR, "Renderer is null.");
    return audioRendererSinkInstance->Drain(AUDIO_DRAIN_EARLY_NOTIFY);
}

int32_t AudioServer::GetCapturePresentationPosition(const std::string& deviceClass, uint64_t& frames, int64_t& timeSec,
    int64_t& timeNanoSec)
{
    AudioCapturerSource *audioCapturerSourceInstance = AudioCapturerSource::GetInstance("primary");
    if (audioCapturerSourceInstance == nullptr) {
        AUDIO_ERR_LOG("Capturer is null.");
        return ERROR;
    }
    return audioCapturerSourceInstance->GetPresentationPosition(frames, timeSec, timeNanoSec);
}

int32_t AudioServer::GetRenderPresentationPosition(const std::string& deviceClass, uint64_t& frames, int64_t& timeSec,
    int64_t& timeNanoSec)
{
    IAudioRendererSink *audioRendererSinkInstance = IAudioRendererSink::GetInstance(deviceClass.c_str(), "");
    if (audioRendererSinkInstance == nullptr) {
        AUDIO_ERR_LOG("Renderer is null.");
        return ERROR;
    }
    return audioRendererSinkInstance->GetPresentationPosition(frames, timeSec, timeNanoSec);
}

int32_t AudioServer::OffloadGetPresentationPosition(uint64_t& frames, int64_t& timeSec, int64_t& timeNanoSec)
{
    auto *audioRendererSinkInstance = static_cast<IOffloadAudioRendererSink*> (IAudioRendererSink::GetInstance(
        "offload", ""));

    CHECK_AND_RETURN_RET_LOG(audioRendererSinkInstance != nullptr, ERROR, "Renderer is null.");
    return audioRendererSinkInstance->GetPresentationPosition(frames, timeSec, timeNanoSec);
}

int32_t AudioServer::OffloadSetBufferSize(uint32_t sizeMs)
{
    auto *audioRendererSinkInstance = static_cast<IOffloadAudioRendererSink*> (IAudioRendererSink::GetInstance(
        "offload", ""));

    CHECK_AND_RETURN_RET_LOG(audioRendererSinkInstance != nullptr, ERROR, "Renderer is null.");
    return audioRendererSinkInstance->SetBufferSize(sizeMs);
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

void AudioServer::RequestThreadPriority(uint32_t tid, string bundleName)
{
    AUDIO_INFO_LOG("RequestThreadPriority tid: %{public}u", tid);

    uint32_t pid = IPCSkeleton::GetCallingPid();
    AudioXCollie audioXCollie("AudioServer::ScheduleReportData", SCHEDULE_REPORT_TIME_OUT_SECONDS);
    ScheduleReportData(pid, tid, bundleName.c_str());
}

bool AudioServer::CreatePlaybackCapturerManager()
{
    int32_t audio_policy_server_id = 1041;
    if (IPCSkeleton::GetCallingUid() != audio_policy_server_id) {
        return false;
    }
    std::vector<int32_t> usage;
    PlaybackCapturerManager *playbackCapturerMgr = PlaybackCapturerManager::GetInstance();
    playbackCapturerMgr->SetSupportStreamUsage(usage);
    return true;
}

int32_t AudioServer::SetSupportStreamUsage(std::vector<int32_t> usage)
{
    AUDIO_INFO_LOG("SetSupportStreamUsage with usage num:%{public}zu", usage.size());

    int32_t audio_policy_server_id = 1041;
    if (IPCSkeleton::GetCallingUid() != audio_policy_server_id) {
        return ERR_OPERATION_FAILED;
    }
    PlaybackCapturerManager *playbackCapturerMgr = PlaybackCapturerManager::GetInstance();
    playbackCapturerMgr->SetSupportStreamUsage(usage);
    return SUCCESS;
}

void AudioServer::RegisterAudioCapturerSourceCallback()
{
    IAudioCapturerSource* audioCapturerSourceWakeupInstance =
        IAudioCapturerSource::GetInstance("primary", nullptr, SOURCE_TYPE_WAKEUP);
    if (audioCapturerSourceWakeupInstance != nullptr) {
        audioCapturerSourceWakeupInstance->RegisterWakeupCloseCallback(this);
    }

    IAudioCapturerSource* audioCapturerSourceInstance =
        IAudioCapturerSource::GetInstance("primary", nullptr, SOURCE_TYPE_MIC);
    if (audioCapturerSourceInstance != nullptr) {
        audioCapturerSourceInstance->RegisterAudioCapturerSourceCallback(this);
    }
}

int32_t AudioServer::SetCaptureSilentState(bool state)
{
    int32_t audio_policy_server_id = 1041;
    if (IPCSkeleton::GetCallingUid() != audio_policy_server_id) {
        return ERR_OPERATION_FAILED;
    }

    PlaybackCapturerManager *playbackCapturerMgr = PlaybackCapturerManager::GetInstance();
    playbackCapturerMgr->SetCaptureSilentState(state);
    return SUCCESS;
}

int32_t AudioServer::UpdateSpatializationState(AudioSpatializationState spatializationState)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    CHECK_AND_RETURN_RET_LOG(callingUid == audioUid_ || callingUid == ROOT_UID,
        ERR_NOT_SUPPORTED, "UpdateSpatializationState refused for %{public}d", callingUid);
    AudioEffectChainManager *audioEffectChainManager = AudioEffectChainManager::GetInstance();
    if (audioEffectChainManager == nullptr) {
        AUDIO_ERR_LOG("audioEffectChainManager is nullptr");
        return ERROR;
    }
    return audioEffectChainManager->UpdateSpatializationState(spatializationState);
}

int32_t AudioServer::NotifyStreamVolumeChanged(AudioStreamType streamType, float volume)
{
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    if (callingUid != audioUid_) {
        AUDIO_ERR_LOG("NotifyStreamVolumeChanged refused for %{public}d", callingUid);
        return ERR_NOT_SUPPORTED;
    }
    return AudioService::GetInstance()->NotifyStreamVolumeChanged(streamType, volume);
}
} // namespace AudioStandard
} // namespace OHOS
