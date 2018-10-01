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

#ifndef PMU_H
#define PMU_H

#include <czmq.h>
#include <perfmon/pfmlib_perf_event.h>

/*
 * pmu_info is the container for the pmu information.
 */
struct pmu_info
{
    pfm_pmu_info_t info;
};

/*
 * pmu_topology is the container of the supported pmu by the machine.
 */
struct pmu_topology
{
    zlistx_t *pmus; /* struct pmu_info *info */
};

/*
 * pmu_initialize allocate the resources needed to use the PMUs.
 */
int pmu_initialize();

/*
 * pmu_deinitialize free the allocated resources needed to use the PMUs.
 */
void pmu_deinitialize();

/*
 * pmu_info_create allocate the resources needed for a pmu info container.
 */
struct pmu_info *pmu_info_create();

/*
 * pmu_info_dup duplicate the given pmu info.
 */
struct pmu_info *pmu_info_dup(struct pmu_info *pmu);

/*
 * pmu_info_destroy free the allocated resources for the pmu info container.
 */
void pmu_info_destroy(struct pmu_info **pmu);

/*
 * pmu_topology_create allocate the resource needed for a pmu topology container.
 */
struct pmu_topology *pmu_topology_create();

/*
 * pmu_topology_destroy free the allocated resource of the pmu topology container.
 */
void pmu_topology_destroy(struct pmu_topology *topology);

/*
 * pmu_topology_detect populate the topology container with the available PMUs.
 */
int pmu_topology_detect(struct pmu_topology *topology);

#endif /* PMU_H */

