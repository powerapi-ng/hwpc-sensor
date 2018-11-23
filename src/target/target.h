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

#include <czmq.h>

/*
 * target_type stores the supported target types.
 */
enum target_type
{
    TARGET_TYPE_UNKNOWN = 0,
    TARGET_TYPE_ALL = 1,
    TARGET_TYPE_SYSTEM = 2,
    TARGET_TYPE_KERNEL = 4,
    TARGET_TYPE_DOCKER = 8,
    TARGET_TYPE_KUBERNETES = 16,
    TARGET_TYPE_LIBVIRT = 32,
    TARGET_TYPE_EVERYTHING = 63
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
    enum target_type type;
    char *cgroup_path;
};

/*
 * target_create allocate the resources and configure the target.
 * The type of the target will be automatically detected.
 * To create an 'All' target to monitor the system globally, set the cgroup_path to NULL.
 */
struct target *target_create(const char *cgroup_path);

/*
 * target_validate_type validate the detected type of the given target.
 * The target type is automatically detected at creation, but some targets may requires a more extensive verification.
 */
int target_validate_type(struct target *target);

/*
 * target_resolve_real_name resolve and return the real name of the given target.
 */
char *target_resolve_real_name(struct target *target);

/*
 * target_destroy free the allocated resources for the target.
 */
void target_destroy(struct target *target);

/*
 * target_discover_running returns a list of running targets.
 */
void target_discover_running(enum target_type type_filter, zhashx_t *targets);

#endif /* TARGET_H */

