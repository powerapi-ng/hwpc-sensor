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

#include <czmq.h>
#include <stdint.h>

#include "report.h"
#include "util.h"
#include "hwinfo.h"
#include "events.h"
#include "payload.h"
#include "perf.h"
#include "storage.h"

struct report_config *
report_config_create(struct storage_module *storage_module)
{
    struct report_config *config = malloc(sizeof(struct report_config));

    if (!config)
        return NULL;

    config->storage = storage_module;

    return config;
}

void
report_config_destroy(struct report_config *config)
{
    if (!config)
        return;

    free(config);
}

static struct report_context *
report_context_create(struct report_config *config, zsock_t *pipe)
{
    struct report_context *ctx = malloc(sizeof(struct report_context));
    
    if (!ctx)
        return NULL;

    ctx->terminated = false;
    ctx->pipe = pipe;
    ctx->reporting = zsock_new_pull("inproc://reporting");
    ctx->poller = zpoller_new(ctx->pipe, ctx->reporting, NULL);
    ctx->config = config;
    
    return ctx;
}

static void
report_context_destroy(struct report_context *ctx)
{
    if (!ctx)
        return;

    zpoller_destroy(&ctx->poller);
    zsock_destroy(&ctx->reporting);
    free(ctx);
}

static void
handle_pipe(struct report_context *ctx)
{
    char *command = zstr_recv(ctx->pipe);

    if (streq(command, "$TERM")) {
        ctx->terminated = true;
        zsys_info("reporting: bye!");
    }
    else {
        zsys_error("reporting: invalid pipe command: %s", command);
    }

    zstr_free(&command);
}

static void
handle_reporting(struct report_context *ctx)
{
    struct payload *payload = NULL;

    zsock_recv(ctx->reporting, "p", &payload);
    
    if (!payload)
        return;

    if (STORAGE_MODULE_CALL(ctx->config->storage, store_report, payload)) {
        zsys_error("report: failed to store the report for timestamp=%lu", payload->timestamp);
    }

    payload_destroy(payload);
}

void
reporting_actor(zsock_t *pipe, void *args)
{
    struct report_context *ctx = report_context_create(args, pipe);
    zsock_t *which = NULL;
   
    if (!ctx) {
        zsys_error("reporting: cannot create context");
        return;
    }

    zsock_signal(pipe, 0);

    while (!ctx->terminated) {
        which = zpoller_wait(ctx->poller, -1);

        if (zpoller_terminated(ctx->poller)) {
            break;
        }

        if (which == ctx->pipe) {
            handle_pipe(ctx);
        }
        else if (which == ctx->reporting) {
            handle_reporting(ctx);
        }
    }

    report_context_destroy(ctx);
}

