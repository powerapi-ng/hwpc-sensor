#ifndef PERF_H
#define PERF_H

#include <czmq.h>
#include "config.h"
#include "hwinfo.h"
#include "events.h"

/*
 * perf_target_type stores the supported target types by the module.
 */
enum perf_target_type
{
    PERF_TARGET_SYSTEM,
    PERF_TARGET_DOCKER,
    PERF_TARGET_LIBVIRT,
    PERF_TARGET_UNKNOWN
};

/*
 * perf_config stores the configuration of a perf actor.
 */
struct perf_config
{
    struct hwinfo *hwinfo;
    zhashx_t *events_groups; /* char *group_name -> struct events_group *group_config */
    char *cgroup_name;
    char *cgroup_path;
    enum perf_target_type target_type;
    char *target_name;
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
struct perf_config *perf_config_create(struct hwinfo *hwinfo, zhashx_t *events_groups, const char *cgroup_name, const char *cgroup_path);

/*
 * perf_config_destroy free the resources allocated for the perf configuration structure.
 */
void perf_config_destroy(struct perf_config *config);

/*
 * perf_monitoring_actor handle the monitoring of a cgroup using perf_event. 
 */
void perf_monitoring_actor(zsock_t *pipe, void *args);

#endif /* PERF_H */

