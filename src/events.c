#include <perfmon/pfmlib_perf_event.h>
#include <stdlib.h>

#include "events.h"
#include "util.h"

static int
setup_perf_event_attr(char *event_name, struct perf_event_attr *attr)
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
event_config_create(char *event_name)
{
    struct perf_event_attr attr = {0};
    struct event_config *config = NULL;

    if (!setup_perf_event_attr(event_name, &attr)) {
        config = malloc(sizeof(struct event_config));
        if (config) {
            config->name = event_name;
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
            copy->name = config->name;
            copy->attr = config->attr;
        }
    }

    return copy;
}

struct events_group *
events_group_create(char *name)
{
    struct events_group *group = malloc(sizeof(struct events_group));

    if (group) {
        group->name = name;
        group->type = MONITOR_ALL_CPU_PER_SOCKET; /* by default, monitor all cpu of the available socket(s) */

        group->events = zlistx_new();
        zlistx_set_duplicator(group->events, (zlistx_duplicator_fn *) event_config_dup);
        zlistx_set_destructor(group->events, (zlistx_destructor_fn *) ptrfree);
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
            copy->name = group->name;
            copy->type = group->type;
            copy->events = zlistx_dup(group->events);
        }
    }

    return copy;
}

int
events_group_append_event(struct events_group *group, char *event_name)
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

