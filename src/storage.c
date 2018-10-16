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

#include <stdlib.h>
#include <strings.h>

#include "storage.h"

struct storage_module *
storage_module_create()
{
    struct storage_module *module = malloc(sizeof(struct storage_module));
    
    if (!module)
        return NULL;

    return module;
}

enum storage_type
storage_module_get_type(const char *type_name)
{
    if (strcasecmp(type_name, "csv") == 0) {
        return STORAGE_CSV;
    }

#ifdef HAVE_MONGODB
    if (strcasecmp(type_name, "mongodb") == 0) {
        return STORAGE_MONGODB;
    }
#endif

    return STORAGE_UNKNOWN;
}

void
storage_module_destroy(struct storage_module *module)
{
    if (!module)
        return;

    STORAGE_MODULE_CALL(module, destroy);
    free(module);
}

