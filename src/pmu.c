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

#include <perfmon/pfmlib_perf_event.h>
#include <stdlib.h>

#include "pmu.h"
#include "util.h"

int
pmu_initialize(void)
{
    if (pfm_initialize() != PFM_SUCCESS) {
        return -1;
    }

    return 0;
}

void
pmu_deinitialize(void)
{
    pfm_terminate();
}

struct pmu_info *
pmu_info_create(void)
{
    struct pmu_info *pmu = malloc(sizeof(struct pmu_info));
    return pmu;
}

struct pmu_info *
pmu_info_dup(struct pmu_info *pmu)
{
    struct pmu_info *copy = NULL;

    if (pmu) {
        copy = malloc(sizeof(struct pmu_info));
        if (copy) {
            *copy = *pmu;
        }
    }

    return copy;
}

void
pmu_info_destroy(struct pmu_info **pmu)
{
    if (!*pmu)
        return;

    free(*pmu);
}

struct pmu_topology *
pmu_topology_create(void)
{
    struct pmu_topology *topology = malloc(sizeof(struct pmu_topology));

    if (!topology)
        return NULL;

    topology->pmus = zlistx_new();
    zlistx_set_duplicator(topology->pmus, (zlistx_duplicator_fn *) pmu_info_dup);
    zlistx_set_destructor(topology->pmus, (zlistx_destructor_fn *) pmu_info_destroy);

    return topology;
}

void
pmu_topology_destroy(struct pmu_topology *topology)
{
    if (!topology)
        return;

    zlistx_destroy(&topology->pmus);
    free(topology);
}

int
pmu_topology_detect(struct pmu_topology *topology)
{
    pfm_pmu_t pmu = {0};
    pfm_pmu_info_t pmu_info = {0};

    for (pmu = PFM_PMU_NONE; pmu < PFM_PMU_MAX; pmu++) {
        if (pfm_get_pmu_info(pmu, &pmu_info) != PFM_SUCCESS)
            continue;

        /* filter to only present pmu */
        if (pmu_info.is_present) {
            /* rewrite type for unknown PMU */
            if (pmu_info.type >= PFM_PMU_TYPE_MAX)
                pmu_info.type = PFM_PMU_TYPE_UNKNOWN;

            zlistx_add_end(topology->pmus, &pmu_info);
        }
    }

    return 0;
}

