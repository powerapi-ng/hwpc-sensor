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

