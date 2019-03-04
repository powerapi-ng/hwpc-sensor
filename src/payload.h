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

