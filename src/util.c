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
#include <stdint.h>

#include "util.h"

int *
intdup(int val)
{
    int *res = malloc(sizeof(int));

    if (!res)
        return NULL;

    *res = val;

    return res;
}

int *
intptrdup(const int *ptr)
{
    if (!ptr)
        return NULL;

    return intdup(*ptr);
}

int
intcmp(int a, int b)
{
    return (a < b) ? -1 : (a > b);
}

int
intptrcmp(const int *a, const int *b)
{
    return intcmp((a) ? *a : 0, (b) ? *b : 0);
}

uint64_t *
uint64dup(uint64_t val)
{
    uint64_t *res = malloc(sizeof(uint64_t));

    if (!res)
        return NULL;

    *res = val;

    return res;
}

uint64_t *
uint64ptrdup(const uint64_t *ptr)
{
    if (!ptr)
        return NULL;

    return uint64dup(*ptr);
}

int
uint64cmp(uint64_t a, uint64_t b)
{
    return (a < b) ? -1 : (a > b);
}

int
uint64ptrcmp(const uint64_t *a, const uint64_t *b)
{
    return uint64cmp((a) ? *a : 0, (b) ? *b : 0);
}

void
ptrfree(void **ptr)
{
    free(*ptr);
    *ptr = NULL;
}
