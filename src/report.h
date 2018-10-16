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

#ifndef REPORT_H
#define REPORT_H

#include <czmq.h>
#include <stdint.h>

/*
 * report_config stores the reporting module configuration.
 */
struct report_config
{
    struct storage_module *storage;
};

/*
 * report_context stores the reporting module execution context.
 */
struct report_context
{
    struct report_config *config;
    bool terminated;
    zsock_t *pipe;
    zsock_t *reporting;
    zpoller_t *poller;
};

/*
 * report_config_create allocate the resource of a report configuration structure.
 */
struct report_config *report_config_create(struct storage_module *storage_module);

/*
 * report_config_destroy free the allocated resource of the report configuration structure.
 */
void report_config_destroy(struct report_config *config);

/*
 * reporting_actor is the reporting actor entrypoint.
 */
void reporting_actor(zsock_t *pipe, void *args);

#endif /* REPORT_H */

