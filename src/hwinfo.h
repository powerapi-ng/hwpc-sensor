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

#ifndef HWINFO_H
#define HWINFO_H

#include <stddef.h>
#include <czmq.h>

/*
 * hwinfo_pkg stores information about the package.
 */
struct hwinfo_pkg
{
    zlistx_t *cpus_id; /* char *cpu_id */
};

/*
 * hwinfo stores information about the machine hardware.
 */
struct hwinfo
{
    zhashx_t *pkgs; /* char *pkg_id -> struct hwinfo_pkg *pkg */
};

/*
 * hwinfo_create allocate the needed ressources.
 */
struct hwinfo *hwinfo_create();

/*
 * hwinfo_detect discover and store the machine hardware topology.
 */
int hwinfo_detect(struct hwinfo *hwinfo);

/*
* hwinfo_dup duplicate the hwinfo struct and its members.
 */
struct hwinfo *hwinfo_dup(struct hwinfo *hwinfo);

/*
 * hwinfo_destroy free the allocated memory to store the machine topology.
 */
void hwinfo_destroy(struct hwinfo *hwinfo);

#endif /* HWINFO_H */

