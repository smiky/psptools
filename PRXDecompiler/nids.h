/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#ifndef __NIDS_H
#define __NIDS_H

struct nidstable;

struct nidinfo {
  const char *name;
  unsigned int nid;
  int isvariable;
  int numargs;
  int isvarargs;
};

struct nidstable *nids_load (const char *xmlpath);
struct nidinfo *nids_find (struct nidstable *nids, const char *library, unsigned int nid);
void nids_print (struct nidstable *nids);
void nids_free (struct nidstable *nids);

#endif /* __NIDS_H */
