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

#ifndef POLICY_PROVIDER_STUB_H
#define POLICY_PROVIDER_STUB_H

#include "i_policy_provider_ipc.h"

namespace OHOS {
namespace AudioStandard {
class PolicyProviderStub : public IRemoteStub<IPolicyProviderIpc> {
public:
    virtual ~PolicyProviderStub() = default;
    int OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply, MessageOption &option) override;
private:
    static bool CheckInterfaceToken(MessageParcel &data);

    int32_t HandleGetProcessDeviceInfo(MessageParcel &data, MessageParcel &reply);
    int32_t HandleInitSharedVolume(MessageParcel &data, MessageParcel &reply);

    using HandlerFunc = int32_t(PolicyProviderStub::*)(MessageParcel &data, MessageParcel &reply);
    static inline HandlerFunc funcList_[IPolicyProviderMsg::POLICY_PROVIDER_MAX_MSG] = {
        &PolicyProviderStub::HandleGetProcessDeviceInfo,
        &PolicyProviderStub::HandleInitSharedVolume
    };
};

class PolicyProviderWrapper : public PolicyProviderStub {
public:
    ~PolicyProviderWrapper();
    PolicyProviderWrapper(IPolicyProvider *policyWorker);

    int32_t GetProcessDeviceInfo(const AudioProcessConfig &config, DeviceInfo &deviceInfo) override;
    int32_t InitSharedVolume(std::shared_ptr<AudioSharedMemory> &buffer) override;
private:
    IPolicyProvider *policyWorker_;
};
} // namespace AudioStandard
} // namespace OHOS
#endif // POLICY_PROVIDER_STUB_H
