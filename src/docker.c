#include <bson.h>
#include <string.h>

#include "docker.h"

char *
docker_get_container_name_from_id(const char *full_id)
{
    char config_path[DOCKER_CONFIG_PATH_BUFFER_SIZE];
    int r;
    bson_json_reader_t *reader = NULL;
    bson_t doc = BSON_INITIALIZER;
    bson_error_t error;
    bson_iter_t iter_doc;
    bson_iter_t iter_name;
    char *container_name = NULL;

    r = snprintf(config_path, DOCKER_CONFIG_PATH_BUFFER_SIZE, "/var/lib/docker/containers/%s/config.v2.json", full_id);
    if (r < 0 || r > DOCKER_CONFIG_PATH_BUFFER_SIZE)
        goto end;

    reader = bson_json_reader_new_from_file(config_path, &error);
    if (!reader)
        goto end;

    r = bson_json_reader_read(reader, &doc, &error);
    if (r < 0)
        goto end;

    if (!bson_iter_init(&iter_doc, &doc) || !bson_iter_find_descendant(&iter_doc, "Name", &iter_name) || !BSON_ITER_HOLDS_UTF8(&iter_name))
        goto end;

    /* copy name without the leading slash */
    container_name = strdup(bson_iter_utf8(&iter_name, 0) + 1); 

end:
    bson_json_reader_destroy(reader);
    bson_destroy(&doc);
    return container_name;
}

