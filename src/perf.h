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

#ifndef PERF_H
#define PERF_H

#include <czmq.h>
#include "hwinfo.h"
#include "events.h"

/*
 * perf_config stores the configuration of a perf actor.
 */
struct perf_config
{
    struct hwinfo *hwinfo;
    zhashx_t *events_groups; /* char *group_name -> struct events_group *group_config */
    struct target *target;
};

/*
 * perf_group_cpu_context stores the context of an events group for a specific cpu.
 */
struct perf_group_cpu_context
{
    zlistx_t *perf_fds; /* int *fd */
};

/*
 * perf_group_pkg_context stores the context of an events group for a specific package.
 */
struct perf_group_pkg_context
{
    zhashx_t *cpus_ctx; /* char *cpu_id -> struct perf_group_cpu_context *cpu_ctx */
};

/*
 * perf_group_context stores the context of an events group.
 */
struct perf_group_context
{
    struct events_group *config;
    zhashx_t *pkgs_ctx; /* char *pkg_id -> struct perf_group_pkg_context *pkg_ctx */
};

/*
 * perf_context stores the context of a perf actor.
 */
struct perf_context
{
    struct perf_config *config;
    const char *target_name;
    bool terminated;
    zsock_t *pipe;
    zsock_t *ticker;
    zpoller_t *poller;
    zsock_t *reporting;
    int cgroup_fd;
    zhashx_t *groups_ctx; /* char *group_name -> struct perf_group_context *group_ctx */
};

/*
 * perf_counter_value stores the counter value.
 */
struct perf_counter_value {
    uint64_t value;
};

/*
 * perf_cpu_report stores the events counter value.
 */
struct perf_read_format {
    uint64_t nr;
    uint64_t time_enabled; /* PERF_FORMAT_TOTAL_TIME_ENABLED flag */
    uint64_t time_running; /* PERF_FORMAT_TOTAL_TIME_RUNNING flag */
    struct perf_counter_value values[];
};

/*
 * perf_config_create allocate and configure a perf configuration structure.
 */
struct perf_config *perf_config_create(struct hwinfo *hwinfo, zhashx_t *events_groups, struct target *target);

/*
 * perf_config_destroy free the resources allocated for the perf configuration structure.
 */
void perf_config_destroy(struct perf_config *config);

/*
 * perf_monitoring_actor handle the monitoring of a cgroup using perf_event. 
 */
void perf_monitoring_actor(zsock_t *pipe, void *args);

#endif /* PERF_H */

