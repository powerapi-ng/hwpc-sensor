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
#include <sys/stat.h>
#include <sys/vfs.h>
#include <linux/magic.h>

#include "config.h"
#include "events.h"
#include "storage.h"


struct config *
config_create(void)
{
    struct config *config = malloc(sizeof(struct config));

    if (!config)
        return NULL;

    /* sensor default config */
    config->sensor.verbose = 0;
    config->sensor.frequency = 1000;
    snprintf(config->sensor.cgroup_basepath, PATH_MAX, "%s", "/sys/fs/cgroup");
    gethostname(config->sensor.name, HOST_NAME_MAX);

    /* storage default config */
    config->storage.type = STORAGE_UNKNOWN;
    memset(&config->storage, 0, sizeof(struct config_storage));

    /* events default config */
    config->events.system = zhashx_new();
    zhashx_set_duplicator(config->events.system, (zhashx_duplicator_fn *) events_group_dup);
    zhashx_set_destructor(config->events.system, (zhashx_destructor_fn *) events_group_destroy);

    config->events.containers = zhashx_new();
    zhashx_set_duplicator(config->events.containers, (zhashx_duplicator_fn *) events_group_dup);
    zhashx_set_destructor(config->events.containers, (zhashx_destructor_fn *) events_group_destroy);

    return config;
}

static int
check_cgroup_basepath(const char *cgroup_basepath)
{
    struct statfs sfs;

    if (statfs(cgroup_basepath, &sfs)) {
        zsys_error("config: Failed to get cgroup basepath (%s) information: %s", cgroup_basepath, strerror(errno));
        return -1;
    }

    if (sfs.f_type == CGROUP_SUPER_MAGIC) {
        return 0;
    }

    if (sfs.f_type == CGROUP2_SUPER_MAGIC) {
        return 0;
    }

    if (sfs.f_type == TMPFS_MAGIC) {
        zsys_warning("config: You are probably using a unified cgroupv2 basepath on a machine using the legacy cgroupv1 hierarchy!");
    }

    zsys_error("config: Invalid cgroup basepath: %s", cgroup_basepath);
    return -1;
}

static int
is_events_group_empty(zhashx_t *events_groups)
{
    struct events_group *events_group = NULL;

    for (events_group = zhashx_first(events_groups); events_group; events_group = zhashx_next(events_groups)) {
        if (zlistx_size(events_group->events) == 0) {
            zsys_error("config: Events group '%s' is empty", events_group->name);
            return -1;
        }
    }

    return 0;
}

int
config_validate(struct config *config)
{
    const struct config_sensor *sensor = &config->sensor;
    const struct config_storage *storage = &config->storage;
    const struct config_events *events = &config->events;

    if (!strlen(sensor->name)) {
	    zsys_error("config: You must provide a sensor name");
	    return -1;
    }

    if (!strlen(sensor->cgroup_basepath)) {
        zsys_error("config: You must provide a cgroup basepath");
        return -1;
    }

    if (check_cgroup_basepath(sensor->cgroup_basepath)) {
        return -1;
    }

    if (zhashx_size(events->system) == 0 && zhashx_size(events->containers) == 0) {
	    zsys_error("config: You must provide event(s) to monitor");
	    return -1;
    }

    if (is_events_group_empty(events->system) || is_events_group_empty(events->containers)) {
        return -1;
    }

    if (storage->type == STORAGE_CSV && !strlen(storage->csv.outdir)) {
	    zsys_error("config: CSV storage module requires the 'U' flag to be set");
	    return -1;
    }

    if (storage->type == STORAGE_SOCKET && (!strlen(storage->socket.hostname) || !strlen(storage->socket.port))) {
	    zsys_error("config: Socket storage module requires the 'U' and 'P' flags to be set");
	    return -1;
    }

#ifdef HAVE_MONGODB
    if (storage->type == STORAGE_MONGODB && (!strlen(storage->mongodb.uri) || !strlen(storage->mongodb.database) || !strlen(storage->mongodb.collection))) {
	    zsys_error("config: MongoDB storage module requires the 'U', 'D' and 'C' flags to be set");
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
