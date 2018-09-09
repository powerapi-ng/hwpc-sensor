#include <stdio.h>
#include <string.h>
#include <regex.h>

#include "hwinfo.h"
#include "util.h"

static struct hwinfo_pkg *
hwinfo_pkg_create()
{
    struct hwinfo_pkg *pkg = malloc(sizeof(struct hwinfo_pkg));

    if (!pkg)
        return NULL;

    pkg->cpus_id = zlistx_new();
    zlistx_set_duplicator(pkg->cpus_id, (zlistx_duplicator_fn *) strdup);
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
    if (!*pkg_ptr)
        return;

    zlistx_destroy(&(*pkg_ptr)->cpus_id);
    free(*pkg_ptr);
    *pkg_ptr = NULL;
}

static char *
read_pkg_id_from_file(char *file_path)
{
    FILE *f = NULL;
    char buffer[24]; /* log10(ULLONG_MAX) */
    regex_t re = {0};
    const char *expr = "^([0-9]+)$";
    const size_t num_matches = 2; /* num groups + 1 */
    regmatch_t matches[num_matches];
    char *id = NULL;

    f = fopen(file_path, "r");
    if (f) {
        if (fgets(buffer, sizeof(buffer), f)) {
            if (!regcomp(&re, expr, REG_EXTENDED | REG_NEWLINE)) {
                if (!regexec(&re, buffer, num_matches, matches, 0)) {
                    id = strndup(buffer + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
                }
                regfree(&re);
            }
        }
        fclose(f);
    }

    return id;
}

static char *
parse_cpu_id_from_name(char *str)
{
    regex_t re = {0};
    const char *expr = "^cpu([0-9]+)$";
    const size_t num_matches = 2; /* num groups + 1 */
    regmatch_t matches[num_matches];
    char *id = NULL;

    if (!regcomp(&re, expr, REG_EXTENDED)) {
        if (!regexec(&re, str, num_matches, matches, 0)) {
            id = strndup(str + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
        }
        regfree(&re);
    }

    return id;
}

static int
do_packages_detection(struct hwinfo *hwinfo)
{
    int ret = -1;
    char *sysfs_cpu_path = "/sys/bus/cpu/devices";
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    char path_pkg_id[1024] = {0};
    char *cpu_id = NULL;
    char *pkg_id = NULL;
    struct hwinfo_pkg *pkg = NULL;

    dir = opendir(sysfs_cpu_path);
    if (!dir) {
        zsys_error("hwinfo: failed to open sysfs cpu directory");
        return ret;
    }

    /* extract information from online cpus */
    for (entry = readdir(dir); entry; entry = readdir(dir)) {
        if ((entry->d_type & DT_LNK) && (entry->d_name[0] != '.')) {
            /* extract cpu id from directory name */
            cpu_id = parse_cpu_id_from_name(entry->d_name);
            if (!cpu_id) {
                zsys_error("hwinfo: failed to parse cpu id of directory '%s'", entry->d_name);
                goto cleanup;
            }

            /* extract package id of the cpu */
            snprintf(path_pkg_id, sizeof(path_pkg_id), "%s/%s/topology/physical_package_id", sysfs_cpu_path, entry->d_name);
            pkg_id = read_pkg_id_from_file(path_pkg_id);
            if (!pkg_id) {
                zsys_error("hwinfo: failed to parse package id in '%s'", path_pkg_id);
                goto cleanup;
            }

            /* get cpu pkg or create it if never encountered */
            pkg = zhashx_lookup(hwinfo->pkgs, pkg_id);
            if (!pkg) {
                pkg = hwinfo_pkg_create();
                if (!pkg) {
                    zsys_error("hwinfo: failed to allocate package info struct");
                    goto cleanup;
                }

                zhashx_insert(hwinfo->pkgs, pkg_id, pkg);
                hwinfo_pkg_destroy(&pkg);
                pkg = zhashx_lookup(hwinfo->pkgs, pkg_id); /* get the copy the pkg done by zhashx_insert */
            }

            zlistx_add_end(pkg->cpus_id, cpu_id);

            free(cpu_id);
            cpu_id = NULL;
            free(pkg_id);
            pkg_id = NULL;
        }
    }

    ret = 0;

cleanup:
    free(cpu_id);
    free(pkg_id);
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

    hw->pkgs = zhashx_new();
    zhashx_set_duplicator(hw->pkgs, (zhashx_duplicator_fn *) hwinfo_pkg_dup);
    zhashx_set_destructor(hw->pkgs, (zlistx_destructor_fn *) hwinfo_pkg_destroy);

    return hw;
}

struct hwinfo *
hwinfo_dup(struct hwinfo *hwinfo)
{
    struct hwinfo *hwinfocpy = malloc(sizeof(struct hwinfo));

    if (!hwinfocpy)
        return NULL;

    hwinfocpy->pkgs = zhashx_dup(hwinfo->pkgs);

    return hwinfocpy;
}

void
hwinfo_destroy(struct hwinfo *hwinfo)
{
    if (!hwinfo)
        return;

    zhashx_destroy(&hwinfo->pkgs);
    free(hwinfo);
}

