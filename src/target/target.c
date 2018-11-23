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
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "target.h"
#include "docker.h"
#include "kubernetes.h"

const char *target_types_name[] = {
    [TARGET_TYPE_UNKNOWN] = "unknown",
    [TARGET_TYPE_ALL] = "all",
    [TARGET_TYPE_SYSTEM] = "system",
    [TARGET_TYPE_KERNEL] = "kernel",
    [TARGET_TYPE_DOCKER] = "docker",
    [TARGET_TYPE_KUBERNETES] = "k8s",
    [TARGET_TYPE_LIBVIRT] = "libvirt"
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

    return TARGET_TYPE_UNKNOWN;
}

struct target *
target_create(const char *cgroup_path)
{
    struct target *target = malloc(sizeof(struct target));

    if (!target)
        return NULL;

    target->cgroup_path = (cgroup_path) ? strdup(cgroup_path) : NULL;
    target->type = target_detect_type(cgroup_path);

    return target;
}

int
target_validate_type(struct target *target)
{
    switch (target->type) {
        case TARGET_TYPE_DOCKER:
            return target_kubernetes_validate(target->cgroup_path);
            break;

        case TARGET_TYPE_KUBERNETES:
            return target_kubernetes_validate(target->cgroup_path);
            break;

        default:
            /* other types does not need a validation */
            return true;
    }
}

char *
target_resolve_real_name(struct target *target)
{
    switch (target->type) {
        case TARGET_TYPE_DOCKER:
            return target_docker_resolve_name(target);
            break;

        case TARGET_TYPE_KUBERNETES:
            return target_kubernetes_resolve_name(target);
            break;

        case TARGET_TYPE_ALL:
        case TARGET_TYPE_KERNEL:
        case TARGET_TYPE_SYSTEM:
            /* the above types have static name */
            return strdup(target_types_name[target->type]);
            break;

        default:
            /* return the basename of the cgroup path */
            return strdup(strrchr(target->cgroup_path, '/') + 1);
    }
}

void
target_destroy(struct target *target)
{
    if (!target)
        return;

    free(target->cgroup_path);
    free(target);
}

