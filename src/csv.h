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

#ifndef CSV_H
#define CSV_H

#include <czmq.h>

/*
 * CSV_PATH_BUFFER_SIZE stores the maximum length of a output dir path.
 */
#define CSV_PATH_BUFFER_SIZE 256

/*
 * CSV_LINE_BUFFER_SIZE stores the maximum length of a line in a group csv output file.
 */
#define CSV_LINE_BUFFER_SIZE 512

/*
 * csv_config stores the required information for the module.
 */
struct csv_config
{
    const char *sensor_name;
    const char *output_dir;
};

/*
 * csv_context stores the context of the module.
 */
struct csv_context
{
    struct csv_config config;
    zhashx_t *groups_fd; /* char *group_name -> FILE *fd */
    zhashx_t *groups_events; /* char *group_name -> zlistx_t *group_events */
};

/*
 * csv_create creates and configure a csv storage module..
 */
struct storage_module *csv_create(const char *sensor_name, const char *output_dir);

#endif /* CSV_H */

