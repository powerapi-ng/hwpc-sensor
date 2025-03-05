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

#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <czmq.h>

#include "version.h"
#include "config.h"
#include "config_cli.h"
#include "pmu.h"
#include "events.h"
#include "hwinfo.h"
#include "perf.h"
#include "report.h"
#include "target.h"
#include "storage.h"
#include "storage_null.h"
#include "storage_csv.h"
#include "storage_socket.h"

#ifdef HAVE_MONGODB
#include "storage_mongodb.h"
#endif

static struct storage_module *
setup_storage_module(struct config *config)
{
    switch (config->storage.type)
    {
        case STORAGE_NULL:
            return storage_null_create(config);
        case STORAGE_CSV:
            return storage_csv_create(config);
        case STORAGE_SOCKET:
            return storage_socket_create(config);
#ifdef HAVE_MONGODB
        case STORAGE_MONGODB:
            return storage_mongodb_create(config);
#endif
        default:
            return NULL;
    }
}

static void
sync_cgroups_running_monitored(struct hwinfo *hwinfo, zhashx_t *container_events_groups, const char *cgroup_basepath, zhashx_t *container_monitoring_actors)
{
    zhashx_t *running_targets = NULL; /* char *cgroup_path -> struct target *target */
    zactor_t *perf_monitor = NULL;
    const char *cgroup_path = NULL;
    struct target *target = NULL;
    struct perf_config *monitor_config = NULL;

    /* to store running cgroups name and absolute path */
    running_targets = zhashx_new();

    /* get running (and identifiable) container(s) */
    if (target_discover_running(cgroup_basepath, TARGET_TYPE_EVERYTHING, running_targets)) {
        zsys_error("sensor: error when retrieving the running targets.");
        goto out;
    }

    /* stop monitoring dead container(s) */
    for (perf_monitor = (zactor_t *) zhashx_first(container_monitoring_actors); perf_monitor; perf_monitor = (zactor_t *) zhashx_next(container_monitoring_actors)) {
        cgroup_path = (const char *) zhashx_cursor(container_monitoring_actors);
        target = (struct target *) zhashx_lookup(running_targets, cgroup_path);
        if (!target) {
            zhashx_freefn(running_targets, cgroup_path, (zhashx_free_fn *) target_destroy);
            zhashx_delete(container_monitoring_actors, cgroup_path);
        }
    }

    /* start monitoring new container(s) */
    for (target = (struct target *) zhashx_first(running_targets); target; target = (struct target *) zhashx_next(running_targets)) {
        cgroup_path = (const char *) zhashx_cursor(running_targets);
        if (!zhashx_lookup(container_monitoring_actors, cgroup_path)) {
            monitor_config = perf_config_create(hwinfo, container_events_groups, target);
            perf_monitor = zactor_new(perf_monitoring_actor, monitor_config);
            zhashx_insert(container_monitoring_actors, cgroup_path, perf_monitor);
        } else {
            zhashx_freefn(running_targets, cgroup_path, (zhashx_free_fn *) target_destroy);
        }
    }

out:
    zhashx_destroy(&running_targets);
}

int
main(int argc, char **argv)
{
    int ret = 1;
    struct config *config = NULL;
    struct utsname kernel_info;
    struct pmu_topology *sys_pmu_topology = NULL;
    struct pmu_info *pmu = NULL;
    struct hwinfo *hwinfo = NULL;
    struct storage_module *storage = NULL;
    struct report_config reporting_conf = {};
    zactor_t *reporting = NULL;
    zhashx_t *cgroups_running = NULL; /* char *cgroup_name -> char *cgroup_absolute_path */
    zhashx_t *container_monitoring_actors = NULL; /* char *actor_name -> zactor_t *actor */
    zsock_t *ticker = NULL;
    struct target *system_target = NULL;
    struct perf_config *system_monitor_config = NULL;
    zactor_t *system_perf_monitor = NULL;

    signal(SIGPIPE, SIG_IGN);

    if (!zsys_init()) {
        fprintf(stderr, "czmq: failed to initialize zsys context\n");
        return ret;
    }

    /* disable limit of maximum czmq sockets */
    zsys_set_max_sockets(0);

    /* show build information */
    zsys_info("build: version %s (rev: %s)", VERSION_GIT_TAG, VERSION_GIT_REV);

    /* show Kernel information */
    if (uname(&kernel_info)) {
	    zsys_error("uname: failed to get Kernel information");
	    goto cleanup;
    }
    zsys_info("uname: %s %s %s %s", kernel_info.sysname, kernel_info.release, kernel_info.version, kernel_info.machine);

    /* check if perf_event is working */
    if (perf_try_global_counting_event_open()) {
        zsys_error("perf: error while testing the perf_event support");
        goto cleanup;
    }

    /* initialize the PMU module */
    if (pmu_initialize()) {
        zsys_error("pmu: cannot initialize the pmu module");
        goto cleanup;
    }

    /* detect pmu topology */
    sys_pmu_topology = pmu_topology_create();
    if (!sys_pmu_topology) {
        zsys_error("pmu: cannot allocate pmu topology memory");
        goto cleanup;
    }
    if (pmu_topology_detect(sys_pmu_topology)) {
        zsys_error("pmu: cannot detect system PMU topology");
        goto cleanup;
    }
    for (pmu = (struct pmu_info *) zlistx_first(sys_pmu_topology->pmus); pmu; pmu = (struct pmu_info *) zlistx_next(sys_pmu_topology->pmus)) {
        zsys_info("pmu: found %s '%s' having %d events, %d counters (%d general, %d fixed)",
                pmu->info.name,
                pmu->info.desc,
                pmu->info.nevents,
                pmu->info.num_cntrs + pmu->info.num_fixed_cntrs,
                pmu->info.num_cntrs,
                pmu->info.num_fixed_cntrs);
    }

    /* detect machine hardware */
    hwinfo = hwinfo_create();
    if (!hwinfo) {
        zsys_error("hwinfo: error while creating hardware information container");
        goto cleanup;
    }
    if (hwinfo_detect(hwinfo)) {
        zsys_error("hwinfo: error while detecting hardware information");
        goto cleanup;
    }

    /* get application config */
    config = config_create();
    if (!config) {
	    zsys_error("config: failed to create config container");
	    goto cleanup;
    }
    if (config_setup_from_cli(argc, argv, config)) {
        zsys_error("config: failed to parse the provided command-line arguments");
        goto cleanup;
    }
    if (config_validate(config)) {
        zsys_error("config: failed to validate config");
        goto cleanup;
    }

    /* setup storage module */
    storage = setup_storage_module(config);
    if (!storage) {
        zsys_error("sensor: failed to create '%s' storage module", storage_types_name[config->storage.type]);
        goto cleanup;
    }
    if (storage_module_initialize(storage)) {
        zsys_error("sensor: failed to initialize storage module");
        goto cleanup;
    }
    if (storage_module_ping(storage)) {
        zsys_error("sensor: failed to ping storage module");
        storage_module_deinitialize(storage);
        goto cleanup;
    }

    zsys_info("sensor: configuration is valid, starting monitoring...");

    /* start reporting actor */
    reporting_conf = (struct report_config){
        .storage = storage
    };
    reporting = zactor_new(reporting_actor, &reporting_conf);

    /* create ticker publisher socket */
    ticker = zsock_new_pub("inproc://ticker");

    /* start system monitoring actor only when needed */
    if (zhashx_size(config->events.system)) {
        system_target = target_create(TARGET_TYPE_ALL, NULL, NULL);
        system_monitor_config = perf_config_create(hwinfo, config->events.system, system_target);
        system_perf_monitor = zactor_new(perf_monitoring_actor, system_monitor_config);
    }

    /* monitor running containers */
    container_monitoring_actors = zhashx_new();
    zhashx_set_destructor(container_monitoring_actors, (zhashx_destructor_fn *) zactor_destroy);
    while (!zsys_interrupted) {
        /* monitor containers only when needed */
        if (zhashx_size(config->events.containers)) {
            sync_cgroups_running_monitored(hwinfo, config->events.containers, config->sensor.cgroup_basepath, container_monitoring_actors);
        }

        /* send clock tick to monitoring actors */
        zsock_send(ticker, "s8", "CLOCK_TICK", zclock_time());

        zclock_sleep((int)config->sensor.frequency);
    }

    /* clean storage module ressources */
    storage_module_deinitialize(storage);

    ret = 0;

cleanup:
    zhashx_destroy(&cgroups_running);
    zhashx_destroy(&container_monitoring_actors);
    zactor_destroy(&system_perf_monitor);
    zactor_destroy(&reporting);
    storage_module_destroy(storage);
    zsock_destroy(&ticker);
    config_destroy(config);
    pmu_topology_destroy(sys_pmu_topology);
    pmu_deinitialize();
    hwinfo_destroy(hwinfo);
    zsys_shutdown();
    return ret;
}
