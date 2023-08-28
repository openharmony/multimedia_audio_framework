/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "audio_errors.h"
#include "audio_manager_listener_stub.h"
#include "audio_manager_proxy.h"
#include "audio_log.h"
#include "audio_process_in_client.h"
#include "audio_process_proxy.h"
#include "audio_stream.h"
#include "audio_system_manager.h"
#include <gtest/gtest.h>
#include "iservice_registry.h"
#include "system_ability_definition.h"

using namespace testing::ext;
namespace OHOS {
namespace AudioStandard {
std::unique_ptr<AudioManagerProxy> audioManagerProxy;
std::shared_ptr<AudioProcessInClient> processClient_;
const int32_t TEST_RET_NUM = 0;
const int32_t RENDERER_FLAGS = 0;
class AudioServiceUnitTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

class AudioParameterCallbackTest : public AudioParameterCallback {
    virtual void OnAudioParameterChange(const std::string networkId, const AudioParamKey key,
        const std::string& condition, const std::string& value) {}
};

void AudioServiceUnitTest::SetUpTestCase(void)
{
    // input testsuit setup step，setup invoked before all testcases
}

void AudioServiceUnitTest::TearDownTestCase(void)
{
    // input testsuit teardown step，teardown invoked after all testcases
}

void AudioServiceUnitTest::SetUp(void)
{
    // input testcase setup step，setup invoked before each testcases
}

void AudioServiceUnitTest::TearDown(void)
{
    // input testcase teardown step，teardown invoked after each testcases
}

/**
* @tc.name  : Test AudioProcessProxy API
* @tc.type  : FUNC
* @tc.number: AudioProcessProxy_001
* @tc.desc  : Test AudioProcessProxy interface.
*/
HWTEST(AudioServiceUnitTest, AudioProcessProxy_001, TestSize.Level1)
{
    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    EXPECT_NE(nullptr, samgr);
    sptr<IRemoteObject> object = samgr->GetSystemAbility(AUDIO_DISTRIBUTED_SERVICE_ID);
    EXPECT_NE(nullptr, object);
    std::unique_ptr<AudioProcessProxy> audioProcessProxy = std::make_unique<AudioProcessProxy>(object);

    int32_t ret = -1;
    std::shared_ptr<OHAudioBuffer> buffer;
    uint32_t spanSizeInFrame = 1000;
    uint32_t totalSizeInFrame = spanSizeInFrame - 1;
    uint32_t byteSizePerFrame = 1000;
    buffer = OHAudioBuffer::CreateFormLocal(totalSizeInFrame, spanSizeInFrame, byteSizePerFrame);

    ret=audioProcessProxy->ResolveBuffer(buffer);
    EXPECT_LT(ret, TEST_RET_NUM);

    ret = audioProcessProxy->Start();
    EXPECT_LT(ret, TEST_RET_NUM);

    bool isFlush = true;
    ret = audioProcessProxy->Pause(isFlush);
    EXPECT_LT(ret, TEST_RET_NUM);

    ret = audioProcessProxy->Resume();
    EXPECT_LT(ret, TEST_RET_NUM);

    ret = audioProcessProxy->Stop();
    EXPECT_LT(ret, TEST_RET_NUM);

    ret = audioProcessProxy->RequestHandleInfo();
    EXPECT_EQ(ret, SUCCESS);

    ret = audioProcessProxy->Release();
    EXPECT_LT(ret, TEST_RET_NUM);
}

/**
* @tc.name  : Test AudioManagerProxy API
* @tc.type  : FUNC
* @tc.number: AudioManagerProxy_001
* @tc.desc  : Test AudioManagerProxy interface.
*/
HWTEST(AudioServiceUnitTest, AudioManagerProxy_001, TestSize.Level1)
{
    int32_t ret = -1;

    auto samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    EXPECT_NE(nullptr, samgr);
    sptr<IRemoteObject> object = samgr->GetSystemAbility(AUDIO_DISTRIBUTED_SERVICE_ID);
    EXPECT_NE(nullptr, object);

    audioManagerProxy = std::make_unique<AudioManagerProxy>(object);

    bool isMute = true;
    ret = audioManagerProxy->SetMicrophoneMute(isMute);
    EXPECT_EQ(ret, SUCCESS);

    bool isMuteRet = audioManagerProxy->IsMicrophoneMute();
    EXPECT_EQ(true, isMuteRet);
}

/**
* @tc.name  : Test AudioManagerProxy API
* @tc.type  : FUNC
* @tc.number: AudioManagerProxy_002
* @tc.desc  : Test AudioManagerProxy interface.
*/
HWTEST(AudioServiceUnitTest, AudioManagerProxy_002, TestSize.Level1)
{
    int32_t ret = -1;

    float volume = 0.1;
    ret = audioManagerProxy->SetVoiceVolume(volume);

    const std::string networkId = "LocalDevice";
    const AudioParamKey key = AudioParamKey::VOLUME;
    AudioVolumeType volumeType =AudioVolumeType::STREAM_MEDIA;
    int32_t groupId = 0;
    std::string condition = "EVENT_TYPE=1;VOLUME_GROUP_ID=" + std::to_string(groupId) + ";AUDIO_VOLUME_TYPE="
        + std::to_string(volumeType) + ";";
    std::string value = std::to_string(volume);
    audioManagerProxy->SetAudioParameter(networkId, key, condition, value);
    const std::string retStr = audioManagerProxy->GetAudioParameter(networkId, key, condition);
    EXPECT_NE(retStr, value);

    bool connected = true;
    audioManagerProxy->NotifyDeviceInfo(networkId, connected);
    ret = audioManagerProxy->CheckRemoteDeviceState(networkId, DeviceRole::OUTPUT_DEVICE, true);
    EXPECT_LT(ret, TEST_RET_NUM);
}

/**
* @tc.name  : Test AudioManagerProxy API
* @tc.type  : FUNC
* @tc.number: AudioManagerProxy_003
* @tc.desc  : Test AudioManagerProxy interface.
*/
HWTEST(AudioServiceUnitTest, AudioManagerProxy_003, TestSize.Level1)
{
    int32_t ret = -1;

    AudioScene audioScene = AudioScene::AUDIO_SCENE_DEFAULT;
    DeviceType activeDevice = DeviceType::DEVICE_TYPE_SPEAKER;
    ret = audioManagerProxy->SetAudioScene(audioScene, activeDevice);
    EXPECT_EQ(AUDIO_OK, ret);

    DeviceFlag deviceFlag = DeviceFlag::OUTPUT_DEVICES_FLAG;
    std::vector<sptr<AudioDeviceDescriptor>> audioDeviceDescriptors;
    audioDeviceDescriptors = audioManagerProxy->GetDevices(deviceFlag);
    EXPECT_EQ(audioDeviceDescriptors.size() > 0, false);

    ret = audioManagerProxy->UpdateActiveDeviceRoute(DeviceType::DEVICE_TYPE_SPEAKER, DeviceFlag::OUTPUT_DEVICES_FLAG);
    EXPECT_EQ(AUDIO_OK, ret);

    sptr<IRemoteObject> object = nullptr;
    ret = audioManagerProxy->SetParameterCallback(object);
    EXPECT_EQ(ERR_NULL_OBJECT, ret);

    auto parameterChangeCbStub = new(std::nothrow) AudioManagerListenerStub();
    EXPECT_NE(parameterChangeCbStub, nullptr);
    object = parameterChangeCbStub->AsObject();
    EXPECT_NE(object, nullptr);
    ret = audioManagerProxy->SetParameterCallback(object);
    EXPECT_EQ(AUDIO_OK, ret);
}

/**
* @tc.name  : Test AudioManagerProxy API
* @tc.type  : FUNC
* @tc.number: AudioManagerProxy_004
* @tc.desc  : Test AudioManagerProxy interface.
*/
HWTEST(AudioServiceUnitTest, AudioManagerProxy_004, TestSize.Level1)
{
    AudioProcessConfig config;
    config.appInfo = {};
    config.appInfo.appUid=static_cast<int32_t>(getuid());

    config.audioMode = AudioMode::AUDIO_MODE_RECORD;

    config.streamInfo = {};
    config.streamInfo.samplingRate = AudioSamplingRate::SAMPLE_RATE_44100;
    config.streamInfo.encoding = AudioEncodingType::ENCODING_PCM;
    config.streamInfo.format = AudioSampleFormat::SAMPLE_S16LE;
    config.streamInfo.channels = AudioChannel::STEREO;

    config.rendererInfo = {};
    config.rendererInfo.contentType = ContentType::CONTENT_TYPE_MUSIC;
    config.rendererInfo.streamUsage = StreamUsage::STREAM_USAGE_MEDIA;
    config.rendererInfo.rendererFlags = 0;

    config.capturerInfo = {};
    config.capturerInfo.sourceType = SourceType::SOURCE_TYPE_MIC;
    config.capturerInfo.capturerFlags = 0;
    sptr<IRemoteObject> process = audioManagerProxy->CreateAudioProcess(config);
    EXPECT_EQ(process, nullptr);
}

/**
* @tc.name  : Test AudioManagerProxy API
* @tc.type  : FUNC
* @tc.number: AudioManagerProxy_005
* @tc.desc  : Test AudioManagerProxy interface.
*/
HWTEST(AudioServiceUnitTest, AudioManagerProxy_005, TestSize.Level1)
{
    std::vector<Library> libraries;
    Library library = {};
    library.name = "testname";
    library.path ="test.so";
    libraries.push_back(library);

    std::vector<Effect> effects;
    Effect effect = {};
    effect.name = "test";
    effect.libraryName = "test";
    effects.push_back(effect);

    std::vector<Effect> successEffects;
    bool ret = audioManagerProxy->LoadAudioEffectLibraries(libraries, effects, successEffects);
    EXPECT_EQ(ret, false);
}

/**
* @tc.name  : Test AudioManagerListenerStub API
* @tc.type  : FUNC
* @tc.number: AudioManagerListenerStub_001
* @tc.desc  : Test AudioManagerListenerStub interface.
*/
HWTEST(AudioServiceUnitTest, AudioManagerListenerStub_001, TestSize.Level1)
{
    std::unique_ptr<AudioManagerListenerStub> audioManagerListenerStub = std::make_unique<AudioManagerListenerStub>();

    const std::weak_ptr<AudioParameterCallback> callback = std::make_shared<AudioParameterCallbackTest>();
    audioManagerListenerStub->SetParameterCallback(callback);
    float volume = 0.1;
    const std::string networkId = "LocalDevice";
    const AudioParamKey key = AudioParamKey::VOLUME;
    AudioVolumeType volumeType =AudioVolumeType::STREAM_MEDIA;
    int32_t groupId = 0;
    std::string condition = "EVENT_TYPE=1;VOLUME_GROUP_ID=" + std::to_string(groupId) + ";AUDIO_VOLUME_TYPE="
        + std::to_string(volumeType) + ";";
    std::string value = std::to_string(volume);
    audioManagerListenerStub->OnAudioParameterChange(networkId, key, condition, value);
    EXPECT_NE(value, "");
}


/**
* @tc.name  : Test AudioProcessInClientInner API
* @tc.type  : FUNC
* @tc.number: AudioProcessInClientInner_001
* @tc.desc  : Test AudioProcessInClientInner interface using unsupported parameters.
*/
HWTEST(AudioServiceUnitTest, AudioProcessInClientInner_001, TestSize.Level1)
{
    AudioProcessConfig config;
    config.appInfo.appPid = getpid();
    config.appInfo.appUid = getuid();

    config.audioMode = AUDIO_MODE_PLAYBACK;

    config.rendererInfo.contentType = CONTENT_TYPE_MUSIC;
    config.rendererInfo.streamUsage = STREAM_USAGE_MEDIA;
    config.rendererInfo.rendererFlags = RENDERER_FLAGS;

    config.streamInfo.channels = STEREO;
    config.streamInfo.encoding = ENCODING_PCM;
    config.streamInfo.format = SAMPLE_S16LE;
    config.streamInfo.samplingRate = SAMPLE_RATE_64000;

    processClient_ = AudioProcessInClient::Create(config);
    EXPECT_EQ(processClient_, nullptr);
}

/**
* @tc.name  : Test AudioDeviceDescriptor API
* @tc.type  : FUNC
* @tc.number: AudioDeviceDescriptor_001
* @tc.desc  : Test AudioDeviceDescriptor interface.
*/
HWTEST(AudioServiceUnitTest, AudioDeviceDescriptor_001, TestSize.Level1)
{
    DeviceType type = DeviceType::DEVICE_TYPE_SPEAKER;
    DeviceRole role = DeviceRole::OUTPUT_DEVICE;
    int32_t interruptGroupId = 1;
    int32_t volumeGroupId = 1;
    std::string networkId = "LocalDevice";
    std::unique_ptr<AudioDeviceDescriptor> audioDeviceDescriptor =
        std::make_unique<AudioDeviceDescriptor>(type, role, interruptGroupId, volumeGroupId, networkId);
    EXPECT_NE(audioDeviceDescriptor, nullptr);

    AudioDeviceDescriptor deviceDescriptor;
    deviceDescriptor.deviceType_ = type;
    deviceDescriptor.deviceRole_ = role;
    audioDeviceDescriptor = std::make_unique<AudioDeviceDescriptor>(deviceDescriptor);
    EXPECT_NE(audioDeviceDescriptor, nullptr);

    std::string deviceName = "";
    std::string macAddress = "";
    audioDeviceDescriptor->SetDeviceInfo(deviceName, macAddress);

    AudioStreamInfo audioStreamInfo;
    audioStreamInfo.channels = STEREO;
    audioStreamInfo.encoding = ENCODING_PCM;
    audioStreamInfo.format = SAMPLE_S16LE;
    audioStreamInfo.samplingRate = SAMPLE_RATE_48000;
    int32_t channelMask = 1;
    audioDeviceDescriptor->SetDeviceCapability(audioStreamInfo, channelMask);

    AudioStreamInfo streamInfo = audioDeviceDescriptor->audioStreamInfo_;
    EXPECT_EQ(streamInfo.channels, audioStreamInfo.channels);
    EXPECT_EQ(streamInfo.encoding, audioStreamInfo.encoding);
    EXPECT_EQ(streamInfo.format, audioStreamInfo.format);
    EXPECT_EQ(streamInfo.samplingRate, audioStreamInfo.samplingRate);
}

/**
* @tc.name  : Test AudioServiceClient API
* @tc.type  : FUNC
* @tc.number: AudioServiceClient_001
* @tc.desc  : Test AudioServiceClient interface.
*/
HWTEST(AudioServiceUnitTest, AudioServiceClient_001, TestSize.Level1)
{
    int32_t ret = -1;

    std::unique_ptr<AudioServiceClient> audioServiceClient = std::make_unique<AudioStream>(STREAM_MUSIC,
        AUDIO_MODE_PLAYBACK, getuid());

    ASClientType eClientType = ASClientType::AUDIO_SERVICE_CLIENT_PLAYBACK;
    ret = audioServiceClient->Initialize(eClientType);
    EXPECT_EQ(SUCCESS, ret);

    AudioStreamParams audioParams = {};
    audioParams.samplingRate = AudioSamplingRate::SAMPLE_RATE_44100;
    audioParams.encoding = AudioEncodingType::ENCODING_PCM;
    audioParams.format = AudioSampleFormat::SAMPLE_S16LE;
    audioParams.channels = AudioChannel::STEREO;

    ret = audioServiceClient->CreateStream(audioParams, AudioStreamType::STREAM_SYSTEM);
    EXPECT_EQ(SUCCESS, ret);
    ret = audioServiceClient->CreateStream(audioParams, AudioStreamType::STREAM_NOTIFICATION);
    EXPECT_EQ(SUCCESS, ret);
    ret = audioServiceClient->CreateStream(audioParams, AudioStreamType::STREAM_BLUETOOTH_SCO);
    EXPECT_EQ(SUCCESS, ret);
    ret = audioServiceClient->CreateStream(audioParams, AudioStreamType::STREAM_DTMF);
    EXPECT_EQ(SUCCESS, ret);
    ret = audioServiceClient->CreateStream(audioParams, AudioStreamType::STREAM_TTS);
    EXPECT_EQ(SUCCESS, ret);
    ret = audioServiceClient->CreateStream(audioParams, AudioStreamType::STREAM_DEFAULT);
    EXPECT_EQ(SUCCESS, ret);

    ret = audioServiceClient->SetStreamRenderRate(AudioRendererRate::RENDER_RATE_HALF);
    EXPECT_EQ(SUCCESS, ret);
    ret = audioServiceClient->SetStreamRenderRate(static_cast<AudioRendererRate>(-1));
    EXPECT_EQ(SUCCESS - 2, ret);
    ret = audioServiceClient->SetStreamRenderRate(RENDER_RATE_DOUBLE);
    EXPECT_EQ(SUCCESS, ret);

    uint32_t rate = audioServiceClient->GetRendererSamplingRate();
    EXPECT_EQ((uint32_t)SAMPLE_RATE_44100, rate);
    ret = audioServiceClient->SetRendererSamplingRate(0);
    EXPECT_EQ(SUCCESS - 2, ret);
    rate = audioServiceClient->GetRendererSamplingRate();
    EXPECT_EQ((uint32_t)SAMPLE_RATE_44100, rate);

    audioServiceClient->OnTimeOut();
}
} // namespace AudioStandard
} // namespace OHOS