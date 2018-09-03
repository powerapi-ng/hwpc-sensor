#include <mongoc.h>

#include "mongodb.h"
#include "report.h"
#include "perf.h"

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
mongodb_store_report(struct storage_module *module, struct report_payload *payload)
{
    struct mongodb_context *ctx = module->context;
    bson_t document = BSON_INITIALIZER;
    struct hwinfo_pkg *pkg = NULL;
    unsigned int *pkg_id;
    char uint_buf[32];
    const char *pkg_id_str = NULL;
    unsigned int *cpu_id = NULL;
    const char *cpu_id_str = NULL;
    bson_t dochwc;
    bson_t docpkg;
    bson_t doccpu;
    size_t event_id;
    struct perf_cpu_report *report = NULL;
    bson_error_t error;
    int ret = 0;

    /*
     * construct mongodb document as following:
     * {
     *    "timestamp": 1529868713854,
     *    "sensor": "test.cluster.lan",
     *    "target": "example",
     *    "hwc": {
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
     * }
     */
    BSON_APPEND_DATE_TIME(&document, "timestamp", payload->timestamp);
    BSON_APPEND_UTF8(&document, "sensor", ctx->config->sensor_name);
    BSON_APPEND_UTF8(&document, "target", payload->cgroup_name);
    BSON_APPEND_DOCUMENT_BEGIN(&document, "hwc", &dochwc);
    for (pkg = zhashx_first(ctx->config->hwinfo->pkgs); pkg; pkg = zhashx_next(ctx->config->hwinfo->pkgs)) {
        pkg_id = (unsigned int *) zhashx_cursor(ctx->config->hwinfo->pkgs);
        bson_uint32_to_string(*pkg_id, &pkg_id_str, uint_buf, sizeof(uint_buf));
        BSON_APPEND_DOCUMENT_BEGIN(&dochwc, pkg_id_str, &docpkg);

        for (cpu_id = zlistx_first(pkg->cpus_id); cpu_id; cpu_id = zlistx_next(pkg->cpus_id)) {
            report = zhashx_lookup(payload->reports, cpu_id);
            if (!report) {
                continue;
            }
            if (report->nr != payload->events->num_attrs) {
                zsys_warning("mongodb: invalid event count for timestamp=%lu target=%s cpu=%lu", payload->timestamp, payload->cgroup_name, *cpu_id);
                continue;
            }

            bson_uint32_to_string(*cpu_id, &cpu_id_str, uint_buf, sizeof(uint_buf));
            BSON_APPEND_DOCUMENT_BEGIN(&docpkg, cpu_id_str, &doccpu);

            BSON_APPEND_INT64(&doccpu, "time_enabled", report->time_enabled);
            BSON_APPEND_INT64(&doccpu, "time_running", report->time_running);

            for (event_id = 0; event_id < payload->events->num_attrs; event_id++) {
                BSON_APPEND_DOUBLE(&doccpu, payload->events->attrs[event_id].name, report->values[event_id].value);
            }

            bson_append_document_end(&docpkg, &doccpu);
        }

        bson_append_document_end(&dochwc, &docpkg);
    }
    bson_append_document_end(&document, &dochwc);

    /* insert document into collection */
    if (!mongoc_collection_insert_one(ctx->collection, &document, NULL, NULL, &error)) {
        zsys_error("mongodb: failed insert timestamp=%lu target=%s: %s", payload->timestamp, payload->cgroup_name, error.message);
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

