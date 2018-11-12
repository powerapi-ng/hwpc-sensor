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
#include <fts.h>
#include <sys/stat.h>

#include "cgroups.h"

int
cgroups_get_running_subgroups(char * const base_path, zhashx_t *subgroups)
{
    char * const path[] = { base_path, NULL };
    FTS *file_system = NULL;
    FTSENT *node = NULL;

    file_system = fts_open(path, FTS_LOGICAL | FTS_NOCHDIR, NULL);
    if (!file_system)
        return -1;

    for(node = fts_read(file_system); node; node = fts_read(file_system)) {
        /*
         * Filtering the directories having 2 links leading to them to only get leaves directories.
         * The cgroup subsystems does not support symlinks, so this will always work.
         */
        if (node->fts_info == FTS_D && node->fts_statp->st_nlink == 2)
            zhashx_insert(subgroups, node->fts_path, NULL);
    }

    fts_close(file_system);
    return 0;
}

