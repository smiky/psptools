/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stdlib.h>
#include <string.h>

#include "prx.h"
#include "utils.h"

static
int check_module_info (struct prx *p)
{
  struct prx_modinfo *info = p->modinfo;
  uint32 vaddr, offset;

  if (info->name[27]) {
    error (__FILE__ ": module name is not null terminated\n");
    return 0;
  }

  if (info->expvaddr) {
    if (info->expvaddr > info->expvaddrbtm) {
      error (__FILE__ ": exports bottom is above top (0x%08X - 0x%08X)", info->expvaddr, info->expvaddrbtm);
      return 0;
    }
    if (!prx_inside_progfile (p->programs, info->expvaddr, info->expvaddrbtm - info->expvaddr)) {
      error (__FILE__ ": exports not inside the first program (0x%08X - 0x%08X)", info->expvaddr, info->expvaddrbtm);
      return 0;
    }
    info->numexports = 0;
    offset = prx_translate (p, info->expvaddr);
    for (vaddr = info->expvaddr; vaddr < info->expvaddrbtm; info->numexports++) {
      uint32 size;
      size = p->data[offset+8];
      if (size < 4) {
        error (__FILE__ ": export size less than 4 words: %d", size);
        return 0;
      }
      vaddr += size << 2;
      offset += size << 2;
    }
    if (vaddr != info->expvaddrbtm) {
      error (__FILE__ ": invalid exports boundary");
      return 0;
    }
  }

  if (info->impvaddr) {
    if (info->impvaddr > info->impvaddrbtm) {
      error (__FILE__ ": imports bottom is above top (0x%08X - 0x%08X)", info->impvaddr, info->impvaddrbtm);
      return 0;
    }
    if (!prx_inside_progfile (p->programs, info->impvaddr, info->impvaddrbtm - info->impvaddr)) {
      error (__FILE__ ": imports not inside the first program (0x%08X - 0x%08X)", info->impvaddr, info->impvaddrbtm);
      return 0;
    }
    info->numimports = 0;
    offset = prx_translate (p, info->impvaddr);
    for (vaddr = info->impvaddr; vaddr < info->impvaddrbtm; info->numimports++) {
      uint32 size;
      uint8 nvars;
      size = p->data[offset+8];
      nvars = p->data[offset+9];
      if (size < 5) {
        error (__FILE__ ": import size less than 5 words: %d", size);
        return 0;
      }
      if (nvars && size < 6) {
        error (__FILE__ ": import size less than 6 words: %d", size);
        return 0;
      }
      vaddr += size << 2;
      offset += size << 2;
    }
    if (vaddr != info->impvaddrbtm) {
      error (__FILE__ ": invalid imports boundary");
      return 0;
    }
  }
  return 1;
}

static
int check_module_import (struct prx *p, uint32 index)
{
  struct prx_import *imp = &p->modinfo->imports[index];

  if (!prx_inside_strprogfile (p->programs, imp->namevaddr)) {
    error (__FILE__ ": import name not inside first program");
    return 0;
  }

  if (!imp->nfuncs && !imp->nvars) {
    error (__FILE__ ": no functions or variables imported");
    return 0;
  }

  if (!prx_inside_progfile (p->programs, imp->funcsvaddr, 8 * imp->nfuncs)) {
    error (__FILE__ ": functions not inside the first program");
    return 0;
  }

  if (!prx_inside_progfile (p->programs, imp->nidsvaddr, 4 * imp->nfuncs)) {
    error (__FILE__ ": nids not inside the first program");
    return 0;
  }

  if (imp->nvars) {
    if (!prx_inside_progfile (p->programs, imp->varsvaddr, 8 * imp->nvars)) {
      error (__FILE__ ": variables not inside first program");
      return 0;
    }
  }


  return 1;
}

static
int check_module_export (struct prx *p, uint32 index)
{
  struct prx_export *exp = &p->modinfo->exports[index];

  if (!prx_inside_strprogfile (p->programs, exp->namevaddr)) {
    error (__FILE__ ": export name not inside first program");
    return 0;
  }

  if (!exp->nfuncs && !exp->nvars) {
    error (__FILE__ ": no functions or variables exported");
    return 0;
  }

  if (!prx_inside_progfile (p->programs, exp->expvaddr, 8 * (exp->nfuncs + exp->nvars))) {
    error (__FILE__ ": functions and variables not inside the first program");
    return 0;
  }

  return 1;
}



static
int load_module_import (struct prx *p, struct prx_import *imp)
{
  uint32 i, offset;
  if (imp->nfuncs) {
    imp->funcs = (struct prx_function *) xmalloc (imp->nfuncs * sizeof (struct prx_function));
    offset = prx_translate (p, imp->nidsvaddr);
    for (i = 0; i < imp->nfuncs; i++) {
      struct prx_function *f = &imp->funcs[i];
      f->nid = read_uint32_le (&p->data[offset + 4 * i]);
      f->vaddr = imp->funcsvaddr + 8 * i;
      f->libname = imp->name;
      f->name = NULL;
      f->numargs = -1;
      f->pfunc = NULL;
    }
  }

  if (imp->nvars) {
    imp->vars = (struct prx_variable *) xmalloc (imp->nvars * sizeof (struct prx_variable));
    offset = prx_translate (p, imp->varsvaddr);
    for (i = 0; i < imp->nvars; i++) {
      struct prx_variable *v = &imp->vars[i];
      v->nid = read_uint32_le (&p->data[offset + 8 * i + 4]);
      v->vaddr = read_uint32_le (&p->data[offset +  8 * i]);
      v->libname = imp->name;
      v->name = NULL;
    }
 }
  return 1;
}

static
int load_module_imports (struct prx *p)
{
  uint32 i = 0, offset;
  struct prx_modinfo *info = p->modinfo;
  if (!info->impvaddr) return 1;

  info->imports = (struct prx_import *) xmalloc (info->numimports * sizeof (struct prx_import));
  memset (info->imports, 0, info->numimports * sizeof (struct prx_import));

  offset = prx_translate (p, info->impvaddr);
  for (i = 0; i < info->numimports; i++) {
    struct prx_import *imp = &info->imports[i];
    imp->namevaddr = read_uint32_le (&p->data[offset]);
    imp->flags = read_uint32_le (&p->data[offset+4]);
    imp->size = p->data[offset+8];
    imp->nvars = p->data[offset+9];
    imp->nfuncs = read_uint16_le (&p->data[offset+10]);
    imp->nidsvaddr = read_uint32_le (&p->data[offset+12]);
    imp->funcsvaddr = read_uint32_le (&p->data[offset+16]);
    if (imp->nvars) imp->varsvaddr = read_uint32_le (&p->data[offset+20]);

    if (!check_module_import (p, i)) return 0;

    if (imp->namevaddr)
      imp->name = (const char *) &p->data[prx_translate (p, imp->namevaddr)];
    else
      imp->name = NULL;

    if (!load_module_import (p, imp)) return 0;
    offset += imp->size << 2;
  }
  return 1;
}

static
const char *resolve_syslib_nid (uint32 nid)
{
  switch (nid) {
  case 0xd3744be0: return "module_bootstart";
  case 0x2f064fa6: return "module_reboot_before";
  case 0xadf12745: return "module_reboot_phase";
  case 0xd632acdb: return "module_start";
  case 0xcee8593c: return "module_stop";
  case 0xf01d73a7: return "module_info";
  case 0x0f7c276c: return "module_start_thread_parameter";
  case 0xcf0cc697: return "module_stop_thread_parameter";
  }
  return NULL;
}

static
int load_module_export (struct prx *p, struct prx_export *exp)
{
  uint32 i, offset, disp;
  offset = prx_translate (p, exp->expvaddr);
  disp = 4 * (exp->nfuncs + exp->nvars);
  if (exp->nfuncs) {
    exp->funcs = (struct prx_function *) xmalloc (exp->nfuncs * sizeof (struct prx_function));
    for (i = 0; i < exp->nfuncs; i++) {
      struct prx_function *f = &exp->funcs[i];
      f->vaddr = read_uint32_le (&p->data[offset + disp]);
      f->nid = read_uint32_le (&p->data[offset]);
      f->name = NULL;
      f->libname = exp->name;
      f->numargs = -1;
      f->pfunc = NULL;
      offset += 4;
      if (exp->namevaddr == 0) {
        f->name = resolve_syslib_nid (f->nid);
      }
    }
  }

  if (exp->nvars) {
    exp->vars = (struct prx_variable *) xmalloc (exp->nvars * sizeof (struct prx_variable));
    for (i = 0; i < exp->nvars; i++) {
      struct prx_variable *v = &exp->vars[i];
      v->vaddr = read_uint32_le (&p->data[offset + disp]);
      v->nid = read_uint32_le (&p->data[offset]);
      v->name = NULL;
      v->libname = exp->name;
      offset += 4;
      if (exp->namevaddr == 0) {
        v->name = resolve_syslib_nid (v->nid);
      }
    }
  }
  return 1;
}

static
int load_module_exports (struct prx *p)
{
  uint32 i = 0, offset;
  struct prx_modinfo *info = p->modinfo;
  if (!info->expvaddr) return 1;

  info->exports = (struct prx_export *) xmalloc (info->numexports * sizeof (struct prx_export));
  memset (info->exports, 0, info->numexports * sizeof (struct prx_export));

  offset = prx_translate (p, info->expvaddr);
  for (i = 0; i < info->numexports; i++) {
    struct prx_export *exp = &info->exports[i];
    exp->namevaddr = read_uint32_le (&p->data[offset]);
    exp->flags = read_uint32_le (&p->data[offset+4]);
    exp->size = p->data[offset+8];
    exp->nvars = p->data[offset+9];
    exp->nfuncs = read_uint16_le (&p->data[offset+10]);
    exp->expvaddr = read_uint32_le (&p->data[offset+12]);

    if (!check_module_export (p, i)) return 0;

    if (exp->namevaddr)
      exp->name = (const char *) &p->data[prx_translate (p, exp->namevaddr)];
    else
      exp->name = "syslib";

    if (!load_module_export (p, exp)) return 0;
    offset += exp->size << 2;
  }
  return 1;
}

int load_module_info (struct prx *p)
{
  struct prx_modinfo *info;
  uint32 offset;
  p->modinfo = NULL;
  if (p->phnum > 0)
    offset = p->programs[0].paddr & 0x7FFFFFFF;
  else {
    error (__FILE__ ": can't find module info for PRX");
    return 0;
  }

  info = (struct prx_modinfo *) xmalloc (sizeof (struct prx_modinfo));
  p->modinfo = info;

  info->attributes = read_uint16_le (&p->data[offset]);
  info->version = read_uint16_le (&p->data[offset+2]);
  info->name = (const char *) &p->data[offset+4];
  info->gp = read_uint32_le (&p->data[offset+32]);
  info->expvaddr = read_uint32_le (&p->data[offset+36]);
  info->expvaddrbtm = read_uint32_le (&p->data[offset+40]);
  info->impvaddr = read_uint32_le (&p->data[offset+44]);
  info->impvaddrbtm = read_uint32_le (&p->data[offset+48]);

  info->imports = NULL;
  info->exports = NULL;

  if (!check_module_info (p)) return 0;

  if (!load_module_imports (p)) return 0;
  if (!load_module_exports (p)) return 0;

  return 1;
}


static
void free_module_import (struct prx_import *imp)
{
  if (imp->funcs) free (imp->funcs);
  if (imp->vars) free (imp->vars);
  imp->funcs = NULL;
  imp->vars = NULL;
}

static
void free_module_imports (struct prx *p)
{
  if (!p->modinfo) return;
  if (p->modinfo->imports) {
    uint32 i;
    for (i = 0; i < p->modinfo->numimports; i++)
      free_module_import (&p->modinfo->imports[i]);
    free (p->modinfo->imports);
  }
  p->modinfo->imports = NULL;
}

static
void free_module_export (struct prx_export *exp)
{
  if (exp->funcs) free (exp->funcs);
  if (exp->vars) free (exp->vars);
  exp->funcs = NULL;
  exp->vars = NULL;
}

static
void free_module_exports (struct prx *p)
{
  if (!p->modinfo) return;
  if (p->modinfo->exports) {
    uint32 i;
    for (i = 0; i < p->modinfo->numexports; i++)
      free_module_export (&p->modinfo->exports[i]);
    free (p->modinfo->exports);
  }
  p->modinfo->imports = NULL;
}

void free_module_info (struct prx *p)
{
  free_module_imports (p);
  free_module_exports (p);
  if (p->modinfo)
    free (p->modinfo);
  p->modinfo = NULL;
}


static
void print_module_imports (struct prx *p)
{
  uint32 idx, i;
  struct prx_modinfo *info = p->modinfo;
  report ("\nImports:\n");
  for (idx = 0; idx < info->numimports; idx++) {
    struct prx_import *imp = &info->imports[idx];
    report ("  %s\n", imp->name);

    report ("     Flags:          0x%08X\n", imp->flags);
    report ("     Size:               %6d\n", imp->size);
    report ("     Num Variables:      %6d\n", imp->nvars);
    report ("     Num Functions:      %6d\n", imp->nfuncs);
    report ("     Nids:           0x%08X\n", imp->nidsvaddr);
    report ("     Functions:      0x%08X\n", imp->funcsvaddr);

    for (i = 0; i < imp->nfuncs; i++) {
      struct prx_function *f = &imp->funcs[i];
      report ("         NID: 0x%08X  VADDR: 0x%08X", f->nid, f->vaddr);
      if (f->name)
        report (" NAME: %s", f->name);
      report ("\n");
    }
    if (imp->nvars) {
      report ("     Variables:      0x%08X\n", imp->varsvaddr);
      for (i = 0; i < imp->nvars; i++) {
        struct prx_variable *v = &imp->vars[i];
        report ("         NID: 0x%08X  VADDR: 0x%08X", v->nid, v->vaddr);
        if (v->name)
          report (" NAME: %s", v->name);
        report ("\n");
      }
    }

    report ("\n");
  }
}

static
void print_module_exports (struct prx *p)
{
  uint32 idx, i;
  struct prx_modinfo *info = p->modinfo;
  report ("\nExports:\n");
  for (idx = 0; idx < info->numexports; idx++) {
    struct prx_export *exp = &info->exports[idx];
    report ("  %s\n", exp->name);

    report ("     Flags:          0x%08X\n", exp->flags);
    report ("     Size:               %6d\n", exp->size);
    report ("     Num Variables:      %6d\n", exp->nvars);
    report ("     Num Functions:      %6d\n", exp->nfuncs);
    report ("     Exports:        0x%08X\n", exp->expvaddr);
    if (exp->nfuncs) {
      report ("     Functions:\n");
      for (i = 0; i < exp->nfuncs; i++) {
        struct prx_function *f = &exp->funcs[i];
        report ("         NID: 0x%08X  VADDR: 0x%08X", f->nid, f->vaddr);
        if (f->name)
          report (" NAME: %s", f->name);
        report ("\n");
      }
    }
    if (exp->nvars) {
      report ("     Variables:\n");
      for (i = 0; i < exp->nvars; i++) {
        struct prx_variable *v = &exp->vars[i];
        report ("         NID: 0x%08X  VADDR: 0x%08X", v->nid, v->vaddr);
        if (v->name)
          report (" NAME: %s", v->name);
        report ("\n");
      }
    }
    report ("\n");
  }
}

void print_module_info (struct prx *p)
{
  struct prx_modinfo *info = p->modinfo;
  if (!info) return;

  report ("\nModule info:\n");
  report ("  Name: %31s\n", info->name);
  report ("  Attributes:                    0x%04X\n", info->attributes);
  report ("  Version:                       0x%04X\n", info->version);
  report ("  GP:                        0x%08X\n", info->gp);
  report ("  Library entry:             0x%08X\n", info->expvaddr);
  report ("  Library entry bottom:      0x%08X\n", info->expvaddrbtm);
  report ("  Library stubs:             0x%08X\n", info->impvaddr);
  report ("  Library stubs bottom:      0x%08X\n", info->impvaddrbtm);

  print_module_imports (p);
  print_module_exports (p);
}


void prx_resolve_nids (struct prx *p, struct nidstable *nids)
{
  uint32 i, j;
  struct nidinfo *ninfo;
  struct prx_modinfo *info = p->modinfo;

  for (i = 0; i < info->numimports; i++) {
    struct prx_import *imp = &info->imports[i];
    for (j = 0; j < imp->nfuncs; j++) {
      struct prx_function *f = &imp->funcs[j];
      ninfo = nids_find (nids, imp->name, f->nid);
      if (ninfo) {
        f->name = ninfo->name;
        f->numargs = ninfo->numargs;
      }
    }
    for (j = 0; j < imp->nvars; j++) {
      struct prx_variable *v = &imp->vars[j];
      ninfo = nids_find (nids, imp->name, v->nid);
      if (ninfo) {
        v->name = ninfo->name;
      }
    }
  }

  for (i = 0; i < info->numexports; i++) {
    struct prx_export *exp = &info->exports[i];
    for (j = 0; j < exp->nfuncs; j++) {
      struct prx_function *f = &exp->funcs[j];
      ninfo = nids_find (nids, exp->name, f->nid);
      if (ninfo) {
        f->name = ninfo->name;
        f->numargs = ninfo->numargs;
      }
    }
    for (j = 0; j < exp->nvars; j++) {
      struct prx_variable *v = &exp->vars[j];
      ninfo = nids_find (nids, exp->name, v->nid);
      if (ninfo) {
        v->name = ninfo->name;
      }
    }
  }
}




