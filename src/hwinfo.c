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

static int
get_cpu_online_status(const char *cpu_dir)
{
    int status = 1;
    char path[256];
    FILE *f = NULL;
    char buffer[2]; /* boolean expected */

    snprintf(path, sizeof(path), "%s/%s/online", SYSFS_CPU_PATH, cpu_dir);

    f = fopen(path, "r");
    if (f) {
	if (fgets(buffer, sizeof(buffer), f)) {
	    if (buffer[0] == '0')
		status = 0;
	}
	fclose(f);
    }

    /*
     * Certain systems cannot disable some CPUs and the "online" file will not be available.
     * In this case, we report the cpu as online.
     */
    return status;
}

static char *
get_package_id(const char *cpu_dir)
{
    FILE *f = NULL;
    char path[256];
    char buffer[24]; /* log10(ULLONG_MAX) */
    char *id = NULL;

    snprintf(path, sizeof(path), "%s/%s/topology/physical_package_id", SYSFS_CPU_PATH, cpu_dir);

    f = fopen(path, "r");
    if (f) {
        if (fgets(buffer, sizeof(buffer), f)) {
            id = strndup(buffer, sizeof(buffer));
            /* Removes trailing `\n` character (which would break csv output) :*/
            id[strcspn(id, "\n")] = 0;
        }
        fclose(f);
    }

    return id;
}

static char *
parse_cpu_id_from_name(const char *str)
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
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    int cpu_online;
    char *cpu_id = NULL;
    char *pkg_id = NULL;
    struct hwinfo_pkg *pkg = NULL;

    dir = opendir(SYSFS_CPU_PATH);
    if (!dir) {
        zsys_error("hwinfo: failed to open sysfs cpu directory");
        return ret;
    }

    /* extract information from online cpus */
    for (entry = readdir(dir); entry; entry = readdir(dir)) {
	if ((entry->d_type & DT_LNK) && (entry->d_name[0] != '.')) {
	    cpu_online = get_cpu_online_status(entry->d_name);
	    if (!cpu_online) {
		zsys_info("hwinfo: %s is offline and will be ignored", entry->d_name);
		continue;
	    }

            cpu_id = parse_cpu_id_from_name(entry->d_name);
            if (!cpu_id) {
                zsys_error("hwinfo: failed to parse cpu id for %s", entry->d_name);
                goto cleanup;
            }

            pkg_id = get_package_id(entry->d_name);
            if (!pkg_id) {
                zsys_error("hwinfo: failed to parse package id for %s", entry->d_name);
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

            zsys_info("hwinfo: found cpu  '%s' id : '%s' for pkg '%s' ", entry->d_name, cpu_id, pkg_id);
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

