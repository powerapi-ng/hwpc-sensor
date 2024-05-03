/*
 *  Copyright (c) 2018, INRIA
 *  Copyright (c) 2018, University of Lille
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <czmq.h>
#include <perfmon/pfmlib_perf_event.h>
#include <stdlib.h>

#include "events.h"
#include "util.h"

static int
setup_perf_event_attr(const char *event_name, struct perf_event_attr *attr)
{
    pfm_perf_encode_arg_t arg = {0};

    attr->size = sizeof(struct perf_event_attr);
    attr->disabled = 1;
    attr->read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_GROUP;

    arg.size = sizeof(pfm_perf_encode_arg_t);
    arg.attr = attr;
    if (pfm_get_os_event_encoding(event_name, PFM_PLM0 | PFM_PLM3, PFM_OS_PERF_EVENT_EXT, &arg) != PFM_SUCCESS) {
        return -1;
    }

    return 0;
}

struct event_config *
event_config_create(const char *event_name)
{
    struct perf_event_attr attr = {0};
    struct event_config *config = NULL;

    if (!setup_perf_event_attr(event_name, &attr)) {
        config = malloc(sizeof(struct event_config));
        if (config) {
            snprintf(config->name, NAME_MAX, "%s", event_name);
            config->attr = attr;
        }
    }

    return config;
}

struct event_config *
event_config_dup(struct event_config *config)
{
    struct event_config *copy = NULL;

    if (config) {
        copy = malloc(sizeof(struct event_config));
        if (copy) {
            snprintf(copy->name, NAME_MAX, "%s", config->name);
            copy->attr = config->attr;
        }
    }

    return copy;
}

void
event_config_destroy(struct event_config **config)
{
    if (!*config)
        return;

    free(*config);
}

struct events_group *
events_group_create(const char *name)
{
    struct events_group *group = malloc(sizeof(struct events_group));

    if (group) {
        snprintf(group->name, NAME_MAX, "%s", name);
        group->type = MONITOR_ALL_CPU_PER_SOCKET; /* by default, monitor all cpu of the available socket(s) */

        group->events = zlistx_new();
        zlistx_set_duplicator(group->events, (zlistx_duplicator_fn *) event_config_dup);
        zlistx_set_destructor(group->events, (zlistx_destructor_fn *) event_config_destroy);
    }

    return group;
}

struct events_group *
events_group_dup(struct events_group *group)
{
    struct events_group *copy = NULL;

    if (group) {
        copy = malloc(sizeof(struct events_group));
        if (copy) {
            snprintf(copy->name, NAME_MAX, "%s", group->name);
            copy->type = group->type;
            copy->events = zlistx_dup(group->events);
        }
    }

    return copy;
}

int
events_group_append_event(struct events_group *group, const char *event_name)
{
    int ret = -1;
    struct event_config *event = NULL;

    if (group) {
        event = event_config_create(event_name);
        if (event) {
            zlistx_add_end(group->events, event);
            free(event);
            ret = 0;
        }
    }

    return ret;
}

void
events_group_destroy(struct events_group **group)
{
    if (*group) {
        zlistx_destroy(&(*group)->events);
        free(*group);
    }
}

