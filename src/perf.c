#include <czmq.h>
#include <errno.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <unistd.h>

#include "hwinfo.h"
#include "events.h"
#include "perf.h"
#include "util.h"
#include "report.h"

struct perf_config *
perf_config_create(struct hwinfo *hwinfo, struct events_config *events, const char *cgroup_name, const char *cgroup_path, bool one_cpu_per_pkg)
{
    struct perf_config *config = malloc(sizeof(struct perf_config));
    
    if (!config)
        return NULL;

    config->hwinfo = hwinfo_dup(hwinfo);
    config->events = events;
    config->cgroup_name = strdup(cgroup_name);
    config->cgroup_path = (cgroup_path) ? strdup(cgroup_path) : NULL;
    config->one_cpu_per_pkg = one_cpu_per_pkg;

    return config;
}

void
perf_config_destroy(struct perf_config *config)
{
    if (!config)
        return;

    hwinfo_destroy(config->hwinfo);
    free(config->cgroup_name);
    free(config->cgroup_path);
    free(config);
}

static struct perf_context *
perf_context_create(struct perf_config *config, zsock_t *pipe)
{
    struct perf_context *ctx = malloc(sizeof(struct perf_context));

    if (!ctx)
        return NULL;

    ctx->config = config;
    ctx->terminated = false;
    ctx->pipe = pipe;
    ctx->ticker = zsock_new_sub("inproc://ticker", "CLOCK_TICK");
    ctx->poller = zpoller_new(ctx->pipe, ctx->ticker, NULL);
    ctx->reporting = zsock_new_push("inproc://reporting");
    ctx->cgroup_fd = -1; /* by default, system wide monitoring */
    ctx->perf_fds = zhashx_new();
    zhashx_set_key_duplicator(ctx->perf_fds, (zhashx_duplicator_fn *) uintptrdup);
    zhashx_set_key_comparator(ctx->perf_fds, (zhashx_comparator_fn *) uintptrcmp);
    zhashx_set_key_destructor(ctx->perf_fds, (zhashx_destructor_fn *) ptrfree);
    zhashx_set_destructor(ctx->perf_fds, (zhashx_destructor_fn *) zlistx_destroy);

    return ctx;
}

static void
perf_context_destroy(struct perf_context *ctx)
{
    if (!ctx)
        return;

    perf_config_destroy(ctx->config);
    zpoller_destroy(&ctx->poller);
    zsock_destroy(&ctx->ticker);
    zsock_destroy(&ctx->reporting);
    zhashx_destroy(&ctx->perf_fds);
    free(ctx);
}

static void
perf_event_fd_destroy(int **fd_ptr)
{
    close(**fd_ptr);
    free(*fd_ptr);
    *fd_ptr = NULL;
}

static int
perf_event_setup_cpu(struct perf_context *ctx, int perf_flags, unsigned int cpu_id)
{
    size_t i;
    int group_fd = -1;
    int perf_fd;
    zlistx_t *fds = NULL;

    fds = zlistx_new();
    zlistx_set_duplicator(fds, (zlistx_duplicator_fn *) intptrdup);
    zlistx_set_destructor(fds, (zlistx_destructor_fn *) perf_event_fd_destroy);

    /* open each events for this cpu */
    for (i = 0; i < ctx->config->events->num_attrs; i++) {
        perf_fd = perf_event_open(&ctx->config->events->attrs[i].perf_attr, ctx->cgroup_fd, cpu_id, group_fd, perf_flags);
        if (perf_fd < 1) {
            zlistx_destroy(&fds);
            zsys_error("perf<%s>: failed opening perf_event for cpu=%d event=%s errno=%d", ctx->config->cgroup_name, cpu_id, ctx->config->events->attrs[i].name, errno);
            return -1;
        }

        /* the first event is the group leader */
        if (i == 0) {
            group_fd = perf_fd;
        }

        /* store the file descriptor of the event */
        zlistx_add_end(fds, &perf_fd);
    }

    zhashx_insert(ctx->perf_fds, &cpu_id, fds);

    return 0;
}

static int
perf_event_initialize(struct perf_context *ctx)
{
    int flags = 0;
    struct hwinfo_pkg *pkg_info = NULL;
    unsigned int *cpu_id = NULL;

    /* open target is a cgroup, open its path */
    if (ctx->config->cgroup_path) {
        ctx->cgroup_fd = open(ctx->config->cgroup_path, O_RDONLY); 
        if (ctx->cgroup_fd < 1) {
            zsys_error("perf<%s>: cannot open cgroup path '%s'", ctx->config->cgroup_name, ctx->config->cgroup_path);
            return -1;
        }
        flags |= PERF_FLAG_PID_CGROUP;
    }

    /* setup perf_event for cpu(s) of detected package(s) */
    for (pkg_info = zhashx_first(ctx->config->hwinfo->pkgs); pkg_info; pkg_info = zhashx_next(ctx->config->hwinfo->pkgs)) {
        for (cpu_id = zlistx_first(pkg_info->cpus_id); cpu_id; cpu_id = zlistx_next(pkg_info->cpus_id)) {
            if (perf_event_setup_cpu(ctx, flags, *cpu_id)) {
                zsys_error("perf<%s>: failed to setup cpu_id=%d", ctx->config->cgroup_name, *cpu_id);
                return -1;
            }

            if (ctx->config->one_cpu_per_pkg)
                break;
        }
    }

    return 0;
}

static void
perf_event_enable(struct perf_context *ctx)
{
    zlistx_t *events_fd = NULL;
    int *fd = NULL;

    for (events_fd = zhashx_first(ctx->perf_fds); events_fd; events_fd = zhashx_next(ctx->perf_fds)) {
        fd = zlistx_first(events_fd);

        if(ioctl(*fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP)) {
            zsys_error("perf<%s>: cannot reset event fd=%d errno=%d", ctx->config->cgroup_name, *fd, errno);
        }

        if (ioctl(*fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP)) {
            zsys_error("perf<%s>: cannot enable event fd=%d errno=%d", ctx->config->cgroup_name, *fd, errno);
        }
    }
}

static struct perf_cpu_report *
perf_event_read_events(struct perf_context *ctx, zlistx_t *events_fd)
{
    int *cgroup_leader_fd = NULL;
    struct perf_cpu_report *buffer = NULL;
    ssize_t read_size = offsetof(struct perf_cpu_report, values) + sizeof(struct perf_report_value[ctx->config->events->num_attrs]);
    ssize_t ret;

    cgroup_leader_fd = zlistx_first(events_fd);
    if (!cgroup_leader_fd)
        goto error;

    buffer = malloc(read_size);
    if (!buffer)
        goto error;

    ret = read(*cgroup_leader_fd, buffer, read_size);
    if (ret != read_size)
        goto error;

    /* events need to be reset in order to count the events per tick */
    if (ioctl(*cgroup_leader_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP))
        goto error;

    return buffer;

error:
    free(buffer);
    return NULL;
}

static void
handle_pipe(struct perf_context *ctx)
{
    char *command = zstr_recv(ctx->pipe);
    
    if (streq(command, "$TERM")) {
        ctx->terminated = true;
        zsys_info("perf<%s>: bye!", ctx->config->cgroup_name);
    }
    else {
        zsys_error("perf<%s>: invalid pipe command: %s", ctx->config->cgroup_name, command);
    }

    zstr_free(&command);
}

static double
compute_perf_multiplexing_ratio(struct perf_cpu_report *cpu_report)
{
    /* prevent divide by zero */
    if (!cpu_report->time_enabled)
        return 0.0;

    return (cpu_report->time_running * 1.0) / cpu_report->time_enabled;
}

static void
handle_ticker(struct perf_context *ctx)
{
    uint64_t timestamp;
    struct report_payload *payload = NULL;
    unsigned int *cpu_id = NULL;
    zlistx_t *events_fd = NULL;
    struct perf_cpu_report *cpu_report = NULL;
    double perf_multiplexing_ratio;
    
    zsock_recv(ctx->ticker, "s8", NULL, &timestamp);

    /* payload to be sent to reporting actor */
    payload = report_payload_create(timestamp, ctx->config->cgroup_name, ctx->config->events);
    if (!payload)
        return;

    for (events_fd = zhashx_first(ctx->perf_fds); events_fd; events_fd = zhashx_next(ctx->perf_fds)) {
        cpu_id = (unsigned int *) zhashx_cursor(ctx->perf_fds);

        /* read perf_event values for this cpu */
        cpu_report = perf_event_read_events(ctx, events_fd);
        if (!cpu_report) {
            zsys_error("perf<%s>: cannot read perf report for cpu=%lu", ctx->config->cgroup_name, *cpu_id);
            continue;
        }

        /* warn if there is multiplexing on this cpu */
        if (cpu_report->time_running != cpu_report->time_enabled) {
            perf_multiplexing_ratio = (1.0 - compute_perf_multiplexing_ratio(cpu_report)) * 100;
            zsys_warning("perf<%s>: there is multiplexing (ena=%lld run=%lld ratio=%.2f%%) for events on cpu=%lu", ctx->config->cgroup_name, cpu_report->time_enabled, cpu_report->time_running, perf_multiplexing_ratio, *cpu_id);
        }

        /* add report to payload */
        zhashx_insert(payload->reports, cpu_id, cpu_report);
    }

    /* send payload to reporting actor */
    zsock_send(ctx->reporting, "p", payload);
}

void
perf_monitoring_actor(zsock_t *pipe, void *args)
{
    struct perf_context *ctx = perf_context_create(args, pipe);
    zsock_t *which = NULL;

    if (!ctx) {
        zsys_error("perf<%s>: cannot create perf context", ctx->config->cgroup_name);
        free(args);
        return;
    }

    zsock_signal(pipe, 0);

    if (perf_event_initialize(ctx)) {
        zsys_error("perf<%s>: cannot initialize perf monitoring", ctx->config->cgroup_name);
        perf_context_destroy(ctx);
        return;
    }

    perf_event_enable(ctx);

    while (!ctx->terminated) {
        which = zpoller_wait(ctx->poller, -1);

        if (zpoller_terminated(ctx->poller)) {
            break;
        }
        else if (which == ctx->pipe) {
            handle_pipe(ctx);
        }
        else if (which == ctx->ticker) {
            handle_ticker(ctx);
        }
    }

    perf_context_destroy(ctx);
}

