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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>

#include "storage.h"
#include "storage_csv.h"
#include "config.h"

static void
group_fd_destroy(FILE **fd_ptr)
{
    if (!*fd_ptr)
        return;

    fflush(*fd_ptr);
    fsync(fileno(*fd_ptr));
    fclose(*fd_ptr);
    *fd_ptr = NULL;
}

static struct csv_context *
csv_context_create(const char *sensor_name, const char *output_dir)
{
    struct csv_context *ctx = malloc(sizeof(struct csv_context));

    if (!ctx)
        return NULL;

    ctx->config.output_dir = output_dir;
    ctx->config.sensor_name = sensor_name;

    ctx->groups_fd = zhashx_new();
    zhashx_set_destructor(ctx->groups_fd, (zhashx_destructor_fn *) group_fd_destroy);

    ctx->groups_events = zhashx_new();
    zhashx_set_destructor(ctx->groups_events, (zhashx_destructor_fn *) zlistx_destroy);

    return ctx;
}

static void
csv_context_destroy(struct csv_context *ctx)
{
    if (!ctx)
        return;

    zhashx_destroy(&ctx->groups_fd);
    zhashx_destroy(&ctx->groups_events);
    free(ctx);
}

static int
csv_initialize(struct storage_module *module)
{
    struct csv_context *ctx = module->context;
    struct stat outdir_stat = {0};

    /* create output directory */
    errno = 0;
    if (mkdir(ctx->config.output_dir, 0755) == -1) {
        /* ignore if directory already exists */
        if (errno != EEXIST) {
            zsys_error("csv: failed to create output directory: %s", strerror(errno));
            return -1;
        }
    }

    /* check if directory exists, above EEXIST check DO NOT guarantee that path is a directory */
    errno = 0;
    if (stat(ctx->config.output_dir, &outdir_stat) == -1) {
        zsys_error("csv: failed to check output dir: %s", strerror(errno));
        return -1;
    }
    if (!S_ISDIR(outdir_stat.st_mode)) {
        zsys_error("csv: output path already exists and is not a directory");
        return -1;
    }

    /* check if we can write into the output directory */
    errno = 0;
    if (access(ctx->config.output_dir, W_OK)) {
        zsys_error("csv: output path is not writable: %s", strerror(errno));
        return -1;
    }

    module->is_initialized = true;
    return 0;
}

static int
csv_ping(struct storage_module *module __attribute__ ((unused)))
{
    /* ping is not needed because the relevant checks are done when initializing the module */
    return 0;
}

static int
write_group_header(struct csv_context *ctx, const char *group, FILE *fd, zhashx_t *events)
{
    char buffer[CSV_LINE_BUFFER_SIZE] = {0};
    int pos = 0;
    zlistx_t *events_name = NULL;
    const char *event_name = NULL;

    events_name = zhashx_keys(events);
    if (!events_name)
        return -1;

    /* sort events by name */
    zlistx_set_comparator(events_name, (zlistx_comparator_fn *) strcmp);
    zlistx_sort(events_name);

    /* write static elements to buffer */
    pos += snprintf(buffer, CSV_LINE_BUFFER_SIZE, "timestamp,sensor,target,socket,cpu");

    /* append dynamic elements (events) to buffer */
    for (event_name = zlistx_first(events_name); event_name; event_name = zlistx_next(events_name)) {
        pos += snprintf(buffer + pos, CSV_LINE_BUFFER_SIZE - pos, ",%s", event_name);
        if (pos >= CSV_LINE_BUFFER_SIZE)
            goto error_buffer_too_small;
    }

    if (fprintf(fd, "%s\n", buffer) < 0)
        goto error_failed_write;

    /* force writing to the disk */
    fflush(fd);

    /* store events name in the order written in header */
    zhashx_insert(ctx->groups_events, group, events_name);

    return 0;

error_failed_write:
error_buffer_too_small:
    zlistx_destroy(&events_name);
    return -1;
}

static int
open_group_outfile(struct csv_context *ctx, const char *group_name)
{
    char path[PATH_MAX] = {0};
    int fd = -1;
    FILE *file = NULL;

    if (snprintf(path, PATH_MAX, "%s/%s.csv", ctx->config.output_dir, group_name) >= PATH_MAX) {
        zsys_error("csv: the destination path for output file of group %s is too long", group_name);
        return -1;
    }

    errno = 0;
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd == -1) {
        zsys_error("csv: failed to open output file for group %s: %s", group_name, strerror(errno));
        return -1;
    }

    errno = 0;
    file = fdopen(fd, "w");
    if (!file) {
        zsys_error("csv: failed to associate a stream to output file of group %s: %s", group_name, strerror(errno));
        close(fd);
        return -1;
    }

    zhashx_insert(ctx->groups_fd, group_name, file);
    return 0;
}

static int
write_events_value(struct csv_context *ctx, const char *group, FILE *fd, uint64_t timestamp, const char *target, const char *socket, const char *cpu, zhashx_t *events)
{
    zlistx_t *events_name = NULL;
    char buffer[CSV_LINE_BUFFER_SIZE] = {0};
    int pos = 0;
    const char *event_name = NULL;
    const uint64_t *event_value = NULL;

    /* get events name in the order of csv header */
    events_name = zhashx_lookup(ctx->groups_events, group);
    if (!events_name)
        return -1;

    /* write static elements to buffer */
    pos += snprintf(buffer, CSV_LINE_BUFFER_SIZE, "%" PRIu64 ",%s,%s,%s,%s", timestamp, ctx->config.sensor_name, target, socket, cpu);
 
    /* write dynamic elements (events) to buffer */
    for (event_name = zlistx_first(events_name); event_name; event_name = zlistx_next(events_name)) {
        event_value = zhashx_lookup(events, event_name);
        if (!event_value)
            return -1;

        pos += snprintf(buffer + pos, CSV_LINE_BUFFER_SIZE - pos, ",%" PRIu64, *event_value);
        if (pos >= CSV_LINE_BUFFER_SIZE)
            return -1;
    }

    if (fprintf(fd, "%s\n", buffer) < 0)
        return -1;

    return 0;
}

static int
csv_store_report(struct storage_module *module, struct payload *payload)
{
    struct csv_context *ctx = module->context;
    struct payload_group_data *group_data = NULL;
    const char *group_name = NULL;
    FILE *group_fd = NULL;
    bool write_header = false;
    struct payload_pkg_data *pkg_data = NULL;
    const char *pkg_id = NULL;
    struct payload_cpu_data *cpu_data = NULL;
    const char *cpu_id = NULL;

    /* 
     * write report into csv file as following: 
     * timestamp,sensor,target,socket,cpu,INSTRUCTIONS_RETIRED,LLC_MISSES
     * 1538327257673,grvingt-64,system,0,56,5996,108
     */
    for (group_data = zhashx_first(payload->groups); group_data; group_data = zhashx_next(payload->groups)) {
        group_name = zhashx_cursor(payload->groups);
        group_fd = zhashx_lookup(ctx->groups_fd, group_name);
        if (!group_fd) {
            if (open_group_outfile(ctx, group_name))
                return -1;

            group_fd = zhashx_lookup(ctx->groups_fd, group_name);
            write_header = true;
        }

        for (pkg_data = zhashx_first(group_data->pkgs); pkg_data; pkg_data = zhashx_next(group_data->pkgs)) {
            pkg_id = zhashx_cursor(group_data->pkgs);

            for (cpu_data = zhashx_first(pkg_data->cpus); cpu_data; cpu_data = zhashx_next(pkg_data->cpus)) {
                cpu_id = zhashx_cursor(pkg_data->cpus);

                if (write_header) {
                    if (write_group_header(ctx, group_name, group_fd, cpu_data->events)) {
                        zsys_error("csv: failed to write header to file for group=%s", group_name);
                        return -1;
                    }
                    write_header = false;
                }
                if (write_events_value(ctx, group_name, group_fd, payload->timestamp, payload->target_name, pkg_id, cpu_id, cpu_data->events)) {
                    zsys_error("csv: failed to write report to file for group=%s timestamp=%" PRIu64, group_name, payload->timestamp);
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int
csv_deinitialize(struct storage_module *module)
{
    module->is_initialized = false;
    return 0;
}

static void
csv_destroy(struct storage_module *module)
{
    if (!module)
        return;

    csv_context_destroy(module->context);
}

struct storage_module *
storage_csv_create(struct config *config)
{
    struct storage_module *module = NULL;
    struct csv_context *ctx = NULL;

    module = malloc(sizeof(struct storage_module));
    if (!module)
        goto error;

    ctx = csv_context_create(config->sensor.name, config->storage.csv.outdir);
    if (!ctx)
        goto error;

    module->type = STORAGE_CSV;
    module->context = ctx;
    module->is_initialized = false;
    module->initialize = csv_initialize;
    module->ping = csv_ping;
    module->store_report = csv_store_report;
    module->deinitialize = csv_deinitialize;
    module->destroy = csv_destroy;

    return module;

error:
    csv_context_destroy(ctx);
    free(module);
    return NULL;
}

