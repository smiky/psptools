/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#ifndef __PRX_H
#define __PRX_H

#include "types.h"
#include "nids.h"

#define ELF_HEADER_IDENT        16
#define ELF_PRX_TYPE            0xFFA0
#define ELF_MACHINE_MIPS        8
#define ELF_VERSION_CURRENT     1
#define ELF_FLAGS_MACH_ALLEGREX 0x00A20000
#define ELF_FLAGS_ABI_EABI32    0x00003000
#define ELF_FLAGS_MIPS_ARCH2    0x10000000


/* Structure to hold prx header data */
struct prx
{
  uint8  ident[ELF_HEADER_IDENT];
  uint16 type;
  uint16 machine;
  uint32 version;
  uint32 entry;
  uint32 phoff;
  uint32 shoff;
  uint32 flags;
  uint16 ehsize;
  uint16 phentsize;
  uint16 phnum;
  uint16 shentsize;
  uint16 shnum;
  uint16 shstrndx;

  uint32 size;
  const uint8 *data;

  struct elf_section *sections;

  struct elf_program *programs;

  uint32 relocnum;
  struct prx_reloc *relocs;
  struct prx_reloc *relocsbyaddr;

  struct prx_modinfo *modinfo;
};

#define SHT_NULL            0
#define SHT_PROGBITS        1
#define SHT_STRTAB          3
#define SHT_NOBITS          8
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7fffffff
#define SHT_LOUSER 0x80000000
#define SHT_HIUSER 0xffffffff

#define SHT_PRXRELOC (SHT_LOPROC | 0xA0)

#define SHF_WRITE               1
#define SHF_ALLOC               2
#define SHF_EXECINSTR           4

/* Structure defining a single elf section */
struct elf_section
{
  uint32 idxname;
  uint32 type;
  uint32 flags;
  uint32 addr;
  uint32 offset;
  uint32 size;
  uint32 link;
  uint32 info;
  uint32 addralign;
  uint32 entsize;

  const uint8 *data;
  const char *name;

};

#define PT_NULL                 0
#define PT_LOAD                 1
#define PT_LOPROC               0x70000000
#define PT_HIPROC               0x7fffffff
#define PT_PRXRELOC             (PT_LOPROC | 0xA0)
#define PT_PRXRELOC2            (PT_LOPROC | 0xA1)

#define PF_X                    1
#define PF_W                    2
#define PF_R                    4

struct elf_program
{
  uint32 type;
  uint32 offset;
  uint32 vaddr;
  uint32 paddr;
  uint32 filesz;
  uint32 memsz;
  uint32 flags;
  uint32 align;

  const uint8 *data;
};


/* MIPS Reloc Entry Types */
#define R_MIPS_NONE     0
#define R_MIPS_16       1
#define R_MIPS_32       2
#define R_MIPS_26       4
#define R_MIPS_HI16     5
#define R_MIPS_LO16     6
#define R_MIPSX_HI16   13
#define R_MIPSX_J26    14
#define R_MIPSX_JAL26  15


struct prx_reloc {
  uint32 offset;
  uint8 type;
  uint8 offsbase;
  uint8 addrbase;
  uint8 extra;
  uint32 addend;
  uint32 vaddr;
  uint32 target;
};

struct prx_modinfo {

  uint16 attributes;
  uint16 version;
  uint32 gp;
  uint32 expvaddr;
  uint32 expvaddrbtm;
  uint32 impvaddr;
  uint32 impvaddrbtm;

  uint32 numimports;
  uint32 numexports;

  struct prx_import *imports;
  struct prx_export *exports;

  const char *name;
};

struct prx_import {

  uint32 namevaddr;
  uint32 flags;
  uint8  size;
  uint8  nvars;
  uint16 nfuncs;
  uint32 nidsvaddr;
  uint32 funcsvaddr;
  uint32 varsvaddr;

  struct prx_function *funcs;
  struct prx_variable *vars;

  const char *name;

};

struct prx_export {
  uint32 namevaddr;
  uint32 flags;
  uint8 size;
  uint8 nvars;
  uint16 nfuncs;
  uint32 expvaddr;

  struct prx_function *funcs;
  struct prx_variable *vars;

  const char *name;
};

struct prx_function {
  uint32 vaddr;
  uint32 nid;
  const char *name;
  const char *libname;
  void *pfunc;
  int numargs;
};

struct prx_variable {
  uint32 vaddr;
  uint32 nid;
  const char *name;
  const char *libname;
};

uint32 read_uint32_le (const uint8 *bytes);
uint16 read_uint16_le (const uint8 *bytes);
void write_uint32_le (uint8 *bytes, uint32 val);

struct prx *prx_load (const char *path);
void prx_free (struct prx *p);
void prx_print (struct prx *p, int prtrelocs);

void prx_resolve_nids (struct prx *p, struct nidstable *nids);

uint32 prx_translate (struct prx *p, uint32 vaddr);

int prx_inside_prx (struct prx *p, uint32 offset, uint32 size);
int prx_inside_progfile (struct elf_program *program, uint32 vaddr, uint32 size);
int prx_inside_progmem (struct elf_program *program, uint32 vaddr, uint32 size);
int prx_inside_strprogfile (struct elf_program *program, uint32 vaddr);

uint32 prx_findreloc (struct prx *p, uint32 target);
uint32 prx_findrelocbyaddr (struct prx *p, uint32 vaddr);



#endif /* __PRX_H */
