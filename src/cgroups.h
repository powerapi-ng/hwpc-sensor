#ifndef CGROUPS_H
#define CGROUPS_H

#include <czmq.h>

/*
 * initialize_cgroups initialize the required ressources to work with cgroups.
 */
int initialize_cgroups();

/*
 * get_running_perf_event_cgroups stores the running perf_event cgroups into the provided hash table.
 * The container id as key and the absolute path to the perf_event cgroup as value. (as string)
 */
int get_running_perf_event_cgroups(const char *base_path, zhashx_t *running);

#endif /* CGROUPS_H */

