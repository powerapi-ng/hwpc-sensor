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
#include <sys/uio.h>
#include <netdb.h>
#include <json.h>

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
        zsys_error("socket: Unable to resolve address: %s", ctx->config.address);
        goto error_no_getaddrinfo;
    }

    /* attemps to connect to any of the resolved address(es) */
    for (rp = result; rp; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        ret = connect(sfd, rp->ai_addr, rp->ai_addrlen);
        if (!ret) {
            zsys_info("socket: Successfully connected to %s:%d", ctx->config.address, ctx->config.port);
            break;
        }

        close(sfd);
    }

    /* no connection have been established */
    if (ret == -1) {
        zsys_error("socket: Failed to connect to %s:%d", ctx->config.address, ctx->config.port);
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

            zsys_error("socket: Failed to reconnect, next try will be in %d seconds", ctx->retry_backoff_time);
            return -1;
        } else {
            ctx->last_retry_time = 0;
            ctx->retry_backoff_time = 1;

            zsys_info("socket: Connection recovered, resuming operation");
            return 0;
        }
    }

    return -1;
}

static int
socket_store_report(struct storage_module *module, struct payload *payload)
{
    struct socket_context *ctx = module->context;
    struct json_object *jobj = NULL;
    struct json_object *jobj_groups = NULL;
    struct payload_group_data *group_data = NULL;
    const char *group_name = NULL;
    struct json_object *jobj_group = NULL;
    struct payload_pkg_data *pkg_data = NULL;
    const char *pkg_id = NULL;
    struct json_object *jobj_pkg = NULL;
    struct payload_cpu_data *cpu_data = NULL;
    const char *cpu_id = NULL;
    struct json_object *jobj_cpu = NULL;
    const char *event_name = NULL;
    uint64_t *event_value = NULL;
    const char *json_report = NULL;
    size_t json_report_length = 0;
    struct iovec socket_iov[2] = {0};
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
     *    "timestamp": 1529868713854,
     *    "sensor": "test.cluster.lan",
     *    "target": "example",
     *    "groups": {
     *      "group_name": {
     *          "pkg_id": {
     *              "cpu_id": {
     *                  "time_enabled": 12345,
     *                  "time_running": 12345,
     *                  "event_name": 1234567890,
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
    jobj = json_object_new_object();

    json_object_object_add(jobj, "timestamp", json_object_new_uint64(payload->timestamp));
    json_object_object_add(jobj, "sensor", json_object_new_string(ctx->config.sensor_name));
    json_object_object_add(jobj, "target", json_object_new_string(payload->target_name));

    jobj_groups = json_object_new_object();
    json_object_object_add(jobj, "groups", jobj_groups);
    for (group_data = zhashx_first(payload->groups); group_data; group_data = zhashx_next(payload->groups)) {

        jobj_group = json_object_new_object();
        group_name = zhashx_cursor(payload->groups);
        json_object_object_add(jobj_groups, group_name, jobj_group);
        for (pkg_data = zhashx_first(group_data->pkgs); pkg_data; pkg_data = zhashx_next(group_data->pkgs)) {

            jobj_pkg = json_object_new_object();
            pkg_id = zhashx_cursor(group_data->pkgs);
            json_object_object_add(jobj_group, pkg_id, jobj_pkg);
            for (cpu_data = zhashx_first(pkg_data->cpus); cpu_data; cpu_data = zhashx_next(pkg_data->cpus)) {
                jobj_cpu = json_object_new_object();
                cpu_id = zhashx_cursor(pkg_data->cpus);
                json_object_object_add(jobj_pkg, cpu_id, jobj_cpu);

                for (event_value = zhashx_first(cpu_data->events); event_value; event_value = zhashx_next(cpu_data->events)) {
                    event_name = zhashx_cursor(cpu_data->events);
                    json_object_object_add(jobj_cpu, event_name, json_object_new_uint64(*event_value));
                }
            }
        }
    }

    json_report = json_object_to_json_string_length(jobj, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOSLASHESCAPE, &json_report_length);
    if (json_report == NULL) {
        zsys_error("socket: Failed to convert report to json string");
        goto error_json_to_string;
    }

    /*
     * PowerAPI socketdb requires a newline character at the end of the json document.
     * Using POSIX IOV allows to efficiently append it at the end of the json string.
     */
    socket_iov[0].iov_base = (void *) json_report;
    socket_iov[0].iov_len = json_report_length;
    socket_iov[1].iov_base = "\n";
    socket_iov[1].iov_len = 1;

    do {
        /*
         * Try to send the serialized report to the endpoint.
         * If the connection have been lost, try to reconnect and send the report again.
         * The exponential backoff on socket reconnect prevents consecutive attempts.
         */
        errno = 0;
        nbsend = writev(ctx->socket_fd, socket_iov, 2);
        if (nbsend == -1) {
            zsys_error("socket: Sending the report failed with error: %s", strerror(errno));

            if (retry_once) {
                zsys_info("socket: Connection has been lost, attempting to reconnect...");
                if (!socket_try_reconnect(ctx))
                    continue;
            }

            goto error_socket_disconnected;
        }
    } while (nbsend == -1 && retry_once--);

    ret = 0;

error_json_to_string:
error_socket_disconnected:
    json_object_put(jobj);
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

    ctx = socket_context_create(config->sensor.name, config->storage.socket.hostname, config->storage.socket.port);
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
