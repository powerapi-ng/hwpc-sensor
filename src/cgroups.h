#ifndef CGROUPS_H
#define CGROUPS_H

#include <czmq.h>

/*
 * cgroups_initialize initialize the cgroups module.
 */
int cgroups_initialize();

/*
 * cgroups_get_running_subgroups stores the running perf_event cgroups name and path into the provided hash table.
 */
int cgroups_get_running_subgroups(const char *controller, const char *base_path, zhashx_t *subgroups);

#endif /* CGROUPS_H */

