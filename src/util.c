#include <stdlib.h>

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

unsigned int *
uintdup(unsigned int val)
{
    unsigned int *res = malloc(sizeof(unsigned int));

    if (!res)
        return NULL;

    *res = val;

    return res;
}

unsigned int *
uintptrdup(const unsigned int *ptr)
{
    if (!ptr)
        return NULL;

    return uintdup(*ptr);
}

int
uintcmp(unsigned int a, unsigned int b)
{
    return (a < b) ? -1 : (a > b);
}

int
uintptrcmp(const unsigned int *a, const unsigned int *b)
{
    return uintcmp((a) ? *a : 0, (b) ? *b : 0);
}

void
ptrfree(void **ptr)
{
    free(*ptr);
    *ptr = NULL;
}
