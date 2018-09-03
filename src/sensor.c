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
    fprintf(stderr, "usage: smartwatts-sensor [-v] [-r] [-p] [-f FREQUENCY] [-g CGROUP_PATH] -e EVENT -n SENSOR_NAME\n"
            "\t-u MONGODB_URI -d MONGODB_DATABASE -c MONGODB_COLLECTION\n");
}

int
main (int argc, char **argv)
{
    int ret = 1;
    int c;
    size_t i;
    int verbose = 0;
    int frequency = 1000; /* in milliseconds */
    struct pmu_topology *sys_pmu_topology = NULL;
    char *cgroup_basepath = "/docker";
    char *rapl_events_name[] = {"RAPL_ENERGY_CORES", "RAPL_ENERGY_PKG", "RAPL_ENERGY_DRAM"};
    size_t num_rapl_events = sizeof(rapl_events_name) / sizeof(rapl_events_name[0]);
    int flag_rapl_events = 0;
    char *pcu_events_name[] = {"UNC_P_POWER_STATE_OCCUPANCY:CORES_C0", "UNC_P_POWER_STATE_OCCUPANCY:CORES_C3", "UNC_P_POWER_STATE_OCCUPANCY:CORES_C6"};
    size_t num_pcu_events = sizeof(pcu_events_name) / sizeof(pcu_events_name[0]);
    int flag_pcu_events = 0;
    char **user_events_name = NULL;
    size_t num_user_events = 0;
    char *sensor_name = NULL;
    char *mongodb_uri = NULL;
    char *mongodb_database = NULL;
    char *mongodb_collection = NULL;
    struct events_config *rapl_events_config = NULL;
    struct events_config *pcu_events_config = NULL;
    struct events_config *user_events_config = NULL;
    struct hwinfo *hwinfo = NULL;
    struct mongodb_config mongodb_conf = {0};
    struct storage_module *storage_module = NULL;
    struct report_config reporting_conf = {0};
    zactor_t *reporting = NULL;
    zhashx_t *cgroups_running = NULL;
    zhashx_t *monitoring_actors = NULL;
    const char *cgroup_path = NULL;
    const char *cgroup_name = NULL;
    zactor_t *perf_monitor = NULL;
    zsock_t *ticker = NULL;
    struct perf_config *monitor_config = NULL;

    if (!zsys_init()) {
        fprintf(stderr, "czmq: failed to initialize zsys context\n");
        return ret;
    }

    /* parse cli arguments */
    while ((c = getopt(argc, argv, "vf:g:e:n:u:d:c:rp")) != -1) {
        switch (c)
        {
            case 'v':
                verbose++;
                break;
            case 'f':
                frequency = atoi(optarg);
                break;
            case 'g':
                cgroup_basepath = optarg;
                break;
            case 'e':
                user_events_name = realloc(user_events_name, (num_user_events + 1) * sizeof(char* ));
                user_events_name[num_user_events++] = optarg;
                break;
            case 'n':
                sensor_name = optarg;
                break;
            case 'u':
                mongodb_uri = optarg;
                break;
            case 'd':
                mongodb_database = optarg;
                break;
            case 'c':
                mongodb_collection = optarg;
                break;
            case 'r':
                flag_rapl_events = 1;
                break;
            case 'p':
                flag_pcu_events = 1;
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

    if (num_user_events == 0) {
        zsys_error("args: you must provide event(s) to monitor");
        goto cleanup;
    }

    if (!mongodb_uri || !mongodb_database || !mongodb_collection) {
        zsys_error("args: you must provide a mongodb configuration");
        goto cleanup;
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

    /* initialize the PMU module */
    if (pmu_initialize()) {
        zsys_error("events: cannot initialize the events module");
        goto cleanup;
    }

    sys_pmu_topology = pmu_topology_create();
    if (!sys_pmu_topology) {
        zsys_error("pmu: cannot allocate pmu topology memory");
        goto cleanup;
    }
    if (pmu_topology_detect(sys_pmu_topology)) {
        zsys_error("pmu: cannot detect system PMU topology");
        goto cleanup;
    }
    for (i = 0; i < sys_pmu_topology->num_pmus; i++) {
        zsys_info("pmu: found %s '%s' having %d events, %d counters (%d general, %d fixed)",
                sys_pmu_topology->pmus[i].name,
                sys_pmu_topology->pmus[i].desc,
                sys_pmu_topology->pmus[i].nevents,
                sys_pmu_topology->pmus[i].num_cntrs + sys_pmu_topology->pmus[i].num_fixed_cntrs,
                sys_pmu_topology->pmus[i].num_cntrs,
                sys_pmu_topology->pmus[i].num_fixed_cntrs);
    }

    /* get rapl events perf configuration */
    if (flag_rapl_events) {
        rapl_events_config = events_config_create();
        if (!rapl_events_config) {
            zsys_error("events: cannot allocate memory for rapl events config");
            goto cleanup;
        }

        for (i = 0; i < num_rapl_events; i++) {
            if (events_config_add(rapl_events_config, rapl_events_name[i])) {
                zsys_error("events: the rapl event '%s' is not supported by this machine", rapl_events_name[i]);
                /* errors are not fatal, some RAPL domains may be unsupported */
            }
        }

        /* if there is no RAPL domain(s) supported, this is a fatal error */
        if (!rapl_events_config->num_attrs) {
            zsys_error("events: RAPL is not supported by this machine");
            goto cleanup;
        }
    }

    /* get PCU events perf configuration */
    if (flag_pcu_events) {
        pcu_events_config = events_config_create();
        if (!pcu_events_config) {
            zsys_error("events: cannot allocate memory for PCU events config");
            goto cleanup;
        }

        for (i = 0; i < num_pcu_events; i++) {
            if (events_config_add(pcu_events_config, pcu_events_name[i])) {
                zsys_error("events: the pcu event '%s' is not supported by this machine", pcu_events_name[i]);
                /* errors are not fatal, some PCU events may be unsupported */
            }
        }

        /* if there is no PCU events supported, this is a fatal error */
        if (!pcu_events_config->num_attrs) {
            zsys_error("events: no PCU events supported by this machine");
            goto cleanup;
        }
    }

    /* get user events perf configuration */
    user_events_config = events_config_create();
    if (!user_events_config) {
        zsys_error("events: cannot allocate memory for user events config");
        goto cleanup;
    }

    for (i = 0; i < num_user_events; i++) {
        if (events_config_add(user_events_config, user_events_name[i])) {
            zsys_error("events: the user event '%s' is invalid or unsupported by this machine", user_events_name[i]);
            goto cleanup;
        }
    }

    /* detect machine hardware */
    hwinfo = hwinfo_create();
    if (!hwinfo || hwinfo_detect(hwinfo)) {
        zsys_error("hwinfo: error while detecting hardware information");
        goto cleanup;
    }

    /* print configuration */
    zsys_info("config: verbose=%d, frequency=%d, num_user_events=%lu num_rapl_events=%lu", verbose, frequency, num_user_events, num_rapl_events);
    zsys_info("config: mongodb: uri=%s database=%s collection=%s", mongodb_uri, mongodb_database, mongodb_collection);

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

    /* watch new/dead container(s) and start/stop monitoring actor(s) */
    if (initialize_cgroups()) {
        zsys_error("cgroups: cannot initialize the cgroup library");
        goto cleanup;
    }

    /* create ticker publisher socket */
    ticker = zsock_new_pub("inproc://ticker");

    /* used to store monitoring actors, the actor is killed when removed from the hash table */
    monitoring_actors = zhashx_new();
    zhashx_set_destructor(monitoring_actors, (zhashx_destructor_fn *) zactor_destroy);

    /* start monitoring actor for rapl target (only if RAPL events are enabled & available) */
    if (flag_rapl_events) {
        monitor_config = perf_config_create(hwinfo, rapl_events_config, "rapl", NULL, true);
        perf_monitor = zactor_new(perf_monitoring_actor, monitor_config);
        zhashx_insert(monitoring_actors, "rapl", perf_monitor);
    }

    /* start monitoring actor for pcu target (only if PCU events are enabled & available */
    if (flag_pcu_events) {
        monitor_config = perf_config_create(hwinfo, pcu_events_config, "pcu", NULL, true);
        perf_monitor = zactor_new(perf_monitoring_actor, monitor_config);
        zhashx_insert(monitoring_actors, "pcu", perf_monitor);
    }

    cgroups_running = zhashx_new();
    zhashx_set_destructor(cgroups_running, (zhashx_destructor_fn *) zstr_free);
    while (!zsys_interrupted) {
        /* get running container(s) */
        get_running_perf_event_cgroups(cgroup_basepath, cgroups_running);

        /* stop monitoring dead container(s) */
        for (perf_monitor = zhashx_first(monitoring_actors); perf_monitor; perf_monitor = zhashx_next(monitoring_actors)) {
            cgroup_name = zhashx_cursor(monitoring_actors);
            if (!streq(cgroup_name, "rapl") && !streq(cgroup_name, "pcu") && !zhashx_lookup(cgroups_running, cgroup_name)) { /* ignore system target */
                zsys_info("killing monitoring actor of %s", cgroup_name);
                zhashx_delete(monitoring_actors, cgroup_name);
            }
        }

        /* start monitoring new container(s) */
        for (cgroup_path = zhashx_first(cgroups_running); cgroup_path; cgroup_path = zhashx_next(cgroups_running)) {
            cgroup_name = zhashx_cursor(cgroups_running);
            if (!zhashx_lookup(monitoring_actors, cgroup_name)) {
                zsys_info("starting monitoring actor for %s (path=%s)", cgroup_name, cgroup_path);
                monitor_config = perf_config_create(hwinfo, user_events_config, cgroup_name, cgroup_path, false);
                perf_monitor = zactor_new(perf_monitoring_actor, monitor_config);
                zhashx_insert(monitoring_actors, cgroup_name, perf_monitor);
            }
        }

        zhashx_purge(cgroups_running);

        /* send clock tick to monitoring actors */
        zsock_send(ticker, "s8", "CLOCK_TICK", zclock_time());

        zclock_sleep(frequency);
    }

    /* clean storage module ressources */
    STORAGE_MODULE_CALL(storage_module, deinitialize);

    ret = 0;

cleanup:
    zhashx_destroy(&cgroups_running);
    zhashx_destroy(&monitoring_actors); /* actors will be terminated, DO NOT FREE shared structures before */
    zactor_destroy(&reporting);
    mongodb_destroy(storage_module);
    events_config_destroy(user_events_config);
    events_config_destroy(rapl_events_config);
    events_config_destroy(pcu_events_config);
    pmu_topology_destroy(sys_pmu_topology);
    pmu_deinitialize();
    free(user_events_name);
    hwinfo_destroy(hwinfo);
    zsock_destroy(&ticker);
    zsys_shutdown();
    return ret;
}

