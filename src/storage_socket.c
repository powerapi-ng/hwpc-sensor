/*
 *  Copyright (c) 2021, INRIA
 *  Copyright (c) 2021, University of Lille
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <bson.h>

#include "report.h"
#include "storage_socket.h"
#include "perf.h"

static struct socket_context *
socket_context_create(const char *sensor_name, const char *address, int port)
{
    struct socket_context *ctx = malloc(sizeof(struct socket_context));

    if (!ctx)
        return NULL;

    ctx->config.sensor_name = sensor_name;
    ctx->config.address = address;
    ctx->config.port = port;
    
    ctx->socket = -1;

    return ctx;
}

static void
socket_context_destroy(struct socket_context *ctx)
{
    if (!ctx)
        return;

    free(ctx);
}

static int
addr_init(struct socket_config config, struct sockaddr_in * addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htons(config.port);
    return inet_pton(AF_INET, config.address, &(addr->sin_addr));
   
}


static int
socket_initialize(struct storage_module *module)
{
    struct socket_context *ctx = module->context;

    if (module->is_initialized)
        return -1;

    if(addr_init(ctx->config, &(ctx->address)) == -1){
        zsys_error("socket: failed to parse uri: %s", ctx->config.address);
        goto error;
    }
    
    ctx->socket = socket(PF_INET, SOCK_STREAM, 0);
    if(ctx->socket == -1) {
        zsys_error("socket: failed create socket");
        goto error;
    }

    if(connect(ctx->socket, (struct sockaddr *)&(ctx->address), sizeof(ctx->address)) == -1){
        zsys_error("socket: unable to connect to %s", ctx->config.address);
        goto error;
    }

    module->is_initialized = true;
    return 0;

error:
    if(ctx->socket != -1)
      close(ctx->socket);
    return -1;
}

static int
socket_ping(struct storage_module *module)
{
    if(module->context != NULL)
      return 0;
    return -1;
}

void timestamp_to_iso_date(unsigned long int timestamp, char * time_buffer, int max_size)
{
    long int date_without_ms;
    struct tm * tm_date;
    int len;

    date_without_ms= timestamp / 1000;
    tm_date = localtime((const long int*)&date_without_ms);
    len = strftime(time_buffer, max_size, "%Y-%m-%dT%H:%M:%S.", tm_date );

    time_buffer[len] = ((timestamp % 1000 - (timestamp % 100)) / 100) + 48;
    time_buffer[len + 1] = ((timestamp % 100 - (timestamp % 10)) / 10) + 48;
    time_buffer[len + 2] = (timestamp % 10) + 48;
    time_buffer[len + 3] = 'Z';
    time_buffer[len + 4] = '\0';
}

static int
socket_store_report(struct storage_module *module, struct payload *payload)
{
    struct socket_context *ctx = module->context;
    bson_t document = BSON_INITIALIZER;
    bson_t doc_groups;
    struct payload_group_data *group_data = NULL;
    const char *group_name = NULL;
    bson_t doc_group;
    struct payload_pkg_data *pkg_data = NULL;
    const char *pkg_id = NULL;
    bson_t doc_pkg;
    struct payload_cpu_data *cpu_data = NULL;
    const char *cpu_id = NULL;
    bson_t doc_cpu;
    const char *event_name = NULL;
    uint64_t *event_value = NULL;
    bson_error_t error;
    int ret = 0;
    char *buffer = NULL;
    size_t length = -1;
    char time_buffer[100];
    /*
     * construct document as following:
     * {
     *    "timestamp": 2020-09-08T15:46:44.856Z,
     *    "sensor": "test.cluster.lan",
     *    "target": "example",
     *    "groups": {
     *      "group_name": {
     *          "pkg_id": {
     *              "cpu_id": {
     *                  "time_enabled": 12345,
     *                  "time_running": 12345,
     *                  "event_name": 123456789.0,
     *                  more events...
     *              },
     *              more cpus...
     *          },
     *          more pkgs...
     *      },
     *      more groups...
     *   }
     * }
     */


    timestamp_to_iso_date( payload->timestamp, time_buffer, 100);
    BSON_APPEND_UTF8(&document, "timestamp", time_buffer);

    BSON_APPEND_UTF8(&document, "sensor", ctx->config.sensor_name);
    BSON_APPEND_UTF8(&document, "target", payload->target_name);

    BSON_APPEND_DOCUMENT_BEGIN(&document, "groups", &doc_groups);
    for (group_data = zhashx_first(payload->groups); group_data; group_data = zhashx_next(payload->groups)) {
        group_name = zhashx_cursor(payload->groups);
        BSON_APPEND_DOCUMENT_BEGIN(&doc_groups, group_name, &doc_group);

        for (pkg_data = zhashx_first(group_data->pkgs); pkg_data; pkg_data = zhashx_next(group_data->pkgs)) {
            pkg_id = zhashx_cursor(group_data->pkgs);
            BSON_APPEND_DOCUMENT_BEGIN(&doc_group, pkg_id, &doc_pkg);

            for (cpu_data = zhashx_first(pkg_data->cpus); cpu_data; cpu_data = zhashx_next(pkg_data->cpus)) {
                cpu_id = zhashx_cursor(pkg_data->cpus);
                BSON_APPEND_DOCUMENT_BEGIN(&doc_pkg, cpu_id, &doc_cpu);

                for (event_value = zhashx_first(cpu_data->events); event_value; event_value = zhashx_next(cpu_data->events)) {
                    event_name = zhashx_cursor(cpu_data->events);
                    BSON_APPEND_DOUBLE(&doc_cpu, event_name, *event_value);
                }

                bson_append_document_end(&doc_pkg, &doc_cpu);
            }

            bson_append_document_end(&doc_group, &doc_pkg);
        }

        bson_append_document_end(&doc_groups, &doc_group);
    }
    bson_append_document_end(&document, &doc_groups);

    buffer = bson_as_json (&document, &length);
    /* buffer[length] = '\r'; */
    /* buffer[length + 1] = '\n'; */
    /* buffer[length + 2] = '\0'; */
    if(buffer == NULL){
      zsys_error("socket: failed convert report to json");
        ret = -1;
    }
    
    
    if (!write(ctx->socket, buffer, length)) {
        zsys_error("socket: failed insert timestamp=%lu target=%s: %s", payload->timestamp, payload->target_name, error.message);
        ret = -1;
    }

    bson_destroy(&document);
    return ret;
}

static int
socket_deinitialize(struct storage_module *module __attribute__ ((unused)))
{
    struct socket_context *ctx = module->context;

    if (!module->is_initialized)
        return -1;

    close(ctx->socket);
    return 0;
}
static void
socket_destroy(struct storage_module *module)
{
    if (!module)
        return;

    socket_context_destroy(module->context);
}

struct storage_module *
storage_socket_create(struct config *config)
{
    struct storage_module *module = NULL;
    struct socket_context *ctx = NULL;

    module = malloc(sizeof(struct storage_module));
    if (!module)
        goto error;

    ctx = socket_context_create(config->sensor.name, config->storage.U_flag, config->storage.P_flag);
    if (!ctx)
        goto error;

    module->type = STORAGE_SOCKET;
    module->context = ctx;
    module->is_initialized = false;
    module->initialize = socket_initialize;
    module->ping = socket_ping;
    module->store_report = socket_store_report;
    module->deinitialize = socket_deinitialize;
    module->destroy = socket_destroy;

    return module;

error:
    socket_context_destroy(ctx);
    free(module);
    return NULL;
}

