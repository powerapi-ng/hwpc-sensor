#include <perfmon/pfmlib_perf_event.h>
#include <stdlib.h>

#include "pmu.h"

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

struct pmu_topology *
pmu_topology_create()
{
    struct pmu_topology *topology = malloc(sizeof(struct pmu_topology));

    if (!topology)
        return NULL;

    topology->num_pmus = 0;
    topology->pmus = NULL;

    return topology;
}

void
pmu_topology_destroy(struct pmu_topology *topology)
{
    if (!topology)
        return;

    free(topology->pmus);
    free(topology);
}

int
pmu_topology_detect(struct pmu_topology *topology)
{
    pfm_pmu_t pmu = {0};
    pfm_pmu_info_t pmu_info = {0};

    pfm_for_all_pmus(pmu) {
        /* get information of pmu */
        if (pfm_get_pmu_info(pmu, &pmu_info) != PFM_SUCCESS)
            continue;

        /* filter to only present pmu */
        if (pmu_info.is_present) {
            /* rewrite type for unknown PMU */
            if (pmu_info.type >= PFM_PMU_TYPE_MAX)
                pmu_info.type = PFM_PMU_TYPE_UNKNOWN;

            /* extend pmu info array */
            topology->pmus = realloc(topology->pmus, sizeof(pfm_pmu_info_t) * (topology->num_pmus + 1));
            if (!topology->pmus)
                return -1;

            /* copy pmu info */
            topology->pmus[topology->num_pmus++] = pmu_info;
        }
    }

    return 0;
}

