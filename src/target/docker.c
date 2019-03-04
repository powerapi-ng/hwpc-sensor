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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "docker.h"
#include "target.h"

int
target_docker_validate(const char *cgroup_path)
{
    regex_t re;
    int is_docker_target = false;

    if (!cgroup_path)
        return false;

    if (!regcomp(&re, TARGET_DOCKER_EXTRACT_CONTAINER_ID_REGEX, REG_EXTENDED | REG_NEWLINE | REG_NOSUB)) {
        if (!regexec(&re, cgroup_path, 0, NULL, 0))
            is_docker_target = true;

        regfree(&re);
    }

    return is_docker_target;
}

static char *
build_container_config_path(const char *cgroup_path)
{
    regex_t re;
    const size_t num_matches = 2;
    regmatch_t matches[num_matches];
    int res;
    char config_path_buffer[TARGET_DOCKER_CONFIG_PATH_BUFFER_SIZE];
    char *config_path = NULL;

    if (!regcomp(&re, TARGET_DOCKER_EXTRACT_CONTAINER_ID_REGEX, REG_EXTENDED | REG_NEWLINE)) {
        if (!regexec(&re, cgroup_path, num_matches, matches, 0)) {
            res = snprintf(config_path_buffer, TARGET_DOCKER_CONFIG_PATH_BUFFER_SIZE,
                    "/var/lib/docker/containers/%.*s/config.v2.json",
                    matches[1].rm_eo - matches[1].rm_so, cgroup_path + matches[1].rm_so);

            if (res > 0 && res < TARGET_DOCKER_CONFIG_PATH_BUFFER_SIZE)
                config_path = strdup(config_path_buffer);
        }
        regfree(&re);
    }

    return config_path;
}

char *
target_docker_resolve_name(struct target *target)
{
    char *config_path = NULL;
    FILE *json_file = NULL;
    char *json = NULL;
    size_t json_len;
    regex_t re;
    const size_t num_matches = 2;
    regmatch_t matches[num_matches];
    char *target_name = NULL;

    config_path = build_container_config_path(target->cgroup_path);
    if (!config_path)
        return NULL;

    json_file = fopen(config_path, "r");
    if (json_file) {
        if (getline(&json, &json_len, json_file) != -1) {
            if (!regcomp(&re, TARGET_DOCKER_CONFIG_EXTRACT_NAME_REGEX, REG_EXTENDED | REG_NEWLINE)) {
                if (!regexec(&re, json, num_matches, matches, 0))
                    target_name = strndup(json + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);

                regfree(&re);
            }
            free(json);
        }
        fclose(json_file);
    }

    free(config_path);
    return target_name;
}

