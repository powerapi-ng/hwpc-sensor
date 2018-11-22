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

/*
 * TARGET_DOCKER_CGROUP_PATH_EXPECTED_REGEX stores the regex used to validate if a given cgroup is a Docker container.
 */
#define TARGET_DOCKER_CGROUP_PATH_EXPECTED_REGEX "perf_event/docker/[a-f0-9]{64}$"

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
 * target_docker_detect returns true if the target pointing at the given cgroup path is a valid Docker target.
 */
int target_docker_detect(const char *cgroup_path);

/*
 * target_docker_create allocate the required resources for a target.
 */
struct target *target_docker_create(char *cgroup_path);

/*
 * target_docker_resolve_name resolve and return the real name of the given target.
 */
char *target_docker_resolve_name(struct target *target);

/*
 * target_docker_destroy free the allocated memory for the given target.
 */
void target_docker_destroy(struct target *target);

#endif /* TARGET_DOCKER_H */

