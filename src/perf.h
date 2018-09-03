#ifndef PERF_H
#define PERF_H

#include <czmq.h>
#include "config.h"
#include "hwinfo.h"
#include "events.h"

struct perf_config
{
    struct hwinfo *hwinfo;
    struct events_config *events;
    char *cgroup_name;
    char *cgroup_path;
    bool one_cpu_per_pkg;
};

struct perf_context
{
    struct perf_config *config;
    bool terminated;
    zsock_t *pipe;
    zsock_t *ticker;
    zpoller_t *poller;
    zsock_t *reporting;
    int cgroup_fd;
    zhashx_t *perf_fds;
};

struct perf_report_value {
    uint64_t value;
};

struct perf_cpu_report {
    uint64_t nr;
    uint64_t time_enabled; /* PERF_FORMAT_TOTAL_TIME_ENABLED flag */
    uint64_t time_running; /* PERF_FORMAT_TOTAL_TIME_RUNNING */
    struct perf_report_value values[];
};

/*
 * perf_config_create
 */
struct perf_config *perf_config_create(struct hwinfo *hwinfo, struct events_config *events, const char *cgroup_name, const char *cgroup_path, bool one_cpu_per_pkg);

/*
 * perf_config_destroy
 */
void perf_config_destroy(struct perf_config *config);

/*
 * perf_monitoring_actor handle the monitoring of a cgroup using perf_event. 
 */
void perf_monitoring_actor(zsock_t *pipe, void *args);

#endif /* PERF_H */

