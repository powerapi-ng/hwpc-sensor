#ifndef UTIL_H
#define UTIL_H

/*
 * intdup returns a pointer to a new integer having the same value as val.
 */
int *intdup(int val);

/*
 * intptrdup returns a pointer to a new integer having the same value as the integer pointed by ptr.
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
 * uintdup returns a pointer to a new unsigned integer having the same value as val.
 */
unsigned int *uintdup(unsigned int val);

/*
 * uintptrdup returns a pointer to a new unsigned integer having the same value as the unsigned integer pointed by ptr.
 */
unsigned int *uintptrdup(const unsigned int *ptr);

/*
 * uintcmp compare two unsigned integer. (same behaviour as strcmp)
 */
int uintcmp(unsigned int a, unsigned int b);

/*
 * uintptrcmp compare the value of two pointers to unsigned integer. (same behaviour as strcmp)
 * Value having a NULL pointer is considered as 0.
 */
int uintptrcmp(const unsigned int *a, const unsigned int *b);

/*
 * ptrfree free the memory pointed by ptr and set ptr to NULL.
 */
void ptrfree(void **ptr);

#endif /* UTIL_H */

