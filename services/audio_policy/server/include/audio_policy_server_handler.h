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
#ifndef AUDIO_POLICY_SERVER_HANDLER_H
#define AUDIO_POLICY_SERVER_HANDLER_H
#include <mutex>

#include "singleton.h"
#include "event_handler.h"
#include "event_runner.h"

#include "audio_log.h"
#include "audio_info.h"
#include "audio_system_manager.h"
#include "audio_policy_client.h"
#include "i_standard_audio_policy_manager_listener.h"
#include "i_standard_audio_routing_manager_listener.h"

namespace OHOS {
namespace AudioStandard {
using namespace std;

class AudioPolicyServerHandler : public AppExecFwk::EventHandler {
    DECLARE_DELAYED_SINGLETON(AudioPolicyServerHandler)
public:
    enum FocusCallbackCategory : int32_t {
        NONE_CALLBACK_CATEGORY,
        REQUEST_CALLBACK_CATEGORY,
        ABANDON_CALLBACK_CATEGORY,
    };

    enum EventAudioServerCmd {
        AUDIO_DEVICE_CHANGE,
        AVAILABLE_AUDIO_DEVICE_CHANGE,
        VOLUME_KEY_EVENT,
        REQUEST_CATEGORY_EVENT,
        ABANDON_CATEGORY_EVENT,
        FOCUS_INFOCHANGE,
        RINGER_MODEUPDATE_EVENT,
        MIC_STATE_CHANGE_EVENT,
        INTERRUPT_EVENT,
        INTERRUPT_EVENT_WITH_SESSIONID,
        INTERRUPT_EVENT_WITH_CLIENTID,
        PREFERRED_OUTPUT_DEVICE_UPDATED,
        PREFERRED_INPUT_DEVICE_UPDATED,
        DISTRIBUTED_ROUTING_ROLE_CHANGE,
        RENDERER_INFO_EVENT,
        CAPTURER_INFO_EVENT,
        RENDERER_DEVICE_CHANGE_EVENT,
        ON_CAPTURER_CREATE,
        ON_CAPTURER_REMOVED,
        ON_WAKEUP_CLOSE,
    };
    /* event data */
    class EventContextObj {
    public:
        DeviceChangeAction deviceChangeAction;
        VolumeEvent volumeEvent;
        AudioInterrupt audioInterrupt;
        std::list<std::pair<AudioInterrupt, AudioFocuState>> focusInfoList;
        AudioRingerMode ringMode;
        MicStateChangeEvent micStateChangeEvent;
        InterruptEventInternal interruptEvent;
        uint32_t sessionID;
        int32_t clientId;
        sptr<AudioDeviceDescriptor> descriptor;
        CastType type;
        std::vector<std::unique_ptr<AudioRendererChangeInfo>> audioRendererChangeInfos;
        std::vector<std::unique_ptr<AudioCapturerChangeInfo>> audioCapturerChangeInfos;
    };

    struct RendererDeviceChangeEvent {
        RendererDeviceChangeEvent() = delete;
        RendererDeviceChangeEvent(const int32_t clientPid, const uint32_t sessionId,
            const DeviceInfo outputDeviceInfo, const AudioStreamDeviceChangeReason &reason)
            : clientPid_(clientPid), sessionId_(sessionId), outputDeviceInfo_(outputDeviceInfo), reason_(reason)
        {}

        const int32_t clientPid_;
        const uint32_t sessionId_;
        const DeviceInfo outputDeviceInfo_;
        const AudioStreamDeviceChangeReason reason_;
    };

    struct CapturerCreateEvent {
        CapturerCreateEvent() = delete;
        CapturerCreateEvent(const AudioCapturerInfo &capturerInfo, const AudioStreamInfo &streamInfo,
            uint64_t sessionId, int32_t error)
            : capturerInfo_(capturerInfo), streamInfo_(streamInfo), sessionId_(sessionId), error_(error)
        {}
        AudioCapturerInfo capturerInfo_;
        AudioStreamInfo streamInfo_;
        uint64_t sessionId_;
        int32_t error_;
    };

    void AddAudioPolicyClientProxyMap(int32_t clientPid, const sptr<IAudioPolicyClient> &cb);
    void RemoveAudioPolicyClientProxyMap(pid_t clientPid);
    void AddInterruptCbsMap(uint32_t sessionID, const std::shared_ptr<AudioInterruptCallback> &callback);
    int32_t RemoveInterruptCbsMap(uint32_t sessionID);
    void AddExternInterruptCbsMap(int32_t clientId, const std::shared_ptr<AudioInterruptCallback> &callback);
    int32_t RemoveExternInterruptCbsMap(int32_t clientId);
    void AddAvailableDeviceChangeMap(int32_t clientId, const AudioDeviceUsage usage,
        const sptr<IStandardAudioPolicyManagerListener> &callback);
    void RemoveAvailableDeviceChangeMap(const int32_t clientId, AudioDeviceUsage usage);
    void AddDistributedRoutingRoleChangeCbsMap(int32_t clientId,
        const sptr<IStandardAudioRoutingManagerListener> &callback);
    int32_t RemoveDistributedRoutingRoleChangeCbsMap(int32_t clientId);
    bool SendDeviceChangedCallback(const vector<sptr<AudioDeviceDescriptor>> &desc, bool isConnected);
    bool SendAvailableDeviceChange(const vector<sptr<AudioDeviceDescriptor>> &desc, bool isConnected);
    bool SendVolumeKeyEventCallback(const VolumeEvent &volumeEvent);
    bool SendAudioFocusInfoChangeCallBack(int32_t callbackCategory, const AudioInterrupt &audioInterrupt,
        const std::list<std::pair<AudioInterrupt, AudioFocuState>> &focusInfoList);
    bool SendRingerModeUpdatedCallBack(const AudioRingerMode &ringMode);
    bool SendMicStateUpdatedCallBack(const MicStateChangeEvent &micStateChangeEvent);
    bool SendInterruptEventInternalCallBack(const InterruptEventInternal &interruptEvent);
    bool SendInterruptEventWithSeesionIdCallBack(const InterruptEventInternal &interruptEvent,
        const uint32_t &sessionID);
    bool SendInterruptEventWithClientIdCallBack(const InterruptEventInternal &interruptEvent,
        const int32_t &clientId);
    bool SendPreferredOutputDeviceUpdated();
    bool SendPreferredInputDeviceUpdated();
    bool SendDistributedRoutingRoleChange(const sptr<AudioDeviceDescriptor> &descriptor,
        const CastType &type);
    bool SendRendererInfoEvent(const std::vector<std::unique_ptr<AudioRendererChangeInfo>> &audioRendererChangeInfos);
    bool SendCapturerInfoEvent(const std::vector<std::unique_ptr<AudioCapturerChangeInfo>> &audioCapturerChangeInfos);
    bool SendRendererDeviceChangeEvent(const int32_t clientPid, const uint32_t sessionId,
        const DeviceInfo &outputDeviceInfo, const AudioStreamDeviceChangeReason reason);
    bool SendCapturerCreateEvent(AudioCapturerInfo capturerInfo, AudioStreamInfo streamInfo,
        uint64_t sessionId, bool isSync, int32_t &error);
    bool SendCapturerRemovedEvent(uint64_t sessionId, bool isSync);
    bool SendWakeupCloseEvent(bool isSync);

protected:
    void ProcessEvent(const AppExecFwk::InnerEvent::Pointer &event) override;

private:
    /* Handle Event*/
    void HandleDeviceChangedCallback(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleAvailableDeviceChange(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleVolumeKeyEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleRequestCateGoryEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleAbandonCateGoryEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleFocusInfoChangeEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleRingerModeUpdatedEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleMicStateUpdatedEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleInterruptEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleInterruptEventWithSessionId(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleInterruptEventWithClientId(const AppExecFwk::InnerEvent::Pointer &event);
    void HandlePreferredOutputDeviceUpdated();
    void HandlePreferredInputDeviceUpdated();
    void HandleDistributedRoutingRoleChangeEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleRendererInfoEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleCapturerInfoEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleRendererDeviceChangeEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleCapturerCreateEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleCapturerRemovedEvent(const AppExecFwk::InnerEvent::Pointer &event);
    void HandleWakeupCloaseEvent(const AppExecFwk::InnerEvent::Pointer &event);

    void HandleServiceEvent(const uint32_t &eventId, const AppExecFwk::InnerEvent::Pointer &event);

    std::mutex runnerMutex_;
    std::unordered_map<int32_t, sptr<IAudioPolicyClient>> audioPolicyClientProxyAPSCbsMap_;
    std::unordered_map<uint32_t, std::shared_ptr<AudioInterruptCallback>> interruptCbsMap_;
    std::unordered_map<int32_t, std::shared_ptr<AudioInterruptCallback>> amInterruptCbsMap_;
    std::map<std::pair<int32_t, AudioDeviceUsage>,
        sptr<IStandardAudioPolicyManagerListener>> availableDeviceChangeCbsMap_;
    std::unordered_map<int32_t, sptr<IStandardAudioRoutingManagerListener>> distributedRoutingRoleChangeCbsMap_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // AUDIO_POLICY_SERVER_HANDLER_H
