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
setup_msr_perf_event_attr_type(struct perf_event_attr *attr)
{
    const char pmu_type_path[PATH_MAX] = "/sys/devices/msr/type";
    FILE *f = NULL;
    char buffer[16] = {0}; /* uint32 expected */
    unsigned int pmu_type = 0;
    int ret = -1;

    f = fopen(pmu_type_path, "r");
    if (f) {
        if (fgets(buffer, sizeof(buffer), f)) {
            if (!str_to_uint(buffer, &pmu_type)) {
                attr->type = pmu_type;
                ret = 0;
            }
        }

        fclose(f);
    }

    return ret;
}

static int
setup_msr_perf_event_attr_config(const char *event_name, struct perf_event_attr *attr)
{
    /* events config from: https://github.com/torvalds/linux/blob/master/arch/x86/events/msr.c */
    const char *msr_events_name[] = {"tsc", "aperf", "mperf", "pperf", "smi", "ptsc", "irperf", "cpu_thermal_margin"};
    const uint64_t msr_events_config[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    const size_t num_msr_events = sizeof(msr_events_name) / sizeof(msr_events_name[0]);
    char event_path[PATH_MAX] = {0};

    for (size_t i = 0; i < num_msr_events; i++) {
        if (!strcasecmp(event_name, msr_events_name[i])) {
            snprintf(event_path, PATH_MAX, "/sys/devices/msr/events/%s", msr_events_name[i]);
            if (!access(event_path, F_OK)) {
                attr->config = msr_events_config[i];
                return 0;
            }
        }
    }

    return -1;
}

static int
get_msr_pmu_event_encoding(const char *event_name, struct perf_event_attr *attr)
{
    attr->size = sizeof(struct perf_event_attr);
    attr->disabled = 1;
    attr->read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_GROUP;

    if (setup_msr_perf_event_attr_type(attr)) return -1;
    if (setup_msr_perf_event_attr_config(event_name, attr)) return -1;

    return 0;
}

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
        /* fallback to MSR PMU event encoding if libpfm fails */
        if (get_msr_pmu_event_encoding(event_name, attr)) {
            return -1;
        }
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

