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
#include <unistd.h>
#include <stdio.h>

#include "report.h"
#include "storage_socket.h"
#include "perf.h"


static int socket_resolve_and_connect(struct socket_context *ctx);

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
    ctx->cnx_fail = -1;
    ctx->current_delay = -1;
    ctx->last_attempt = -1;

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


/* Connect to the socket at ctx->config, 
performing name resolution is the address is not a valid IPV4 address.
This function make a single connection attempt, retry must be implemented by the caller.
Returns 0 on success.
*/
static int
socket_connection(struct socket_context *ctx)
{
    if(addr_init(ctx->config, &(ctx->address)) == 1){
        // ctx.address is a valid IP address, attempt connecting directly:
        ctx->socket = socket(PF_INET, SOCK_STREAM, 0);
        if(ctx->socket == -1) {
            zsys_error("socket: failed creating socket");
            goto error;
        }    

        if (connect(ctx->socket, (struct sockaddr *)&(ctx->address), sizeof(ctx->address)) == -1) {
            zsys_error("socket: failed connecting to %s ", ctx->address);
            goto error;
        }
        zsys_info("socket: Successfully connected");    

    } else {
        if (socket_resolve_and_connect(ctx) != 0) {
            goto error;
        }
    }

    return 0;

error:
    if(ctx->socket != -1)
      close(ctx->socket);
    return -1;
}

/* Attempt reconnection with exponential backoff. 
Calling this method will not always result in an attempt to connect the socket, 
it will only be done once the current delay (for exponential backoff) is reach.
This it can be safely called without hammering the network.
Check the return value, if 0 then connection is successful and the socket can now be used.
*/
static int
socket_reconnection(struct socket_context *ctx)
{
    time_t t0;

    if (ctx->cnx_fail == -1 ) {
        // first call in the backoff strategy, initialize 
        close(ctx-> socket);
        ctx->cnx_fail = time(NULL);
        ctx->current_delay = 0;
        ctx->last_attempt = -1;
    }

    t0 = time(NULL);
    if (difftime(t0, ctx->cnx_fail)> MAX_DURATION_CONNECTION_RETRY) {
        // MAX tentative reach, stop attempting reconnection
        // FIXME : quit ? 
        zsys_error("Stop re-connection attempts, MAX delay reached");
        ctx->last_attempt = -1;
        goto error;

    } else if ( ctx->last_attempt + ctx->current_delay > t0){
        // zsys_debug("Delay not reached %d %d %d", ctx->current_delay , ctx->last_attempt, t0);
        // delay not reached, to not attempt reconnection
        goto error;

    } else {
        zsys_info("Attempt connection  ");

        if (socket_connection(ctx) != 0) {
            // connection failed, update delay for exponential backoff
            ctx->last_attempt = t0;
            if (ctx->current_delay == 0) {
                ctx->current_delay = 1;
            } else {
                // add some random jitter
                // rand is weak but still clearly enough for our need here
                ctx->current_delay = ctx->current_delay * 2 + (rand() % ctx->current_delay/2) ; // NOLINT(cert-msc30-c, cert-msc50-cpp)

            }
            zsys_warning("socket: reconnection failed, next attempt in %d seconds", ctx->current_delay);
            close(ctx-> socket);
            goto error;
        }
    }

    zsys_info("socket: reconnected ");
    // reset backoff counters
    ctx->cnx_fail = -1;
    ctx->current_delay = -1;
    ctx->last_attempt = -1;
    return 0;

error:
    return -1;
}


/* Attemp to resolve network name from config.address and connect. */
static int
socket_resolve_and_connect(struct socket_context *ctx)
{
    struct addrinfo *result, *rp;
    struct addrinfo hints;
    int s;
    char portstr[8];
    bool is_connected = false;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;    /*  IPv4 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    bson_snprintf (portstr, sizeof portstr, "%hu", ctx->config.port);

    s = getaddrinfo(ctx->config.address, portstr, &hints, &result);
    if (s!= 0) {
        zsys_error("unable to get addr info from %s", ctx->config.address);
        goto error;
    }
    
    zsys_debug("Name %s resolved, attempt connecting", ctx->config.address);
    ctx->socket = socket(PF_INET, SOCK_STREAM, 0);
    if(ctx->socket == -1) {
        zsys_error("socket: failed create socket");
        goto error;
    }
    // attemps to connect to the first possible result from getaddrinfo
    for (rp = result; rp; rp = rp->ai_next) {
        memcpy(&ctx->address, rp->ai_addr, rp->ai_addrlen);
        if(socket_connection(ctx) == -1){
            zsys_warning("socket: unable to connect to %s", ctx->config.address);
        } else {
            zsys_info("Connected !!");
            is_connected = true;
            break;
        }
    }
    freeaddrinfo (result);
    if (! is_connected) {
         zsys_error("Could not connect to any resolved address for  %s", ctx->config.address);
        goto error;
    }

    return 0;

error:
    return -1;
}


/* Initialize storage module by connecting the output socket.
This function blocks and retry connecting every second until the connection is successful 
or MAX_DURATION_CONNECTION_RETRY is reached.
Returns 0 on success.
*/
static int
socket_initialize(struct storage_module *module)
{
    time_t t0;
    struct socket_context *ctx = module->context;

    if (module->is_initialized)
        return -1;

    t0 = time(NULL);
    while(difftime(time(NULL), t0) < MAX_DURATION_CONNECTION_RETRY  && !zsys_interrupted){

        if (socket_connection(ctx) == 0) {
            module->is_initialized = true;
            return 0;
        }
        sleep(1);
    }

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

void timestamp_to_iso_date(long int timestamp, char * time_buffer, int max_size)
{
    long int date_without_ms;
    struct tm * tm_date;
    int len;

    date_without_ms = timestamp / 1000;
    tm_date = localtime((const long int*)&date_without_ms);
    len = strftime(time_buffer, max_size, "%Y-%m-%dT%H:%M:%S.", tm_date );

    time_buffer[len] = (char)(((timestamp % 1000 - (timestamp % 100)) / 100) + 48);
    time_buffer[len + 1] = (char)(((timestamp % 100 - (timestamp % 10)) / 10) + 48);
    time_buffer[len + 2] = (char)((timestamp % 10) + 48);
    time_buffer[len + 3] = '\0';
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
    // bson_error_t error;
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
    
    zsys_debug("writing to socket");
    int r = write(ctx->socket, buffer, length);
    zsys_debug("written %d", r);
    if (r == -1) {

        zsys_info("socket write failure, close ");
        close(ctx-> socket);
        ctx->socket = socket(PF_INET, SOCK_STREAM, 0);
        if(ctx->socket == -1) {
            zsys_error("socket: failed create socket");
        } else {

            zsys_info("socket write failure, attempt reconnecting ");

            // FIXME : we probably wait way too long here if connection does not work !
            //  * Implement exponential back-off
            //  * do not bock the actor while attempting to reconnect
            socket_connection(ctx); // FIXME : check return code
            zsys_info("socket reconnected ");
        }
    
        zsys_error("socket: error %d - failed insert timestamp=%lu target=%s ", errno, payload->timestamp, payload->target_name);
        ret = -1;
    }
    zsys_debug("written");

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

