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

