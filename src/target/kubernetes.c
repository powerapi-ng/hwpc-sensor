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

#include <stdlib.h>
#include <regex.h>

#include "kubernetes.h"
#include "target.h"

int
target_kubernetes_detect(const char *cgroup_path)
{
    regex_t re = {0};
    int is_kubernetes_target = 0;

    if (!cgroup_path)
        return 0;

    if (!regcomp(&re, TARGET_KUBERNETES_EXTRACT_CONTAINER_ID_REGEX, REG_EXTENDED | REG_NOSUB)) {
        if (!regexec(&re, cgroup_path, 0, NULL, 0))
            is_kubernetes_target = 1;

        regfree(&re);
    }

    return is_kubernetes_target;
}

static char *
build_container_config_path(const char *cgroup_path)
{
    regex_t re;
    const size_t num_matches = 5;
    regmatch_t matches[num_matches];
    int res;
    char path_buffer[TARGET_KUBERNETES_CONFIG_PATH_BUFFER_SIZE];
    char *config_path = NULL;

    if (!regcomp(&re, TARGET_KUBERNETES_EXTRACT_CONTAINER_ID_REGEX, REG_EXTENDED | REG_NEWLINE)) {
        if (!regexec(&re, cgroup_path, num_matches, matches, 0)) {
            res = snprintf(path_buffer, TARGET_KUBERNETES_CONFIG_PATH_BUFFER_SIZE,
                    "/var/lib/docker/containers/%.*s/config.v2.json",
                    matches[3].rm_eo - matches[3].rm_so, cgroup_path + matches[3].rm_so);

            if (res > 0 || res < TARGET_KUBERNETES_CONFIG_PATH_BUFFER_SIZE)
                config_path = strdup(path_buffer);
        }
        regfree(&re);
    }

    return config_path;
}

char *
target_kubernetes_resolve_name(struct target *target)
{
    char *config_path = NULL;
    FILE *json_file = NULL;
    char *json = NULL;
    size_t json_len = 0;
    regex_t re = {0};
    const size_t num_matches = 2;
    regmatch_t matches[num_matches];
    char *target_name = NULL;

    config_path = build_container_config_path(target->cgroup_path);
    if (!config_path)
        return NULL;

    json_file = fopen(config_path, "r");
    if (json_file) {
        if (getline(&json, &json_len, json_file) != -1) {
            if (!regcomp(&re, TARGET_KUBERNETES_EXTRACT_CONTAINER_NAME_REGEX, REG_EXTENDED | REG_NEWLINE)) {
                if (!regexec(&re, json, num_matches, matches, 0)) {
                    target_name = strndup(json + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
                }
                regfree(&re);
            }
            free(json);
        }
        fclose(json_file);
    }

    free(config_path);
    return target_name;
}

void
target_kubernetes_destroy(struct target *target)
{
    if (!target)
        return;

    free(target->cgroup_path);
    free(target);
}

struct target *
target_kubernetes_create(char *cgroup_path)
{
    struct target *target = malloc(sizeof(struct target));

    if (!target)
        return NULL;

    target->type = TARGET_TYPE_KUBERNETES;
    target->cgroup_path = cgroup_path;
    target->resolve_name = target_kubernetes_resolve_name;
    target->destroy = target_kubernetes_destroy;

    return target;
}

