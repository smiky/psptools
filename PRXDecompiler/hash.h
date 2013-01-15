/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#ifndef __HASH_H
#define __HASH_H

#include <stddef.h>

struct _hashtable;
typedef struct _hashtable *hashtable;

struct _hashpool;
typedef struct _hashpool *hashpool;

typedef unsigned int (*hashfn) (void *key);
typedef int (*hashequalsfn) (void *key1, void *key2, unsigned int hash);
typedef void (*hashtraversefn) (void *key, void *value, unsigned int hash, void *arg);

hashpool hashpool_create (size_t numtables, size_t numentries);
void hashpool_destroy (hashpool pool);

hashtable hashtable_alloc (hashpool pool, unsigned int size, hashfn hashfn, hashequalsfn eqfn);
void hashtable_free (hashtable ht, hashtraversefn destroyfn, void *arg);

unsigned int hashtable_count (hashtable ht);

void hashtable_insert (hashtable ht, void *key, void *value);
void hashtable_inserthash (hashtable ht, void *key, void *value, unsigned int hash);
void *hashtable_search (hashtable ht, void *key, void **key_found);
void *hashtable_searchhash (hashtable ht, void *key, void **key_found, unsigned int hash);
int hashtable_haskey (hashtable ht, void *key, void **key_found);
int hashtable_haskeyhash (hashtable ht, void *key, void **key_found, unsigned int hash);
void *hashtable_remove (hashtable ht, void *key, void **key_found);
void *hashtable_removehash (hashtable ht, void *key, void **key_found, unsigned int hash);

void hashtable_traverse (hashtable ht, hashtraversefn traversefn, void *arg);

int hashtable_string_compare (void *key1, void *key2, unsigned int hash);
int hashtable_pointer_compare (void *key1, void *key2, unsigned int hash);

unsigned int hashtable_hash_bytes (unsigned char *key, size_t len);
unsigned int hashtable_hash_string (void *key);

#endif /* __HASH_H */
