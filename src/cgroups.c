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

#include <czmq.h>
#include <libcgroup.h>

#include "cgroups.h"

int
cgroups_initialize()
{
    return cgroup_init();
}

int
cgroups_get_running_subgroups(const char *controller, const char *base_path, zhashx_t *subgroups)
{
    int ret = -1;
    void *handle = NULL;
    struct cgroup_file_info info = {0};
    int lvl;
    char *container_id = NULL;

    if (cgroup_walk_tree_begin(controller, base_path, 1, &handle, &info, &lvl) != 0) {
        return ret;
    }

    while ((ret = cgroup_walk_tree_next(0, &handle, &info, lvl)) == 0) {
        if (info.type == CGROUP_FILE_TYPE_DIR) {
            container_id = strrchr(info.full_path, '/') + 1;
            zhashx_insert(subgroups, container_id, (char *) info.full_path);
        }
    }

    if (ret == ECGEOF) {
        ret = 0;
    }

    cgroup_walk_tree_end(&handle);
    return ret;
}

