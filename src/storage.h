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

#ifndef STORAGE_H
#define STORAGE_H

#include "payload.h"
#include "report.h"

/*
 * STORAGE_MODULE_CALL simplify the use of storage modules functions.
 */
#define STORAGE_MODULE_CALL(module, func, ...)  (*module->func)(module, ##__VA_ARGS__)

/*
 * storage_type enumeration allows to select a storage type to generate.
 */
enum storage_type
{
    STORAGE_UNKNOWN,
    STORAGE_CSV,
#ifdef HAVE_MONGODB
    STORAGE_MONGODB,
#endif
};

/*
 * storage_module is a generic interface for storage modules.
 */
struct storage_module
{
    enum storage_type type;
    void *context;
    bool is_initialized;
    int (*initialize)(struct storage_module *self);
    int (*ping)(struct storage_module *self);
    int (*store_report)(struct storage_module *self, struct payload *payload);
    int (*deinitialize)(struct storage_module *self);
    void (*destroy)(struct storage_module *self);
};

/*
 * storage_module_create allocate the required ressources for a storage module.
 */
struct storage_module *storage_module_create();

/*
 * storage_module_get_type returns the type of the given storage module name.
 */
enum storage_type storage_module_get_type(const char *type_name);

/*
 * storage_module_destroy free the allocated ressources for the storage module.
 */
void storage_module_destroy(struct storage_module *module);

#endif /* STORAGE_H */

