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

