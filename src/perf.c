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
#include <errno.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <unistd.h>

#include "target.h"
#include "hwinfo.h"
#include "events.h"
#include "payload.h"
#include "perf.h"
#include "util.h"
#include "report.h"

struct perf_config *
perf_config_create(struct hwinfo *hwinfo, zhashx_t *events_groups, struct target *target)
{
    struct perf_config *config = malloc(sizeof(struct perf_config));
    
    if (!config)
        return NULL;

    config->hwinfo = hwinfo_dup(hwinfo);
    config->events_groups = zhashx_dup(events_groups);
    config->target = target;

    return config;
}

void
perf_config_destroy(struct perf_config *config)
{
    if (!config)
        return;

    hwinfo_destroy(config->hwinfo);
    zhashx_destroy(&config->events_groups);
    target_destroy(config->target);
    free(config);
}

static void
perf_event_fd_destroy(int **fd_ptr)
{
    if (!*fd_ptr)
        return;

    close(**fd_ptr);
    free(*fd_ptr);
    *fd_ptr = NULL;
}

static struct perf_group_cpu_context *
perf_group_cpu_context_create(void)
{
    struct perf_group_cpu_context *ctx = malloc(sizeof(struct perf_group_cpu_context));

    if (!ctx)
        return NULL;

    ctx->perf_fds = zlistx_new();
    zlistx_set_duplicator(ctx->perf_fds, (zlistx_duplicator_fn *) intptrdup);
    zlistx_set_destructor(ctx->perf_fds, (zlistx_destructor_fn *) perf_event_fd_destroy);

    return ctx;
}

static void
perf_group_cpu_context_destroy(struct perf_group_cpu_context **ctx)
{
    if (!*ctx)
        return;

    zlistx_destroy(&(*ctx)->perf_fds);
    free(*ctx);
    *ctx = NULL;
}

static struct perf_group_pkg_context *
perf_group_pkg_context_create(void)
{
    struct perf_group_pkg_context *ctx = malloc(sizeof(struct perf_group_pkg_context));

    if (!ctx)
        return NULL;

    ctx->cpus_ctx = zhashx_new();
    zhashx_set_destructor(ctx->cpus_ctx, (zhashx_destructor_fn *) perf_group_cpu_context_destroy);

    return ctx;
}

static void
perf_group_pkg_context_destroy(struct perf_group_pkg_context **ctx)
{
    if (!*ctx)
        return;

    zhashx_destroy(&(*ctx)->cpus_ctx);
    free(*ctx);
    *ctx = NULL;
}

static struct perf_group_context *
perf_group_context_create(struct events_group *group)
{
    struct perf_group_context *ctx = malloc(sizeof(struct perf_group_context));

    if (!ctx)
        return NULL;

    ctx->config = group;
    ctx->pkgs_ctx = zhashx_new();
    zhashx_set_destructor(ctx->pkgs_ctx, (zhashx_destructor_fn *) perf_group_pkg_context_destroy);

    return ctx;
}

static void
perf_group_context_destroy(struct perf_group_context **ctx)
{
    if (!*ctx)
        return;

    zhashx_destroy(&(*ctx)->pkgs_ctx);
    free(*ctx);
    *ctx = NULL;
}

static struct perf_context *
perf_context_create(struct perf_config *config, zsock_t *pipe, const char *target_name)
{
    struct perf_context *ctx = malloc(sizeof(struct perf_context));

    if (!ctx)
        return NULL;

    ctx->config = config;
    ctx->target_name = target_name;
    ctx->terminated = false;
    ctx->pipe = pipe;
    ctx->ticker = zsock_new_sub("inproc://ticker", "CLOCK_TICK");
    ctx->poller = zpoller_new(ctx->pipe, ctx->ticker, NULL);
    ctx->reporting = zsock_new_push("inproc://reporting");
    ctx->cgroup_fd = -1; /* by default, system wide monitoring */
    ctx->groups_ctx = zhashx_new();
    zhashx_set_destructor(ctx->groups_ctx, (zhashx_destructor_fn *) perf_group_context_destroy);

    return ctx;
}

static void
perf_context_destroy(struct perf_context *ctx)
{
    if (!ctx)
        return;

    zpoller_destroy(&ctx->poller);
    zsock_destroy(&ctx->ticker);
    zsock_destroy(&ctx->reporting);
    close(ctx->cgroup_fd);
    zhashx_destroy(&ctx->groups_ctx);
    free(ctx);
}

static int
perf_events_group_setup_cpu(struct perf_context *ctx, struct perf_group_cpu_context *cpu_ctx, struct events_group *group, unsigned long perf_flags, const char *cpu_id)
{
    int group_fd = -1;
    int perf_fd;
    char *cpu_id_endp = NULL;
    long cpu;
    struct event_config *event = NULL;

    errno = 0;
    cpu = strtol(cpu_id, &cpu_id_endp, 0);
    if (*cpu_id == '\0' || *cpu_id_endp != '\0' || errno) {
        zsys_error("perf<%s>: failed convert cpu id for group=%s cpu=%s", ctx->target_name, group->name, cpu_id);
        return -1;
    }
    if (cpu > INT_MAX || cpu < INT_MIN) {
        zsys_error("perf<%s>: cpu id is out of range for group=%s cpu=%s", ctx->target_name, group->name, cpu_id);
        return -1;
    }

    for (event = zlistx_first(group->events); event; event = zlistx_next(group->events)) {
        errno = 0;
        perf_fd = perf_event_open(&event->attr, ctx->cgroup_fd, (int) cpu, group_fd, perf_flags);
        if (perf_fd < 1) {
            zsys_error("perf<%s>: failed opening perf event for group=%s cpu=%d event=%s errno=%d", ctx->target_name, group->name, (int) cpu, event->name, errno);
            return -1;
        }

        /* the first event is the group leader */
        if (group_fd == -1)
            group_fd = perf_fd;

        zlistx_add_end(cpu_ctx->perf_fds, &perf_fd);
    }

    return 0;
}

static int
perf_events_groups_initialize(struct perf_context *ctx)
{
    unsigned long perf_flags = 0;
    struct events_group *events_group = NULL;
    const char *events_group_name = NULL;
    struct perf_group_context *group_ctx = NULL;
    struct hwinfo_pkg *pkg = NULL;
    const char *pkg_id = NULL;
    struct perf_group_pkg_context *pkg_ctx = NULL;
    const char *cpu_id = NULL;
    struct perf_group_cpu_context *cpu_ctx = NULL;

    if (ctx->config->target->cgroup_path) {
        perf_flags |= PERF_FLAG_PID_CGROUP;
        errno = 0;
        ctx->cgroup_fd = open(ctx->config->target->cgroup_path, O_RDONLY); 
        if (ctx->cgroup_fd < 1) {
            zsys_error("perf<%s>: cannot open cgroup dir path=%s errno=%d", ctx->target_name, ctx->config->target->cgroup_path, errno);
            goto error;
        }
    }

    for (events_group = zhashx_first(ctx->config->events_groups); events_group; events_group = zhashx_next(ctx->config->events_groups)) {
        events_group_name = zhashx_cursor(ctx->config->events_groups);

        /* create group context */
        group_ctx = perf_group_context_create(events_group);
        if (!group_ctx) {
            zsys_error("perf<%s>: failed to create context for group=%s", ctx->target_name, events_group_name);
            goto error;
        }

        for (pkg = zhashx_first(ctx->config->hwinfo->pkgs); pkg; pkg = zhashx_next(ctx->config->hwinfo->pkgs)) {
            pkg_id = zhashx_cursor(ctx->config->hwinfo->pkgs);

            /* create package context */
            pkg_ctx = perf_group_pkg_context_create();
            if (!pkg_ctx) {
                zsys_error("perf<%s>: failed to create pkg context for group=%s pkg=%s", ctx->target_name, events_group_name, pkg_id);
                goto error;
            }

            for (cpu_id = zlistx_first(pkg->cpus_id); cpu_id; cpu_id = zlistx_next(pkg->cpus_id)) {
                /* create cpu context */
                cpu_ctx = perf_group_cpu_context_create();
                if (!cpu_ctx) {
                    zsys_error("perf<%s>: failed to create cpu context for group=%s pkg=%s cpu=%s", ctx->target_name, events_group_name, pkg_id, cpu_id);
                    goto error;
                }

                /* open events of the group for the cpu */
                if (perf_events_group_setup_cpu(ctx, cpu_ctx, events_group, perf_flags, cpu_id)) {
                    zsys_error("perf<%s>: failed to setup perf for group=%s pkg=%s cpu=%s", ctx->target_name, events_group_name, pkg_id, cpu_id);
                    goto error;
                }

                /* store cpu context */
                zhashx_insert(pkg_ctx->cpus_ctx, cpu_id, cpu_ctx);

                if (events_group->type == MONITOR_ONE_CPU_PER_SOCKET)
                    break;
            }

            /* store pkg context */
            zhashx_insert(group_ctx->pkgs_ctx, pkg_id, pkg_ctx);
        }

        /* stores per-cpu events fd for group */
        zhashx_insert(ctx->groups_ctx, events_group_name, group_ctx);
    }

    return 0;

error:
    close(ctx->cgroup_fd);
    perf_group_context_destroy(&group_ctx);
    perf_group_pkg_context_destroy(&pkg_ctx);
    perf_group_cpu_context_destroy(&cpu_ctx);
    return -1;
}

static void
perf_events_groups_enable(struct perf_context *ctx)
{
    struct perf_group_context *group_ctx = NULL;
    const char *group_name = NULL;
    struct perf_group_pkg_context *pkg_ctx = NULL;
    const char *pkg_id = NULL;
    struct perf_group_cpu_context *cpu_ctx = NULL;
    const char *cpu_id = NULL;
    const int *group_leader_fd = NULL;

    for (group_ctx = zhashx_first(ctx->groups_ctx); group_ctx; group_ctx = zhashx_next(ctx->groups_ctx)) {
        group_name = zhashx_cursor(ctx->groups_ctx);

        for (pkg_ctx = zhashx_first(group_ctx->pkgs_ctx); pkg_ctx; pkg_ctx = zhashx_next(group_ctx->pkgs_ctx)) {
            pkg_id = zhashx_cursor(group_ctx->pkgs_ctx);

            for (cpu_ctx = zhashx_first(pkg_ctx->cpus_ctx); cpu_ctx; cpu_ctx = zhashx_next(pkg_ctx->cpus_ctx)) {
                cpu_id = zhashx_cursor(pkg_ctx->cpus_ctx);
                group_leader_fd = zlistx_first(cpu_ctx->perf_fds);
                if (!group_leader_fd) {
                    zsys_error("perf<%s>: no group leader fd for group=%s pkg=%s cpu=%s", ctx->target_name, group_name, pkg_id, cpu_id);
                    continue;
                }

                errno = 0;
                if (ioctl(*group_leader_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP))
                    zsys_error("perf<%s>: cannot reset events for group=%s pkg=%s cpu=%s errno=%d", ctx->target_name, group_name, pkg_id, cpu_id, errno);

                errno = 0;
                if (ioctl(*group_leader_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP))
                    zsys_error("perf<%s>: cannot enable events for group=%s pkg=%s cpu=%s errno=%d", ctx->target_name, group_name, pkg_id, cpu_id, errno);
            }
        }
    }
}

static int
perf_events_group_read_cpu(struct perf_group_cpu_context *cpu_ctx, struct perf_read_format *buffer, size_t buffer_size)
{
    int *group_leader_fd = NULL;

    group_leader_fd = zlistx_first(cpu_ctx->perf_fds);
    if (!group_leader_fd)
        return -1;

    if (read(*group_leader_fd, buffer, buffer_size) != (ssize_t) buffer_size)
        return -1;

    /* counters need to be reset in order to count the events per tick */
    if (ioctl(*group_leader_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP))
        return -1;

    return 0;
}

static void
handle_pipe(struct perf_context *ctx)
{
    char *command = zstr_recv(ctx->pipe);
    
    if (streq(command, "$TERM")) {
        ctx->terminated = true;
        zsys_info("perf<%s>: shutting down actor", ctx->target_name);
    }
    else
        zsys_error("perf<%s>: invalid pipe command: %s", ctx->target_name, command);

    zstr_free(&command);
}

static inline double
compute_perf_multiplexing_ratio(struct perf_read_format *report)
{
    return (!report->time_enabled) ? 1.0 : (double) report->time_running / (double) report->time_enabled;
}

static int
populate_payload(struct perf_context *ctx, struct payload *payload)
{
    struct perf_group_context *group_ctx = NULL;
    const char *group_name = NULL;
    struct payload_group_data *group_data = NULL;
    struct perf_group_pkg_context *pkg_ctx = NULL;
    const char *pkg_id = NULL;
    struct payload_pkg_data *pkg_data = NULL;
    struct perf_group_cpu_context *cpu_ctx = NULL;
    const char *cpu_id = NULL;
    struct payload_cpu_data *cpu_data = NULL;
    size_t perf_read_buffer_size;
    struct perf_read_format *perf_read_buffer = NULL;
    double perf_multiplexing_ratio;
    const struct event_config *event = NULL;
    int event_i;

    for (group_ctx = zhashx_first(ctx->groups_ctx); group_ctx; group_ctx = zhashx_next(ctx->groups_ctx)) {
        group_name = zhashx_cursor(ctx->groups_ctx);
        group_data = payload_group_data_create();
        if (!group_data) {
            zsys_error("perf<%s>: failed to allocate group data for group=%s", ctx->target_name, group_name);
            goto error;
        }

        /* shared perf read buffer */
        perf_read_buffer_size = offsetof(struct perf_read_format, values) + sizeof(struct perf_counter_value[zlistx_size(group_ctx->config->events)]);
        perf_read_buffer = malloc(perf_read_buffer_size);
        if (!perf_read_buffer) {
            zsys_error("perf<%s>: failed to allocate perf read buffer for group=%s", ctx->target_name, group_name);
            goto error;
        }

        for (pkg_ctx = zhashx_first(group_ctx->pkgs_ctx); pkg_ctx; pkg_ctx = zhashx_next(group_ctx->pkgs_ctx)) {
            pkg_id = zhashx_cursor(group_ctx->pkgs_ctx);
            pkg_data = payload_pkg_data_create();
            if (!pkg_data) {
                zsys_error("perf<%s>: failed to allocate pkg data for group=%s pkg=%s", ctx->target_name, group_name, pkg_id);
                goto error;
            }

            for (cpu_ctx = zhashx_first(pkg_ctx->cpus_ctx); cpu_ctx; cpu_ctx = zhashx_next(pkg_ctx->cpus_ctx)) {
                cpu_id = zhashx_cursor(pkg_ctx->cpus_ctx);
                cpu_data = payload_cpu_data_create();
                if (!cpu_data) {
                    zsys_error("perf<%s>: failed to allocate cpu data for group=%s pkg=%s cpu=%s", ctx->target_name, group_name, pkg_id, cpu_id);
                    goto error;
                }

                /* read counters value for the cpu */
                if (perf_events_group_read_cpu(cpu_ctx, perf_read_buffer, perf_read_buffer_size)) {
                    zsys_error("perf<%s>: cannot read perf values for group=%s pkg=%s cpu=%s", ctx->target_name, group_name, pkg_id, cpu_id);
                    goto error;
                }

                /* warn if PMU multiplexing is happening */
                perf_multiplexing_ratio = compute_perf_multiplexing_ratio(perf_read_buffer);
                if (perf_multiplexing_ratio < 1.0) {
                    zsys_warning("perf<%s>: perf multiplexing for group=%s pkg=%s cpu=%s ratio=%f", ctx->target_name, group_name, pkg_id, cpu_id, perf_multiplexing_ratio);
                }

                /* store events value */
                zhashx_insert(cpu_data->events, "time_enabled", &perf_read_buffer->time_enabled);
                zhashx_insert(cpu_data->events, "time_running", &perf_read_buffer->time_running);
                for (event = zlistx_first(group_ctx->config->events), event_i = 0; event; event = zlistx_next(group_ctx->config->events), event_i++) {
                    zhashx_insert(cpu_data->events, event->name, &perf_read_buffer->values[event_i].value);
                }

                zhashx_insert(pkg_data->cpus, cpu_id, cpu_data);
            }

            zhashx_insert(group_data->pkgs, pkg_id, pkg_data);
        }

        free(perf_read_buffer);
        perf_read_buffer = NULL;
        zhashx_insert(payload->groups, group_name, group_data);
    }

    return 0;

error:
    free(perf_read_buffer);
    payload_cpu_data_destroy(&cpu_data);
    payload_pkg_data_destroy(&pkg_data);
    payload_group_data_destroy(&group_data);
    return -1;
}

static void
handle_ticker(struct perf_context *ctx)
{
    uint64_t timestamp;
    struct payload *payload = NULL;
    
    /* get tick timestamp */
    zsock_recv(ctx->ticker, "s8", NULL, &timestamp);

    payload = payload_create(timestamp, ctx->target_name);
    if (!payload) {
        zsys_error("perf<%s>: failed to allocate payload for timestamp=%lu", ctx->target_name, timestamp);
        return;
    }

    if (populate_payload(ctx, payload)) {
        zsys_error("perf<%s>: failed to populate payload for timestamp=%lu", ctx->target_name, timestamp);
        payload_destroy(payload);
        return;
    }

    /* send payload to reporting socket */
    zsock_send(ctx->reporting, "p", payload);
}

void
perf_monitoring_actor(zsock_t *pipe, void *args)
{
    struct perf_config *config = args;
    char *target_name = NULL;
    struct perf_context *ctx = NULL;
    zsock_t *which = NULL;

    zsock_signal(pipe, 0);

    target_name = target_resolve_real_name(config->target);
    if (!target_name) {
        zsys_error("perf: failed to resolve name of target for cgroup '%s'", config->target->cgroup_path);
        goto cleanup;
    }

    ctx = perf_context_create(args, pipe, target_name);
    if (!ctx) {
        zsys_error("perf<%s>: cannot create perf context", target_name);
        goto cleanup;
    }

    if (perf_events_groups_initialize(ctx)) {
        zsys_error("perf<%s>: cannot initialize perf monitoring", target_name);
        goto cleanup;
    }

    perf_events_groups_enable(ctx);

    zsys_info("perf<%s>: monitoring actor started", target_name);

    while (!ctx->terminated) {
        which = zpoller_wait(ctx->poller, -1);

        if (zpoller_terminated(ctx->poller))
            break;

        if (which == ctx->pipe)
            handle_pipe(ctx);
        else if (which == ctx->ticker)
            handle_ticker(ctx);
    }

cleanup:
    free(target_name);
    perf_config_destroy(config);
    perf_context_destroy(ctx);
}

int
perf_try_global_counting_event_open(void)
{
    struct perf_event_attr pe = {0};
    int fd;

    /* check support of perf_event by the kernel */
    if (access("/proc/sys/kernel/perf_event_paranoid", F_OK) == -1) {
        zsys_error("perf: kernel doesn't have perf_event support");
        return -1;
    }

    /* configure basic event that should be available on most CPUs */
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;

    errno = 0;
    fd = perf_event_open(&pe, -1, 0, -1, 0);
    if (fd == -1) {
        zsys_error("perf: perf_event_open failed with error: %s", strerror(errno));
        if (errno == EACCES || errno == EPERM) {
            zsys_error("perf: perf_event requires the CAP_PERFMON or CAP_SYS_ADMIN capability to work");
        }
        return -1;
    }

    close(fd);
    return 0;
}

