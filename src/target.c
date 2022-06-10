/*
 *  Copyright (c) 2018, INRIA
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
#include "target_docker.h"
#include "target_kubernetes.h"

const char *target_types_name[] = {
    [TARGET_TYPE_UNKNOWN] = "unknown",
    [TARGET_TYPE_ALL] = "all",
    [TARGET_TYPE_SYSTEM] = "system",
    [TARGET_TYPE_KERNEL] = "kernel",
    [TARGET_TYPE_DOCKER] = "docker",
    [TARGET_TYPE_KUBERNETES] = "k8s",
    [TARGET_TYPE_LIBVIRT] = "libvirt",
    [TARGET_TYPE_LXC] = "lxc"
};

enum target_type
target_detect_type(const char *cgroup_path)
{
    /* All running processes/threads (not a cgroup) */
    if (!cgroup_path)
        return TARGET_TYPE_ALL;

    /* System (running processes/threads in system cgroup) */
    if (strstr(cgroup_path, "perf_event/system"))
        return TARGET_TYPE_SYSTEM;

    /* Kernel (running processes/threads in kernel cgroup) */
    if (strstr(cgroup_path, "perf_event/kernel"))
        return TARGET_TYPE_KERNEL;

    /* Docker (running containers) */
    if (strstr(cgroup_path, "perf_event/docker"))
        return TARGET_TYPE_DOCKER;

    /* Kubernetes (running containers) */
    if (strstr(cgroup_path, "perf_event/kubepods"))
        return TARGET_TYPE_KUBERNETES;

    /* LibVirt (running virtual machine) */
    if (strstr(cgroup_path, "perf_event/machine.slice"))
        return TARGET_TYPE_LIBVIRT;

    /* LXC (running containers) */
    if (strstr(cgroup_path, "perf_event/lxc"))
	return TARGET_TYPE_LXC;

    return TARGET_TYPE_UNKNOWN;
}

int
target_validate_type(enum target_type type, const char *cgroup_path)
{
    switch (type) {
        case TARGET_TYPE_DOCKER:
            return target_docker_validate(cgroup_path);

        case TARGET_TYPE_KUBERNETES:
            return target_kubernetes_validate(cgroup_path);

        default:
            /* other types does not need a validation */
            return true;
    }
}

struct target *
target_create(enum target_type type, const char *cgroup_basedir, const char *cgroup_path)
{
    struct target *target = malloc(sizeof(struct target));

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
        case TARGET_TYPE_DOCKER:
            target_real_name = target_docker_resolve_name(target);
            break;

        case TARGET_TYPE_KUBERNETES:
            target_real_name = target_kubernetes_resolve_name(target);
            break;

        case TARGET_TYPE_ALL:
        case TARGET_TYPE_KERNEL:
        case TARGET_TYPE_SYSTEM:
            /* the above types have static name */
            target_real_name = strdup(target_types_name[target->type]);
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
target_discover_running(const char *base_path, enum target_type type_mask, zhashx_t *targets)
{
    const char *path[] = { base_path, NULL };
    FTS *file_system = NULL;
    FTSENT *node = NULL;
    enum target_type type;
    struct target *target = NULL;

    file_system = fts_open((char * const *)path, FTS_LOGICAL | FTS_NOCHDIR, NULL);
    if (!file_system)
        return -1;

    for (node = fts_read(file_system); node; node = fts_read(file_system)) {
        /*
         * Filtering the directories having 2 hard links leading to them to only get leaves directories.
         * The cgroup subsystems does not support hard links, so this will always work.
         */
        if (node->fts_info == FTS_D && node->fts_statp->st_nlink == 2) {
            type = target_detect_type(node->fts_path);
            if ((type & type_mask) && target_validate_type(type, node->fts_path)) {
                target = target_create(type, base_path, node->fts_path);
                if (target)
                    zhashx_insert(targets, node->fts_path, target);
            }
        }
    }

    fts_close(file_system);
    return 0;
}

