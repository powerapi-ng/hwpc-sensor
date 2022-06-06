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

#include <stdio.h>
#include <unistd.h>
#include <sys/random.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <bson.h>

#include "perf.h"
#include "report.h"
#include "storage_socket.h"

static struct socket_context *
socket_context_create(const char *sensor_name, const char *address, const int port)
{
    struct socket_context *ctx = malloc(sizeof(struct socket_context));

    if (!ctx)
        return NULL;

    ctx->config.sensor_name = sensor_name;
    ctx->config.address = address;
    ctx->config.port = port;
    
    ctx->socket_fd = -1;
    ctx->last_retry_time = 0;
    ctx->retry_backoff_time = 1;

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
socket_resolve_and_connect(struct socket_context *ctx)
{
    struct addrinfo hints = {0};
    struct addrinfo *result = NULL, *rp = NULL;
    char port_str[PORT_STR_BUFFER_SIZE] = {0};
    int sfd = -1;
    int ret = -1;

    /* setup hints for address resolution */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    /* convert port number to string */
    snprintf(port_str, PORT_STR_BUFFER_SIZE, "%d", ctx->config.port);

    if (getaddrinfo(ctx->config.address, port_str, &hints, &result)) {
        zsys_error("socket: unable to resolve address: %s", ctx->config.address);
        goto error_no_getaddrinfo;
    }

    /* attemps to connect to any of the resolved address(es) */
    for (rp = result; rp; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        ret = connect(sfd, rp->ai_addr, rp->ai_addrlen);
        if (!ret) {
            zsys_info("socket: successfully connected to %s:%d", ctx->config.address, ctx->config.port);
            break;
        }

        close(sfd);
    }

    /* no connection have been established */
    if (ret == -1) {
        zsys_error("socket: failed to connect to %s:%d", ctx->config.address, ctx->config.port);
        goto error_not_connected;
    }

    ctx->socket_fd = sfd;

error_not_connected:
    freeaddrinfo(result);
error_no_getaddrinfo:
    return ret;
}

static int
socket_initialize(struct storage_module *module)
{
    struct socket_context *ctx = module->context;

    if (module->is_initialized)
        return -1;

    if (socket_resolve_and_connect(ctx))
        return -1;

    module->is_initialized = 1;
    return 0;
}

static int
socket_ping(struct storage_module *module __attribute__ ((unused)))
{
    /* ping is not supported by this module */
    return 0;
}

static int
socket_try_reconnect(struct socket_context *ctx)
{
    time_t current_time = time(NULL);
    ssize_t nbrand;
    uint8_t rand_jitter;

    /* close the current socket */
    if (ctx->socket_fd != -1) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }

    /* retry socket connection with an exponential backoff */
    if (difftime(current_time, ctx->last_retry_time) >= (double) ctx->retry_backoff_time) {
        if (socket_resolve_and_connect(ctx)) {
            ctx->last_retry_time = current_time;
            if (ctx->retry_backoff_time < MAX_DURATION_CONNECTION_RETRY) {
                nbrand = getrandom(&rand_jitter, sizeof(uint8_t), 0);
                ctx->retry_backoff_time = ctx->retry_backoff_time * 2 + (nbrand != -1 ? rand_jitter % 10 : 0);
            }

            zsys_error("socket: failed to reconnect, next try will be in %d seconds", ctx->retry_backoff_time);
            return -1;
        } else {
            ctx->last_retry_time = 0;
            ctx->retry_backoff_time = 1;

            zsys_info("socket: connection recovered, resuming operation");
            return 0;
        }
    }

    return -1;
}

static int
socket_store_report(struct storage_module *module, struct payload *payload)
{
    struct socket_context *ctx = module->context;
    bson_t document = BSON_INITIALIZER;
    char timestamp_str[TIMESTAMP_STR_BUFFER_SIZE] = {0};
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
    char *json_report = NULL;
    size_t json_report_length = 0;
    ssize_t nbsend;
    int retry_once = 1;
    int ret = -1;

    /* try to reconnect the socket before building the document */
    if (ctx->socket_fd == -1) {
        if (socket_try_reconnect(ctx))
            return -1;
    }

    /*
     * {
     *    "timestamp": "1529868713854",
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
    snprintf(timestamp_str, TIMESTAMP_STR_BUFFER_SIZE, "%" PRIu64, payload->timestamp);
    BSON_APPEND_UTF8(&document, "timestamp", timestamp_str);

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

    json_report = bson_as_json(&document, &json_report_length);
    if (json_report == NULL) {
        zsys_error("socket: failed to convert report to json string");
        goto error_bson_to_json;
    }

    do {
        /*
         * Try to send the serialized report to the endpoint.
         * If the connection have been lost, try to reconnect and send the report again.
         * The exponential backoff on socket reconnect prevents consecutive attempts.
         */
        errno = 0;
        nbsend = send(ctx->socket_fd, json_report, json_report_length, MSG_NOSIGNAL);
        if (nbsend == -1) {
            zsys_error("socket: sending the report failed with error: %s", strerror(errno));

            if (retry_once) {
                zsys_info("socket: connection has been lost, attempting to reconnect...");
                if (!socket_try_reconnect(ctx))
                    continue;
            }

            goto error_socket_disconnected;
        }
    } while (nbsend == -1 && retry_once--);

    ret = 0;

error_socket_disconnected:
    bson_free(json_report);
error_bson_to_json:
    bson_destroy(&document);
    return ret;
}

static int
socket_deinitialize(struct storage_module *module)
{
    struct socket_context *ctx = module->context;

    if (!module->is_initialized)
        return -1;

    if (ctx->socket_fd != -1)
        close(ctx->socket_fd);

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

