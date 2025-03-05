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

#include <czmq.h>
#include <stdlib.h>

#include "util.h"
#include "payload.h"

struct payload_cpu_data *
payload_cpu_data_create(void)
{
    struct payload_cpu_data *data = (struct payload_cpu_data *) malloc(sizeof(struct payload_cpu_data));

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
payload_pkg_data_create(void)
{
    struct payload_pkg_data *data = (struct payload_pkg_data *) malloc(sizeof(struct payload_pkg_data));

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
payload_group_data_create(void)
{
    struct payload_group_data *data = (struct payload_group_data *) malloc(sizeof(struct payload_group_data));

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
    struct payload *payload = (struct payload *) malloc(sizeof(struct payload));

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

