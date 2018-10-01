/*
 * Copyright 2018 University of Lille
 * Copyright 2018 INRIA
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "docker.h"

char *
docker_get_container_name_from_id(const char *full_id)
{
    char config_path[DOCKER_CONFIG_PATH_BUFFER_SIZE];
    int r;
    FILE *stream = NULL;
    char *line = NULL;
    size_t len = 0;
    regex_t re = {0};
    const char *expr = "\"Name\":\"/([a-zA-Z0-9][a-zA-Z0-9_.-]+)\"";
    const size_t num_matches = 2;
    regmatch_t matches[num_matches];
    char *container_name = NULL;

    r = snprintf(config_path, DOCKER_CONFIG_PATH_BUFFER_SIZE, "/var/lib/docker/containers/%s/config.v2.json", full_id);
    if (r < 0 || r > DOCKER_CONFIG_PATH_BUFFER_SIZE)
        return NULL;

    stream = fopen(config_path, "r");
    if (!stream)
        return NULL;

    if (getline(&line, &len, stream) != -1) {
        if (!regcomp(&re, expr, REG_EXTENDED | REG_NEWLINE)) {
            if (!regexec(&re, line, num_matches, matches, 0)) {
                container_name = strndup(line + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
            }
            regfree(&re);
        }
        free(line);
    }

    fclose(stream);
    return container_name;
}

