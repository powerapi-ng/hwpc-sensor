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

