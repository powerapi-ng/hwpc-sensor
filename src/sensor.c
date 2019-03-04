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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
#include <czmq.h>

#include "pmu.h"
#include "events.h"
#include "hwinfo.h"
#include "perf.h"
#include "report.h"
#include "target/target.h"
#include "storage.h"
#include "csv.h"

#ifdef HAVE_MONGODB
#include "mongodb.h"
#endif

void
usage()
{
    fprintf(stderr, "usage: smartwatts-sensor [-v] [-f FREQUENCY] [-p CGROUP_PATH] -n SENSOR_NAME\n"
            "\t-c | -s EVENT_GROUP_NAME [-o] -e EVENT_NAME\n"
            "\t-r STORAGE_MODULE -U STORAGE_URL [-D STORAGE_D] [-C STORAGE_C]\n"
    );
}

static struct storage_module *
setup_storage_module(enum storage_type type, char *sensor_name, char *Uflag, char *Dflag __attribute__((unused)), char *Cflag __attribute__((unused)))
{
    switch (type)
    {
        case STORAGE_CSV:
            return csv_create(sensor_name, Uflag);
#ifdef HAVE_MONGODB
        case STORAGE_MONGODB:
            return mongodb_create(sensor_name, Uflag, Dflag, Cflag);
#endif
        default:
            return NULL;
    }
}

static void
sync_cgroups_running_monitored(struct hwinfo *hwinfo, zhashx_t *container_events_groups, char *cgroup_basepath, zhashx_t *container_monitoring_actors)
{
    zhashx_t *running_targets = NULL; /* char *cgroup_path -> struct target *target */
    zactor_t *perf_monitor = NULL;
    const char *cgroup_path = NULL;
    struct target *target = NULL;
    struct perf_config *monitor_config = NULL;

    /* to store running cgroups name and absolute path */
    running_targets = zhashx_new();

    /* get running (and identifiable) container(s) */
    if (target_discover_running(cgroup_basepath, TARGET_TYPE_EVERYTHING ^ TARGET_TYPE_UNKNOWN, running_targets)) {
        zsys_error("sensor: error when retrieving the running targets.");
        goto out;
    }

    /* stop monitoring dead container(s) */
    for (perf_monitor = zhashx_first(container_monitoring_actors); perf_monitor; perf_monitor = zhashx_next(container_monitoring_actors)) {
        cgroup_path = zhashx_cursor(container_monitoring_actors);
        target = zhashx_lookup(running_targets, cgroup_path);
        if (!target) {
            zhashx_freefn(running_targets, cgroup_path, (zhashx_free_fn *) target_destroy);
            zhashx_delete(container_monitoring_actors, cgroup_path);
        }
    }

    /* start monitoring new container(s) */
    for (target = zhashx_first(running_targets); target; target = zhashx_next(running_targets)) {
        cgroup_path = zhashx_cursor(running_targets);
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
    int c;
    int verbose = 0;
    char *frequency_endp = NULL;
    long frequency = 1000; /* in milliseconds */
    struct pmu_topology *sys_pmu_topology = NULL;
    char *cgroup_basepath = "/sys/fs/cgroup/perf_event";
    zhashx_t *system_events_groups = NULL; /* char *group_name -> struct events_group *group */
    zhashx_t *container_events_groups = NULL; /* char *group_name -> struct events_group *group */
    struct events_group *current_events_group = NULL;
    char *sensor_name = NULL;
    enum storage_type storage_type = STORAGE_CSV; /* use csv storage module by default */
    char *Uflag = NULL;
    char *Dflag = NULL;
    char *Cflag = NULL;
    struct pmu_info *pmu = NULL;
    struct hwinfo *hwinfo = NULL;
    struct storage_module *storage = NULL;
    struct report_config reporting_conf = {0};
    zactor_t *reporting = NULL;
    zhashx_t *cgroups_running = NULL; /* char *cgroup_name -> char *cgroup_absolute_path */
    zhashx_t *container_monitoring_actors = NULL; /* char *actor_name -> zactor_t *actor */
    zsock_t *ticker = NULL;
    struct target *system_target = NULL;
    struct perf_config *system_monitor_config = NULL;
    zactor_t *system_perf_monitor = NULL;

    if (!zsys_init()) {
        fprintf(stderr, "czmq: failed to initialize zsys context\n");
        return ret;
    }

    /* disable limit of maximum czmq sockets */
    zsys_set_max_sockets(0);

    /* check if run as root */
    if (geteuid()) {
        zsys_error("perms: this program requires to be run as root to work");
        goto cleanup;
    }

    /* initialize the PMU module */
    if (pmu_initialize()) {
        zsys_error("pmu: cannot initialize the pmu module");
        goto cleanup;
    }

    /* stores events to monitor globally (system) */
    system_events_groups = zhashx_new();
    zhashx_set_duplicator(system_events_groups, (zhashx_duplicator_fn *) events_group_dup);
    zhashx_set_destructor(system_events_groups, (zhashx_destructor_fn *) events_group_destroy);

    /* stores events to monitor per-container */
    container_events_groups = zhashx_new();
    zhashx_set_duplicator(container_events_groups, (zhashx_duplicator_fn *) events_group_dup);
    zhashx_set_destructor(container_events_groups, (zhashx_destructor_fn *) events_group_destroy);

    /* parse cli arguments */
    while ((c = getopt(argc, argv, "vf:p:n:s:c:e:or:U:D:C:")) != -1) {
        switch (c)
        {
            case 'v':
                verbose++;
                break;
            case 'f':
                errno = 0;
                frequency = strtol(optarg, &frequency_endp, 0);
                if (*optarg == '\0' || *frequency_endp != '\0' || errno) {
                    zsys_error("args: the given frequency is invalid");
                    goto cleanup;
                }
                if (frequency < INT_MIN || frequency > INT_MAX) {
                    zsys_error("args: the given frequency is out of range");
                    goto cleanup;
                }
                break;
            case 'p':
                cgroup_basepath = optarg;
                break;
            case 'n':
                sensor_name = optarg;
                break;
            case 's':
                current_events_group = events_group_create(optarg);
                if (!current_events_group) {
                    zsys_error("args: failed to create the '%s' system group", optarg);
                    goto cleanup;
                }
                zhashx_insert(system_events_groups, optarg, current_events_group);
                events_group_destroy(&current_events_group);
                current_events_group = zhashx_lookup(system_events_groups, optarg); /* get the duplicated events group */
                break;
            case 'c':
                current_events_group = events_group_create(optarg);
                if (!current_events_group) {
                    zsys_error("args: failed to create the '%s' container group", optarg);
                    goto cleanup;
                }
                zhashx_insert(container_events_groups, optarg, current_events_group);
                events_group_destroy(&current_events_group);
                current_events_group = zhashx_lookup(container_events_groups, optarg); /* get the duplicated events group */
                break;
            case 'e':
                if (!current_events_group) {
                    zsys_error("args: you cannot add events to an inexisting events group");
                    goto cleanup;
                }
                if (events_group_append_event(current_events_group, optarg)) {
                    zsys_error("args: the event '%s' is invalid or unsupported by this machine", optarg);
                    goto cleanup;
                }
                break;
            case 'o':
                if (!current_events_group) {
                    zsys_error("args: you cannot set the type of an inexistent events group");
                    goto cleanup;
                }
                current_events_group->type = MONITOR_ONE_CPU_PER_SOCKET;
                break;
            case 'r':
                storage_type = storage_module_get_type(optarg);
                if (storage_type == STORAGE_UNKNOWN) {
                    zsys_error("args: storage module '%s' is invalid or disabled at compile time", optarg);
                    goto cleanup;
                }
                break;
            case 'U':
                Uflag = optarg;
                break;
            case 'D':
                Dflag = optarg;
                break;
            case 'C':
                Cflag = optarg;
                break;
            default:
                usage();
                goto cleanup;
        }
    }

    /* check program configuration */
    if (frequency <= 0) {
        zsys_error("args: the measurement frequency must be > 0");
        goto cleanup;
    }

    if (!sensor_name) {
        zsys_error("args: you must provide a sensor name");
        goto cleanup;
    }

    if (zhashx_size(system_events_groups) == 0 && zhashx_size(container_events_groups) == 0) {
        zsys_error("args: you must provide event(s) to monitor");
        goto cleanup;
    }

    if (storage_type == STORAGE_CSV && (!Uflag)) {
        zsys_error("args: you must provide the required CSV storage module configuration");
        goto cleanup;
    }

#ifdef HAVE_MONGODB
    if (storage_type == STORAGE_MONGODB && (!Uflag || !Dflag || !Cflag)) {
        zsys_error("args: you must provide the required MongoDB storage module configuration");
        goto cleanup;
    }
#endif

    zsys_info("sensor: starting...");

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
    for (pmu = zlistx_first(sys_pmu_topology->pmus); pmu; pmu = zlistx_next(sys_pmu_topology->pmus)) {
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

    /* setup storage module */
    storage = setup_storage_module(storage_type, sensor_name, Uflag, Dflag, Cflag);
    if (!storage) {
        zsys_error("sensor: failed to create %s storage module", storage_type);
        goto cleanup;
    }
    if (STORAGE_MODULE_CALL(storage, initialize)) {
        zsys_error("sensor: failed to initialize storage module");
        goto cleanup;
    }
    if (STORAGE_MODULE_CALL(storage, ping)) {
        zsys_error("sensor: failed to ping storage module");
        STORAGE_MODULE_CALL(storage, deinitialize);
        goto cleanup;
    }

    /* start reporting actor */
    reporting_conf = (struct report_config){
        .storage = storage
    };
    reporting = zactor_new(reporting_actor, &reporting_conf);

    /* create ticker publisher socket */
    ticker = zsock_new_pub("inproc://ticker");

    /* start system monitoring actor only when needed */
    if (zhashx_size(system_events_groups)) {
        system_target = target_create(TARGET_TYPE_ALL, NULL);
        system_monitor_config = perf_config_create(hwinfo, system_events_groups, system_target);
        system_perf_monitor = zactor_new(perf_monitoring_actor, system_monitor_config);
    }

    /* monitor running containers */
    container_monitoring_actors = zhashx_new();
    zhashx_set_destructor(container_monitoring_actors, (zhashx_destructor_fn *) zactor_destroy);
    while (!zsys_interrupted) {
        /* monitor containers only when needed */
        if (zhashx_size(container_events_groups)) {
            sync_cgroups_running_monitored(hwinfo, container_events_groups, cgroup_basepath, container_monitoring_actors);
        }

        /* send clock tick to monitoring actors */
        zsock_send(ticker, "s8", "CLOCK_TICK", zclock_time());

        zclock_sleep((int) frequency);
    }

    /* clean storage module ressources */
    STORAGE_MODULE_CALL(storage, deinitialize);

    ret = 0;

cleanup:
    zhashx_destroy(&cgroups_running);
    zhashx_destroy(&container_monitoring_actors);
    zactor_destroy(&system_perf_monitor);
    zactor_destroy(&reporting);
    storage_module_destroy(storage);
    zsock_destroy(&ticker);
    zhashx_destroy(&system_events_groups);
    zhashx_destroy(&container_events_groups);
    pmu_topology_destroy(sys_pmu_topology);
    pmu_deinitialize();
    hwinfo_destroy(hwinfo);
    zsys_shutdown();
    return ret;
}

