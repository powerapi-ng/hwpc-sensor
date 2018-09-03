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
 * report_config_create allocate the resource of a report configuration structure.
 */
struct report_config *report_config_create(struct storage_module *storage_module);

/*
 * report_config_destroy free the allocated resource of the report configuration structure.
 */
void report_config_destroy(struct report_config *config);

/*
 * reporting_actor is the reporting actor entrypoint.
 */
void reporting_actor(zsock_t *pipe, void *args);

#endif /* REPORT_H */

