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

#include "storage.h"
#include "storage_null.h"
#include "config.h"

static int
null_initialize(struct storage_module *module)
{
    zsys_warning("null: this output module should be used for debug only, no data will be stored");

    module->is_initialized = true;
    return 0;
}

static int
null_ping(struct storage_module *module __attribute__ ((unused)))
{
    return 0;
}

static int
null_store_report(struct storage_module *module  __attribute__ ((unused)), struct payload *payload  __attribute__ ((unused)))
{
    return 0;
}

static int
null_deinitialize(struct storage_module *module)
{
    module->is_initialized = false;
    return 0;
}

static void
null_destroy(struct storage_module *module  __attribute__ ((unused)))
{
    return;
}

struct storage_module *
storage_null_create(struct config *config  __attribute__ ((unused)))
{
    struct storage_module *module = (struct storage_module *) malloc(sizeof(struct storage_module));

    if (!module)
        return NULL;

    module->type = STORAGE_NULL;
    module->context = NULL;
    module->is_initialized = false;
    module->initialize = null_initialize;
    module->ping = null_ping;
    module->store_report = null_store_report;
    module->deinitialize = null_deinitialize;
    module->destroy = null_destroy;

    return module;
}

