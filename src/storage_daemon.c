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
#include <signal.h>

#include "report.h"
#include "storage_daemon.h"
#include "perf.h"

#include <pthread.h>

pthread_mutex_t __daemon_mutex__ = PTHREAD_MUTEX_INITIALIZER;

static struct daemon_context*
daemon_context_create(const char *sensor_name, int port)
{
    struct daemon_context *ctx = malloc (sizeof(struct daemon_context));

    if (!ctx)
        return NULL;

    ctx-> config.sensor_name = sensor_name;
    ctx-> config.port = port;

    ctx-> server = -1;
    ctx-> nb_clients = 0;

    return ctx;
}

static void
daemon_context_destroy(struct daemon_context *ctx)
{
    if (!ctx)
        return;

    pthread_mutex_lock (&__daemon_mutex__);    
    
    if (ctx-> server) {
	close (ctx-> server);
    }

    for (int i = 0 ; i < ctx-> nb_clients ; i ++) {
	close (ctx-> clients [i]);
    }

    pthread_cancel (ctx-> tid);
    
//end:
    
    pthread_mutex_unlock (&__daemon_mutex__);        
    free (ctx);
}


void*
daemon_accept_loop (void* data)
{
    zsys_info ("daemon: accept loop start");
    struct daemon_context *ctx = (struct daemon_context *) data;
    while (true) {
	struct sockaddr_in clientAddr;
	socklen_t len = sizeof (struct sockaddr_in);

	int client = accept (ctx-> server, (struct sockaddr *) (&clientAddr), &len);
	if (client < 0) 
	    break ;
	
	pthread_mutex_lock (&__daemon_mutex__);

	if (ctx-> nb_clients < 255) {
	    zsys_info ("daemon: new client");
	    ctx-> clients [ctx-> nb_clients] = client;
	    ctx-> nb_clients = ctx-> nb_clients + 1;
	}
	
	pthread_mutex_unlock (&__daemon_mutex__);	
    }
    
    return NULL;
}

static int
daemon_spawn_accepting_thread (struct daemon_context *ctx)
{
    pthread_mutex_init (&__daemon_mutex__, NULL);
    if (pthread_create (&ctx-> tid, NULL, daemon_accept_loop, ctx) != 0) {
	return -1;
    }
    
    return 0;
}


static int
addr_init(struct daemon_config config, struct sockaddr_in *addr)
{
    addr-> sin_family = AF_INET;
    addr-> sin_port = htons (config.port);
    return inet_pton(AF_INET, "0.0.0.0", &(addr-> sin_addr));
}

static int
daemon_initialize(struct storage_module *module)
{
    struct daemon_context *ctx = module-> context;

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
    
    if (module-> is_initialized)
        return -1;

    if (addr_init(ctx-> config, &(ctx-> address)) == -1) {
	zsys_error("daemon: failed to parse uri: %s", "0.0.0.0");
	goto error;
    }

    ctx-> server = socket(PF_INET, SOCK_STREAM, 0); // PF_INET ??
    if (ctx-> server == -1) {
	zsys_error("daemon: failed to create socket");
	goto error;
    }

    if (bind(ctx-> server, (struct sockaddr *)&(ctx-> address), sizeof (ctx-> address)) == -1) {
	zsys_error("daemon: unable to bind");
	goto error;
    }
    
    if (listen(ctx-> server, 5) != 0) {
	zsys_error("daemon: failed to listen to socket");
	goto error;
    }

    if (daemon_spawn_accepting_thread (ctx) == -1) {
	zsys_error("daemon: failed to spawn thread");
	goto error;
    }

       
    module-> is_initialized = true;
    return 0;

error:
    if (ctx-> server != -1) 
	close(ctx-> server);
    return -1;
}

static int
daemon_ping(struct storage_module *module)
{
    if (module-> context != NULL)
        return 0;
    return -1;
}

void timestamp_to_iso_date(unsigned long int timestamp, char * time_buffer, int max_size);

static void
daemon_close_client(struct daemon_context *ctx, int i)
{ // assume that mutex is already locked by send_document
    close (ctx-> clients [i]);
    zsys_info ("daemon: closing client");
    for (int j = i ; j < ctx-> nb_clients - 1; j ++) {
	ctx-> clients [j] = ctx-> clients [j + 1];
    }

    ctx-> nb_clients = ctx-> nb_clients - 1;
}

static int
daemon_send_document(struct daemon_context *ctx, char *buffer, size_t length)
{
    int code = 0;
    pthread_mutex_lock (&__daemon_mutex__);

    for (int i = 0 ; i < ctx-> nb_clients; i ++) {
	if (write(ctx->clients [i], buffer, length) != (ssize_t) length) {
	    code = -1;
	    daemon_close_client(ctx, i);
	}
    }
    
//end:
    pthread_mutex_unlock (&__daemon_mutex__);

    return code;
}

static int
daemon_store_report(struct storage_module *module, struct payload *payload)
{
    struct daemon_context *ctx = module->context;
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
      zsys_error("daemon: failed convert report to json");
        ret = -1;
    }
    

    if (daemon_send_document (ctx, buffer, length) == -1) {
	zsys_error("daemon: failed insert timestamp=%lu target=%s: %s", payload->timestamp, payload->target_name, error.message);
        ret = -1;
    }
    
    bson_destroy(&document);
    return ret;
}
       

static int
daemon_deinitialize(struct storage_module *module)
{
    struct daemon_context *ctx = module-> context;
    if (!module-> is_initialized)
        return -1;

    pthread_mutex_lock (&__daemon_mutex__);    
    
    if (ctx-> server) {
	close (ctx-> server);
    }

    for (int i = 0 ; i < ctx-> nb_clients ; i ++) {
	close (ctx-> clients [i]);
    }

    pthread_cancel (ctx-> tid);
    
    pthread_mutex_unlock (&__daemon_mutex__);        
    
    return 0;
}

static void
daemon_destroy(struct storage_module *module)
{
    if (!module)
        return ;

    daemon_context_destroy(module-> context);
}


struct storage_module *
storage_daemon_create(struct config *config)
{
    struct storage_module *module = NULL;
    struct daemon_context *ctx = NULL;

    module = malloc(sizeof(struct storage_module));
    if (!module)
        goto error;

    ctx = daemon_context_create(config-> sensor.name, config-> storage.P_flag);
    if (!ctx)
        goto error;

    module-> type = STORAGE_DAEMON;
    module-> context = ctx;
    module-> is_initialized = false;
    module-> initialize = daemon_initialize;
    module-> ping = daemon_ping;
    module-> store_report = daemon_store_report;
    module-> deinitialize = daemon_deinitialize;
    module-> destroy = daemon_destroy;

    return module;

error:

    daemon_context_destroy(ctx);
    if (module)
        free(module);
    
    return NULL;
}
