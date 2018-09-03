#include <czmq.h>
#include <stdint.h>
#include <mongoc.h>

#include "report.h"
#include "util.h"
#include "hwinfo.h"
#include "events.h"
#include "perf.h"
#include "storage.h"

struct report_payload *
report_payload_create(uint64_t timestamp, char *cgroup_name, struct events_config *events)
{
    struct report_payload *rpl = malloc(sizeof(struct report_payload));

    if (!rpl)
        return NULL;

    rpl->timestamp = timestamp;
    rpl->cgroup_name = cgroup_name;
    rpl->events = events;
    rpl->reports = zhashx_new();
    zhashx_set_key_duplicator(rpl->reports, (zhashx_duplicator_fn *) uintptrdup);
    zhashx_set_key_comparator(rpl->reports, (zhashx_comparator_fn *) uintptrcmp);
    zhashx_set_key_destructor(rpl->reports, (zhashx_destructor_fn *) ptrfree);
    zhashx_set_destructor(rpl->reports, (zhashx_destructor_fn *) ptrfree);

    return rpl;
}

void
report_payload_destroy(struct report_payload *rpl)
{
    if (!rpl)
        return;

    zhashx_destroy(&rpl->reports);
    free(rpl);
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
    struct report_payload *payload = NULL;

    zsock_recv(ctx->reporting, "p", &payload);
    
    if (!payload)
        return;

    if (STORAGE_MODULE_CALL(ctx->config->storage, store_report, payload)) {
        zsys_error("report: failed to store the report for timestamp=%lu", payload->timestamp);
    }

    report_payload_destroy(payload);
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

