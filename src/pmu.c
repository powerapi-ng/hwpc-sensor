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

#include <perfmon/pfmlib_perf_event.h>
#include <stdlib.h>

#include "pmu.h"
#include "util.h"

int
pmu_initialize()
{
    if (pfm_initialize() != PFM_SUCCESS) {
        return -1;
    }

    return 0;
}

void
pmu_deinitialize()
{
    pfm_terminate();
}

struct pmu_info *
pmu_info_create()
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
pmu_topology_create()
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

    pfm_for_all_pmus(pmu) {
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

