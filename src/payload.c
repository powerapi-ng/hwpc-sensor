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

#include <czmq.h>
#include <stdlib.h>

#include "util.h"
#include "payload.h"

struct payload_cpu_data *
payload_cpu_data_create()
{
    struct payload_cpu_data *data = malloc(sizeof(struct payload_cpu_data));

    if (!data)
        return NULL;

    data->events = zhashx_new();
    zhashx_set_duplicator(data->events, (zhashx_duplicator_fn *) uint64ptrdup);
    zhashx_set_destructor(data->events, (zhashx_destructor_fn *) ptrfree);

    return data;
}

void
payload_cpu_data_destroy(struct payload_cpu_data **data_ptr)
{
    if (!*data_ptr)
        return;

    zhashx_destroy(&(*data_ptr)->events);
    free(*data_ptr);
    *data_ptr = NULL;
}

struct payload_pkg_data *
payload_pkg_data_create()
{
    struct payload_pkg_data *data = malloc(sizeof(struct payload_pkg_data));

    if (!data)
        return NULL;

    data->cpus = zhashx_new();
    zhashx_set_destructor(data->cpus, (zhashx_destructor_fn *) payload_cpu_data_destroy);

    return data;
}

void
payload_pkg_data_destroy(struct payload_pkg_data **data_ptr)
{
    if (!*data_ptr)
        return;

    zhashx_destroy(&(*data_ptr)->cpus);
    free(*data_ptr);
    *data_ptr = NULL;
}

struct payload_group_data *
payload_group_data_create()
{
    struct payload_group_data *data = malloc(sizeof(struct payload_group_data));

    if (!data)
        return NULL;

    data->pkgs = zhashx_new();
    zhashx_set_destructor(data->pkgs, (zhashx_destructor_fn *) payload_pkg_data_destroy);

    return data;
}

void
payload_group_data_destroy(struct payload_group_data **data_ptr)
{
    if (!*data_ptr)
        return;

    zhashx_destroy(&(*data_ptr)->pkgs);
    free(*data_ptr);
    *data_ptr = NULL;
}

struct payload *
payload_create(uint64_t timestamp, const char *target_name)
{
    struct payload *payload = malloc(sizeof(struct payload));

    if (!payload)
        return NULL;

    payload->timestamp = timestamp;
    payload->target_name = strdup(target_name);
    payload->groups = zhashx_new();
    zhashx_set_destructor(payload->groups, (zhashx_destructor_fn *) payload_group_data_destroy);

    return payload;
}

void
payload_destroy(struct payload *payload)
{
    if (!payload)
        return;

    free(payload->target_name);
    zhashx_destroy(&payload->groups);
    free(payload);
}

