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

#include "storage.h"
#include "storage_csv.h"

static void
group_fd_destroy(FILE **fd_ptr)
{
    if (!*fd_ptr)
        return;

    fflush(*fd_ptr);
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

    module->is_initialized = true;
    return 0;
}

static int
csv_ping(struct storage_module *module)
{
    struct csv_context *ctx = module->context;
    char path_buffer[CSV_PATH_BUFFER_SIZE];
    const char *ping_filename = "hwpc-sensor-csv-module-ping-file";
    FILE *ping_file = NULL;
    int ret = -1;
    int r;

    /* test path buffer length */
    r = snprintf(path_buffer, CSV_PATH_BUFFER_SIZE, "%s/%s", ctx->config.output_dir, ping_filename);
    if (r < 0 || r > CSV_PATH_BUFFER_SIZE) {
        zsys_error("csv: the test file path exceed the maximum length (%d)", CSV_PATH_BUFFER_SIZE);
        return -1;
    }

    /* test creating a file in output dir */
    errno = 0;
    ping_file = fopen(path_buffer, "w");
    if (!ping_file) {
        zsys_error("csv: failed to create test file: %s", strerror(errno));
        return -1;
    }

    /* test writing into test file  */
    errno = 0;
    if (fputs("test", ping_file) < 0) {
        zsys_error("csv: failed to write into test file: %s", strerror(errno));
        goto end;
    }

    /* remove test file */
    errno = 0;
    if (unlink(path_buffer) == -1) {
        zsys_error("csv: failed to remove test file: %s", strerror(errno));
        goto end;
    }

    ret = 0;

end:
    fclose(ping_file);
    return ret;
}

static int
write_group_header(struct csv_context *ctx, const char *group, FILE *fd, zhashx_t *events)
{
    char buffer[CSV_LINE_BUFFER_SIZE];
    int r;
    zlistx_t *events_name = NULL;
    const char *event_name = NULL;
    int written = 0;

    /* get events name */
    events_name = zhashx_keys(events);
    if (!events_name)
        goto error;

    /* sort events by name */
    zlistx_set_comparator(events_name, (zlistx_comparator_fn *) strcmp);
    zlistx_sort(events_name);

    /* write static elements to buffer */
    r = snprintf(buffer, CSV_LINE_BUFFER_SIZE, "timestamp,sensor,target,socket,cpu");
    if (r < 0 || r > CSV_LINE_BUFFER_SIZE)
        goto error;
    written += r;

    /* append dynamic elements (events) to buffer */
    for (event_name = zlistx_first(events_name); event_name; event_name = zlistx_next(events_name)) {
        r = snprintf(buffer + written, CSV_LINE_BUFFER_SIZE - written, ",%s", event_name);
        if (r < 0 || r > (CSV_LINE_BUFFER_SIZE - written))
            goto error;
        written += r;
    }

    /* write header in file */
    if (fprintf(fd, "%s\n", buffer) < 0)
        goto error;

    /* force writing to the disk */
    fflush(fd);

    /* store events name in the order written in header */
    zhashx_insert(ctx->groups_events, group, events_name);

    return 0;

error:
    zlistx_destroy(&events_name);
    return -1;
}

static int
open_group_outfile(struct csv_context *ctx, const char *group_name)
{
    char path_buffer[CSV_PATH_BUFFER_SIZE];
    int r;
    struct stat outfile_stat = {0};
    FILE *group_fd = NULL;

    /* construct group output file path */
    r = snprintf(path_buffer, CSV_PATH_BUFFER_SIZE, "%s/%s", ctx->config.output_dir, group_name);
    if (r < 0 || r > CSV_PATH_BUFFER_SIZE) {
        zsys_error("csv: failed to build path for group=%s", group_name);
        return -1;
    }

    /* check if output file already exists */
    if (stat(path_buffer, &outfile_stat) != -1) {
        zsys_error("csv: output file for group=%s already exists", group_name);
        return -1;
    }

    /* open/create output file */
    errno = 0;
    group_fd = fopen(path_buffer, "w");
    if (!group_fd) {
        zsys_error("csv: failed to open output file for group=%s: %s", group_name, strerror(errno));
        return -1;
    }

    zhashx_insert(ctx->groups_fd, group_name, group_fd);
    return 0;
}

static int
write_events_value(struct csv_context *ctx, const char *group, FILE *fd, uint64_t timestamp, const char *target, const char *socket, const char *cpu, zhashx_t *events)
{
    zlistx_t *events_name = NULL;
    int r;
    char buffer[CSV_LINE_BUFFER_SIZE];
    int written = 0;
    const char *event_name = NULL;
    const uint64_t *event_value = NULL;

    /* get events name in the order of csv header */
    events_name = zhashx_lookup(ctx->groups_events, group);
    if (!events_name) {
        return -1;
    }

    /* write static elements to buffer */
    r = snprintf(buffer, CSV_LINE_BUFFER_SIZE, "%" PRIu64 ",%s,%s,%s,%s", timestamp, ctx->config.sensor_name, target, socket, cpu);
    if (r < 0 || r > CSV_LINE_BUFFER_SIZE) {
        return -1;
    }
    written += r;
 
    /* write dynamic elements (events) to buffer */
    for (event_name = zlistx_first(events_name); event_name; event_name = zlistx_next(events_name)) {
        /* get event counter value */
        event_value = zhashx_lookup(events, event_name);
        if (!event_value) {
            return -1;
        }

        /* add event counter value to line buffer */
        r = snprintf(buffer + written, CSV_LINE_BUFFER_SIZE - written, ",%" PRIu64, *event_value);
        if (r < 0 || r > (CSV_LINE_BUFFER_SIZE - written)) {
            return -1;
        }
        written += r;
    }

    /* write line buffer in file */
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
            if (open_group_outfile(ctx, group_name)) {
                zsys_error("csv: failed to open outfile for group=%s", group_name);
                return -1;
            }
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
storage_csv_create(const char *sensor_name, const char *output_dir)
{
    struct storage_module *module = NULL;
    struct csv_context *ctx = NULL;

    module = storage_module_create();
    if (!module)
        goto error;

    ctx = csv_context_create(sensor_name, output_dir);
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
    storage_module_destroy(module);
    return NULL;
}

