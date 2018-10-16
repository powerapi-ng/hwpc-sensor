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

#ifndef MONGODB_H
#define MONGODB_H

#include <mongoc.h>
#include "storage.h"
#include "report.h"

/*
 * mongodb_config stores the required information for the module.
 */
struct mongodb_config
{
    const char *sensor_name;
    const char *uri;
    const char *database_name;
    const char *collection_name;
};

/*
 * mongodb_context stores the context of the module.
 */
struct mongodb_context
{
    struct mongodb_config config;
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_collection_t *collection;
};

/*
 * mongodb_create creates and configure a mongodb storage module.
 */
struct storage_module *mongodb_create(const char *sensor_name, const char *uri, const char *database, const char *collection);

#endif

