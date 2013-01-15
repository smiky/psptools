/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Lib/expat.h"

#include "nids.h"
#include "hash.h"
#include "alloc.h"
#include "utils.h"

struct nidstable {
  hashpool pool;
  hashtable libs;
  fixedpool infopool;
  char *buffer;
};

enum XMLSCOPE {
  XMLS_LIBRARY,
  XMLS_FUNCTION,
  XMLS_VARIABLE
};

enum XMLELEMENT {
  XMLE_DEFAULT,
  XMLE_NID,
  XMLE_NAME,
  XMLE_NUMARGS
};

struct xml_data {
  enum XMLSCOPE scope;
  enum XMLELEMENT last;

  size_t buffer_pos;
  struct nidstable *result;
  hashtable curlib;
  const char *libname;

  struct nidinfo currnid;
  int error;
};

void nids_free (struct nidstable *nids)
{
  nids->libs = NULL;
  if (nids->buffer)
    free (nids->buffer);
  nids->buffer = NULL;
  if (nids->pool)
    hashpool_destroy (nids->pool);
  nids->pool = NULL;
  if (nids->infopool)
    fixedpool_destroy (nids->infopool, NULL, NULL);
  nids->infopool = NULL;
  free (nids);
}


static
void start_hndl (void *data, const char *el, const char **attr)
{
  struct xml_data *d = (struct xml_data *) data;

  d->last = XMLE_DEFAULT;

  if (strcmp (el, "LIBRARY") == 0) {
    d->scope = XMLS_LIBRARY;
  } else if (strcmp (el, "NAME") == 0) {
    d->last = XMLE_NAME;
  } else if (strcmp (el, "FUNCTION") == 0) {
    d->currnid.isvariable = 0;
    d->scope = XMLS_FUNCTION;
  } else if (strcmp (el, "VARIABLE") == 0) {
    d->currnid.isvariable = 1;
    d->scope = XMLS_VARIABLE;
  } else if (strcmp (el, "NID") == 0) {
    d->last = XMLE_NID;
  } else if (strcmp (el, "NUMARGS") == 0) {
    d->last = XMLE_NUMARGS;
  }
}

static
void end_hndl (void *data, const char *el)
{
  struct xml_data *d = (struct xml_data *) data;

  d->last = XMLE_DEFAULT;

  if (strcmp (el, "LIBRARY") == 0) {
    d->curlib = NULL;
    d->libname = NULL;
  } else if (strcmp (el, "FUNCTION") == 0 || strcmp (el, "VARIABLE") == 0) {
    d->scope = XMLS_LIBRARY;
    if (d->currnid.name && d->currnid.nid && d->curlib) {
      struct nidinfo *info = hashtable_searchhash (d->curlib, NULL, NULL, d->currnid.nid);
      if (info) {
        if (strcmp (info->name, d->currnid.name)) {
          error (__FILE__ ": NID `0x%08X' repeated in library `%s'", d->currnid.nid, d->libname);
          d->error = 1;
        }
      } else {
        info = fixedpool_alloc (d->result->infopool);
        memcpy (info, &d->currnid, sizeof (struct nidinfo));
        hashtable_inserthash (d->curlib, NULL, info, d->currnid.nid);
      }
    } else {
      error (__FILE__ ": missing function or variable definition");
      d->error = 1;
    }
    d->currnid.name = NULL;
    d->currnid.nid = 0;
    d->currnid.numargs = -1;
  }
}

static
const char *dup_string (struct xml_data *d, const char *txt, size_t len)
{
  char *result;

  result = &d->result->buffer[d->buffer_pos];
  memcpy (result, txt, len);
  result[len] = '\0';
  d->buffer_pos += len + 1;

  return (const char *) result;
}

static
void char_hndl (void *data, const char *txt, int txtlen)
{
  struct xml_data *d = (struct xml_data *) data;

  switch (d->scope) {
  case XMLS_FUNCTION:
  case XMLS_VARIABLE:
    if (d->last == XMLE_NAME) {
      if (d->currnid.name) {
        error (__FILE__ ": repeated name in function/variable");
        d->error = 1;
      } else {
        d->currnid.name = dup_string (d, txt, txtlen);
      }
    } else if (d->last == XMLE_NID || d->last == XMLE_NUMARGS) {
      char buffer[256];

      if (txtlen > sizeof (buffer) - 1)
        txtlen = sizeof (buffer) - 1;
      memcpy (buffer, txt, txtlen);
      buffer[txtlen] = '\0';

      if (d->last == XMLE_NID) {
        if (d->currnid.nid) {
          error (__FILE__ ": nid repeated in function/variable");
          d->error = 1;
        } else {
          d->currnid.nid = 0;
          sscanf (buffer, "0x%X", &d->currnid.nid);
        }
      } else {
        d->currnid.numargs = -1;
        sscanf (buffer, "%d", &d->currnid.numargs);
      }
    }
    break;
  case XMLS_LIBRARY:
    if (d->last == XMLE_NAME) {
      d->libname = dup_string (d, txt, txtlen);
      if (d->curlib) {
        error (__FILE__ ": current lib is not null");
        d->error = 1;
      } else {
        d->curlib = hashtable_search (d->result->libs, (void *) d->libname, NULL);
        if (!d->curlib) {
          d->curlib = hashtable_alloc (d->result->pool, 128, NULL, &hashtable_pointer_compare);
          hashtable_insert (d->result->libs, (void *) d->libname, d->curlib);
        }
      }
    }
    break;
  }
}

struct nidstable *nids_load (const char *xmlpath)
{
  XML_Parser p;
  struct xml_data data;
  size_t size;
  void *buf;

  buf = read_file (xmlpath, &size);
  if (!buf) {
    return NULL;
  }

  p = XML_ParserCreate (NULL);
  if (!p) {
    error (__FILE__ ": can't create XML parser");
    free (buf);
    return 0;
  }

  data.error = 0;
  data.curlib = NULL;
  data.libname = NULL;
  data.currnid.name = NULL;
  data.currnid.nid = 0;
  data.currnid.numargs = 0;
  data.scope = XMLS_LIBRARY;
  data.last = XMLE_DEFAULT;

  data.result =
    (struct nidstable *) xmalloc (sizeof (struct nidstable));
  data.result->pool =
    hashpool_create (256, 8192);
  data.result->libs =
    hashtable_alloc (data.result->pool, 32, &hashtable_hash_string,
                     &hashtable_string_compare);
  data.result->infopool = fixedpool_create (sizeof (struct nidinfo), 8192, 0);

  data.buffer_pos = 0;
  data.result->buffer = buf;
  buf = xmalloc (size);

  memcpy (buf, data.result->buffer, size);

  XML_SetUserData (p, (void *) &data);
  XML_SetElementHandler (p, &start_hndl, &end_hndl);
  XML_SetCharacterDataHandler (p, &char_hndl);

  if (!XML_Parse (p, buf, size, 1)) {
    error (__FILE__ ": parse error at line %d:\n  %s\n", XML_GetCurrentLineNumber (p),
           XML_ErrorString (XML_GetErrorCode (p)));
    data.error = 1;
  }

  XML_ParserFree (p);
  free (buf);

  if (data.error) {
    nids_free (data.result);
    return NULL;
  }

  return data.result;
}

static
void print_level2 (void *key, void *value, unsigned int hash, void *arg)
{
  struct nidinfo *info = value;
  report ("    NID: 0x%08X ", info->nid);
  if (info->isvariable) report ("(Variable)");
  else report ("Num args: %d", info->numargs);
  report (" Name: %s\n", info->name);
}

static
void print_level1 (void *key, void *value, unsigned int hash, void *arg)
{
  hashtable lib = (hashtable) value;
  report ("  %s:\n", (char *) key);
  hashtable_traverse (lib, &print_level2, NULL);
  report ("\n");
}

void nids_print (struct nidstable *nids)
{
  report ("Libraries:\n");
  hashtable_traverse (nids->libs, &print_level1, NULL);
}


struct nidinfo *nids_find (struct nidstable *nids, const char *library, unsigned int nid)
{
  hashtable lib;

  lib = hashtable_search (nids->libs, (void *) library, NULL);
  if (lib) return hashtable_searchhash (lib, NULL, NULL, nid);
  return NULL;
}


#ifdef TEST_NIDS
int main (int argc, char **argv)
{
  struct nidstable *nids = NULL;

  nids = nids_load (argv[1]);
  if (nids) {
    nids_print (nids);
    nids_free (nids);
  }
  return 0;
}

#endif /* TEST_NIDS */
