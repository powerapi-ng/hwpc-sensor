/*
 *  Copyright (c) 2024, Inria
 *  Copyright (c) 2024, University of Lille
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
#include <limits.h>
#include <json.h>
#include <netdb.h>

#include "config_json.h"
#include "util.h"

/*
 * JSON_FILE_BUFFER_SIZE is the size of the buffer where the content of the json config file will be stored.
 */
#define JSON_FILE_BUFFER_SIZE 4096


static int
setup_verbose(struct config *config, json_object *verbose_obj)
{
    int verbose;

    errno = 0;
    verbose = json_object_get_int(verbose_obj);
    if (errno != 0 || verbose < 0) {
        zsys_error("config: json: Verbose value is invalid (boolean or positive integer expected)");
        return -1;
    }

    config->sensor.verbose = (unsigned int) verbose;
    return 0;
}

static int
setup_cgroup_basepath(struct config *config, json_object *cgroup_basepath_obj)
{
    const char *cgroup_basepath = NULL;

    cgroup_basepath = json_object_get_string(cgroup_basepath_obj);
    if (snprintf(config->sensor.cgroup_basepath, PATH_MAX, "%s", cgroup_basepath) >= PATH_MAX) {
        zsys_error("config: json: Cgroup basepath is too long");
        return -1;
    }

    return 0;
}

static int
setup_sensor_name(struct config *config, json_object *sensor_name_obj)
{
    const char *sensor_name = NULL;

    sensor_name = json_object_get_string(sensor_name_obj);
    if (snprintf(config->sensor.name, HOST_NAME_MAX, "%s", sensor_name) >= HOST_NAME_MAX) {
        zsys_error("config: json: Sensor name is too long");
        return -1;
    }

    return 0;
}

static int
setup_frequency(struct config *config, json_object *frequency_obj)
{
    int frequency;

    errno = 0;
    frequency = json_object_get_int(frequency_obj);
    if (errno != 0 || frequency < 0) {
        zsys_error("config: json: Frequency value is invalid (positive integer expected)");
        return -1;
    }

    config->sensor.frequency = (unsigned int) frequency;
    return 0;
}

static int
setup_storage_type(struct config *config, json_object *storage)
{
    json_object *storage_type_obj = NULL;
    const char *storage_module_name = NULL;
    enum storage_type type;

    if (!json_object_object_get_ex(storage, "type", &storage_type_obj)) {
        zsys_error("config: json: The storage module 'type' field is required");
        return -1;
    }

    storage_module_name = json_object_get_string(storage_type_obj);
    type = storage_module_get_type(storage_module_name);
    if (type == STORAGE_UNKNOWN) {
        zsys_error("config: json: Storage module '%s' is invalid or disabled at compile time", storage_module_name);
        return -1;
    }

    config->storage.type = type;
    return 0;
}

static int
setup_storage_null_parameters(struct config *config __attribute__((unused)), json_object *storage_obj)
{
    json_object_object_foreach(storage_obj, key, value) {
        if (!strcasecmp(key, "type")) {
            continue;
        }
        else {
            zsys_error("config: json: Invalid parameter '%s' for Null storage module", key);
            return -1;
        }
    }

    return 0;
}

static int
setup_storage_csv_parameters(struct config *config, json_object *storage_obj)
{
    const char *output_dir = NULL;

    json_object_object_foreach(storage_obj, key, value) {
        if (!strcasecmp(key, "type")) {
            continue; /* This field have already been processed */
        }
        if (!strcasecmp(key, "directory") || !strcasecmp(key, "outdir")) {
            output_dir = json_object_get_string(value);
            if (snprintf(config->storage.csv.outdir, PATH_MAX, "%s", output_dir) >= PATH_MAX) {
                zsys_error("config: json: CSV output directory path is too long");
                return -1;
            }
        }
        else {
            zsys_error("config: json: Invalid parameter '%s' for CSV storage module", key);
            return -1;
        }
    }

    return 0;
}

static int
setup_storage_socket_parameters(struct config *config, json_object *storage_obj)
{
    const char *host = NULL;
    const char *port = NULL;

    json_object_object_foreach(storage_obj, key, value) {
        if (!strcasecmp(key, "type")) {
            continue; /* This field have already been processed */
        }
        else if (!strcasecmp(key, "uri") || !strcasecmp(key, "host")) {
            host = json_object_get_string(value);
            if (snprintf(config->storage.socket.hostname, HOST_NAME_MAX, "%s", host) >= HOST_NAME_MAX) {
                zsys_error("config: json: Socket output host is too long");
                return -1;
            }
        }
        else if (!strcasecmp(key, "port")) {
            port = json_object_get_string(value);
            if (snprintf(config->storage.socket.port, NI_MAXSERV, "%s", port) >= NI_MAXSERV) {
                zsys_error("config: json: Socket output port is too long");
                return -1;
            }
        }
        else {
            zsys_error("config: json: Invalid parameter '%s' for Socket storage module", key);
            return -1;
        }
    }

    return 0;
}

#ifdef HAVE_MONGODB
static int
setup_storage_mongodb_parameters(struct config *config, json_object *storage_obj)
{
    const char *uri = NULL;
    const char *database = NULL;
    const char *collection = NULL;

    json_object_object_foreach(storage_obj, key, value) {
        if (!strcasecmp(key, "type")) {
            continue; /* This field have already been processed */
        }
        else if (!strcasecmp(key, "uri")) {
            uri = json_object_get_string(value);
            if (snprintf(config->storage.mongodb.uri, PATH_MAX, "%s", uri) >= PATH_MAX) {
                zsys_error("config: json: MongoDB URI is too long");
                return -1;
            }
        }
        else if (!strcasecmp(key, "database")) {
            database = json_object_get_string(value);
            if (snprintf(config->storage.mongodb.database, NAME_MAX, "%s", database) >= NAME_MAX) {
                zsys_error("config: json: MongoDB database name is too long");
                return -1;
            }
        }
        else if (!strcasecmp(key, "collection")) {
            collection = json_object_get_string(value);
            if (snprintf(config->storage.mongodb.collection, NAME_MAX, "%s", collection) >= NAME_MAX) {
                zsys_error("config: json: MongoDB collection name is too long");
                return -1;
            }
        }
        else {
            zsys_error("config: json: Invalid parameter '%s' for MongoDB storage module", key);
            return -1;
        }
    }

    return 0;
}
#endif

static int
handle_storage_parameters(struct config *config, json_object *storage_obj)
{
    /*
     * Each storage module is configured with its own set of fields.
     * It is therefore required to know the storage type before processing any field.
     */
    if (setup_storage_type(config, storage_obj)) {
        return -1;
    }

    switch (config->storage.type)
    {
        case STORAGE_NULL:
        return setup_storage_null_parameters(config, storage_obj);

        case STORAGE_CSV:
        return setup_storage_csv_parameters(config, storage_obj);

        case STORAGE_SOCKET:
        return setup_storage_socket_parameters(config, storage_obj);

#ifdef HAVE_MONGODB
        case STORAGE_MONGODB:
        return setup_storage_mongodb_parameters(config, storage_obj);
#endif

        default:
        return -1;
    }
}

static int
setup_perf_events_group_events(struct events_group *events_group, json_object *events_group_obj)
{
    const char *event_name = NULL;

    if (!json_object_is_type(events_group_obj, json_type_array)) {
        zsys_error("config: json: Invalid 'events' field type for group '%s'", events_group->name);
        return -1;
    }

    for (size_t i = 0; i < json_object_array_length(events_group_obj); i++) {
        event_name = json_object_get_string(json_object_array_get_idx(events_group_obj, i));
        if (events_group_append_event(events_group, event_name)) {
            zsys_error("config: json: Failed to add event '%s' to group '%s'", event_name, events_group->name);
            return -1;
        }
    }

    return 0;
}

static int
setup_perf_events_group_mode(struct events_group *events_group, json_object *mode_obj)
{
    const char *mode = NULL;

    mode = json_object_get_string(mode_obj);
    if (!strcasecmp(mode, "MONITOR_ONE_CPU_PER_SOCKET") || !strcasecmp(mode, "ONE_CPU_PER_SOCKET")) {
        events_group->type = MONITOR_ONE_CPU_PER_SOCKET;
        return 0;
    }
    if (!strcasecmp(mode, "MONITOR_ALL_CPU_PER_SOCKET") || !strcasecmp(mode, "ALL_CPU_PER_SOCKET")) {
        events_group->type = MONITOR_ALL_CPU_PER_SOCKET;
        return 0;
    }

    zsys_error("config: json: Invalid monitoring mode '%s' for events group '%s'", mode, events_group->name);
    return -1;
}

static int
handle_perf_events_group_parameters(const char *events_group_name, json_object *events_group_obj, zhashx_t *events_groups)
{
    int ret = -1;
    struct events_group *events_group = NULL;

    events_group = events_group_create(events_group_name);
    if (!events_group) {
        zsys_error("config: json: Failed to create '%s' events group", events_group_name);
        return -1;
    }

    json_object_object_foreach(events_group_obj, key, value) {
        if (!strcasecmp(key, "events")) {
            if (setup_perf_events_group_events(events_group, value)) {
                goto cleanup;
            }
        }
        else if (!strcasecmp(key, "monitoring_type") || !strcasecmp(key, "mode")) {
            if (setup_perf_events_group_mode(events_group, value)) {
                goto cleanup;
            }
        }
        else {
            zsys_error("config: json: Invalid parameter '%s' for '%s' events group", key, events_group);
            goto cleanup;
        }
    }

    ret = 0;
    zhashx_insert(events_groups, events_group_name, events_group);

cleanup:
    events_group_destroy(&events_group); /* The events group are duplicated on insert */
    return ret;
}

static int
handle_perf_events_groups(json_object *events_groups_obj, zhashx_t *events_groups)
{
    json_object_object_foreach(events_groups_obj, key, value) {
        if (handle_perf_events_group_parameters(key, value, events_groups)) {
            return -1;
        }
    }

    return 0;
}

static int
process_json_fields(struct config *config, json_object *root)
{
    json_object_object_foreach(root, key, value) {
        if (!strcasecmp(key, "verbose")) {
            if (setup_verbose(config, value)) {
                return -1;
            }
        }
        else if (!strcasecmp(key, "name") || !strcasecmp(key, "sensor-name")) {
            if (setup_sensor_name(config, value)) {
                return -1;
            }
        }
        else if (!strcasecmp(key, "cgroup_basepath") || !strcasecmp(key, "cgroup-basepath")) {
            if (setup_cgroup_basepath(config, value)) {
                return -1;
            }
        }
        else if (!strcasecmp(key, "frequency")) {
            if (setup_frequency(config, value)) {
                return -1;
            }
        }
        else if (!strcasecmp(key, "output") || !strcasecmp(key, "storage")) {
            if (handle_storage_parameters(config, value)) {
                return -1;
            }
        }
        else if (!strcasecmp(key, "system") || !strcasecmp(key, "global")) {
            if (handle_perf_events_groups(value, config->events.system)) {
                return -1;
            }
        }
        else if (!strcasecmp(key, "container") || !strcasecmp(key, "cgroups")) {
            if (handle_perf_events_groups(value, config->events.containers)) {
                return -1;
            }
        }
        else {
            zsys_error("config: json: Unknown parameter: '%s'", key);
            return -1;
        }
    }

    return 0;
}

static int
read_file_content(int fd, char *buffer, size_t buffer_size)
{
    struct stat sb;
    ssize_t read_bytes = 0;

    if (fstat(fd, &sb) == -1) {
        zsys_error("config: json: Failed to get configuration file information: %s", strerror(errno));
        return -1;
    }

    if (!S_ISREG(sb.st_mode)) {
        zsys_error("config: json: Configuration file is not a regular file");
        return -1;
    }

    if (sb.st_size == 0) {
        zsys_error("config: json: Configuration file is empty");
        return -1;
    }

    if (sb.st_size >= (off_t) buffer_size) {
        zsys_error("config: json: Configuration file size is too big (current: %lu KB, max: %lu KB)", sb.st_size / 1024, buffer_size / 1024);
        return -1;
    }

    read_bytes = read(fd, buffer, buffer_size - 1);
    if (read_bytes == -1) {
        zsys_error("config: json: Failed to read the configuration file: %s", strerror(errno));
        return -1;
    }

    buffer[read_bytes] = '\0';
    return 0;
}

static void
compute_current_position_from_offset(const char *str, size_t target_offset, size_t *line, size_t *column)
{
    *line = 1;
    *column = 1;
    for (size_t current_offset = 0; current_offset < target_offset; current_offset++)
    {
        if (str[current_offset] == '\n') {
            (*line)++;
            *column = 1;
        }
        else {
            (*column)++;
        }
    }
}

static int
parse_json_configuration_file_from_fd(int fd, json_object **obj)
{
    int ret = -1;
    char buffer[JSON_FILE_BUFFER_SIZE] = {0};
    json_tokener *tok = NULL;
    enum json_tokener_error jerr;
    size_t line, column;

    if (read_file_content(fd, buffer, JSON_FILE_BUFFER_SIZE)) {
        goto error_read_content;
    }

    tok = json_tokener_new();
    if (!tok) {
        zsys_error("config: json: Failed to initialize json tokener");
        goto error_init_tokener;
    }

    *obj = json_tokener_parse_ex(tok, buffer, (int) strlen(buffer) + 1);
    jerr = json_tokener_get_error(tok);

    if (jerr != json_tokener_success) {
        compute_current_position_from_offset(buffer, json_tokener_get_parse_end(tok), &line, &column);
        zsys_error("config: json: Failed to parse json: %s (line: %lu, column: %lu)", json_tokener_error_desc(jerr), line, column);
        goto error_tokener_parse;
    }

    ret = 0;

error_tokener_parse:
    json_tokener_free(tok);
error_read_content:
error_init_tokener:
    return ret;
}

int
config_setup_from_json_file(struct config *config, const char *filepath)
{
    int ret = -1;
    int fd = -1;
    json_object *root = NULL;

    fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        zsys_error("config: json: Failed to open configuration file: %s", strerror(errno));
        goto error_open_file;
    }

    if (parse_json_configuration_file_from_fd(fd, &root)) {
        goto error_parse_file;
    }

    if (process_json_fields(config, root)) {
        zsys_error("config: json: Failed to process the given configuration file");
        goto error_process_fields;
    }

    ret = 0;

error_parse_file:
error_process_fields:
    close(fd);
    json_object_put(root);
error_open_file:
    return ret;
}
