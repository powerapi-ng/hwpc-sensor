/*
 *  Copyright (c) 2021, INRIA
 *  Copyright (c) 2021, University of Lille
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

#ifndef STORAGE_SOCKET_H
#define STORAGE_SOCKET_H

#include "storage.h"
#include "config.h"

/*
 * PORT_STR_BUFFER_SIZE stores the maximum length of the buffer used to convert the
 * port stored as uint64_t to a null terminated string.
 * Typically, the port is between the 0-65535 range.
 */
#define PORT_STR_BUFFER_SIZE 8

/*
 * MAX_DURATION_CONNECTION_RETRY stores the maximal value of a connection retry. (in seconds)
 */
#define MAX_DURATION_CONNECTION_RETRY 1800

/*
 * socket_config stores the required information for the module.
 */
struct socket_config
{
    const char *sensor_name;
    const char *address;
    int port;
};

/*
 * socket_context stores the context of the module.
 */
struct socket_context
{
    struct socket_config config;
    int socket_fd;
    time_t last_retry_time;
    time_t retry_backoff_time;
};

/*
 * storage_socket_create creates and configure a socket storage module.
 */
struct storage_module *storage_socket_create(struct config *config);

#endif /* STORAGE_SOCKET_H */
