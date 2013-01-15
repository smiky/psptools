/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stdlib.h>

#include "lists.h"
#include "alloc.h"
#include "utils.h"

struct _element {
  struct _listpool *const pool;

  struct _list *lst;
  struct _element *next;
  struct _element *prev;

  void *value;
};

struct _list {
  struct _listpool *const pool;
  struct _element *head;
  struct _element *tail;
  int size;
};

struct _listpool {
  fixedpool lstpool;
  fixedpool elmpool;
};

listpool listpool_create (size_t numelms, size_t numlsts)
{
  listpool result = (listpool) xmalloc (sizeof (struct _listpool));
  result->elmpool = fixedpool_create (sizeof (struct _element), numelms, 0);
  result->lstpool = fixedpool_create (sizeof (struct _list), numlsts, 0);
  return result;
}

void listpool_destroy (listpool pool)
{
  fixedpool_destroy (pool->lstpool, NULL, NULL);
  fixedpool_destroy (pool->elmpool, NULL, NULL);
  free (pool);
}


list list_alloc (listpool pool)
{
  list l;
  listpool *ptr;
  l = fixedpool_alloc (pool->lstpool);

  ptr = (listpool *) &l->pool;
  *ptr = pool;
  l->head = l->tail = NULL;
  l->size = 0;
  return l;
}

void list_free (list l)
{
  listpool pool;
  list_reset (l);
  pool = l->pool;
  fixedpool_free (pool->lstpool, l);
}

void list_reset (list l)
{
  element el, ne;
  for (el = l->head; el; el = ne) {
    ne = el->next;
    element_free (el);
  }
  l->head = l->tail = NULL;
  l->size = 0;
}

int list_size (list l)
{
  return l->size;
}

element list_head (list l)
{
  return l->head;
}

void *list_headvalue (list l)
{
  if (list_head (l))
    return element_getvalue (list_head (l));
  return NULL;
}

element list_tail (list l)
{
  return l->tail;
}

void *list_tailvalue (list l)
{
  if (list_tail (l))
    return element_getvalue (list_tail (l));
  return NULL;
}

element list_inserthead (list l, void *val)
{
  element el = element_alloc (l->pool, val);
  if (l->size == 0) {
    el->lst = l;
    l->size++;
    l->head = l->tail = el;
  } else {
    element_insertbefore (l->head, el);
  }
  return el;
}

element list_inserttail (list l, void *val)
{
  element el = element_alloc (l->pool, val);
  if (l->size == 0) {
    el->lst = l;
    l->size++;
    l->head = l->tail = el;
  } else {
    element_insertafter (l->tail, el);
  }
  return el;
}

void *list_removehead (list l)
{
  element el;
  el = list_head (l);
  if (!el) return NULL;
  return element_free (el);
}

void *list_removetail (list l)
{
  element el;
  el = list_tail (l);
  if (!el) return NULL;
  return element_free (el);
}



element element_alloc (listpool pool, void *val)
{
  element el;
  listpool *ptr;
  el = fixedpool_alloc (pool->elmpool);

  ptr = (listpool *) &el->pool;
  *ptr = pool;
  el->prev = NULL;
  el->next = NULL;
  el->value = val;
  el->lst = NULL;

  return el;
}

void element_remove (element el)
{
  list l = el->lst;
  if (l) l->size--;

  if (!el->next) {
    if (l) l->tail = el->prev;
  } else {
    el->next->prev = el->prev;
  }

  if (!el->prev) {
    if (l) l->head = el->next;
  } else {
    el->prev->next = el->next;
  }
  el->next = el->prev = NULL;
  el->lst = NULL;
}

void *element_free (element el)
{
  listpool pool;
  void *val;
  val = element_getvalue (el);
  pool = el->pool;
  element_remove (el);
  fixedpool_free (pool->elmpool, el);
  return val;
}

void *element_getvalue (element el)
{
  return el->value;
}

void element_setvalue (element el, void *val)
{
  el->value = val;
}

element element_next (element el)
{
  return el->next;
}

element element_previous (element el)
{
  return el->prev;
}


void element_insertbefore (element el, element inserted)
{
  inserted->lst = el->lst;
  inserted->prev = el->prev;
  inserted->next = el;
  if (el->prev) el->prev->next = inserted;
  el->prev = inserted;

  if (inserted->lst) {
    inserted->lst->size++;
    if (!inserted->prev) inserted->lst->head = inserted;
  }
}

void element_insertafter (element el, element inserted)
{
  inserted->lst = el->lst;
  inserted->next = el->next;
  inserted->prev = el;
  if (el->next) el->next->prev = inserted;
  el->next = inserted;

  if (inserted->lst) {
    inserted->lst->size++;
    if (!inserted->next) inserted->lst->tail = inserted;
  }
}
