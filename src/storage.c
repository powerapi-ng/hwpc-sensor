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

#include <stdlib.h>
#include <strings.h>

#include "storage.h"

const char *storage_types_name[] = {
    [STORAGE_UNKNOWN] = "unknown",
    [STORAGE_CSV] = "csv",
    [STORAGE_SOCKET] = "socket",
#ifdef HAVE_MONGODB
    [STORAGE_MONGODB] = "mongodb",
#endif
};

enum storage_type
storage_module_get_type(const char *type_name)
{
    if (strcasecmp(type_name, storage_types_name[STORAGE_CSV]) == 0) {
        return STORAGE_CSV;
    }

    if (strcasecmp(type_name, storage_types_name[STORAGE_SOCKET]) == 0) {
        return STORAGE_SOCKET;
    }

#ifdef HAVE_MONGODB
    if (strcasecmp(type_name, storage_types_name[STORAGE_MONGODB]) == 0) {
        return STORAGE_MONGODB;
    }
#endif

    return STORAGE_UNKNOWN;
}

int
storage_module_initialize(struct storage_module *module)
{
    return (*module->initialize)(module);
}

int
storage_module_ping(struct storage_module *module)
{
    return (*module->ping)(module);
}

int
storage_module_store_report(struct storage_module *module, struct payload *payload)
{
    return (*module->store_report)(module, payload);
}

int
storage_module_deinitialize(struct storage_module *module)
{
    return (*module->deinitialize)(module);
}

void
storage_module_destroy(struct storage_module *module)
{
    if (!module)
        return;

    (*module->destroy)(module);
    free(module);
}

