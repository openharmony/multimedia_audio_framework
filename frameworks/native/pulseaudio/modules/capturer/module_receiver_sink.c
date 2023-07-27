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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/xmalloc.h>

#include <pulsecore/namereg.h>
#include <pulsecore/sink.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/rtpoll.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>

#include "securec.h"
#include "audio_log.h"

PA_MODULE_AUTHOR("OpenHarmony");
PA_MODULE_DESCRIPTION("Virtual channel sink");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(false);
PA_MODULE_USAGE(
        "sink_name=<name for the sink> "
        "master=<name of sink to remap> "
);

#define DEFAULT_BUFFER_SIZE 8192

struct userdata {
    pa_module *module;

    pa_sink *sink;
    pa_sink_input *sink_input;

    uint32_t buffer_size;
    pa_core *core;

    pa_usec_t block_usec;
    pa_usec_t timestamp;

    pa_rtpoll *rtpoll;
    pa_thread *thread;
    pa_thread_mq thread_mq;

    pa_sample_spec sampleSpec;
    pa_channel_map sinkMap;

    bool auto_desc;
};

static const char * const VALID_MODARGS[] = {
    "sink_name",
    "master",
    "buffer_size",
    NULL
};

/* Called from I/O thread context */
static int SinkPorcessMsg(pa_msgobject *o, int code, void *data, int64_t offset, pa_memchunk *chunk)
{
    struct userdata *u = PA_SINK(o)->userdata;

    switch (code) {
        case PA_SINK_MESSAGE_GET_LATENCY:
            /* The sink is _put() before the sink input is, so let's
             * make sure we don't access it yet */
            if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
                !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state)) {
                *((int64_t*) data) = 0;
                return 0;
            }

            *((int64_t*) data) =
                /* Get the latency of the master sink */
                pa_sink_get_latency_within_thread(u->sink_input->sink, true) +

                /* Add the latency internal to our sink input on top */
                pa_bytes_to_usec(pa_memblockq_get_length(u->sink_input->thread_info.render_memblockq),
                    &u->sink_input->sink->sample_spec);
            return 0;
        default:
            break;
    }
    return pa_sink_process_msg(o, code, data, offset, chunk);
}

/* Called from main context */
static int SinkSetStateInMainThread(pa_sink *s, pa_sink_state_t state, pa_suspend_cause_t suspend_cause)
{
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(state) ||
        !PA_SINK_INPUT_IS_LINKED(u->sink_input->state)) {
        return 0;
    }

    pa_sink_input_cork(u->sink_input, state == PA_SINK_SUSPENDED);
    return 0;
}

/* Called from the IO thread. */
static int SinkSetStateInIoThreadCb(pa_sink *s, pa_sink_state_t new_state, pa_suspend_cause_t new_suspend_cause)
{
    struct userdata *u;

    pa_assert(s);
    pa_assert_se(u = s->userdata);

    /* When set to running or idle for the first time, request a rewind
     * of the master sink to make sure we are heard immediately */
    if (PA_SINK_IS_OPENED(new_state) && s->thread_info.state == PA_SINK_INIT) {
        AUDIO_DEBUG_LOG("Requesting rewind due to state change.");
        pa_sink_input_request_rewind(u->sink_input, 0, false, true, true);
    }

    return 0;
}

/* Called from I/O thread context */
static void SinkRequestRewind(pa_sink *s)
{
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
        !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state)) {
        return;
    }

    pa_sink_input_request_rewind(u->sink_input, s->thread_info.rewind_nbytes, true, false, false);
}

/* Called from I/O thread context */
static void SinkUpdateRequestedLatency(pa_sink *s)
{
    struct userdata *u;

    pa_sink_assert_ref(s);
    pa_assert_se(u = s->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state) ||
        !PA_SINK_INPUT_IS_LINKED(u->sink_input->thread_info.state)) {
        return;
    }

    /* Just hand this one over to the master sink */
    pa_sink_input_set_requested_latency_within_thread(
        u->sink_input, pa_sink_get_requested_latency_within_thread(s));
}

/* Called from I/O thread context */
static int SinkInputPopCb(pa_sink_input *i, size_t nbytes, pa_memchunk *chunk)
{
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert(chunk);
    pa_assert_se(u = i->userdata);

    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state)) {
        return -1;
    }

    /* Hmm, process any rewind request that might be queued up */
    pa_sink_process_rewind(u->sink, 0);

    pa_sink_render(u->sink, nbytes, chunk);
    return 0;
}

/* Called from I/O thread context */
static void SinkInputProcessRewindCb(pa_sink_input *i, size_t nbytes)
{
    size_t amount = 0;
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* If the sink is not yet linked, there is nothing to rewind */
    if (!PA_SINK_IS_LINKED(u->sink->thread_info.state)) {
        return;
    }

    if (u->sink->thread_info.rewind_nbytes > 0) {
        amount = PA_MIN(u->sink->thread_info.rewind_nbytes, nbytes);
        u->sink->thread_info.rewind_nbytes = 0;
    }

    pa_sink_process_rewind(u->sink, amount);
}

/* Called from I/O thread context */
static void SinkInputUpdateMaxRewindCb(pa_sink_input *i, size_t nbytes)
{
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_max_rewind_within_thread(u->sink, nbytes);
}

/* Called from I/O thread context */
static void SinkInputUpdateMaxRequestCb(pa_sink_input *i, size_t nbytes)
{
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_max_request_within_thread(u->sink, nbytes);
}

/* Called from I/O thread context */
static void SinkInputUpdateSinkLatencyRangeCb(pa_sink_input *i)
{
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_latency_range_within_thread(u->sink, i->sink->thread_info.min_latency,
        i->sink->thread_info.max_latency);
}

/* Called from I/O thread context */
static void SinkInputUpdateSinkFixedLatencyCb(pa_sink_input *i)
{
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_fixed_latency_within_thread(u->sink, i->sink->thread_info.fixed_latency);
}

/* Called from I/O thread context */
static void SinkInputDetachCb(pa_sink_input *i)
{
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (PA_SINK_IS_LINKED(u->sink->thread_info.state)) {
        pa_sink_detach_within_thread(u->sink);
    }

    pa_sink_set_rtpoll(u->sink, NULL);
}

/* Called from I/O thread context */
static void SinkInputAttachCb(pa_sink_input *i)
{
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_sink_set_rtpoll(u->sink, i->sink->thread_info.rtpoll);
    pa_sink_set_latency_range_within_thread(u->sink, i->sink->thread_info.min_latency,
        i->sink->thread_info.max_latency);
    pa_sink_set_fixed_latency_within_thread(u->sink, i->sink->thread_info.fixed_latency);
    pa_sink_set_max_request_within_thread(u->sink, pa_sink_input_get_max_request(i));
    pa_sink_set_max_rewind_within_thread(u->sink, pa_sink_input_get_max_rewind(i));

    if (PA_SINK_IS_LINKED(u->sink->thread_info.state)) {
        pa_sink_attach_within_thread(u->sink);
    }
}

/* Called from main context */
static void SinkInputKillCb(pa_sink_input *i)
{
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    /* The order here matters! We first kill the sink so that streams
     * can properly be moved away while the sink input is still connected
     * to the master. */
    pa_sink_input_cork(u->sink_input, true);
    pa_sink_unlink(u->sink);
    pa_sink_input_unlink(u->sink_input);

    pa_sink_input_unref(u->sink_input);
    u->sink_input = NULL;

    pa_sink_unref(u->sink);
    u->sink = NULL;

    pa_module_unload_request(u->module, true);
}

/* Called from main context */
static void SinkInputMovingCb(pa_sink_input *i, pa_sink *dest)
{
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (dest) {
        pa_sink_set_asyncmsgq(u->sink, dest->asyncmsgq);
        pa_sink_update_flags(u->sink, PA_SINK_LATENCY|PA_SINK_DYNAMIC_LATENCY, dest->flags);
    } else {
        pa_sink_set_asyncmsgq(u->sink, NULL);
    }

    if (u->auto_desc && dest) {
        const char *k;
        pa_proplist *pl;

        pl = pa_proplist_new();
        k = pa_proplist_gets(dest->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(pl, PA_PROP_DEVICE_DESCRIPTION, "Remapped %s", k ? k : dest->name);

        pa_sink_update_proplist(u->sink, PA_UPDATE_REPLACE, pl);
        pa_proplist_free(pl);
    }
}

int InitFail(pa_module *m, pa_modargs *ma)
{
    AUDIO_ERR_LOG("Receiver sink init failed.");
    if (ma)
        pa_modargs_free(ma);
    pa__done(m);
    return -1;
}

int CreateSink(pa_module *m, pa_modargs *ma, pa_sink *master, struct userdata *u)
{
    pa_sample_spec ss;
    pa_channel_map sinkMap;
    pa_sink_new_data sinkData;

    ss = master->sample_spec;
    sinkMap = master->channel_map;
    if (pa_modargs_get_sample_spec_and_channel_map(ma, &ss, &sinkMap, PA_CHANNEL_MAP_DEFAULT) < 0) {
        AUDIO_ERR_LOG("Invalid sample format specification or channel map");
        return -1;
    }

    pa_sink_new_data_init(&sinkData);
    sinkData.driver = __FILE__;
    sinkData.module = m;
    if (!(sinkData.name = pa_xstrdup(pa_modargs_get_value(ma, "sink_name", NULL))))
        sinkData.name = pa_sprintf_malloc("%s.remapped", master->name);
    pa_sink_new_data_set_sample_spec(&sinkData, &ss);
    pa_sink_new_data_set_channel_map(&sinkData, &sinkMap);
    pa_proplist_sets(sinkData.proplist, PA_PROP_DEVICE_MASTER_DEVICE, master->name);
    pa_proplist_sets(sinkData.proplist, PA_PROP_DEVICE_CLASS, "filter");
    pa_proplist_sets(sinkData.proplist, PA_PROP_DEVICE_STRING, "receiver");

    if (pa_modargs_get_proplist(ma, "sink_properties", sinkData.proplist, PA_UPDATE_REPLACE) < 0) {
        AUDIO_ERR_LOG("Invalid properties");
        pa_sink_new_data_done(&sinkData);
        return -1;
    }

    if ((u->auto_desc = !pa_proplist_contains(sinkData.proplist, PA_PROP_DEVICE_DESCRIPTION))) {
        const char *k;

        k = pa_proplist_gets(master->proplist, PA_PROP_DEVICE_DESCRIPTION);
        pa_proplist_setf(sinkData.proplist, PA_PROP_DEVICE_DESCRIPTION, "Remapped %s", k ? k : master->name);
    }

    u->sink = pa_sink_new(m->core, &sinkData, master->flags & (PA_SINK_LATENCY|PA_SINK_DYNAMIC_LATENCY));
    pa_sink_new_data_done(&sinkData);

    if (!u->sink) {
        return -1;
    }

    u->sink->parent.process_msg = SinkPorcessMsg;
    u->sink->set_state_in_main_thread = SinkSetStateInMainThread;
    u->sink->set_state_in_io_thread = SinkSetStateInIoThreadCb;
    u->sink->update_requested_latency = SinkUpdateRequestedLatency;
    u->sink->request_rewind = SinkRequestRewind;
    u->sink->userdata = u;
    u->sampleSpec = ss;
    u->sinkMap = sinkMap;

    pa_sink_set_asyncmsgq(u->sink, master->asyncmsgq);
    return 0;
}

int CreateSinkInput(pa_module *m, pa_modargs *ma, pa_sink *master, struct userdata *u)
{
    pa_sink_input_new_data sinkInputData;
    pa_resample_method_t resampleMethod = PA_RESAMPLER_SRC_SINC_FASTEST;

    if (pa_modargs_get_resample_method(ma, &resampleMethod) < 0) {
        AUDIO_ERR_LOG("Invalid resampling method");
        return -1;
    }

    if (u->sinkMap.channels != u->sampleSpec.channels) {
        AUDIO_ERR_LOG("Number of channels doesn't match");
        return -1;
    }

    if (pa_channel_map_equal(&u->sinkMap, &master->channel_map)) {
        AUDIO_WARNING_LOG("Number of channels doesn't match of [%{public}s] and [%{public}s]",
            u->sink->name, master->name);
    }

    pa_sink_input_new_data_init(&sinkInputData);
    sinkInputData.driver = __FILE__;
    sinkInputData.module = m;
    pa_sink_input_new_data_set_sink(&sinkInputData, master, false, true);
    sinkInputData.origin_sink = u->sink;
    pa_proplist_sets(sinkInputData.proplist, PA_PROP_MEDIA_NAME, "Remapped Stream");
    pa_proplist_sets(sinkInputData.proplist, PA_PROP_MEDIA_ROLE, "filter");
    pa_sink_input_new_data_set_sample_spec(&sinkInputData, &u->sampleSpec);
    pa_sink_input_new_data_set_channel_map(&sinkInputData, &u->sinkMap);
    sinkInputData.flags = PA_SINK_INPUT_START_CORKED;
    sinkInputData.resample_method = PA_RESAMPLER_SRC_SINC_FASTEST;

    pa_sink_input_new(&u->sink_input, m->core, &sinkInputData);
    pa_sink_input_new_data_done(&sinkInputData);

    if (!u->sink_input) {
        return -1;
    }

    u->sink_input->pop = SinkInputPopCb;
    u->sink_input->process_rewind = SinkInputProcessRewindCb;
    u->sink_input->update_max_rewind = SinkInputUpdateMaxRewindCb;
    u->sink_input->update_max_request = SinkInputUpdateMaxRequestCb;
    u->sink_input->update_sink_latency_range = SinkInputUpdateSinkLatencyRangeCb;
    u->sink_input->update_sink_fixed_latency = SinkInputUpdateSinkFixedLatencyCb;
    u->sink_input->attach = SinkInputAttachCb;
    u->sink_input->detach = SinkInputDetachCb;
    u->sink_input->kill = SinkInputKillCb;
    u->sink_input->moving = SinkInputMovingCb;
    u->sink_input->userdata = u;

    u->sink->input_to_master = u->sink_input;
    return 0;
}

int pa__init(pa_module *m)
{
    struct userdata *u;
    pa_modargs *ma;
    pa_sink *master;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, VALID_MODARGS))) {
        AUDIO_ERR_LOG("Failed to parse module arguments.");
        return InitFail(m, ma);
    }

    if (!(master = pa_namereg_get(m->core, pa_modargs_get_value(ma, "master", NULL), PA_NAMEREG_SINK))) {
        pa_log("Master sink not found");
        return InitFail(m, ma);
    }

    u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    m->userdata = u;
    u->rtpoll = pa_rtpoll_new();

    if (pa_thread_mq_init(&u->thread_mq, m->core->mainloop, u->rtpoll) < 0) {
        AUDIO_ERR_LOG("pa_thread_mq_init failed.");
        return InitFail(m, ma);
    }

    /* Create sink */
    if (CreateSink(m, ma, master, u) < 0) {
        AUDIO_ERR_LOG("CreateSink failed.");
        return InitFail(m, ma);
    }
    pa_sink_set_rtpoll(u->sink, u->rtpoll);
    u->buffer_size = DEFAULT_BUFFER_SIZE;
    if (pa_modargs_get_value_u32(ma, "buffer_size", &u->buffer_size) < 0) {
        AUDIO_ERR_LOG("Failed to get buffer_size argument in effect sink");
        return InitFail(m, ma);
    }
    u->block_usec = pa_bytes_to_usec(u->buffer_size, &u->sink->sample_spec);
    pa_sink_set_latency_range(u->sink, 0, u->block_usec);
    pa_sink_set_max_request(u->sink, u->buffer_size);

    /* Create sink input */
    if (CreateSinkInput(m, ma, master, u) < 0) {
        AUDIO_ERR_LOG("CreateSinkInput failed.");
        return InitFail(m, ma);
    }

    /* The order here is important. The input must be put first,
     * otherwise streams might attach to the sink before the sink
     * input is attached to the master. */
    pa_sink_input_put(u->sink_input);
    pa_sink_put(u->sink);
    pa_sink_input_cork(u->sink_input, false);

    pa_modargs_free(ma);

    return 0;
}

int pa__get_n_used(pa_module *m)
{
    struct userdata *u;

    pa_assert(m);
    pa_assert_se(u = m->userdata);

    return pa_sink_linked_by(u->sink);
}

void pa__done(pa_module *m)
{
    struct userdata *u;

    pa_assert(m);

    if (!(u = m->userdata)) {
        return;
    }

    /* See comments in SinkInputKillCb() above regarding
     * destruction order! */

    if (u->sink_input) {
        pa_sink_input_cork(u->sink_input, true);
    }

    if (u->sink) {
        pa_sink_unlink(u->sink);
    }

    if (u->thread) {
        pa_asyncmsgq_send(u->thread_mq.inq, NULL, PA_MESSAGE_SHUTDOWN, NULL, 0, NULL);
        pa_thread_free(u->thread);
    }
    pa_thread_mq_done(&u->thread_mq);

    if (u->sink_input) {
        pa_sink_input_unlink(u->sink_input);
        pa_sink_input_unref(u->sink_input);
    }

    if (u->sink) {
        pa_sink_unref(u->sink);
    }

    if (u->rtpoll) {
        pa_rtpoll_free(u->rtpoll);
    }

    pa_xfree(u);
}
