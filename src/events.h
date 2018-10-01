/*
 * Copyright 2018 University of Lille
 * Copyright 2018 INRIA
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EVENTS_H
#define EVENTS_H

#include <czmq.h>
#include <perfmon/pfmlib_perf_event.h>

/*
 * events_group_monitoring_type stores the possible monitoring type of an events group.
 */
enum events_group_monitoring_type
{
    MONITOR_ALL_CPU_PER_SOCKET,
    MONITOR_ONE_CPU_PER_SOCKET
};

/*
 * event_config is the event configuration container.
 */
struct event_config
{
    char *name;
    struct perf_event_attr attr;
};

/*
 * events_group is the events group container.
 */
struct events_group
{
    char *name;
    enum events_group_monitoring_type type;
    zlistx_t *events; /* struct event_config *event */
};

/*
 * event_config_create allocate the required resources for the event config container.
 */
struct event_config *event_config_create(char *event_name);

/*
 * event_config_dup duplicate the given event config container.
 */
struct event_config *event_config_dup(struct event_config *config);

/*
 * event_config_destroy free the allocated resources of the event config container.
 */
void event_config_destroy(struct event_config **config);

/*
 * events_group_create allocate the required resources for the events group container.
 */
struct events_group *events_group_create(char *name);

/*
 * events_group_dup duplicate the given events group container.
 */
struct events_group *events_group_dup(struct events_group *group);

/*
 * events_group_append_event get the event attributes from its name (if available) and store it into the events group container.
 */
int events_group_append_event(struct events_group *group, char *event_name);

/*
 * events_group_destroy free the allocated resources of the events group container.
 */
void events_group_destroy(struct events_group **group);

#endif /* EVENTS_H */

