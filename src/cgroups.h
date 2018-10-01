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

#ifndef CGROUPS_H
#define CGROUPS_H

#include <czmq.h>

/*
 * cgroups_initialize initialize the cgroups module.
 */
int cgroups_initialize();

/*
 * cgroups_get_running_subgroups stores the running perf_event cgroups name and path into the provided hash table.
 */
int cgroups_get_running_subgroups(const char *controller, const char *base_path, zhashx_t *subgroups);

#endif /* CGROUPS_H */

