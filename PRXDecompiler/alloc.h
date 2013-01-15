/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#ifndef __ALLOC_H
#define __ALLOC_H

#include <stddef.h>

struct _fixedpool;
typedef struct _fixedpool *fixedpool;

typedef void (*pooltraversefn) (void *ptr, void *arg);

fixedpool fixedpool_create (size_t size, size_t grownum, int setzero);
void fixedpool_destroy (fixedpool p, pooltraversefn destroyfn, void *arg);

void fixedpool_grow (fixedpool p, void *ptr, size_t ptrsize);
void *fixedpool_alloc (fixedpool p);
void fixedpool_free (fixedpool p, void *ptr);

#endif /* __ALLOC_H */
