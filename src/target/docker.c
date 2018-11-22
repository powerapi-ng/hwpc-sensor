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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "docker.h"
#include "target.h"

int
target_docker_detect(const char *cgroup_path)
{
    regex_t re = {0};
    int is_docker_target = 0;

    if (!cgroup_path)
        return 0;

    if (!regcomp(&re, TARGET_DOCKER_CGROUP_PATH_EXPECTED_REGEX, REG_EXTENDED | REG_NOSUB)) {
        if (!regexec(&re, cgroup_path, 0, NULL, 0))
            is_docker_target = 1;

        regfree(&re);
    }

    return is_docker_target;
}

char *
target_docker_resolve_name(struct target *target)
{
	const char *container_id = strrchr(target->cgroup_path, '/') + 1;
	char config_path[TARGET_DOCKER_CONFIG_PATH_BUFFER_SIZE];
	int res;
	FILE *json_file = NULL;
	char *json = NULL;
	size_t json_len = 0;
	regex_t re;
	const size_t num_matches = 2;
	regmatch_t matches[num_matches];
	char *target_name = NULL;

	res = snprintf(config_path, TARGET_DOCKER_CONFIG_PATH_BUFFER_SIZE, "/var/lib/docker/containers/%s/config.v2.json", container_id);
	if (res < 0 || res > TARGET_DOCKER_CONFIG_PATH_BUFFER_SIZE)
		return NULL;

	json_file = fopen(config_path, "r");
	if (!json_file)
		return NULL;

	if (getline(&json, &json_len, json_file) != -1) {
		if (!regcomp(&re, TARGET_DOCKER_CONFIG_EXTRACT_NAME_REGEX, REG_EXTENDED | REG_NEWLINE)) {
			if (!regexec(&re, json, num_matches, matches, 0)) {
				target_name = strndup(json + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
			}
			regfree(&re);
		}
		free(json);
	}

	fclose(json_file);
	return target_name;
}

void
target_docker_destroy(struct target *target)
{
    if (!target)
        return;

    free(target->cgroup_path);
    free(target);
}

struct target *
target_docker_create(char *cgroup_path)
{
    struct target *target = malloc(sizeof(struct target));

    if (!target)
        return NULL;

    target->type = TARGET_TYPE_DOCKER;
    target->cgroup_path = cgroup_path;
    target->resolve_name = target_docker_resolve_name;
    target->destroy = target_docker_destroy;

    return target;
}

