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
 * events_group_monitoring_type stores the possible monitoring type of an events group.
 */
enum events_group_monitoring_type
{
    MONITOR_ALL_CPU_PER_SOCKET,
    MONITOR_ONE_CPU_PER_SOCKET
};

/*
 * events_group is the events group container.
 */
struct events_group
{
    char *name;
    enum events_group_monitoring_type type;
    struct events_config *events;
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

/*
 * events_group_create allocate the required resources for the events group container.
 */
struct events_group *events_group_create(char *name);

/*
 * events_group_destroy free the allocated resources of the events group container.
 */
void events_group_destroy(struct events_group *group);

#endif /* EVENTS_H */

