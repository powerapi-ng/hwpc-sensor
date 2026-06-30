/*
 *  Copyright (c) 2026, Inria
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
#include <sys/timerfd.h>
#include <errno.h>

#include "ticker.h"


struct ticker_config *
ticker_config_create(unsigned int perf_sampling_interval_ms)
{
    struct ticker_config *config = (struct ticker_config *) malloc(sizeof(struct ticker_config));

    if (!config)
        return NULL;

    config->perf_sampling_interval_ms = perf_sampling_interval_ms;

    return config;
}

void
ticker_config_destroy(struct ticker_config *config)
{
    if (!config)
        return;

    free(config);
}

/*
 * ticker_context stores the context of a ticker actor.
 */
struct ticker_context
{
    struct ticker_config *config;
    bool terminated;
    zsock_t *pipe;
    zsock_t *ticker;
    int timer_fd;
    zpoller_t *poller;
};

static struct ticker_context *
ticker_context_create(struct ticker_config *config, zsock_t *pipe)
{
    struct ticker_context *ctx = (struct ticker_context *) malloc(sizeof(struct ticker_context));
    
    if (!ctx)
        return NULL;

    ctx->config = config;
    ctx->terminated = false;
    ctx->pipe = pipe;
    ctx->ticker = zsock_new_pub("inproc://ticker");
    ctx->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    ctx->poller = zpoller_new(ctx->pipe, &ctx->timer_fd, NULL);

    return ctx;
}

static void
ticker_context_destroy(struct ticker_context *ctx)
{
    if (!ctx)
        return;
    
    zpoller_destroy(&ctx->poller);
    zsock_destroy(&ctx->ticker);
    close(ctx->timer_fd);
    free(ctx);
}

static int
start_timerfd(struct ticker_context *ctx)
{
    struct timespec interval = {
        .tv_sec = ctx->config->perf_sampling_interval_ms / 1000,
        .tv_nsec = (long) (ctx->config->perf_sampling_interval_ms % 1000) * 1000000L,
    };
    const struct itimerspec spec = { .it_interval = interval, .it_value = interval };

    if (timerfd_settime(ctx->timer_fd, 0, &spec, NULL) == -1) {
        zsys_error("ticker: Failed to start timerfd: %s", strerror(errno));
        return -1;
    }

    return 0;
}

static void
handle_pipe(struct ticker_context *ctx)
{
    char *command = zstr_recv(ctx->pipe);

    if (streq(command, "$TERM")) {
        ctx->terminated = true;
        zsys_info("ticker: bye!");
    }
    else
        zsys_error("ticker: Invalid pipe command: %s", command);

    zstr_free(&command);
}

static void
handle_timerfd(struct ticker_context *ctx)
{
    uint64_t expirations = 0;

    if (read(ctx->timer_fd, &expirations, sizeof(expirations)) != sizeof(expirations)) {
        zsys_error("ticker: Failed to read timerfd: %s", strerror(errno));
        return;
    }

    if (expirations > 1)
        zsys_warning("ticker: Missed %" PRIu64 " tick periods", expirations - 1);

    zsock_send(ctx->ticker, "s8", "CLOCK_TICK", zclock_time());
}

void ticker_actor(zsock_t *pipe, void *args)
{
    struct ticker_context *ctx = ticker_context_create((struct ticker_config *) args, pipe);
    void *which = NULL;  /* The poller mixes ZMQ sockets and a raw timer fd handle. */

    if (!ctx) {
        zsys_error("ticker: Failed to create actor context");
        return;
    }

    zsock_signal(pipe, 0);

    if (start_timerfd(ctx))
        goto cleanup;

    while (!ctx->terminated) {
        which = zpoller_wait(ctx->poller, -1);

        if (zpoller_terminated(ctx->poller))
            break;

        if (which == ctx->pipe)
            handle_pipe(ctx);
        else if (which == &ctx->timer_fd)
            handle_timerfd(ctx);
    }

cleanup:
    ticker_context_destroy(ctx);
}
