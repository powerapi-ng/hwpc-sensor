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

#ifndef PAYLOAD_H
#define PAYLOAD_H

#include <czmq.h>

/*
 * payload_cpu_data stores the events values of a cpu.
 */
struct payload_cpu_data
{
    zhashx_t *events; /* char *event_name -> uint64_t *event_value */
};

/*
 * payload_pkg_data stores the payloads for a cpu package.
 */
struct payload_pkg_data
{
    zhashx_t *cpus; /* char *cpu_id -> struct payload_cpu_data *cpu_data */
};

/*
 * payload_group_data stores the payloads for an events group.
 */
struct payload_group_data
{
    zhashx_t *pkgs; /* char *pkg_id -> struct payload_pkg_data *pkg_data */
};

/*
 * payload stores the data collected by the monitoring module for the reporting module.
 */
struct payload
{
    uint64_t timestamp;
    char *target_name;
    zhashx_t *groups; /* char *group_name -> struct payload_group_data *group_data */
};

/*
 * payload_create allocate the required resources of a monitoring payload.
 */
struct payload *payload_create(uint64_t timestamp, const char *target_name);

/*
 * payload_destroy free the allocated resources of the monitoring payload.
 */
void payload_destroy(struct payload *payload);

/*
 * payload_group_data_create allocate the resources of an events group data container.
 */
struct payload_group_data *payload_group_data_create();

/*
 * payload_group_data_destroy free the allocated resources of the events group data container.
 */
void payload_group_data_destroy(struct payload_group_data **data_ptr);

/*
 * payload_pkg_data_create allocate the resources of a package data container.
 */
struct payload_pkg_data *payload_pkg_data_create();

/*
 * payload_pkg_data_destroy free the allocated resources of the package data container.
 */
void payload_pkg_data_destroy(struct payload_pkg_data **data_ptr);

/*
 * payload_cpu_data_create allocate the resources of a cpu data container.
 */
struct payload_cpu_data *payload_cpu_data_create();

/*
 * payload_cpu_data_destroy free the allocated resources of the cpu data container.
 */
void payload_cpu_data_destroy(struct payload_cpu_data **data_ptr);

#endif /* PAYLOAD_H */

