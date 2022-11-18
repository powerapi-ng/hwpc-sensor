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
int pmu_initialize(void);

/*
 * pmu_deinitialize free the allocated resources needed to use the PMUs.
 */
void pmu_deinitialize(void);

/*
 * pmu_info_create allocate the resources needed for a pmu info container.
 */
struct pmu_info *pmu_info_create(void);

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
struct pmu_topology *pmu_topology_create(void);

/*
 * pmu_topology_destroy free the allocated resource of the pmu topology container.
 */
void pmu_topology_destroy(struct pmu_topology *topology);

/*
 * pmu_topology_detect populate the topology container with the available PMUs.
 */
int pmu_topology_detect(struct pmu_topology *topology);

#endif /* PMU_H */

