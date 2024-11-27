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

#include <stdlib.h>
#include <regex.h>

#include "target.h"
#include "target_kubernetes.h"

/*
 * CONTAINER_ID_REGEX is the regex used to extract the Docker container id from a cgroup path.
 * CONTAINER_ID_REGEX_EXPECTED_MATCHES is the number of matches expected from the regex. (num groups + 1)
 */
#define CONTAINER_ID_REGEX \
    "perf_event/kubepods.slice/" \
    "kubepods-(besteffort|burstable|).slice/" \
    "kubepods-(besteffort|burstable|)-(pod[a-f0-9][a-f0-9_-]+).slice/" /* Pod ID */ \
    "cri-containerd-([a-f0-9]{64}).scope" /* Container ID */

#define CONTAINER_ID_REGEX_EXPECTED_MATCHES 5
#define CONTAINER_SHORT_ID_LENGTH 14
#define POD_ID_LENGTH 16


int
target_kubernetes_validate(const char *cgroup_path)
{
    regex_t re;
    bool is_kubernetes_target = false;

    if (!cgroup_path)
        return false;

    if (!regcomp(&re, CONTAINER_ID_REGEX, REG_EXTENDED | REG_NEWLINE | REG_NOSUB)) {
        if (!regexec(&re, cgroup_path, 0, NULL, 0))
            is_kubernetes_target = true;

        regfree(&re);
    }

    return is_kubernetes_target;
}

char *
target_kubernetes_container_id(char *cgroup_path)
{
     regex_t re;
    regmatch_t matches[CONTAINER_ID_REGEX_EXPECTED_MATCHES];
    char *container_id = NULL;
    char *target_name;
    target_name = (char *) malloc ((CONTAINER_SHORT_ID_LENGTH + 1) * sizeof (char));
    if (!regcomp(&re, CONTAINER_ID_REGEX, REG_EXTENDED | REG_NEWLINE)) {
        if (!regexec(&re, cgroup_path, CONTAINER_ID_REGEX_EXPECTED_MATCHES, matches, 0)) {
            container_id = cgroup_path + matches[4].rm_so;
            strncpy(target_name, container_id, CONTAINER_SHORT_ID_LENGTH);
            target_name[CONTAINER_SHORT_ID_LENGTH] = '\0';
        }
        regfree(&re);
    }

    return target_name;
}

char *
target_kubernetes_pod_id(char *cgroup_path)
{
    regex_t re;
    regmatch_t matches[CONTAINER_ID_REGEX_EXPECTED_MATCHES];
    char *pod_id = NULL;
    char *target_name;
    target_name = (char *) malloc ((POD_ID_LENGTH + 1) * sizeof (char));
    if (!regcomp(&re, CONTAINER_ID_REGEX, REG_EXTENDED | REG_NEWLINE)) {
        if (!regexec(&re, cgroup_path, CONTAINER_ID_REGEX_EXPECTED_MATCHES, matches, 0)) {
            pod_id = cgroup_path + matches[3].rm_so;
           strncpy(target_name,&(pod_id[3]),POD_ID_LENGTH - 3);
           target_name[POD_ID_LENGTH - 3] = '\0';
        }
        regfree(&re);
    }
    return target_name;
}

char *
target_kubernetes_global_id(char *pod_id, char *container_id)
{
    char *target_name;
    char * tmp;
    tmp = (char *) malloc (((POD_ID_LENGTH - 3) + CONTAINER_SHORT_ID_LENGTH + 1) * sizeof (char));
    target_name = (char *) malloc (((POD_ID_LENGTH - 3) + CONTAINER_SHORT_ID_LENGTH + 1) * sizeof (char));
    strncpy(tmp,pod_id,POD_ID_LENGTH - 3);
    tmp[POD_ID_LENGTH - 4] = '-';
    tmp[POD_ID_LENGTH - 3] = '\0';
    strncat(tmp,container_id,CONTAINER_SHORT_ID_LENGTH);
    tmp[CONTAINER_SHORT_ID_LENGTH + (POD_ID_LENGTH - 3)] = '\0';
    strncpy(target_name, tmp,CONTAINER_SHORT_ID_LENGTH + (POD_ID_LENGTH - 3));
    target_name[CONTAINER_SHORT_ID_LENGTH + (POD_ID_LENGTH - 3)] = '\0';
    free(tmp);
    return target_name;
}