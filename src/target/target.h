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

#ifndef TARGET_H
#define TARGET_H

#include <czmq.h>

/*
 * target_type stores the supported target types.
 */
enum target_type
{
    TARGET_TYPE_UNKNOWN = 1,
    TARGET_TYPE_ALL = 2,
    TARGET_TYPE_SYSTEM = 4,
    TARGET_TYPE_KERNEL = 8,
    TARGET_TYPE_DOCKER = 16,
    TARGET_TYPE_KUBERNETES = 32,
    TARGET_TYPE_LIBVIRT = 64,
    TARGET_TYPE_EVERYTHING = 127
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
 * target_detect_type returns the target type of the given cgroup path.
 */
enum target_type target_detect_type(const char *cgroup_path);

/*
 * target_validate_type validate the target type of the given cgroup path.
 */
int target_validate_type(enum target_type type, const char *cgroup_path);

/*
 * target_create allocate the resources and configure the target.
 */
struct target *target_create(enum target_type type, const char *cgroup_path);

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
int target_discover_running(char *base_path, enum target_type type_mask, zhashx_t *targets);

#endif /* TARGET_H */

