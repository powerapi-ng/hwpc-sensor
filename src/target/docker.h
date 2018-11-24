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

#ifndef TARGET_DOCKER_H
#define TARGET_DOCKER_H

#include "target.h"

/*
 * TARGET_DOCKER_EXTRACT_CONTAINER_ID_REGEX stores the regex used to extract the container id from of a cgroup path.
 */
#define TARGET_DOCKER_EXTRACT_CONTAINER_ID_REGEX "perf_event/docker/([a-f0-9]{64})$"

/*
 * DOCKER_CONFIG_PATH_BUFFER_SIZE stores the max length of the path to the container config file.
 * /var/lib/docker/containers/756535dc6e9ab9b560f84c85063f55952273a23192641fc2756aa9721d9d1000/config.v2.json = 106
 */
#define TARGET_DOCKER_CONFIG_PATH_BUFFER_SIZE 128

/*
 * DOCKER_CONFIG_EXTRACT_NAME_REGEX stores the regex used to extract the name of the container from its json config file.
 */
#define TARGET_DOCKER_CONFIG_EXTRACT_NAME_REGEX "\"Name\":\"/([a-zA-Z0-9][a-zA-Z0-9_.-]+)\""

/*
 * target_docker_validate check if the cgroup path lead to a valid Docker target.
 */
int target_docker_validate(const char *cgroup_path);

/*
 * target_docker_resolve_name resolve and return the real name of the given target.
 */
char *target_docker_resolve_name(struct target *target);

#endif /* TARGET_DOCKER_H */

