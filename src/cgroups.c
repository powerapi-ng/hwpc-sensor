#include <czmq.h>
#include <libcgroup.h>

#include "cgroups.h"

int
initialize_cgroups()
{
    return cgroup_init();
}

int
get_running_perf_event_cgroups(const char *base_path, zhashx_t *running)
{
    int ret = -1;
    void *handle = NULL;
    struct cgroup_file_info info = {0};
    int lvl;
    char *container_id = NULL;

    if (cgroup_walk_tree_begin("perf_event", base_path, 1, &handle, &info, &lvl) != 0) {
        return ret;
    }

    while ((ret = cgroup_walk_tree_next(0, &handle, &info, lvl)) == 0) {
        if (info.type == CGROUP_FILE_TYPE_DIR) {
            container_id = strrchr(info.full_path, '/') + 1;
            zhashx_insert(running, container_id, (char *) info.full_path);
        }
    }

    if (ret == ECGEOF) {
        ret = 0;
    }

    cgroup_walk_tree_end(&handle);
    return ret;
}

