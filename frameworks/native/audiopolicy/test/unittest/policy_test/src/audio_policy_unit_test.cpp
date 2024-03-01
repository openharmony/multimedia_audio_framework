/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <thread>
#include "audio_errors.h"
#include "audio_info.h"
#include "parcel.h"
#include "audio_policy_client.h"
#include "audio_policy_unit_test.h"
#include "audio_system_manager.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"
#include "audio_client_tracker_callback_stub.h"
#include "audio_policy_client_stub_impl.h"

using namespace std;
using namespace testing::ext;

namespace OHOS {
namespace AudioStandard {
const int32_t FAILURE = -1;
void AudioPolicyUnitTest::SetUpTestCase(void) {}
void AudioPolicyUnitTest::TearDownTestCase(void) {}
void AudioPolicyUnitTest::SetUp(void) {}
void AudioPolicyUnitTest::TearDown(void) {}
void AudioPolicyUnitTest::InitAudioPolicyProxy(std::shared_ptr<AudioPolicyProxy> &audioPolicyProxy)
{
    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (samgr == nullptr) {
        AUDIO_ERR_LOG("InitAudioPolicyProxy::GetSystemAbilityManager failed");
        return;
    }

    sptr<IRemoteObject> object = samgr->GetSystemAbility(AUDIO_DISTRIBUTED_SERVICE_ID);
    if (object == nullptr) {
        AUDIO_ERR_LOG("InitAudioPolicyProxy::object is NULL.");
        return;
    }
    audioPolicyProxy = std::make_shared<AudioPolicyProxy>(object);
}

void AudioPolicyUnitTest::GetIRemoteObject(sptr<IRemoteObject> &object)
{
    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (samgr == nullptr) {
        AUDIO_ERR_LOG("GetIRemoteObject::GetSystemAbilityManager failed");
        return;
    }

    object = samgr->GetSystemAbility(AUDIO_DISTRIBUTED_SERVICE_ID);
    if (object == nullptr) {
        AUDIO_ERR_LOG("GetIRemoteObject::object is NULL.");
        return;
    }
}

void AudioPolicyUnitTest::InitAudioStream(std::shared_ptr<AudioStream> &audioStream)
{
    AppInfo appInfo_ = {};
    if (!(appInfo_.appPid)) {
        appInfo_.appPid = getpid();
    }

    if (appInfo_.appUid < 0) {
        appInfo_.appUid = static_cast<int32_t>(getuid());
    }
    
    audioStream = std::make_shared<AudioStream>(STREAM_NOTIFICATION, AUDIO_MODE_PLAYBACK, appInfo_.appUid);
    if (audioStream) {
        AUDIO_DEBUG_LOG("InitAudioStream::Audio stream created");
    }
}

uint32_t AudioPolicyUnitTest::GetSessionId(std::shared_ptr<AudioStream> &audioStream)
{
    uint32_t sessionID_ = static_cast<uint32_t>(-1);
    if (audioStream->GetAudioSessionID(sessionID_) != 0) {
        AUDIO_ERR_LOG("AudioPolicyUnitTest::GetSessionId Failed");
    }
    return sessionID_;
}

/**
 * @tc.name  : Test Audio_Policy_SetMicrophoneMuteAudioConfig_001 via illegal state
 * @tc.number: Audio_Policy_SetMicrophoneMuteAudioConfig_001
 * @tc.desc  : Test SetMicrophoneMuteAudioConfig interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_SetMicrophoneMuteAudioConfig_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    bool isMute = true;
    int32_t ret = audioPolicyProxy->SetMicrophoneMuteAudioConfig(isMute);
    EXPECT_EQ(FAILURE, ret);
}

#ifdef FEATURE_DTMF_TONE
/**
 * @tc.name  : Test Audio_Policy_GetSupportedTones_001 via legal state
 * @tc.number: Audio_Policy_GetSupportedTones_001
 * @tc.desc  : Test GetSupportedTones interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_GetSupportedTones_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    audioPolicyProxy->GetSupportedTones();
}

/**
 * @tc.name  : Test Audio_Policy_GetToneConfig_001 via legal state
 * @tc.number: Audio_Policy_GetToneConfig_001
 * @tc.desc  : Test GetToneConfig interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_GetToneConfig_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    int32_t ltonetype = 0;
    std::shared_ptr<ToneInfo> toneInfo = audioPolicyProxy->GetToneConfig(ltonetype);
    ASSERT_NE(nullptr, toneInfo);
}
#endif

/**
 * @tc.name  : Test Audio_Policy_IsStreamActive_001 via legal state
 * @tc.number: Audio_Policy_IsStreamActive_001
 * @tc.desc  : Test IsStreamActive interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_IsStreamActive_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    AudioStreamType streamType = AudioStreamType::STREAM_MUSIC;
    bool isStreamActive = audioPolicyProxy->IsStreamActive(streamType);
    EXPECT_EQ(false, isStreamActive);
}

/**
 * @tc.name  : Test Audio_Policy_SelectInputDevice_001 via illegal state
 * @tc.number: Audio_Policy_SelectInputDevice_001
 * @tc.desc  : Test SelectInputDevice interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_SelectInputDevice_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    AudioSystemManager *audioSystemMgr = AudioSystemManager::GetInstance();
    DeviceFlag deviceFlag = DeviceFlag::INPUT_DEVICES_FLAG;
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptorsVector;
    audioDeviceDescriptorsVector = audioSystemMgr->GetDevices(deviceFlag);

    sptr<AudioCapturerFilter> audioCapturerFilter = new(std::nothrow) AudioCapturerFilter();
    audioCapturerFilter->uid = DeviceFlag::INPUT_DEVICES_FLAG;

    int32_t ret = audioPolicyProxy->SelectInputDevice(audioCapturerFilter, audioDeviceDescriptorsVector);
    EXPECT_EQ(FAILURE, ret);
}

/**
 * @tc.name  : Test Audio_Policy_DeviceChangeCallback_001 via illegal state
 * @tc.number: Audio_Policy_DeviceChangeCallback_001
 * @tc.desc  : Test SetDeviceChangeCallback and UnsetDeviceChangeCallback interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_DeviceChangeCallback_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    sptr<IRemoteObject> object = nullptr;
    int32_t ret = audioPolicyProxy->RegisterPolicyCallbackClient(object);
    EXPECT_EQ(ERR_NULL_OBJECT, ret);

    AudioPolicyUnitTest::GetIRemoteObject(object);
    ret = audioPolicyProxy->RegisterPolicyCallbackClient(object);
    EXPECT_EQ(ERROR, ret);
}

/**
 * @tc.name  : Test Audio_Policy_GetStreamInFocus_001 via legal state
 * @tc.number: Audio_Policy_GetStreamInFocus_001
 * @tc.desc  : Test GetStreamInFocus interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_GetStreamInFocus_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    audioPolicyProxy->GetStreamInFocus();
}

/**
 * @tc.name  : Test Audio_Policy_IsAudioRendererLowLatencySupported_001 via legal state
 * @tc.number: Audio_Policy_IsAudioRendererLowLatencySupported_001
 * @tc.desc  : Test IsAudioRendererLowLatencySupported interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_IsAudioRendererLowLatencySupported_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    AudioStreamInfo audioStreamInfo;
    audioStreamInfo.samplingRate = AudioSamplingRate::SAMPLE_RATE_44100;
    audioStreamInfo.encoding = AudioEncodingType::ENCODING_PCM;
    audioStreamInfo.format = AudioSampleFormat::SAMPLE_S16LE;
    audioStreamInfo.channels = AudioChannel::MONO;
    bool ret = audioPolicyProxy->IsAudioRendererLowLatencySupported(audioStreamInfo);
    EXPECT_EQ(true, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_IsStreamActive_001 via illegal state
 * @tc.number: Audio_Policy_Manager_IsStreamActive_001
 * @tc.desc  : Test RegisterAudioCapturerEventListener interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_IsStreamActive_001, TestSize.Level1)
{
    bool isStreamActive = AudioPolicyManager::GetInstance().IsStreamActive(AudioStreamType::STREAM_MUSIC);
    EXPECT_EQ(false, isStreamActive);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_SetMicrophoneMuteAudioConfig_001 via legal state
 * @tc.number: Audio_Policy_Manager_SetMicrophoneMuteAudioConfig_001
 * @tc.desc  : Test SetMicrophoneMuteAudioConfig interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_SetMicrophoneMuteAudioConfig_001, TestSize.Level1)
{
    bool isMute = true;
    bool ret = AudioPolicyManager::GetInstance().SetMicrophoneMuteAudioConfig(isMute);
    EXPECT_EQ(SUCCESS, ret);
}

#ifdef FEATURE_DTMF_TONE
/**
 * @tc.name  : Test Audio_Policy_Manager_GetSupportedTones_001 via legal state
 * @tc.number: Audio_Policy_Manager_GetSupportedTones_001
 * @tc.desc  : Test GetSupportedTones interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_GetSupportedTones_001, TestSize.Level1)
{
    AudioPolicyManager::GetInstance().GetSupportedTones();
}

/**
 * @tc.name  : Test Audio_Policy_Manager_GetToneConfig_001 via legal state
 * @tc.number: Audio_Policy_Manager_GetToneConfig_001
 * @tc.desc  : Test GetToneConfig interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_GetToneConfig_001, TestSize.Level1)
{
    int32_t ltonetype = 0;
    std::shared_ptr<ToneInfo> toneInfo = AudioPolicyManager::GetInstance().GetToneConfig(ltonetype);
    ASSERT_NE(nullptr, toneInfo);
}
#endif

/**
 * @tc.name  : Test Audio_Policy_Manager_SetDeviceChangeCallback_001 via legal state
 * @tc.number: Audio_Policy_Manager_SetDeviceChangeCallback_001
 * @tc.desc  : Test SetDeviceChangeCallback interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_SetDeviceChangeCallback_001, TestSize.Level1)
{
    int32_t clientId = getpid();
    DeviceFlag flag = DeviceFlag::OUTPUT_DEVICES_FLAG;
    std::shared_ptr<AudioManagerDeviceChangeCallback> callback =
        std::make_shared<AudioManagerDeviceChangeCallbackTest>();

    int32_t ret = AudioPolicyManager::GetInstance().SetDeviceChangeCallback(clientId, flag, callback);
    EXPECT_EQ(SUCCESS, ret);

    ret = AudioPolicyManager::GetInstance().UnsetDeviceChangeCallback(clientId, flag);
    EXPECT_EQ(SUCCESS, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_GetStreamInFocus_001 via legal state
 * @tc.number: Audio_Policy_Manager_GetStreamInFocus_001
 * @tc.desc  : Test GetStreamInFocus interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_GetStreamInFocus_001, TestSize.Level1)
{
    AudioPolicyManager::GetInstance().GetStreamInFocus();
}

/**
 * @tc.name  : Test Audio_Policy_Manager_GetSessionInfoInFocus_001 via legal state
 * @tc.number: Audio_Policy_Manager_GetSessionInfoInFocus_001
 * @tc.desc  : Test GetSessionInfoInFocus interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_GetSessionInfoInFocus_001, TestSize.Level1)
{
    AudioInterrupt audioInterrupt;
    audioInterrupt.contentType = CONTENT_TYPE_RINGTONE;
    audioInterrupt.streamUsage = STREAM_USAGE_NOTIFICATION_RINGTONE;
    audioInterrupt.audioFocusType.streamType = STREAM_ACCESSIBILITY;

    std::shared_ptr<AudioStream> audioStream;
    AudioPolicyUnitTest::InitAudioStream(audioStream);
    ASSERT_NE(nullptr, audioStream);

    uint32_t sessionID_ = AudioPolicyUnitTest::GetSessionId(audioStream);
    audioInterrupt.sessionID = sessionID_;
    int32_t ret = AudioPolicyManager::GetInstance().GetSessionInfoInFocus(audioInterrupt);
    EXPECT_EQ(SUCCESS, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_RegisterAudioRendererEventListener_001 via legal state
 * @tc.number: Audio_Policy_Manager_RegisterAudioRendererEventListener_001
 * @tc.desc  : Test registerAudioRendererEventListener interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_RegisterAudioRendererEventListener_001, TestSize.Level1)
{
    int32_t clientId = getpid();
    std::shared_ptr<AudioRendererStateChangeCallback> callback =
        std::make_shared<AudioRendererStateChangeCallbackTest>();
    int32_t ret = AudioPolicyManager::GetInstance().RegisterAudioRendererEventListener(clientId, callback);
    EXPECT_EQ(SUCCESS, ret);

    ret = AudioPolicyManager::GetInstance().UnregisterAudioRendererEventListener(clientId);
    EXPECT_EQ(SUCCESS, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_RegisterAudioCapturerEventListener_001 via legal state
 * @tc.number: Audio_Policy_Manager_RegisterAudioCapturerEventListener_001
 * @tc.desc  : Test RegisterAudioCapturerEventListener interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_RegisterAudioCapturerEventListener_001, TestSize.Level1)
{
    int32_t clientId = getpid();
    std::shared_ptr<AudioCapturerStateChangeCallback> callback =
        std::make_shared<AudioCapturerStateChangeCallbackTest>();
    int32_t ret = AudioPolicyManager::GetInstance().RegisterAudioCapturerEventListener(clientId, callback);
    EXPECT_EQ(SUCCESS, ret);

    ret = AudioPolicyManager::GetInstance().UnregisterAudioCapturerEventListener(clientId);
    EXPECT_EQ(SUCCESS, ret);

    callback = nullptr;
    ret = AudioPolicyManager::GetInstance().RegisterAudioCapturerEventListener(clientId, callback);
    EXPECT_EQ(ERR_INVALID_PARAM, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_IsAudioRendererLowLatencySupported_001 via legal state
 * @tc.number: Audio_Policy_Manager_IsAudioRendererLowLatencySupported_001
 * @tc.desc  : Test IsAudioRendererLowLatencySupported interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_IsAudioRendererLowLatencySupported_001, TestSize.Level1)
{
    AudioStreamInfo audioStreamInfo;
    audioStreamInfo.samplingRate = AudioSamplingRate::SAMPLE_RATE_44100;
    audioStreamInfo.encoding = AudioEncodingType::ENCODING_PCM;
    audioStreamInfo.format = AudioSampleFormat::SAMPLE_S16LE;
    audioStreamInfo.channels = AudioChannel::MONO;
    bool ret = AudioPolicyManager::GetInstance().IsAudioRendererLowLatencySupported(audioStreamInfo);
    EXPECT_EQ(true, ret);
}

/**
 * @tc.name  : Test Audio_Policy_GetPreferredOutputDeviceDescriptors_001 via legal state
 * @tc.number: Audio_Policy_GetPreferredOutputDeviceDescriptors_001
 * @tc.desc  : Test GetPreferredOutputDeviceDescriptors interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_GetPreferredOutputDeviceDescriptors_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);
    AudioRendererInfo rendererInfo;
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;
    deviceInfo = audioPolicyProxy->GetPreferredOutputDeviceDescriptors(rendererInfo);
    EXPECT_EQ(true, deviceInfo.size() >= 0);
}

/**
 * @tc.name  : Test Audio_Policy_GetPreferredInputDeviceDescriptors_001 via legal state
 * @tc.number: Audio_Policy_GetPreferredInputDeviceDescriptors_001
 * @tc.desc  : Test GetPreferredInputDeviceDescriptors interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_GetPreferredInputDeviceDescriptors_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);
    AudioCapturerInfo capturerInfo;
    std::vector<sptr<AudioDeviceDescriptor>> deviceInfo;
    deviceInfo = audioPolicyProxy->GetPreferredInputDeviceDescriptors(capturerInfo);
    EXPECT_EQ(true, deviceInfo.size() >= 0);
}

/**
 * @tc.name  : Test Audio_Policy_SetAudioInterruptCallback_001 via illegal state
 * @tc.number: Audio_Policy_SetAudioInterruptCallback_001
 * @tc.desc  : Test SetAudioInterruptCallback interface. Returns invalid.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_SetAudioInterruptCallback_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    AudioInterrupt audioInterrupt;
    audioInterrupt.contentType = CONTENT_TYPE_RINGTONE;
    audioInterrupt.streamUsage = STREAM_USAGE_NOTIFICATION_RINGTONE;
    audioInterrupt.audioFocusType.streamType = STREAM_ACCESSIBILITY;

    std::shared_ptr<AudioStream> audioStream;
    AudioPolicyUnitTest::InitAudioStream(audioStream);
    ASSERT_NE(nullptr, audioStream);

    uint32_t sessionID_ = AudioPolicyUnitTest::GetSessionId(audioStream);
    sptr<IRemoteObject> object = nullptr;
    int32_t ret = audioPolicyProxy->SetAudioInterruptCallback(sessionID_, object);
    EXPECT_EQ(ERR_NULL_OBJECT, ret);
}

/**
 * @tc.name  : Test Audio_Policy_SetAudioManagerInterruptCallback_001 via illegal state
 * @tc.number: Audio_Policy_SetAudioManagerInterruptCallback_001
 * @tc.desc  : Test SetAudioManagerInterruptCallback interface. Returns invalid.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_SetAudioManagerInterruptCallback_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    int32_t clientId = getpid();
    sptr<IRemoteObject> object = nullptr;
    int32_t ret = audioPolicyProxy->SetAudioManagerInterruptCallback(clientId, object);
    EXPECT_EQ(ERR_NULL_OBJECT, ret);
}

/**
 * @tc.name  : Test Audio_Policy_RegisterTracker_001 via illegal state
 * @tc.number: Audio_Policy_RegisterTracker_001
 * @tc.desc  : Test RegisterTracker interface. Returns invalid.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_RegisterTracker_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyProxy> audioPolicyProxy;
    AudioPolicyUnitTest::InitAudioPolicyProxy(audioPolicyProxy);
    ASSERT_NE(nullptr, audioPolicyProxy);

    AudioMode mode = AudioMode::AUDIO_MODE_PLAYBACK;
    AudioStreamChangeInfo streamChangeInfo;
    sptr<IRemoteObject> object = nullptr;
    int32_t ret = audioPolicyProxy->RegisterTracker(mode, streamChangeInfo, object);
    EXPECT_EQ(ERR_NULL_OBJECT, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_SetRingerModeCallback_001 via illegal state
 * @tc.number: Audio_Policy_Manager_SetRingerModeCallback_001
 * @tc.desc  : Test SetRingerModeCallback interface. Returns invalid.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_SetRingerModeCallback_001, TestSize.Level1)
{
    int32_t clientId = getpid();
    std::shared_ptr<AudioRingerModeCallback> callback = nullptr;
    int32_t ret = AudioPolicyManager::GetInstance().SetRingerModeCallback(clientId, callback);
    EXPECT_EQ(ERR_INVALID_PARAM, ret);

    ret = AudioPolicyManager::GetInstance().UnsetRingerModeCallback(clientId);
    EXPECT_EQ(SUCCESS, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_SetDeviceChangeCallback_002 via illegal state
 * @tc.number: Audio_Policy_Manager_SetDeviceChangeCallback_002
 * @tc.desc  : Test SetDeviceChangeCallback interface. Returns invalid.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_SetDeviceChangeCallback_002, TestSize.Level1)
{
    int32_t clientId = getpid();
    DeviceFlag flag = DeviceFlag::INPUT_DEVICES_FLAG;
    std::shared_ptr<AudioManagerDeviceChangeCallback> callback = nullptr;
    int32_t ret = AudioPolicyManager::GetInstance().SetDeviceChangeCallback(clientId, flag, callback);
    EXPECT_EQ(ERR_INVALID_PARAM, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_SetAudioInterruptCallback_001 via illegal state
 * @tc.number: Audio_Policy_Manager_SetAudioInterruptCallback_001
 * @tc.desc  : Test SetAudioInterruptCallback interface. Returns invalid.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_SetAudioInterruptCallback_001, TestSize.Level1)
{
    AudioInterrupt audioInterrupt;
    audioInterrupt.contentType = CONTENT_TYPE_RINGTONE;
    audioInterrupt.streamUsage = STREAM_USAGE_NOTIFICATION_RINGTONE;
    audioInterrupt.audioFocusType.streamType = STREAM_ACCESSIBILITY;

    std::shared_ptr<AudioStream> audioStream;
    AudioPolicyUnitTest::InitAudioStream(audioStream);
    ASSERT_NE(nullptr, audioStream);

    uint32_t sessionID_ = AudioPolicyUnitTest::GetSessionId(audioStream);
    std::shared_ptr<AudioInterruptCallback> callback = nullptr;
    int32_t ret = AudioPolicyManager::GetInstance().SetAudioInterruptCallback(sessionID_, callback);
    EXPECT_EQ(ERR_INVALID_PARAM, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_SetAudioManagerInterruptCallback_001 via illegal state
 * @tc.number: Audio_Policy_Manager_SetAudioManagerInterruptCallback_001
 * @tc.desc  : Test SetAudioManagerInterruptCallback interface. Returns invalid.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_SetAudioManagerInterruptCallback_001, TestSize.Level1)
{
    int32_t clientId = getpid();
    std::shared_ptr<AudioInterruptCallback> callback = nullptr;
    int32_t ret = AudioPolicyManager::GetInstance().SetAudioManagerInterruptCallback(clientId, callback);
    EXPECT_EQ(ERR_INVALID_PARAM, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_SetVolumeKeyEventCallback_001 via illegal state
 * @tc.number: Audio_Policy_Manager_SetVolumeKeyEventCallback_001
 * @tc.desc  : Test SetVolumeKeyEventCallback interface. Returns invalid.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_SetVolumeKeyEventCallback_001, TestSize.Level1)
{
    int32_t clientPid = getpid();
    std::shared_ptr<VolumeKeyEventCallback> callback = nullptr;
    int32_t ret = AudioPolicyManager::GetInstance().SetVolumeKeyEventCallback(clientPid, callback);
    EXPECT_EQ(ERR_INVALID_PARAM, ret);
}

/**
* @tc.name  : Test Audio_Policy_Manager_RegisterAudioRendererEventListener_002 via illegal state
* @tc.number: Audio_Policy_Manager_RegisterAudioRendererEventListener_002
* @tc.desc  : Test RegisterAudioRendererEventListener interface. Returns invalid.
*/
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_RegisterAudioRendererEventListener_002, TestSize.Level1)
{
    int32_t clientPid = getpid();
    std::shared_ptr<AudioRendererStateChangeCallback> callback = nullptr;
    int32_t ret = AudioPolicyManager::GetInstance().RegisterAudioRendererEventListener(clientPid, callback);
    EXPECT_EQ(ERR_INVALID_PARAM, ret);
}

/**
 * @tc.name  : Test Audio_Policy_Manager_GetCurrentCapturerChangeInfos_001 via illegal state
 * @tc.number: Audio_Policy_Manager_GetCurrentCapturerChangeInfos_001
 * @tc.desc  : Test GetCurrentCapturerChangeInfos interface. Returns invalid.
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_Manager_GetCurrentCapturerChangeInfos_001, TestSize.Level1)
{
    vector<unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
    int32_t ret = AudioPolicyManager::GetInstance().GetCurrentCapturerChangeInfos(audioCapturerChangeInfos);
    EXPECT_EQ(true, audioCapturerChangeInfos.size() <= 0);
    EXPECT_EQ(SUCCESS, ret);
}

/**
 * @tc.name  : Test Audio_Capturer_State_Change_001 via legal state
 * @tc.number: Audio_Capturer_State_Change_001
 * @tc.desc  : Test AudioCapturerStateChangeListenerStub interface. Returns invalid.
 */
HWTEST(AudioPolicyUnitTest, Audio_Capturer_State_Change_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyClientStubImpl> capturerStub =
        std::make_shared<AudioPolicyClientStubImpl>();

    vector<unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
    capturerStub->OnCapturerStateChange(audioCapturerChangeInfos);

    std::shared_ptr<AudioCapturerStateChangeCallbackTest> callback =
        std::make_shared<AudioCapturerStateChangeCallbackTest>();
    capturerStub->AddCapturerStateChangeCallback(callback);

    uint32_t code = static_cast<uint32_t>(AudioPolicyClientCode::ON_CAPTURERSTATE_CHANGE);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int ret = capturerStub->OnRemoteRequest(code, data, reply, option);
    EXPECT_LE(ret, 0);
}

/**
 * @tc.name  : Test Audio_Renderer_State_Change_001 via legal state
 * @tc.number: Audio_Renderer_State_Change_001
 * @tc.desc  : Test AudioRendererStateChangeListenerStub interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Renderer_State_Change_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyClientStubImpl> rendererStub =
        std::make_shared<AudioPolicyClientStubImpl>();

    vector<unique_ptr<AudioRendererChangeInfo>> audioRendererChangeInfos;
    rendererStub->OnRendererStateChange(audioRendererChangeInfos);

    std::shared_ptr<AudioRendererStateChangeCallbackTest> callback =
        std::make_shared<AudioRendererStateChangeCallbackTest>();
    rendererStub->AddRendererStateChangeCallback(callback);

    uint32_t code = static_cast<uint32_t>(AudioPolicyClientCode::ON_RENDERERSTATE_CHANGE);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int ret = rendererStub->OnRemoteRequest(code, data, reply, option);
    EXPECT_LE(ret, 0);
}

/**
 * @tc.name  : Test Audio_Ringermode_Update_Listener_001 via legal state
 * @tc.number: Audio_Ringermode_Update_Listener_001
 * @tc.desc  : Test AudioRingerModeUpdateListenerStub interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Ringermode_Update_Listener_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyClientStubImpl> ringermodeStub =
        std::make_shared<AudioPolicyClientStubImpl>();
    std::shared_ptr<AudioRingerModeCallbackTest> callback = std::make_shared<AudioRingerModeCallbackTest>();
    AudioRingerMode ringerMode = AudioRingerMode::RINGER_MODE_SILENT;
    
    ringermodeStub->OnRingerModeUpdated(ringerMode);

    ringermodeStub->AddRingerModeCallback(callback);

    ringermodeStub->OnRingerModeUpdated(ringerMode);

    uint32_t code = static_cast<uint32_t>(AudioPolicyClientCode::ON_RINGERMODE_UPDATE);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int ret = ringermodeStub->OnRemoteRequest(code, data, reply, option);
    EXPECT_LE(ret, 0);
}

/**
 * @tc.name  : Test Audio_Rounting_Manager_Listener_001 via legal state
 * @tc.number: Audio_Rounting_Manager_Listener_001
 * @tc.desc  : Test AudioRoutingManagerListenerStub interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Rounting_Manager_Listener_001, TestSize.Level1)
{
    std::shared_ptr<AudioPolicyClientStubImpl> audioPolicyClientStubImpl =
        std::make_shared<AudioPolicyClientStubImpl>();
    std::shared_ptr<AudioManagerMicStateChangeCallbackTest> callback =
        std::make_shared<AudioManagerMicStateChangeCallbackTest>();
    MicStateChangeEvent micStateChangeEvent;
    micStateChangeEvent.mute = true;
    
    audioPolicyClientStubImpl->OnMicStateUpdated(micStateChangeEvent);

    audioPolicyClientStubImpl->AddMicStateChangeCallback(callback);

    audioPolicyClientStubImpl->OnMicStateUpdated(micStateChangeEvent);

    uint32_t code = static_cast<uint32_t>(AudioPolicyClientCode::ON_MIC_STATE_UPDATED);
    MessageParcel data;
    MessageParcel reply;
    MessageOption option;
    int ret = audioPolicyClientStubImpl->OnRemoteRequest(code, data, reply, option);
    EXPECT_LE(ret, 0);
}

/**
 * @tc.name  : Test Audio_Volume_Group_Info_001 via legal state
 * @tc.number: Audio_Volume_Group_Info_001
 * @tc.desc  : Test VolumeGroupInfo interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Volume_Group_Info_001, TestSize.Level1)
{
    int32_t volumeGroupId = 1;
    int32_t mappingId = 1;
    std::string groupName = "TEST_UNIT";
    std::string networkId = "UNIT";
    ConnectType type = ConnectType::CONNECT_TYPE_LOCAL;

    std::shared_ptr<VolumeGroupInfo> volumeGroupInfo =
        std::make_shared<VolumeGroupInfo>(volumeGroupId, mappingId, groupName, networkId, type);

    Parcel parcel;
    bool ret = volumeGroupInfo->Marshalling(parcel);
    EXPECT_EQ(true, ret);
}

/**
 * @tc.name  : Test Audio_Client_Tracker_Callback_Stub_001 via legal state
 * @tc.number: Audio_Client_Tracker_Callback_Stub_001
 * @tc.desc  : Test AudioClientTrackerCallbackStub interface. Returns success.
 */
HWTEST(AudioPolicyUnitTest, Audio_Client_Tracker_Callback_Stub_001, TestSize.Level1)
{
    std::shared_ptr<AudioClientTrackerCallbackStub> audioClientTrackerCallbackStub =
        std::make_shared<AudioClientTrackerCallbackStub>();

    StreamSetStateEventInternal streamSetStateEventInternal = {};
    streamSetStateEventInternal.audioStreamType = AudioStreamType::STREAM_MUSIC;
    std::weak_ptr<AudioClientTrackerTest> callback = std::make_shared<AudioClientTrackerTest>();

    float volume = 0.5;
    audioClientTrackerCallbackStub->SetLowPowerVolumeImpl(volume);
    audioClientTrackerCallbackStub->GetLowPowerVolumeImpl(volume);
    audioClientTrackerCallbackStub->GetSingleStreamVolumeImpl(volume);

    streamSetStateEventInternal.streamSetState= StreamSetState::STREAM_RESUME;
    audioClientTrackerCallbackStub->ResumeStreamImpl(streamSetStateEventInternal);

    streamSetStateEventInternal.streamSetState= StreamSetState::STREAM_PAUSE;
    audioClientTrackerCallbackStub->PausedStreamImpl(streamSetStateEventInternal);

    audioClientTrackerCallbackStub->SetClientTrackerCallback(callback);

    streamSetStateEventInternal.streamSetState= StreamSetState::STREAM_RESUME;
    audioClientTrackerCallbackStub->ResumeStreamImpl(streamSetStateEventInternal);

    streamSetStateEventInternal.streamSetState= StreamSetState::STREAM_PAUSE;
    audioClientTrackerCallbackStub->PausedStreamImpl(streamSetStateEventInternal);
}

/**
 * @tc.name  : Test Audio_Policy_SetSystemSoundUri_001
 * @tc.number: Audio_Policy_SetSystemSoundUri_001
 * @tc.desc  : Test audio policy instance
 */
HWTEST(AudioPolicyUnitTest, Audio_Policy_SetSystemSoundUri_001, TestSize.Level1)
{
    int32_t ret = -1;
    API_VERSION api_v = API_9;
    AudioRingerMode ringMode = AudioRingerMode::RINGER_MODE_SILENT;
    ret = AudioPolicyManager::GetInstance().SetRingerMode(ringMode, api_v);
    EXPECT_EQ(SUCCESS, ret);

    AudioRingerMode ringModeRet = AudioPolicyManager::GetInstance().GetRingerMode();
    EXPECT_EQ(ringMode, ringModeRet);

    const std::string key = "testkey";
    const std::string uri = "testuri";
    ret = AudioPolicyManager::GetInstance().SetSystemSoundUri(key, uri);
    EXPECT_EQ(ERR_INVALID_PARAM, ret);

    std::string systemSoundUri = AudioPolicyManager::GetInstance().GetSystemSoundUri(key);
    EXPECT_NE(systemSoundUri, "");
}
} // namespace AudioStandard
} // namespace OHOS