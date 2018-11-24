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

#ifndef TARGET_KUBERNETES_H
#define TARGET_KUBERNETES_H

#include "target.h"

/*
 * TARGET_KUBERNETES_EXTRACT_CONTAINER_ID_REGEX stores the regex used to extract the Docker container id from a cgroup path.
 */
#define TARGET_KUBERNETES_EXTRACT_CONTAINER_ID_REGEX \
    "perf_event/kubepods/" \
    "(besteffort/|burstable/|)" \
    "(pod[a-zA-Z0-9][a-zA-Z0-9.-]+)/" /* Pod ID */ \
    "([a-f0-9]{64})" /* Container ID */ \
    "(/[a-zA-Z0-9][a-zA-Z0-9.-]+|)" /* Resource group */

/*
 * TARGET_KUBERNETES_EXTRACT_CONTAINER_NAME_REGEX stores the regex used to extract the name of the Docker container from
 * the json configuration file.
 */
#define TARGET_KUBERNETES_EXTRACT_CONTAINER_NAME_REGEX "\"Name\":\"/([a-zA-Z0-9][a-zA-Z0-9_.-]+)\""

/*
 * TARGET_KUBERNETES_CONFIG_PATH_BUFFER_SIZE stores the buffer size for the path to the Docker config file.
 */
#define TARGET_KUBERNETES_CONFIG_PATH_BUFFER_SIZE 128

/*
 * target_kubernetes_validate check if the cgroup path lead to a valid Kubernetes target.
 */
int target_kubernetes_validate(const char *cgroup_path);

/*
 * target_kubernetes_resolve_name resolve and return the real name of the given target.
 */
char *target_kubernetes_resolve_name(struct target *target);

#endif /* TARGET_KUBERNETES_H */

