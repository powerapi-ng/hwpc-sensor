#ifndef PMU_H
#define PMU_H

#include <perfmon/pfmlib_perf_event.h>

/*
 * pmu_topology is the container of the supported pmu by the machine.
 */
struct pmu_topology
{
    size_t num_pmus;
    pfm_pmu_info_t *pmus;
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

