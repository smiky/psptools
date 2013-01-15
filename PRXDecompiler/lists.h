/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#ifndef __LISTS_H
#define __LISTS_H

#include <stddef.h>

struct _element;
typedef struct _element *element;

struct _list;
typedef struct _list *list;

struct _listpool;
typedef struct _listpool *listpool;

listpool listpool_create (size_t numelms, size_t numlsts);
void listpool_destroy (listpool pool);

list list_alloc (listpool pool);
void list_free (list l);
void list_reset (list l);
int  list_size (list l);

element list_head (list l);
element list_tail (list l);

void *list_headvalue (list l);
void *list_tailvalue (list l);

element list_inserthead (list l, void *val);
element list_inserttail (list l, void *val);

void *list_removehead (list l);
void *list_removetail (list l);

void *element_getvalue (element el);
void element_setvalue (element el, void *val);

void element_insertbefore (element el, element inserted);
void element_insertafter (element el, element inserted);

element element_next (element el);
element element_previous (element el);

element element_alloc (listpool pool, void *val);
void element_remove (element el);
void *element_free (element el);


#endif /* __LISTS_H */
