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

#include <mongoc.h>

#include "report.h"
#include "storage_mongodb.h"
#include "perf.h"

static struct mongodb_context *
mongodb_context_create(const char *sensor_name, const char *uri, const char *database, const char *collection)
{
    struct mongodb_context *ctx = (struct mongodb_context *) malloc(sizeof(struct mongodb_context));

    if (!ctx)
        return NULL;

    ctx->config.sensor_name = sensor_name;
    ctx->config.uri = uri;
    ctx->config.database_name = database;
    ctx->config.collection_name = collection;

    ctx->client = NULL;
    ctx->collection = NULL;

    return ctx;
}

static void
mongodb_context_destroy(struct mongodb_context *ctx)
{
    if (!ctx)
        return;

    free(ctx);
}

static int
mongodb_initialize(struct storage_module *module)
{
    struct mongodb_context *ctx = (struct mongodb_context *) module->context;
    bson_error_t error;

    if (module->is_initialized)
        return -1;

    mongoc_init();

    ctx->uri = mongoc_uri_new_with_error(ctx->config.uri, &error);
    if (!ctx->uri) {
        zsys_error("mongodb: failed to parse uri: %s", error.message);
        goto error;
    }

    ctx->client = mongoc_client_new_from_uri(ctx->uri);
    if (!ctx->client) {
        zsys_error("mongodb: failed to create client");
        goto error;
    }

    ctx->collection = mongoc_client_get_collection(ctx->client, ctx->config.database_name, ctx->config.collection_name);
    /* collection is automatically created if non-existent */

    module->is_initialized = true;
    return 0;

error:
    mongoc_uri_destroy(ctx->uri);
    mongoc_client_destroy(ctx->client);
    return -1;
}

static int
mongodb_ping(struct storage_module *module)
{
    struct mongodb_context *ctx = (struct mongodb_context *) module->context;
    int ret = 0;
    bson_t *ping_cmd = BCON_NEW("ping", BCON_INT32(1));
    bson_error_t error;

    if (!mongoc_client_command_simple(ctx->client, "admin", ping_cmd, NULL, NULL, &error)) {
        zsys_error("mongodb: failed to ping mongodb server: %s", error.message);
        ret = -1;
    }

    bson_destroy(ping_cmd);
    return ret;
}

static int
mongodb_store_report(struct storage_module *module, struct payload *payload)
{
    struct mongodb_context *ctx = (struct mongodb_context *) module->context;
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

    /*
     * construct mongodb document as following:
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
    BSON_APPEND_DATE_TIME(&document, "timestamp", payload->timestamp);
    BSON_APPEND_UTF8(&document, "sensor", ctx->config.sensor_name);
    BSON_APPEND_UTF8(&document, "target", payload->target_name);

    BSON_APPEND_DOCUMENT_BEGIN(&document, "groups", &doc_groups);
    for (group_data = (struct payload_group_data *) zhashx_first(payload->groups); group_data; group_data = (struct payload_group_data *) zhashx_next(payload->groups)) {
        group_name = (const char *) zhashx_cursor(payload->groups);
        BSON_APPEND_DOCUMENT_BEGIN(&doc_groups, group_name, &doc_group);

        for (pkg_data = (struct payload_pkg_data *) zhashx_first(group_data->pkgs); pkg_data; pkg_data = (struct payload_pkg_data *) zhashx_next(group_data->pkgs)) {
            pkg_id = (const char * ) zhashx_cursor(group_data->pkgs);
            BSON_APPEND_DOCUMENT_BEGIN(&doc_group, pkg_id, &doc_pkg);

            for (cpu_data = (struct payload_cpu_data *) zhashx_first(pkg_data->cpus); cpu_data; cpu_data = (struct payload_cpu_data *) zhashx_next(pkg_data->cpus)) {
                cpu_id = (const char *) zhashx_cursor(pkg_data->cpus);
                BSON_APPEND_DOCUMENT_BEGIN(&doc_pkg, cpu_id, &doc_cpu);

                for (event_value = (uint64_t *) zhashx_first(cpu_data->events); event_value; event_value = (uint64_t *) zhashx_next(cpu_data->events)) {
                    event_name = (const char *) zhashx_cursor(cpu_data->events);
                    BSON_APPEND_DOUBLE(&doc_cpu, event_name, *event_value);
                }

                bson_append_document_end(&doc_pkg, &doc_cpu);
            }

            bson_append_document_end(&doc_group, &doc_pkg);
        }

        bson_append_document_end(&doc_groups, &doc_group);
    }
    bson_append_document_end(&document, &doc_groups);

    /* insert document into collection */
    if (!mongoc_collection_insert_one(ctx->collection, &document, NULL, NULL, &error)) {
        zsys_error("mongodb: failed insert timestamp=%lu target=%s: %s", payload->timestamp, payload->target_name, error.message);
        ret = -1;
    }

    bson_destroy(&document);
    return ret;
}

static int
mongodb_deinitialize(struct storage_module *module __attribute__ ((unused)))
{
    struct mongodb_context *ctx = (struct mongodb_context *) module->context;

    if (!module->is_initialized)
        return -1;

    mongoc_collection_destroy(ctx->collection);
    mongoc_client_destroy(ctx->client);
    mongoc_uri_destroy(ctx->uri);
    mongoc_cleanup();
    module->is_initialized = false;
    return 0;
}

static void
mongodb_destroy(struct storage_module *module)
{
    if (!module)
        return;

    mongodb_context_destroy((struct mongodb_context *) module->context);
}

struct storage_module *
storage_mongodb_create(struct config *config)
{
    struct storage_module *module = NULL;
    struct mongodb_context *ctx = NULL;

    module = (struct storage_module *) malloc(sizeof(struct storage_module));
    if (!module)
        goto error;

    ctx = mongodb_context_create(config->sensor.name, config->storage.mongodb.uri, config->storage.mongodb.database, config->storage.mongodb.collection);
    if (!ctx)
        goto error;

    module->type = STORAGE_MONGODB;
    module->context = ctx;
    module->is_initialized = false;
    module->initialize = mongodb_initialize;
    module->ping = mongodb_ping;
    module->store_report = mongodb_store_report;
    module->deinitialize = mongodb_deinitialize;
    module->destroy = mongodb_destroy;

    return module;

error:
    mongodb_context_destroy(ctx);
    free(module);
    return NULL;
}

