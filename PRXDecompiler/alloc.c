/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stdlib.h>
#include <string.h>
#include "alloc.h"
#include "utils.h"

struct _link {
  struct _link *next;
  size_t size;
};

struct _fixedpool {
  struct _link *allocated;
  struct _link *nextfree;
  size_t size;
  size_t grownum;
  size_t bytes;
  int setzero;
};


fixedpool fixedpool_create (size_t size, size_t grownum, int setzero)
{
  fixedpool p = (fixedpool) xmalloc (sizeof (struct _fixedpool));

  if (size < sizeof (struct _link)) size = sizeof (struct _link);
  if (grownum < 2) grownum = 2;

  p->size = size;
  p->grownum = grownum;
  p->allocated = NULL;
  p->nextfree = NULL;
  p->bytes = 0;
  p->setzero = setzero;
  return p;
}

void fixedpool_destroy (fixedpool p, pooltraversefn destroyfn, void *arg)
{
  struct _link *ptr, *nptr;
  char *c;
  size_t count;

  for (ptr = p->allocated; ptr; ptr = nptr) {
    if (destroyfn) {
      c = (char *) ptr;
      c += p->size;
      count = 2 * p->size;

      while (count <= ptr->size) {
        destroyfn (c, arg);
        c += p->size;
        count += p->size;
      }
    }

    nptr = ptr->next;
    free (ptr);
  }

  p->allocated = NULL;
  p->nextfree = NULL;
  free (p);
}

void fixedpool_grow (fixedpool p, void *ptr, size_t ptrsize)
{
  char *c;
  struct _link *l;
  size_t count;

  if (ptrsize < 2 * p->size) {
    free (ptr);
    return;
  }

  l = ptr;
  l->next = p->allocated;
  l->size = ptrsize;
  p->bytes += ptrsize;
  p->allocated = l;

  c = ptr;
  c += p->size;
  count = 2 * p->size;

  while (count <= ptrsize) {
    l = (struct _link *) c;
    l->next = p->nextfree;
    p->nextfree = l;
    c += p->size;
    count += p->size;
  }
}

void *fixedpool_alloc (fixedpool p)
{
  struct _link *l;
  if (!p->nextfree) {
    size_t size;
    void *ptr;

    size = p->grownum * p->size;
    ptr = xmalloc (size);
    fixedpool_grow (p, ptr, size);
  }
  l = p->nextfree;
  p->nextfree = l->next;
  if (p->setzero)
    memset (l, 0, p->size);
  return (void *) l;
}

void fixedpool_free (fixedpool p, void *ptr)
{
  struct _link *l = ptr;
  l->next = p->nextfree;
  p->nextfree = l;
}


