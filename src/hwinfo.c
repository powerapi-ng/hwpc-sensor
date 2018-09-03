#include <stdio.h>
#include <string.h>

#include "hwinfo.h"
#include "util.h"

static struct hwinfo_pkg *
hwinfo_pkg_create()
{
    struct hwinfo_pkg *pkg = malloc(sizeof(struct hwinfo_pkg));

    if (!pkg)
        return NULL;

    pkg->cpus_id = zlistx_new();
    zlistx_set_duplicator(pkg->cpus_id, (zlistx_duplicator_fn *) uintptrdup);
    zlistx_set_destructor(pkg->cpus_id, (zlistx_destructor_fn *) ptrfree);

    return pkg;
}

static struct hwinfo_pkg *
hwinfo_pkg_dup(struct hwinfo_pkg *pkg)
{
    struct hwinfo_pkg *pkgcpy = malloc(sizeof(struct hwinfo_pkg));

    if (!pkgcpy)
        return NULL;

    pkgcpy->cpus_id = zlistx_dup(pkg->cpus_id);

    return pkgcpy;
}

static void
hwinfo_pkg_destroy(struct hwinfo_pkg **pkg_ptr)
{
    struct hwinfo_pkg *pkg = *pkg_ptr;

    if (!pkg)
        return;

    zlistx_destroy(&pkg->cpus_id);
    free(pkg);
    *pkg_ptr = NULL;
}

static int
read_id_from_file(char *file_path)
{
    FILE *f = NULL;
    int id = -1;

    if ((f = fopen(file_path, "r")) != NULL) {
        if (fscanf(f, "%d", &id) != 1) {
            id = -1;
        }
    }

    fclose(f);
    return id;
}

static int
do_packages_detection(struct hwinfo *hwinfo)
{
    int ret = -1;
    char *sysfs_cpu_path = "/sys/devices/system/cpu";
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    char path_pkg_id[1024] = {0};
    int cpu_id;
    int package_id;
    struct hwinfo_pkg *pkg = NULL;

    if ((dir = opendir(sysfs_cpu_path)) == NULL) {
        zsys_error("hwinfo: failed to open sysfs cpu directory");
        return ret;
    }

    for (entry = readdir(dir); entry; entry = readdir(dir), cpu_id = -1) {
        /* only pick visible directories */
        if ((entry->d_type & DT_DIR) && (entry->d_name[0] != '.')) {
            /* extract cpu id from directory name */
            sscanf(entry->d_name, "cpu%d", &cpu_id);
            if (cpu_id >= 0) {
                /* extract package id of the cpu */
                snprintf(path_pkg_id, sizeof(path_pkg_id), "%s/%s/topology/physical_package_id", sysfs_cpu_path, entry->d_name);
                package_id = read_id_from_file(path_pkg_id);
                if (package_id >= 0) {
                    /* get cpu pkg, create it if never encountered */
                    pkg = zhashx_lookup(hwinfo->pkgs, &package_id);
                    if (!pkg) {
                        pkg = hwinfo_pkg_create();
                        if (!pkg) {
                            zsys_error("hwinfo: failed to allocate package info struct");
                            goto cleanup;
                        }

                        /* store package info */
                        zhashx_insert(hwinfo->pkgs, &package_id, pkg);
                    }

                    /* add cpu to its package */
                    zlistx_add_end(pkg->cpus_id, &cpu_id);

                    /* add cpu to available cpu list */
                    zlistx_add_end(hwinfo->available_cpus_id, &cpu_id);
                }
            }
        }
    }

    /* set a duplicator to the hash table */
    zhashx_set_duplicator(hwinfo->pkgs, (zhashx_duplicator_fn *) hwinfo_pkg_dup);

    ret = 0;

cleanup:
    closedir(dir);
    return ret;
}

int
hwinfo_detect(struct hwinfo *hwinfo)
{
    if (do_packages_detection(hwinfo)) {
        return -1;
    }

    return 0;
}

struct hwinfo *
hwinfo_create()
{
    struct hwinfo *hw = malloc(sizeof(struct hwinfo));

    if (!hw)
        return NULL;

    hw->available_cpus_id = zlistx_new();
    zlistx_set_duplicator(hw->available_cpus_id, (zlistx_duplicator_fn *) uintptrdup);
    zlistx_set_destructor(hw->available_cpus_id, (zlistx_destructor_fn *) ptrfree);

    hw->pkgs = zhashx_new();
    zhashx_set_key_duplicator(hw->pkgs, (zhashx_duplicator_fn *) uintptrdup);
    zhashx_set_key_comparator(hw->pkgs, (zhashx_comparator_fn *) uintptrcmp);
    zhashx_set_key_destructor(hw->pkgs, (zhashx_destructor_fn *) ptrfree);
    zhashx_set_destructor(hw->pkgs, (zlistx_destructor_fn *) hwinfo_pkg_destroy);

    return hw;
}

struct hwinfo *
hwinfo_dup(struct hwinfo *hwinfo)
{
    struct hwinfo *hwinfocpy = malloc(sizeof(struct hwinfo));

    if (!hwinfocpy)
        return NULL;

    hwinfocpy->available_cpus_id = zlistx_dup(hwinfo->available_cpus_id);
    hwinfocpy->pkgs = zhashx_dup(hwinfo->pkgs);

    return hwinfocpy;
}

void
hwinfo_destroy(struct hwinfo *hwinfo)
{
    if (!hwinfo)
        return;

    zlistx_destroy(&hwinfo->available_cpus_id);
    zhashx_destroy(&hwinfo->pkgs);
    free(hwinfo);
}

