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

#endif /* UTIL_H */

