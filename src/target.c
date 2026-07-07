/*
 *  Copyright (c) 2018, Inria
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

#include <czmq.h>
#include <fts.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "target.h"


struct target *
target_create(enum target_type type, const char *cgroup_basedir, const char *cgroup_path)
{
    struct target *target = (struct target *) malloc(sizeof(struct target));

    if (!target)
        return NULL;

    target->cgroup_basedir = cgroup_basedir;
    target->cgroup_path = (cgroup_path) ? strdup(cgroup_path) : NULL;
    target->type = type;

    return target;
}

char *
target_resolve_real_name(struct target *target)
{
    char *target_real_name = NULL;

    switch (target->type) {
        case TARGET_TYPE_GLOBAL:
            target_real_name = strdup("all");
            break;

        default:
            break;
    }

    /* if name cannot be resolved, use the cgroup path relative to cgroup base dir */
    if (!target_real_name && target->cgroup_path && target->cgroup_basedir) {
        target_real_name = strdup(target->cgroup_path + strlen(target->cgroup_basedir));
    }

    return target_real_name;
}

void
target_destroy(struct target *target)
{
    if (!target)
        return;

    free(target->cgroup_path);
    free(target);
}

int
target_discover_running(const char *base_path, zhashx_t *targets)
{
    const char *path[] = { base_path, NULL };
    FTS *file_system = NULL;
    FTSENT *node = NULL;
    struct target *target = NULL;

    file_system = fts_open((char * const *) path, FTS_PHYSICAL | FTS_NOCHDIR | FTS_NOSTAT, NULL);
    if (!file_system)
        return -1;

    for (node = fts_read(file_system); node; node = fts_read(file_system)) {
        /*
         * Mark a directory's parent when a child directory is seen in pre-order.
         * Then, when the directory is seen in post-order, it is a leaf if no
         * child directory marked it.
         */
        if (node->fts_info == FTS_D) {
            if (node->fts_parent)
                node->fts_parent->fts_number = 1;

            continue;
        }

        if (node->fts_info != FTS_DP)
            continue;

        /* Do not report the traversal root itself as a target */
        if (node->fts_level == FTS_ROOTLEVEL)
            continue;

        /* A child directory was seen, so this directory is not a leaf */
        if (node->fts_number != 0)
            continue;

        target = target_create(TARGET_TYPE_CGROUP, base_path, node->fts_path);
        zhashx_insert(targets, node->fts_path, target);
    }

    fts_close(file_system);
    return 0;
}
