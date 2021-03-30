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

#include <czmq.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "events.h"
#include "storage.h"


struct config *
config_create()
{
    struct config *config = malloc(sizeof(struct config));

    if (!config)
	return NULL;

    /* sensor default config */
    config->sensor.verbose = 0;
    config->sensor.frequency = 1000;
    config->sensor.cgroup_basepath = "/sys/fs/cgroup/perf_event";
    config->sensor.name = NULL;

    /* storage default config */
    config->storage.type = STORAGE_CSV;
    config->storage.U_flag = NULL;
    config->storage.D_flag = NULL;
    config->storage.C_flag = NULL;

    /* events default config */
    config->events.system = NULL;
    config->events.containers = NULL;

    return config;
}

static int
parse_frequency(const char *str, unsigned int *frequency)
{
    long value;
    char *str_endp = NULL;

    errno = 0;
    value = strtol(str, &str_endp, 0);

    /* check if the string have been fully processed */
    if (*optarg == '\0' || *str_endp != '\0' || errno) {
	return -1;
    }

    /* check if the extracted value fit in the destination type before casting */
    if (value < 0U || value > UINT_MAX) {
	return -1;
    }

    *frequency = (unsigned int)value;
    return 0;
}

static void
print_usage()
{
    // TODO: write an usage text
}

int
config_setup_from_cli(int argc, char **argv, struct config *config)
{
    int ret = -1;
    int c;
    struct events_group *current_events_group = NULL;

    /* stores events to monitor globally (system) */
    config->events.system = zhashx_new();
    if (!config->events.system) {
	zsys_error("config: failed to create system events group container");
	goto end;
    }
    zhashx_set_duplicator(config->events.system, (zhashx_duplicator_fn *) events_group_dup);
    zhashx_set_destructor(config->events.system, (zhashx_destructor_fn *) events_group_destroy);

    /* stores events to monitor per-container */
    config->events.containers = zhashx_new();
    if (!config->events.containers) {
	zsys_error("config: failed to create containers events group container");
	goto end;
    }
    zhashx_set_duplicator(config->events.containers, (zhashx_duplicator_fn *) events_group_dup);
    zhashx_set_destructor(config->events.containers, (zhashx_destructor_fn *) events_group_destroy);

    while ((c = getopt(argc, argv, "vf:p:n:s:c:e:or:U:D:C:P:")) != -1) {
	switch (c) {
	    case 'v':
		config->sensor.frequency++;
		break;
	    case 'f':
		if(parse_frequency(optarg, &config->sensor.frequency)) {
		    zsys_error("config: the given frequency is invalid or out of range");
		    goto end;
		}
		break;
	    case 'p':
		config->sensor.cgroup_basepath = optarg;
		break;
	    case 'n':
		config->sensor.name = optarg;
		break;
	    case 's':
		current_events_group = events_group_create(optarg);
		if (!current_events_group) {
		    zsys_error("config: failed to create system events group");
		    goto end;
		}
		zhashx_insert(config->events.system, optarg, current_events_group);
		events_group_destroy(&current_events_group);
		current_events_group = zhashx_lookup(config->events.system, optarg); /* get the duplicated events group */
		break;
	    case 'c':
		current_events_group = events_group_create(optarg);
		if (!current_events_group) {
		    zsys_error("config: failed to create containers events group");
		    goto end;
		}
		zhashx_insert(config->events.containers, optarg, current_events_group);
		events_group_destroy(&current_events_group);
		current_events_group = zhashx_lookup(config->events.containers, optarg); /* get the duplicated events group */
		break;
	    case 'o':
		if (!current_events_group) {
		    zsys_error("config: you cannot set the type of an inexistent events group");
		    goto end;
		}
		current_events_group->type = MONITOR_ONE_CPU_PER_SOCKET;
		break;
	    case 'e':
		if (!current_events_group) {
		    zsys_error("config: you cannot add an event to an inexisting events group");
		    goto end;
		}
		if (events_group_append_event(current_events_group, optarg)) {
		    zsys_error("config: event '%s' is invalid or unsupported by this machine", optarg);
		    goto end;
		}
		break;
	    case 'r':
		config->storage.type = storage_module_get_type(optarg);
		if (config->storage.type == STORAGE_UNKNOWN) {
		    zsys_error("config: storage module '%s' is invalid or disabled at compile time", optarg);
		    goto end;
		}
		break;
	    case 'U':
		config->storage.U_flag = optarg;
		break;
	    case 'D':
		config->storage.D_flag = optarg;
		break;
	    case 'C':
		config->storage.C_flag = optarg;
		break;
	    case 'P':
                config->storage.P_flag = strtol(optarg, NULL, 10);
		if(config->storage.P_flag == 0){
		    zsys_error("config:  '%s' is not a valid port number", optarg);
		    goto end;
		}
		break;
	    default:
		print_usage();
		goto end;
	}
    }

    ret = 0;

end:
    return ret;
}

int
config_validate(struct config *config)
{
    const struct config_sensor *sensor = &config->sensor;
    const struct config_storage *storage = &config->storage;
    const struct config_events *events = &config->events;

    if (!sensor->name) {
	zsys_info("config: you must provide a sensor name");
	return -1;
    }

    if (zhashx_size(events->system) == 0 && zhashx_size(events->containers) == 0) {
	zsys_error("config: you must provide event(s) to monitor");
	return -1;
    }

    if (storage->type == STORAGE_CSV && (!storage->U_flag)) {
	zsys_error("config: the CSV storage module requires the 'U' flag to be set");
	return -1;
    }

    if (storage->type == STORAGE_SOCKET && (!storage->U_flag || !storage->P_flag)) {
	zsys_error("config: the socket storage module requires the 'U' and 'P' flags to be set");
	return -1;
    }

#ifdef HAVE_MONGODB
    if (storage->type == STORAGE_MONGODB && (!storage->U_flag || !storage->D_flag || !storage->C_flag)) {
	zsys_error("config: the MongoDB storage module requires the 'U', 'D' and 'C' flags to be set");
	return -1;
    }
#endif

    return 0;
}

void
config_destroy(struct config *config)
{
    if (!config)
	return;

    zhashx_destroy(&config->events.containers);
    zhashx_destroy(&config->events.system);
    free(config);
}
