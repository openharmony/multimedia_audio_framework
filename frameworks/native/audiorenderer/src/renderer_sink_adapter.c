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

#include <stdlib.h>
#include <string.h>

#include "audio_types.h"

#include "renderer_sink_adapter.h"

#include "media_log.h"

#ifdef __cplusplus
extern "C" {
#endif

const int32_t  SUCCESS = 0;
const int32_t  ERROR = -1;

const int32_t CLASS_TYPE_PRIMARY = 0;
const int32_t CLASS_TYPE_A2DP = 1;

const char *g_deviceClassPrimary = "primary";
const char *g_deviceClassA2Dp = "a2dp";

int32_t g_deviceClass = -1;

static int32_t RendererSinkInitInner(const SinkAttr *attr)
{
    MEDIA_INFO_LOG("%{public}s: RendererSinkInitInner", __func__);
    if (attr == NULL) {
        MEDIA_ERR_LOG("%{public}s: Invalid parameter", __func__);
        return ERROR;
    }

    if (g_deviceClass == -1) {
        MEDIA_ERR_LOG("%{public}s: Adapter not loaded", __func__);
        return ERROR;
    }

    if (g_deviceClass == CLASS_TYPE_PRIMARY) {
        MEDIA_INFO_LOG("%{public}s: CLASS_TYPE_PRIMARY", __func__);
        return AudioRendererSinkInit((AudioSinkAttr *)attr);
    } else if (g_deviceClass == CLASS_TYPE_A2DP) {
        MEDIA_INFO_LOG("%{public}s: CLASS_TYPE_A2DP", __func__);
        return BluetoothRendererSinkInit((BluetoothSinkAttr *)attr);
    } else {
        MEDIA_ERR_LOG("%{public}s: Device not supported", __func__);
        return ERROR;
    }
}

int32_t LoadSinkAdapter(const char *device, struct RendererSinkAdapter **sinkAdapter)
{
    MEDIA_INFO_LOG("%{public}s: LoadSinkAdapter: %{public}s", __func__, device);
    if ((device == NULL) || (sinkAdapter == NULL)) {
        MEDIA_ERR_LOG("%{public}s: Invalid parameter", __func__);
        return ERROR;
    }

    struct RendererSinkAdapter *adapter = (struct RendererSinkAdapter *)calloc(1, sizeof(*adapter));
    if (adapter == NULL) {
        MEDIA_ERR_LOG("%{public}s: alloc sink adapter failed", __func__);
        return ERROR;
    }

    if (!strcmp(device, g_deviceClassPrimary)) {
        MEDIA_INFO_LOG("%{public}s: primary device", __func__);
        adapter->RendererSinkInit = RendererSinkInitInner;
        adapter->RendererSinkDeInit = AudioRendererSinkDeInit;
        adapter->RendererSinkStart = AudioRendererSinkStart;
        adapter->RendererSinkStop = AudioRendererSinkStop;
        adapter->RendererRenderFrame = AudioRendererRenderFrame;
        adapter->RendererSinkSetVolume = AudioRendererSinkSetVolume;
        adapter->RendererSinkGetLatency = AudioRendererSinkGetLatency;
        g_deviceClass = CLASS_TYPE_PRIMARY;
    } else if (!strcmp(device, g_deviceClassA2Dp)) {
        MEDIA_INFO_LOG("%{public}s: a2dp device", __func__);
        adapter->RendererSinkInit = RendererSinkInitInner;
        adapter->RendererSinkDeInit = BluetoothRendererSinkDeInit;
        adapter->RendererSinkStart = BluetoothRendererSinkStart;
        adapter->RendererSinkStop = BluetoothRendererSinkStop;
        adapter->RendererRenderFrame = BluetoothRendererRenderFrame;
        adapter->RendererSinkSetVolume = BluetoothRendererSinkSetVolume;
        adapter->RendererSinkGetLatency = BluetoothRendererSinkGetLatency;
        g_deviceClass = CLASS_TYPE_A2DP;
    } else {
        MEDIA_ERR_LOG("%{public}s: Device not supported", __func__);
        free(adapter);
        return ERROR;
    }

    *sinkAdapter = adapter;

    return SUCCESS;
}

int32_t UnLoadSinkAdapter(struct RendererSinkAdapter *sinkAdapter)
{
    if (sinkAdapter == NULL) {
        MEDIA_ERR_LOG("%{public}s: Invalid parameter", __func__);
        return ERROR;
    }

    free(sinkAdapter);

    return SUCCESS;
}
#ifdef __cplusplus
}
#endif
