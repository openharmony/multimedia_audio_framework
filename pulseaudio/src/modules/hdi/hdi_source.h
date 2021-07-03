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

#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/source.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/rtpoll.h>

#include <audio_manager.h>
#include <audio_capturer_source_intf.h>

#define DEFAULT_SOURCE_NAME "hdi_input"
#define DEFAULT_AUDIO_DEVICE_NAME "Internal Mic"

#define DEFAULT_BUFFER_SIZE (1024 * 16)
#define MAX_VOLUME_VALUE 15.0
#define DEFAULT_LEFT_VOLUME MAX_VOLUME_VALUE
#define DEFAULT_RIGHT_VOLUME MAX_VOLUME_VALUE
#define FIVE_MSEC  5000
#define MAX_RETRIES  5
#define MAX_LATENCY_USEC (PA_USEC_PER_SEC * 2)
#define MIN_LATENCY_USEC (500)
#define AUDIO_POINT_NUM  1024
#define AUDIO_FRAME_NUM_IN_BUF 30

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_source *source;
    pa_thread *thread;
    pa_thread_mq thread_mq;
    pa_rtpoll *rtpoll;
    size_t buffer_size;
    pa_usec_t block_usec;
    pa_usec_t timestamp;
    AudioSourceAttr attrs;
    // A flag to signal us to prevent silent record during bootup
    bool IsReady;
    bool IsCapturerInit;
};

pa_source* pa_hdi_source_new(pa_module *m, pa_modargs *ma, const char*driver);

void pa_hdi_source_free(pa_source *s);
