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

#ifndef CONFIG_H
#define CONFIG_H

#include <czmq.h>
#include <limits.h>
#include <netdb.h>

#include "events.h"
#include "storage.h"

/*
 * config_sensor stores sensor specific config.
 */
struct config_sensor
{
    unsigned int verbose;
    unsigned int frequency;
    char cgroup_basepath[PATH_MAX];
    char name[HOST_NAME_MAX];
};

/*
 * config_storage stores storage specific config.
 */
struct config_storage
{
    enum storage_type type;
    union {
        struct {
            char outdir[PATH_MAX];
        } csv;

        struct {
            char hostname[HOST_NAME_MAX];
            char port[NI_MAXSERV];
        } socket;

        #ifdef HAVE_MONGODB
        struct {
            char uri[PATH_MAX];
            char database[NAME_MAX];
            char collection[NAME_MAX];
        } mongodb;
        #endif
    };
};

/*
 * config_events stores events specific config.
 */
struct config_events
{
    zhashx_t *system; /* char *group_name -> struct events_group *group */
    zhashx_t *containers; /* char *group_name -> struct events_group *group */
};

/*
 * config stores the application configuration.
 */
struct config
{
    struct config_sensor sensor;
    struct config_storage storage;
    struct config_events events;
};

/*
 * config_create allocate the required resources and setup the default config.
 */
struct config *config_create(void);

/*
 * config_validate check the validity of the given config.
 */
int config_validate(struct config *config);

/*
 * config_destroy free the allocated memory for the storage of the global config.
 */
void config_destroy(struct config *config);

#endif /* CONFIG_H */
