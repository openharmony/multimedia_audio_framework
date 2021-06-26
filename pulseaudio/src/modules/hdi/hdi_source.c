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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <securec.h>

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/util.h>
#include <pulse/xmalloc.h>
#include <pulsecore/core.h>
#include <pulsecore/module.h>
#include <pulsecore/memchunk.h>
#include <pulsecore/modargs.h>
#include <pulsecore/core-rtclock.h>
#include <pulsecore/core-util.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/rtpoll.h>

#include <audio_manager.h>
#include <audio_capturer_source_intf.h>

#include "hdi_source.h"
#include "media_log.h"

static void userdata_free(struct userdata *u) {
    pa_assert(u);
    if (u->source)
        pa_source_unlink(u->source);

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }

    pa_thread_mq_done(&u->thread_mq);

    if (u->source)
        pa_source_unref(u->source);

    if (u->rtpoll)
        pa_rtpoll_free(u->rtpoll);

    AudioCapturerSourceStop();
    AudioCapturerSourceDeInit();
    pa_xfree(u);
}

static int source_process_msg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u = PA_SOURCE(o)->userdata;

    switch (code) {
        case PA_SOURCE_MESSAGE_GET_LATENCY: {
             pa_usec_t now;
             now = pa_rtclock_now();
             *((int64_t*) data) = (int64_t)now - (int64_t)u->timestamp;
             return 0;
        }
        default: {
             pa_log("source_process_msg default case");
             return pa_source_process_msg(o, code, data, offset, chunk);
        }
    }
}

/* Called from the IO thread. */
static int source_set_state_in_io_thread_cb(pa_source *s, pa_source_state_t new_state, pa_suspend_cause_t new_suspend_cause) {
    struct userdata *u;
    pa_assert(s);
    pa_assert_se(u = s->userdata);
    if (s->thread_info.state == PA_SOURCE_SUSPENDED || s->thread_info.state == PA_SOURCE_INIT) {
        if (PA_SOURCE_IS_OPENED(new_state))
            u->timestamp = pa_rtclock_now();
    }

    return 0;
}

static pa_hook_result_t source_output_fixate_hook_cb(pa_core *c, pa_source_output_new_data *data,
        struct userdata *u) {
    MEDIA_DEBUG_LOG("HDI Source: Detected source output");
    pa_assert(data);
    pa_assert(u);

    if (!strcmp(u->source->name, data->source->name)) {
        // Signal Ready when a Source Output is connected
        if (!u->IsReady)
            u->IsReady = true;
        pa_source_suspend(u->source, false, PA_SUSPEND_IDLE);
    }
    return PA_HOOK_OK;
}

static void thread_func(void *userdata) {
    struct userdata *u = userdata;
    bool timer_elapsed = false;

    pa_assert(u);

    if (u->core->realtime_scheduling)
        pa_thread_make_realtime(u->core->realtime_priority);

    pa_thread_mq_install(&u->thread_mq);
    u->timestamp = pa_rtclock_now();
    MEDIA_DEBUG_LOG("HDI Source: u->timestamp : %{public}llu", u->timestamp);

    for (;;) {
        int ret = 0;
        int32_t retries = 0;
        uint64_t requestBytes;
        uint64_t replyBytes = 0;
        void *p;

        MEDIA_DEBUG_LOG("HDI Source: replyBytes before read : %{public}llu", replyBytes);
        if (PA_SOURCE_IS_OPENED(u->source->thread_info.state) &&
           (u->source->thread_info.state != PA_SOURCE_SUSPENDED && u->IsReady)) {
            MEDIA_DEBUG_LOG("HDI Source: PA_SOURCE_IS_OPENED");
            pa_usec_t now;
            pa_memchunk chunk;

            now = pa_rtclock_now();
            MEDIA_DEBUG_LOG("HDI Source: now : %{public}llu", now);
            MEDIA_DEBUG_LOG("HDI Source: Is timer_elapsed : %{public}d", timer_elapsed);

            if (timer_elapsed && (chunk.length = pa_usec_to_bytes(now - u->timestamp, &u->source->sample_spec)) > 0) {
                chunk.length = u->buffer_size;
                MEDIA_DEBUG_LOG("HDI Source: chunk.length = u->buffer_size: %{public}zu", chunk.length);
                chunk.memblock = pa_memblock_new(u->core->mempool, chunk.length);
                pa_assert(chunk.memblock);
                p = pa_memblock_acquire(chunk.memblock);
                pa_assert(p);

                requestBytes = pa_memblock_get_length(chunk.memblock);
                AudioCapturerSourceFrame((char *) p, (uint64_t)requestBytes, &replyBytes);

                pa_memblock_release(chunk.memblock);
                MEDIA_DEBUG_LOG("HDI Source: request bytes: %{public}llu, replyBytes: %{public}llu", requestBytes, replyBytes);
                if(replyBytes > requestBytes) {
                    MEDIA_ERR_LOG("HDI Source: Error replyBytes > requestBytes. Requested data Length: %{public}llu, Read: %{public}llu bytes, %{public}d ret", requestBytes, replyBytes, ret);
                    pa_memblock_unref(chunk.memblock);
                    break;
                }
                if (replyBytes == 0) {
                    MEDIA_INFO_LOG("HDI Source: reply bytes 0");
                    if (retries < MAX_RETRIES) {
                        usleep(FIVE_MSEC);
                        ++retries;
                        MEDIA_DEBUG_LOG("HDI Source: Requested data Length: %{public}llu bytes, Read: %{public}llu bytes. Sleep: %{public}d usec microseconds. Retry Times %{public}d", requestBytes, replyBytes, FIVE_MSEC, retries);
                        continue;
                    } else {
                        MEDIA_ERR_LOG("HDI Source: Failed to read after %{public}d retries, Requested data Length: %{public}llu bytes, Read: %{public}llu bytes, %{public}d ret", retries, requestBytes, replyBytes, ret);
                        pa_memblock_unref(chunk.memblock);
                        break;
                    }
                }

                retries = 0;
                chunk.index = 0;
                chunk.length = replyBytes;
                pa_source_post(u->source, &chunk);
                pa_memblock_unref(chunk.memblock);
                u->timestamp += pa_bytes_to_usec(chunk.length, &u->source->sample_spec);
                MEDIA_INFO_LOG("HDI Source: new u->timestamp : %{public}llu", u->timestamp);
            }

            pa_rtpoll_set_timer_absolute(u->rtpoll, u->timestamp + u->block_usec);
        } else {
            pa_rtpoll_set_timer_disabled(u->rtpoll);
            MEDIA_INFO_LOG("HDI Source: pa_rtpoll_set_timer_disabled done ");
        }

        /* Hmm, nothing to do. Let's sleep */
        if ((ret = pa_rtpoll_run(u->rtpoll)) < 0) {
            /* If this was no regular exit from the loop we have to continue
            * processing messages until we received PA_MESSAGE_SHUTDOWN */
            MEDIA_INFO_LOG("HDI Source: pa_rtpoll_run ret:%{public}d failed", ret );
            pa_asyncmsgq_post(u->thread_mq.outq, PA_MSGOBJECT(u->core), PA_CORE_MESSAGE_UNLOAD_MODULE, u->module, 0, NULL, NULL);
            pa_asyncmsgq_wait_for(u->thread_mq.inq, PA_MESSAGE_SHUTDOWN);
            return;
        }

        timer_elapsed = pa_rtpoll_timer_elapsed(u->rtpoll);

        if (ret == 0) {
            MEDIA_INFO_LOG("HDI Source: pa_rtpoll_run ret:%{public}d return", ret );
            return;
        }
    }
}

pa_source *pa_hdi_source_new(pa_module *m, pa_modargs *ma, const char*driver) {
    struct userdata *u = NULL;
    pa_sample_spec ss;
    char *thread_name = NULL;
    pa_channel_map map;
    pa_source_new_data data;
    int32_t ret;

    pa_assert(m);
    pa_assert(ma);

    ss = m->core->default_sample_spec;
    map = m->core->default_channel_map;

    /* Override with modargs if provided */
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &map, PA_CHANNEL_MAP_DEFAULT) < 0) {
        MEDIA_INFO_LOG("Failed to parse sample specification and channel map");
        goto fail;
    }

    u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->rtpoll = pa_rtpoll_new();

    // Set IsReady to false at start. will be made true when a Source Output is connected
    u->IsReady = false;

    if (pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll) < 0) {
        MEDIA_INFO_LOG("pa_thread_mq_init() failed.");
        goto fail;
    }

    u->buffer_size = DEFAULT_BUFFER_SIZE;

    AudioSourceAttr attrs;
    attrs.format = AUDIO_FORMAT_PCM_16_BIT;
    attrs.channel = ss.channels;
    attrs.sampleRate = ss.rate;

    MEDIA_INFO_LOG("AudioDeviceCreateCapture format: %{public}d, channel: %{public}d, sampleRate: %{public}d",
           attrs.format, attrs.channel, attrs.sampleRate);

    ret = AudioCapturerSourceInit(&attrs);
    if (ret != 0) {
        MEDIA_INFO_LOG("Audio capture init failed!");
        goto fail;
    }

    ret = AudioCapturerSourceStart();
    if (ret != 0) {
        MEDIA_INFO_LOG("Audio capture start failed!");
        AudioCapturerSourceDeInit();
        goto fail;
    }

    ret = AudioCapturerSourceSetVolume(VOLUME_VALUE, VOLUME_VALUE);
    if (ret != 0) {
        MEDIA_INFO_LOG("audio capture set volume failed!");
        AudioCapturerSourceStop();
        AudioCapturerSourceDeInit();
        goto fail;
    }

    pa_source_new_data_init(&data);
    data.driver = __FILE__;
    data.module = m;
    pa_source_new_data_set_name(&data, pa_modargs_get_value(ma, "source_name", DEFAULT_SOURCE_NAME));
    pa_proplist_sets(data.proplist, PA_PROP_DEVICE_STRING, DEFAULT_AUDIO_DEVICE_NAME);
    pa_proplist_setf(data.proplist, PA_PROP_DEVICE_DESCRIPTION, "HDI source %s", DEFAULT_AUDIO_DEVICE_NAME);
    pa_source_new_data_set_sample_spec(&data, &ss);
    pa_source_new_data_set_channel_map(&data, &map);;
    pa_proplist_setf(data.proplist, PA_PROP_DEVICE_BUFFERING_BUFFER_SIZE, "%lu", (unsigned long)u->buffer_size);

    if (pa_modargs_get_proplist(ma, "source_properties", data.proplist, PA_UPDATE_REPLACE) < 0) {
        MEDIA_INFO_LOG("Invalid properties");
        pa_source_new_data_done(&data);
        goto fail;
    }

    u->source = pa_source_new(m->core, &data, PA_SOURCE_HARDWARE|PA_SOURCE_LATENCY);
    pa_source_new_data_done(&data);

    if (!u->source) {
        MEDIA_INFO_LOG("Failed to create source object");
        goto fail;
    }

    u->source->parent.process_msg = source_process_msg;
    u->source->set_state_in_io_thread = source_set_state_in_io_thread_cb;
    u->source->userdata = u;

    pa_source_set_asyncmsgq(u->source, u->thread_mq.inq);
    pa_source_set_rtpoll(u->source, u->rtpoll);

    if (pa_modargs_get_value_u32(ma, "buffer_size", &u->buffer_size) < 0) {
        MEDIA_INFO_LOG("Failed to parse buffer_size argument.");
        goto fail;
    }

    u->block_usec = pa_bytes_to_usec(u->buffer_size, &u->source->sample_spec);
    pa_source_set_latency_range(u->source, 0, u->block_usec);
    u->source->thread_info.max_rewind = pa_usec_to_bytes(u->block_usec, &u->source->sample_spec);

    pa_module_hook_connect(m, &m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_FIXATE], PA_HOOK_NORMAL,
                          (pa_hook_cb_t) source_output_fixate_hook_cb, u);

    thread_name = pa_sprintf_malloc("hdi-source-record");
    if (!(u->thread = pa_thread_new(thread_name, thread_func, u))) {
        MEDIA_INFO_LOG("Failed to create thread.");
        goto fail;
    }
    pa_xfree(thread_name);
    thread_name = NULL;
    pa_source_put(u->source);
    return u->source;

fail:
    pa_xfree(thread_name);

    if (u)
        userdata_free(u);

    return NULL;
}

void pa_hdi_source_free(pa_source *s) {
    struct userdata *u;
    pa_source_assert_ref(s);
    pa_assert_se(u = s->userdata);
    userdata_free(u);
}
