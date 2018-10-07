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

#include <mongoc.h>

#include "mongodb.h"
#include "report.h"
#include "perf.h"

struct mongodb_config *
mongodb_config_create(char *sensor_name, char *uri, char *database, char *collection)
{
    struct mongodb_config *config = malloc(sizeof(struct mongodb_config));

    if (!config)
        return NULL;

    config->sensor_name = sensor_name;
    config->uri = uri;
    config->database_name = database;
    config->collection_name = collection;

    return config;
}

void
mongodb_config_destroy(struct mongodb_config *config)
{
    if (!config)
        return;

    free(config);
}

static struct mongodb_context *
mongodb_context_create(struct mongodb_config *config)
{
    struct mongodb_context *ctx = malloc(sizeof(struct mongodb_context));

    if (!ctx)
        return NULL;

    ctx->config = config;
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
    struct mongodb_context *ctx = module->context;
    bson_error_t error;

    if (module->is_initialized)
        return -1;

    mongoc_init();

    ctx->uri = mongoc_uri_new_with_error(ctx->config->uri, &error);
    if (!ctx->uri) {
        zsys_error("mongodb: failed to parse uri: %s", error.message);
        goto error;
    }

    ctx->client = mongoc_client_new_from_uri(ctx->uri);
    if (!ctx->client) {
        zsys_error("mongodb: failed to create client");
        goto error;
    }

    ctx->collection = mongoc_client_get_collection(ctx->client, ctx->config->database_name, ctx->config->collection_name);
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
    struct mongodb_context *ctx = module->context;
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
    struct mongodb_context *ctx = module->context;
    bson_t document = BSON_INITIALIZER;
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
     *    "group_name": {
     *       "pkg_id": {
     *          "cpu_id": {
     *              "time_enabled": 12345,
     *              "time_running": 12345,
     *              "event_name": 123456789.0,
     *              more events...
     *          },
     *          more cpus...
     *      },
     *      more pkgs...
     *    },
     *    more groups...
     * }
     */
    BSON_APPEND_DATE_TIME(&document, "timestamp", payload->timestamp);
    BSON_APPEND_UTF8(&document, "sensor", ctx->config->sensor_name);
    BSON_APPEND_UTF8(&document, "target", payload->target_name);

    for (group_data = zhashx_first(payload->groups); group_data; group_data = zhashx_next(payload->groups)) {
        group_name = zhashx_cursor(payload->groups);
        BSON_APPEND_DOCUMENT_BEGIN(&document, group_name, &doc_group);

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

        bson_append_document_end(&document, &doc_group);
    }

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
    struct mongodb_context *ctx = module->context;

    if (!module->is_initialized)
        return -1;

    mongoc_collection_destroy(ctx->collection);
    mongoc_client_destroy(ctx->client);
    mongoc_uri_destroy(ctx->uri);
    mongoc_cleanup();
    module->is_initialized = false;
    return 0;
}

struct storage_module *
mongodb_create(struct mongodb_config *config)
{
    struct storage_module *module = storage_module_create();
    struct mongodb_context *ctx = mongodb_context_create(config);

    if (!module || !ctx) {
        storage_module_destroy(module);
        mongodb_context_destroy(ctx);
        return NULL;
    }

    module->type = STORAGE_MONGODB;
    module->context = ctx;
    module->is_initialized = false;
    module->initialize = mongodb_initialize;
    module->ping = mongodb_ping;
    module->store_report = mongodb_store_report;
    module->deinitialize = mongodb_deinitialize;

    return module;
}

void
mongodb_destroy(struct storage_module *module)
{
    if (!module)
        return;

    mongodb_context_destroy(module->context);
    storage_module_destroy(module);
}

