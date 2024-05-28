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

#include <czmq.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <netdb.h>

#include "config_cli.h"
#include "config_json.h"
#include "util.h"


const char short_opts[] = "x:vf:p:n:s:c:e:or:U:D:C:P:";
static struct option long_opts[] = {
    {"config-file", required_argument, 0, 'x'},
    {NULL, 0, NULL, 0}
};

static int
setup_config_from_file(struct config *config, char *filepath)
{
    char *file_extension = NULL;

    errno = 0;
    if (access(filepath, R_OK) == -1) {
        zsys_error("config: cli: Unable to access configuration file: %s", strerror(errno));
        return -1;
    }

    file_extension = strrchr(filepath, '.');
    if (!file_extension) {
        zsys_error("config: cli: Missing extension to configuration file");
        return -1;
    }

    if (strcasecmp(file_extension, ".json") == 0) {
        return config_setup_from_json_file(config, filepath);
    }

    zsys_error("config: cli: Unsupported configuration file format: %s", file_extension);
    return -1;
}

static int
setup_cgroup_basepath(struct config *config, const char *cgroup_basepath)
{
    if (snprintf(config->sensor.cgroup_basepath, PATH_MAX, "%s", cgroup_basepath) >= PATH_MAX) {
        zsys_error("config: cli: Cgroup basepath is too long");
        return -1;
    }

    return 0;
}

static int
setup_sensor_name(struct config *config, const char *sensor_name)
{
    if (snprintf(config->sensor.name, HOST_NAME_MAX, "%s", sensor_name) >= HOST_NAME_MAX) {
        zsys_error("config: cli: Sensor name is too long");
        return -1;
    }

    return 0;
}

static int
setup_frequency(struct config *config, const char *value_str)
{
    unsigned int frequency;

    if (str_to_uint(value_str, &frequency)) {
        zsys_error("config: cli: Frequency value is invalid");
        return -1;
    }

    config->sensor.frequency = frequency;
    return 0;
}

static int
setup_global_events_group(struct config *config, const char *group_name)
{
    struct events_group *events_group = NULL;

    events_group = events_group_create(group_name);
    if (!events_group) {
        zsys_error("config: cli: Failed to create '%s' global events group");
        return -1;
    }

    zhashx_insert(config->events.system, group_name, events_group);
    events_group_destroy(&events_group); /* The events group are duplicated on insert */
    return 0;
}

static int
setup_cgroups_events_group(struct config *config, const char *group_name)
{
    struct events_group *events_group = NULL;

    events_group = events_group_create(group_name);
    if (!events_group) {
        zsys_error("config: cli: Failed to create '%s' cgroups events group", group_name);
        return -1;
    }

    zhashx_insert(config->events.containers, group_name, events_group);
    events_group_destroy(&events_group); /* The events group are duplicated on insert */
    return 0;
}

static int
setup_cgroups_events_group_type(struct events_group *events_group, enum events_group_monitoring_type type)
{
    if (!events_group) {
        zsys_error("config: cli: No events group defined before setting type");
        return -1;
    }

    events_group->type = type;
    return 0;
}

static int
append_event_to_events_group(struct events_group *events_group, const char *event_name)
{
    if (!events_group) {
        zsys_error("config: cli: No events group defined for event: %s", event_name);
        return -1;
    }

    if (events_group_append_event(events_group, event_name)) {
        zsys_error("config: cli: Failed to add event '%s' to group '%s'", optarg, events_group->name);
        return -1;
    }
    return 0;
}

static int
setup_storage_module(struct config *config, const char *module_name)
{
    enum storage_type type;

    type = storage_module_get_type(module_name);
    if (type == STORAGE_UNKNOWN) {
        zsys_error("config: cli: Storage module '%s' is invalid or disabled at compile time", module_name);
        return -1;
    }

    config->storage.type = type;
    return 0;
}

static int
setup_storage_csv_parameters(struct config *config, int opt, const char *value)
{
    switch (opt)
    {
        case 'U': /* Output directory path */
        if (snprintf(config->storage.csv.outdir, PATH_MAX, "%s", value) >= PATH_MAX) {
            zsys_error("config: cli: CSV output directory path is too long");
            return -1;
        }
        break;

        default:
        return -1;
    }

    return 0;
}

static int
setup_storage_socket_parameters(struct config *config, int opt, const char *value)
{
    switch (opt)
    {
        case 'U': /* Destination IP/hostname */
        if (snprintf(config->storage.socket.hostname, HOST_NAME_MAX, "%s", value) >= HOST_NAME_MAX) {
            zsys_error("config: cli: Socket output host is too long");
            return -1;
        }
        break;

        case 'P': /* Destination port number */
        if (snprintf(config->storage.socket.port, NI_MAXSERV, "%s", value) >= NI_MAXSERV) {
            zsys_error("config: cli: Socket output port is too long");
            return -1;
        }
        break;

        default:
        return -1;
    }

    return 0;
}

#ifdef HAVE_MONGODB
static int
setup_storage_mongodb_parameters(struct config *config, int opt, const char *value)
{
    switch (opt)
    {
        case 'U': /* MongoDB URI (mongodb://x) */
        if (snprintf(config->storage.mongodb.uri, PATH_MAX, "%s", value) >= PATH_MAX) {
            zsys_error("config: cli: MongoDB URI is too long");
            return -1;
        }
        break;

        case 'D': /* MongoDB Database name */
        if (snprintf(config->storage.mongodb.database, NAME_MAX, "%s", value) >= NAME_MAX) {
            zsys_error("config: cli: MongoDB database name is too long");
            return -1;
        }
        break;

        case 'C': /* MongoDB Collection name */
        if (snprintf(config->storage.mongodb.collection, NAME_MAX, "%s", value) >= NAME_MAX) {
            zsys_error("config: cli: MongoDB collection name is too long");
            return -1;
        }
        break;

        default:
        return -1;
    }

    return 0;
}
#endif

static int
setup_storage_parameters(struct config *config, int opt, const char *value)
{
    switch (config->storage.type)
    {
        case STORAGE_NULL:
        return 0; /* Ignore parameters */

        case STORAGE_CSV:
        return setup_storage_csv_parameters(config, opt, value);

        case STORAGE_SOCKET:
        return setup_storage_socket_parameters(config, opt, value);

#ifdef HAVE_MONGODB
        case STORAGE_MONGODB:
        return setup_storage_mongodb_parameters(config, opt, value);
#endif

        default:
        return -1;
    }
}

int
config_setup_from_cli(int argc, char **argv, struct config *config)
{
    int option_index = 0;
    int opt = -1;
    struct events_group *current_events_group = NULL;

    opterr = 0; /* Disable getopt error messages  */

    while ((opt = getopt_long(argc, argv, short_opts, long_opts, &option_index)) != -1) {
        switch (opt)
        {
            case 'x':
            if (setup_config_from_file(config, optarg)) {
                return -1;
            }
            break;

            case 'v':
            config->sensor.verbose++;
            break;

            case 'p':
            if (setup_cgroup_basepath(config, optarg)) {
                return -1;
            }
            break;

            case 'n':
            if (setup_sensor_name(config, optarg)) {
                return -1;
            }
            break;

            case 'f':
            if (setup_frequency(config, optarg)) {
                return -1;
            }
            break;

            case 's':
            if (setup_global_events_group(config, optarg)) {
                return -1;
            }
            current_events_group = zhashx_lookup(config->events.system, optarg);
            break;

            case 'c':
            if (setup_cgroups_events_group(config, optarg)) {
                return -1;
            }
            current_events_group = zhashx_lookup(config->events.containers, optarg);
            break;

            case 'o':
            if (setup_cgroups_events_group_type(current_events_group, MONITOR_ONE_CPU_PER_SOCKET)) {
                return -1;
            }
            break;

            case 'e':
            if (append_event_to_events_group(current_events_group, optarg)) {
                return -1;
            }
            break;

            case 'r':
            if (setup_storage_module(config, optarg)) {
                return -1;
            }
            break;

            case 'U':
            case 'D':
            case 'C':
            case 'P':
            if (setup_storage_parameters(config, opt, optarg)) {
                return -1;
            }
            break;

            default:
            zsys_error("config: Argument '%c' is unknown", optopt);
            return -1;
        }
    }

    return 0;
}
