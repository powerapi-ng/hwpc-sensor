#ifndef HWINFO_H
#define HWINFO_H

#include <stddef.h>
#include <czmq.h>

/*
 * hwinfo_pkg stores information about the package.
 */
struct hwinfo_pkg
{
    zlistx_t *cpus_id; /* all cpus id as (unsigned int *) */
};

/*
 * hwinfo stores information about the machine hardware.
 */
struct hwinfo
{
    zhashx_t *pkgs; /* packages information as (unsigned int *, struct hwinfo_pkg *) */
    zlistx_t *available_cpus_id; /* all available cpus id as (unsigned int *) */
};

/*
 * hwinfo_create allocate the needed ressources.
 */
struct hwinfo *hwinfo_create();

/*
 * hwinfo_detect discover and store the machine hardware topology.
 */
int hwinfo_detect(struct hwinfo *hwinfo);

/*
* hwinfo_dup duplicate the hwinfo struct and its members.
 */
struct hwinfo *hwinfo_dup(struct hwinfo *hwinfo);

/*
 * hwinfo_destroy free the allocated memory to store the machine topology.
 */
void hwinfo_destroy(struct hwinfo *hwinfo);

#endif /* HWINFO_H */

