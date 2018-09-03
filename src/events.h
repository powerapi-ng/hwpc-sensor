#ifndef EVENTS_H
#define EVENTS_H

#include <perfmon/pfmlib_perf_event.h>

/*
 * event_attr stores information about the perf event.
 */
struct event_attr
{
    char *name;
    struct perf_event_attr perf_attr;
};

/*
 * events_config is the events attributes container.
 */
struct events_config
{
    size_t num_attrs;
    struct event_attr *attrs;
};

/*
 * events_config_create allocated the required resources for the events config container.
 */
struct events_config *events_config_create();

/*
 * events_config_destroy free the allocated resources of the events config container.
 */
void events_config_destroy(struct events_config *events);

/*
 * events_config_add get the event configuration from its name (if the event is available) and store it into the container.
 */
int events_config_add(struct events_config *events, char *event_name);

#endif /* EVENTS_H */

