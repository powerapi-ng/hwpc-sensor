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

#ifndef EVENTS_H
#define EVENTS_H

#include <limits.h>
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
    char name[NAME_MAX];
    struct perf_event_attr attr;
};

/*
 * events_group is the events group container.
 */
struct events_group
{
    char name[NAME_MAX];
    enum events_group_monitoring_type type;
    zlistx_t *events; /* struct event_config *event */
};

/*
 * event_config_create allocate the required resources for the event config container.
 */
struct event_config *event_config_create(const char *event_name);

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
struct events_group *events_group_create(const char *name);

/*
 * events_group_dup duplicate the given events group container.
 */
struct events_group *events_group_dup(struct events_group *group);

/*
 * events_group_append_event get the event attributes from its name (if available) and store it into the events group container.
 */
int events_group_append_event(struct events_group *group, const char *event_name);

/*
 * events_group_destroy free the allocated resources of the events group container.
 */
void events_group_destroy(struct events_group **group);

#endif /* EVENTS_H */

