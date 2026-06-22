/*
 *  Copyright (c) 2026, Inria
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

#include <sys/resource.h>
#include <errno.h>
#include <czmq.h>

#include "rlimits.h"

int
rlimits_initialize(void)
{
    struct rlimit nofile_limit;
    rlim_t old_nofile_soft_limit;

    if (getrlimit(RLIMIT_NOFILE, &nofile_limit)) {
        zsys_error("rlimits: Failed to get current open files (nofile) limits: %s", strerror(errno));
        return -1;
    }

    zsys_info("rlimits: Current open files (nofile) limits: soft=%lu hard=%lu", nofile_limit.rlim_cur, nofile_limit.rlim_max);

    if (nofile_limit.rlim_cur == nofile_limit.rlim_max)
        return 0;

    old_nofile_soft_limit = nofile_limit.rlim_cur;
    nofile_limit.rlim_cur = nofile_limit.rlim_max;
    if (setrlimit(RLIMIT_NOFILE, &nofile_limit)) {
        zsys_error("rlimits: Failed to raise open files (nofile) soft limit: %s", strerror(errno));
        return -1;
    }

    zsys_info("rlimits: Raised open files (nofile) soft limit from %lu to %lu", old_nofile_soft_limit, nofile_limit.rlim_max);
    return 0;
}
