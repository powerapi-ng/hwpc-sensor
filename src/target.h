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

#ifndef TARGET_H
#define TARGET_H

/*
 * DOCKER_CONFIG_PATH_BUFFER_SIZE stores the max length of the path to the container config file.
 * /var/lib/docker/containers/756535dc6e9ab9b560f84c85063f55952273a23192641fc2756aa9721d9d1000/config.v2.json = 106
 */
#define DOCKER_CONFIG_PATH_BUFFER_SIZE 128

/*
 * target_type stores the supported target types.
 */
enum target_type
{
    PERF_TARGET_UNKNOWN,
    PERF_TARGET_SYSTEM,
    PERF_TARGET_DOCKER,
    PERF_TARGET_LIBVIRT
};

/*
 * target_types_name stores the name (as string) of the supported target types.
 */
extern const char *target_types_name[];

/*
 * target stores various information about the target.
 */
struct target
{
    char *cgroup_path;
    enum target_type type;
};

/*
 * target_create allocate the resources and configure the target.
 * Set cgroup_path to NULL to create a system target.
 */
struct target *target_create(const char *cgroup_path);

/*
 * target_resolve_real_name resolve and return the real name of the given target.
 */
char *target_resolve_real_name(struct target *target);

/*
 * target_destroy free the allocated resources for the target.
 */
void target_destroy(struct target *target);

#endif /* TARGET_H */

