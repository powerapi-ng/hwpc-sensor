#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
#include <czmq.h>

#include "pmu.h"
#include "events.h"
#include "hwinfo.h"
#include "cgroups.h"
#include "perf.h"
#include "report.h"
#include "mongodb.h"

void
usage()
{
    fprintf(stderr, "usage: smartwatts-sensor [-v] [-f FREQUENCY] [-p CGROUP_PATH] -n SENSOR_NAME\n"
            "\t-c | -s EVENT_GROUP_NAME [-o] -e EVENT_NAME\n"
            "\t[-r mongodb]\n"
            "\t\tMongoDB: -U MONGODB_URI -D MONGODB_DATABASE -C MONGODB_COLLECTION\n");
}

void
sync_cgroups_running_monitored(struct hwinfo *hwinfo, zhashx_t *container_events_groups, char *cgroup_basepath, zhashx_t *container_monitoring_actors)
{
    zhashx_t *cgroups_running = NULL;
    zactor_t *perf_monitor = NULL;
    const char *cgroup_path = NULL;
    const char *cgroup_name = NULL;
    struct perf_config *monitor_config = NULL;

    /* to store running cgroups name and absolute path */
    cgroups_running = zhashx_new();
    zhashx_set_duplicator(cgroups_running, (zhashx_duplicator_fn *) strdup);
    zhashx_set_destructor(cgroups_running, (zhashx_destructor_fn *) zstr_free);

    /* get running container(s) */
    get_running_perf_event_cgroups(cgroup_basepath, cgroups_running);

    /* stop monitoring dead container(s) */
    for (perf_monitor = zhashx_first(container_monitoring_actors); perf_monitor; perf_monitor = zhashx_next(container_monitoring_actors)) {
        cgroup_name = zhashx_cursor(container_monitoring_actors);
        if (!zhashx_lookup(cgroups_running, cgroup_name)) {
            zsys_info("sensor: killing monitoring actor of %s", cgroup_name);
            zhashx_delete(container_monitoring_actors, cgroup_name);
        }
    }

    /* start monitoring new container(s) */
    for (cgroup_path = zhashx_first(cgroups_running); cgroup_path; cgroup_path = zhashx_next(cgroups_running)) {
        cgroup_name = zhashx_cursor(cgroups_running);
        if (!zhashx_lookup(container_monitoring_actors, cgroup_name)) {
            zsys_info("sensor: starting monitoring actor for %s (path=%s)", cgroup_name, cgroup_path);
            monitor_config = perf_config_create(hwinfo, container_events_groups, cgroup_name, cgroup_path);
            perf_monitor = zactor_new(perf_monitoring_actor, monitor_config);
            zhashx_insert(container_monitoring_actors, cgroup_name, perf_monitor);
        }
    }

    zhashx_destroy(&cgroups_running);
}

int
main (int argc, char **argv)
{
    int ret = 1;
    int c;
    int verbose = 0;
    char *frequency_endp = NULL;
    long frequency = 1000; /* in milliseconds */
    struct pmu_topology *sys_pmu_topology = NULL;
    char *cgroup_basepath = "/docker";
    zhashx_t *system_events_groups = NULL; /* char *group_name -> struct events_group *group */
    zhashx_t *container_events_groups = NULL; /* char *group_name -> struct events_group *group */
    struct events_group *current_events_group = NULL;
    char *sensor_name = NULL;
    char *mongodb_uri = NULL;
    char *mongodb_database = NULL;
    char *mongodb_collection = NULL;
    struct pmu_info *pmu = NULL;
    struct hwinfo *hwinfo = NULL;
    struct mongodb_config mongodb_conf = {0};
    struct storage_module *storage_module = NULL;
    struct report_config reporting_conf = {0};
    zactor_t *reporting = NULL;
    zhashx_t *cgroups_running = NULL; /* char *cgroup_name -> char *cgroup_absolute_path */
    zhashx_t *container_monitoring_actors = NULL; /* char *actor_name -> zactor_t *actor */
    zactor_t *system_perf_monitor = NULL;
    zsock_t *ticker = NULL;
    struct perf_config *monitor_config = NULL;

    if (!zsys_init()) {
        fprintf(stderr, "czmq: failed to initialize zsys context\n");
        return ret;
    }

    /* check if run as root */
    if (geteuid()) {
        fprintf(stderr, "perms: this program requires to be run as root to work\n");
        goto cleanup;
    }

    /* set scheduling priority of the program */
    if (setpriority(PRIO_PROCESS, 0, -20)) {
        fprintf(stderr, "priority: cannot set the process priority\n");
        goto cleanup;
    }

    /* initialize the cgroups module */
    if (initialize_cgroups()) {
        zsys_error("cgroups: cannot initialize the cgroup module");
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
    while ((c = getopt(argc, argv, "vf:p:n:s:c:e:oU:D:C:")) != -1) {
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
                    zsys_error("sensor: failed to create the '%s' system group", optarg);
                    goto cleanup;
                }
                zhashx_insert(system_events_groups, optarg, current_events_group);
                events_group_destroy(&current_events_group);
                current_events_group = zhashx_lookup(system_events_groups, optarg); /* get the duplicated events group */
                break;
            case 'c':
                current_events_group = events_group_create(optarg);
                if (current_events_group) {
                    zsys_error("sensor: failed to create the '%s' container group", optarg);
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
                    zsys_error("events: you cannot set the type of an inexisting events group");
                    goto cleanup;
                }
                current_events_group->type = MONITOR_ONE_CPU_PER_SOCKET;
                break;
            case 'U':
                mongodb_uri = optarg;
                break;
            case 'D':
                mongodb_database = optarg;
                break;
            case 'C':
                mongodb_collection = optarg;
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

    if (zhashx_size(system_events_groups) == 0 && zhashx_size(container_events_groups) == 0) {
        zsys_error("args: you must provide event(s) to monitor");
        goto cleanup;
    }

    if (!mongodb_uri || !mongodb_database || !mongodb_collection) {
        zsys_error("args: you must provide a mongodb configuration");
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

    /* setup mongodb */
    mongodb_conf = (struct mongodb_config){
        .hwinfo = hwinfo,
        .sensor_name = sensor_name,
        .uri = mongodb_uri,
        .database_name = mongodb_database,
        .collection_name = mongodb_collection
    };
    storage_module = mongodb_create(&mongodb_conf);
    if (!storage_module) {
        zsys_error("sensor: failed to create storage module");
        goto cleanup;
    }
    if (STORAGE_MODULE_CALL(storage_module, initialize)) {
        zsys_error("sensor: failed to initialize storage module");
        goto cleanup;
    }
    if (STORAGE_MODULE_CALL(storage_module, ping)) {
        zsys_error("sensor: failed to ping storage module");
        STORAGE_MODULE_CALL(storage_module, deinitialize);
        goto cleanup;
    }

    /* start reporting actor */
    reporting_conf = (struct report_config){
        .storage = storage_module
    };
    reporting = zactor_new(reporting_actor, &reporting_conf);

    /* create ticker publisher socket */
    ticker = zsock_new_pub("inproc://ticker");

    /* start system monitoring actor */
    monitor_config = perf_config_create(hwinfo, system_events_groups, "system", NULL);
    system_perf_monitor = zactor_new(perf_monitoring_actor, monitor_config);

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
    STORAGE_MODULE_CALL(storage_module, deinitialize);

    ret = 0;

cleanup:
    zhashx_destroy(&cgroups_running);
    zhashx_destroy(&container_monitoring_actors);
    zactor_destroy(&system_perf_monitor);
    zactor_destroy(&reporting);
    mongodb_destroy(storage_module);
    zsock_destroy(&ticker);
    zhashx_destroy(&system_events_groups);
    zhashx_destroy(&container_events_groups);
    pmu_topology_destroy(sys_pmu_topology);
    pmu_deinitialize();
    hwinfo_destroy(hwinfo);
    zsys_shutdown();
    return ret;
}

