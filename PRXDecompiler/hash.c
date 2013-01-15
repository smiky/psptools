/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hash.h"
#include "alloc.h"
#include "utils.h"
#include "types.h"

#define INDEX_FOR(hash, size) ((hash) & ((size) - 1))

struct _entry {
  void *key, *value;
  unsigned int hash;
  struct _entry *next;
};

typedef struct _entry *entry;

struct _hashtable {
  struct _hashpool *const pool;
  unsigned int tablelength;
  unsigned int entrycount;
  unsigned int loadlimit;
  struct _entry **table;
  hashfn hashfn;
  hashequalsfn eqfn;
};

struct _hashpool {
  fixedpool tablepool;
  fixedpool entrypool;
};


hashpool hashpool_create (size_t numtables, size_t numentries)
{
  hashpool pool = (hashpool) xmalloc (sizeof (struct _hashpool));
  pool->tablepool = fixedpool_create (sizeof (struct _hashtable), numtables, 0);
  pool->entrypool = fixedpool_create (sizeof (struct _entry), numentries, 0);
  return pool;
}

static
void destroy_hashtable (void *ptr, void *arg)
{
  hashtable ht = ptr;
  if (ht->table) {
    free (ht->table);
    ht->table = NULL;
  }
}

void hashpool_destroy (hashpool pool)
{
  fixedpool_destroy (pool->tablepool, &destroy_hashtable, NULL);
  fixedpool_destroy (pool->entrypool, NULL, NULL);
  free (pool);
}

static
entry alloc_entry (hashpool pool)
{
  return fixedpool_alloc (pool->entrypool);
}

static
void free_entry (hashpool pool, entry e)
{
  fixedpool_free (pool->entrypool, e);
}


hashtable hashtable_alloc (hashpool pool, unsigned int size, hashfn hashfn, hashequalsfn eqfn)
{
  hashtable ht;
  hashpool *ptr;

  ht = fixedpool_alloc (pool->tablepool);
  ht->table = (entry *) xmalloc (sizeof (entry) * size);
  memset (ht->table, 0, size * sizeof (entry));

  ptr = (hashpool *) &ht->pool;
  *ptr = pool;

  ht->tablelength = size;
  ht->entrycount = 0;
  ht->hashfn = hashfn;
  ht->eqfn = eqfn;
  ht->loadlimit = size >> 1;

  return ht;
}

void hashtable_free (hashtable ht, hashtraversefn destroyfn, void *arg)
{
  entry e;
  unsigned int i;

  for (i = 0; i < ht->tablelength; i++) {
    for (e = ht->table[i]; e; e = e->next) {
      if (destroyfn)
        destroyfn (e->key, e->value, e->hash, arg);
      fixedpool_free (ht->pool->entrypool, e);
    }
  }

  fixedpool_grow (ht->pool->entrypool, ht->table, ht->tablelength);
  ht->table = NULL;
  ht->tablelength = 0;
  ht->entrycount = 0;

  fixedpool_free (ht->pool->tablepool, ht);
}

static
void hashtable_grow (hashtable ht)
{
  entry *newtable;
  entry e, ne;
  unsigned int newsize, i, index;

  newsize = ht->tablelength << 1;

  newtable = (entry *) xmalloc (sizeof (entry) * newsize);
  memset (newtable, 0, newsize * sizeof (entry));

  for (i = 0; i < ht->tablelength; i++) {
    for (e = ht->table[i]; e; e = ne) {
      ne = e->next;
      index = INDEX_FOR (e->hash, newsize);
      e->next = newtable[index];
      newtable[index] = e;
    }
  }

  fixedpool_grow (ht->pool->entrypool, ht->table, ht->tablelength);

  ht->table = newtable;
  ht->tablelength = newsize;
  ht->loadlimit = newsize >> 1;
}

unsigned int hashtable_count (hashtable ht)
{
  return ht->entrycount;
}

void hashtable_insert (hashtable ht, void *key, void *value)
{
  hashtable_inserthash (ht, key, value, ht->hashfn (key));
}

void hashtable_inserthash (hashtable ht, void *key, void *value, unsigned int hash)
{
  unsigned int index;
  entry e;

  if (ht->entrycount >= ht->loadlimit) {
    hashtable_grow (ht);
  }

  e = alloc_entry (ht->pool);
  e->hash = hash;
  index = INDEX_FOR (e->hash, ht->tablelength);
  e->key = key;
  e->value = value;
  e->next = ht->table[index];
  ht->entrycount++;
  ht->table[index] = e;
}


static
entry find_entry (hashtable ht, void *key, unsigned int hash, int remove)
{
  entry e;
  entry *prev;
  unsigned int index;

  index = INDEX_FOR (hash, ht->tablelength);
  for (prev = &(ht->table[index]); (e = *prev) ; prev = &e->next) {
    if (hash != e->hash) continue;
    if (key != e->key)
      if (!ht->eqfn (key, e->key, hash))
        continue;

    if (remove) {
      *prev = e->next;
      ht->entrycount--;
      free_entry (ht->pool, e);
    }

    return e;
  }
  return NULL;
}

void *hashtable_search (hashtable ht, void *key, void **key_found)
{
  return hashtable_searchhash (ht, key, key_found, ht->hashfn (key));
}

void *hashtable_searchhash (hashtable ht, void *key, void **key_found, unsigned int hash)
{
  entry e;
  e = find_entry (ht, key, hash, 0);
  if (e) {
    if (key_found)
      *key_found = e->key;
    return e->value;
  }
  return NULL;
}

int hashtable_haskey (hashtable ht, void *key, void **key_found)
{
  return hashtable_haskeyhash (ht, key, key_found, ht->hashfn (key));
}

int hashtable_haskeyhash (hashtable ht, void *key, void **key_found, unsigned int hash)
{
  entry e = find_entry (ht, key, hash, 0);
  if (e) {
    if (key_found)
      *key_found = e->key;
    return TRUE;
  }
  return FALSE;
}

void *hashtable_remove (hashtable ht, void *key, void **key_found)
{
  return hashtable_removehash (ht, key, key_found, ht->hashfn (key));
}

void *hashtable_removehash (hashtable ht, void *key, void **key_found, unsigned int hash)
{
  entry e = find_entry (ht, key, hash, 1);
  if (e) {
    if (key_found)
      *key_found = e->key;
    return e->value;
  }
  return NULL;
}


void hashtable_traverse (hashtable ht, hashtraversefn traversefn, void *arg)
{
  entry e;
  unsigned int i;

  for (i = 0; i < ht->tablelength; i++) {
    for (e = ht->table[i]; e; e = e->next) {
      traversefn (e->key, e->value, e->hash, arg);
    }
  }
}

int hashtable_string_compare (void *key1, void *key2, unsigned int hash)
{
  return (strcmp (key1, key2) == 0);
}

int hashtable_pointer_compare (void *key1, void *key2, unsigned int hash)
{
  return (key1 == key2);
}


unsigned int hashtable_hash_bytes (unsigned char *key, size_t len)
{
  unsigned int hash = 0;
  size_t i;

  for (i = 0; i < len; i++) {
    hash += key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}

unsigned int hashtable_hash_string (void *key)
{
  unsigned int hash = 0;
  unsigned char *bytes = (unsigned char *) key;

  while (*bytes) {
    hash += *bytes++;
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}
