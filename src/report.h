#ifndef REPORT_H
#define REPORT_H

#include <czmq.h>
#include <stdint.h>
#include <mongoc.h>

/*
 * report_config stores the reporting module configuration.
 */
struct report_config
{
    struct storage_module *storage;
};

/*
 * report_context stores the reporting module execution context.
 */
struct report_context
{
    struct report_config *config;
    bool terminated;
    zsock_t *pipe;
    zsock_t *reporting;
    zpoller_t *poller;
};

/*
 * report_payload stores the required information for the storage module to handle events reports.
 */
struct report_payload
{
    uint64_t timestamp;
    char *cgroup_name;
    struct events_config *events;
    zhashx_t *reports; /* unsigned int *cpu_id -> struct perf_cpu_report *report */
};

/*
 * report_payload_create allocate and set the required resources of a report payload.
 */
struct report_payload *report_payload_create(uint64_t timestamp, char *cgroup_name, struct events_config *events);

/*
 * report_payload_destroy free the allocated resources of the report payload.
 */
void report_payload_destroy(struct report_payload *rpl);

/*
 * reporting_actor is the reporting actor entrypoint.
 */
void reporting_actor(zsock_t *pipe, void *args);

#endif /* REPORT_H */

