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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "csv.h"
#include "storage.h"

struct csv_config *
csv_config_create(char *sensor_name, char *output_dir)
{
    struct csv_config *config = malloc(sizeof(struct csv_config));

    if (!config)
        return NULL;

    config->sensor_name = sensor_name;
    config->output_dir = output_dir;

    return config;
}

void
csv_config_destroy(struct csv_config *config)
{
    if (!config)
        return;

    free(config);
}

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
csv_context_create(struct csv_config *config)
{
    struct csv_context *ctx = malloc(sizeof(struct csv_context));

    if (!ctx)
        return NULL;

    ctx->config = config;

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
    if (mkdir(ctx->config->output_dir, 0755) == -1) {
        /* ignore if directory already exists */
        if (errno != EEXIST) {
            zsys_error("csv: failed to create output directory: %s", strerror(errno));
            return -1;
        }
    }

    /* check if directory exists, above EEXIST check DO NOT guarantee that path is a directory */
    errno = 0;
    if (stat(ctx->config->output_dir, &outdir_stat) == -1) {
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
    r = snprintf(path_buffer, CSV_PATH_BUFFER_SIZE, "%s/%s", ctx->config->output_dir, ping_filename);
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
write_group_header(FILE *fd, zlistx_t *events_name)
{
    char buffer[CSV_LINE_BUFFER_SIZE];
    int r;
    const char *event_name = NULL;
    int written = 0;

    /* static elements */
    r = snprintf(buffer, CSV_LINE_BUFFER_SIZE, "timestamp,sensor,target,socket,cpu");
    if (r < 0 || r > CSV_LINE_BUFFER_SIZE)
        return -1;

    written += r;

    /* dynamic elements */
    for (event_name = zlistx_first(events_name); event_name; event_name = zlistx_next(events_name)) {
        r = snprintf(buffer + written, CSV_LINE_BUFFER_SIZE - written, ",%s", event_name);
        if (r < 0 || r > (CSV_LINE_BUFFER_SIZE - written))
            return -1;

        written += r;
    }

    /* write header in file */
    if (fprintf(fd, "%s\n", buffer) < 0)
        return -1;

    /* force writing of the header in file */
    fflush(fd);

    return 0;
}

static FILE *
open_group_outfile(struct csv_context *ctx, const char *group_name, zlistx_t *events_name)
{
    char path_buffer[CSV_PATH_BUFFER_SIZE];
    int r;
    struct stat outfile_stat = {0};
    bool outfile_already_exists = false;
    FILE *group_fd = NULL;

    /* construct group output file path */
    r = snprintf(path_buffer, CSV_PATH_BUFFER_SIZE, "%s/%s", ctx->config->output_dir, group_name);
    if (r < 0 || r > CSV_PATH_BUFFER_SIZE) {
        zsys_error("csv: failed to build path for group=%s", group_name);
        return NULL;
    }

    /* check if output file already exists */
    if (stat(path_buffer, &outfile_stat) != -1) {
        if (!S_ISREG(outfile_stat.st_mode)) {
            zsys_error("csv: output file for group=%s already exists but is not a regular file", group_name);
            return NULL;
        }
        outfile_already_exists = true;
    }

    /* open/create output file */
    errno = 0;
    group_fd = fopen(path_buffer, "a");
    if (!group_fd) {
        zsys_error("csv: failed to open output file for group=%s: %s", group_name, strerror(errno));
        return NULL;
    }

    /* write csv header when needed */
    if (!outfile_already_exists) {
        if(write_group_header(group_fd, events_name)) {
            zsys_error("csv: failed to write csv header for group=%s", group_name);
            fclose(group_fd);
            return NULL;
        }

        /* store events name in the order written in header of the file */
        zhashx_insert(ctx->groups_events, group_name, events_name);
    }

    return group_fd;
}

static int
write_report_values(struct csv_context *ctx, uint64_t timestamp, const char *group, const char *target, const char *socket, const char *cpu, zhashx_t *events_value)
{
    FILE *fd = NULL;
    zlistx_t *events_name = NULL;
    int r;
    char buffer[CSV_LINE_BUFFER_SIZE];
    int written = 0;
    const char *event_name = NULL;
    const uint64_t *event_value = NULL;

    /* get group output file fd */
    fd = zhashx_lookup(ctx->groups_fd, group);
    if (!fd) {
        events_name = zhashx_keys(events_value);
        fd = open_group_outfile(ctx, group, events_name);
        if (!fd) {
            zlistx_destroy(&events_name);
            return -1;
        }

        /* store fd of group outfile */
        zhashx_insert(ctx->groups_fd, group, fd);
    }

    /* get events name in the order of csv header */
    events_name = zhashx_lookup(ctx->groups_events, group);
    if (!events_name) {
        return -1;
    }

    /* static elements */
    r = snprintf(buffer, CSV_LINE_BUFFER_SIZE, "%" PRIu64 ",%s,%s,%s,%s", timestamp, ctx->config->sensor_name, target, socket, cpu);
    if (r < 0 || r > CSV_LINE_BUFFER_SIZE) {
        return -1;
    }
    written += r;
 
    /* dynamic elements */
    for (event_name = zlistx_first(events_name); event_name; event_name = zlistx_next(events_name)) {
        /* get event counter value */
        event_value = zhashx_lookup(events_value, event_name);
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

        for (pkg_data = zhashx_first(group_data->pkgs); pkg_data; pkg_data = zhashx_next(group_data->pkgs)) {
            pkg_id = zhashx_cursor(group_data->pkgs);

            for (cpu_data = zhashx_first(pkg_data->cpus); cpu_data; cpu_data = zhashx_next(pkg_data->cpus)) {
                cpu_id = zhashx_cursor(pkg_data->cpus);

                if (write_report_values(ctx, payload->timestamp, group_name, payload->target_name, pkg_id, cpu_id, cpu_data->events)) {
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

struct storage_module *
csv_create(struct csv_config *config)
{
    struct storage_module *module = storage_module_create();
    struct csv_context *ctx = csv_context_create(config);

    if (!module || !ctx) {
        storage_module_destroy(module);
        csv_context_destroy(ctx);
        return NULL;
    }

    module->type = STORAGE_CSV;
    module->context = ctx;
    module->is_initialized = false;
    module->initialize = csv_initialize;
    module->ping = csv_ping;
    module->store_report = csv_store_report;
    module->deinitialize = csv_deinitialize;

    return module;
}

void
csv_destroy(struct storage_module *module)
{
    if (!module)
        return;

    csv_context_destroy(module->context);
    storage_module_destroy(module);
}

