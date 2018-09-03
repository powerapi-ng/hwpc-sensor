#include <perfmon/pfmlib_perf_event.h>
#include <stdlib.h>

#include "events.h"

static int
get_perf_event_attr(char *event_name, struct event_attr *config)
{
    pfm_perf_encode_arg_t arg = {0};

    config->name = event_name;
    config->perf_attr.size = sizeof(struct perf_event_attr);
    config->perf_attr.disabled = 1;
    config->perf_attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_GROUP;

    arg.size = sizeof(pfm_perf_encode_arg_t);
    arg.attr = &config->perf_attr;
    if (pfm_get_os_event_encoding(event_name, PFM_PLM0 | PFM_PLM3, PFM_OS_PERF_EVENT_EXT, &arg) != PFM_SUCCESS) {
        return -1;
    }

    return 0;
}

struct events_config *
events_config_create()
{
    struct events_config *events = malloc(sizeof(struct events_config));

    if (!events)
        return NULL;

    events->num_attrs = 0;
    events->attrs = NULL;

    return events;
}

void
events_config_destroy(struct events_config *events)
{
    if (!events)
        return;

    free(events->attrs);
    free(events);
}

int
events_config_add(struct events_config *events, char *event_name)
{
    struct event_attr attr = {0};

    if (!events || !event_name)
        return -1;

    if (get_perf_event_attr(event_name, &attr))
        return -1;

    events->attrs = realloc(events->attrs, sizeof(struct event_attr) * (events->num_attrs + 1));
    if (!events->attrs)
        return -1;

    events->attrs[events->num_attrs++] = attr;
    return 0;
}

struct events_group *
events_group_create(char *name)
{
    struct events_group *group = malloc(sizeof(struct events_group));

    if (!group)
        return NULL;

    group->name = name;
    group->events = events_config_create();
    group->type = MONITOR_ALL_CPU_PER_SOCKET; /* by default, monitor all cpu of the available socket(s) */

    return group;
}

void
events_group_destroy(struct events_group *group)
{
    if (!group)
        return;

    events_config_destroy(group->events);
    free(group);
}

