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

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

/*
 * intdup returns a pointer to a new integer having the same value as val.
 */
int *intdup(int val);

/*
 * intptrdup returns a pointer to a new integer having the same value as the one pointed by ptr.
 */
int *intptrdup(const int *ptr);

/*
 * intcmp compare two integers. (same behaviour as strcmp)
 */
int intcmp(int a, int b);

/*
 * intptrcmp compare the value of two pointers to integer. (same behavious as strcmp)
 * Value having a NULL pointer is considered as 0.
 */
int intptrcmp(const int *a, const int *b);

/*
 * uint64dup returns a pointer to a new uint64_t having the same value as val.
 */
uint64_t *uint64dup(uint64_t val);

/*
 * uint64ptrdup returns a pointer to a new uint64_t having the same value as the one pointed by ptr.
 */
uint64_t *uint64ptrdup(const uint64_t *ptr);

/*
 * uint64cmp compare two uint64_t. (same behaviour as strcmp)
 */
int uint64cmp(uint64_t a, uint64_t b);

/*
 * uint64ptrcmp compare the value of two pointers to uint64_t. (same behaviour as strcmp)
 * Value having a NULL pointer is considered as 0.
 */
int uint64ptrcmp(const uint64_t *a, const uint64_t *b);

/*
 * ptrfree free the memory pointed by ptr and set ptr to NULL.
 */
void ptrfree(void **ptr);

/*
 * str_to_uint safely converts a string to an unsigned int.
 */
int str_to_uint(const char *str, unsigned int *out);

/*
 * str_to_int safely converts a string to an integer.
 */
int str_to_int(const char *str, int *out);

#endif /* UTIL_H */
