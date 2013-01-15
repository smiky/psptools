/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stdlib.h>
#include <string.h>

#include "prx.h"
#include "nids.h"
#include "utils.h"

#define ELF_HEADER_SIZE              52
#define ELF_SECTION_HEADER_ENT_SIZE  40
#define ELF_PROGRAM_HEADER_ENT_SIZE  32
#define ELF_PRX_FLAGS                (ELF_FLAGS_MIPS_ARCH2 | ELF_FLAGS_MACH_ALLEGREX | ELF_FLAGS_MACH_ALLEGREX)
#define PRX_MODULE_INFO_SIZE         52

extern int load_relocs (struct prx *p);
extern void free_relocs (struct prx *p);
extern void print_relocs (struct prx *p);

extern int load_module_info (struct prx *p);
extern void free_module_info (struct prx *p);
extern void print_module_info (struct prx *p);


static const uint8 valid_ident[] = {
  0x7F, 'E', 'L', 'F',
  0x01, /* Elf class = ELFCLASS32 */
  0x01, /* Elf data = ELFDATA2LSB */
  0x01, /* Version = EV_CURRENT */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 /* Padding */
};

uint32 read_uint32_le (const uint8 *bytes)
{
  uint32 r;
  r  = *bytes++;
  r |= *bytes++ << 8;
  r |= *bytes++ << 16;
  r |= *bytes++ << 24;
  return r;
}

uint16 read_uint16_le (const uint8 *bytes)
{
  uint16 r;
  r  = *bytes++;
  r |= *bytes++ << 8;
  return r;
}

void write_uint32_le (uint8 *bytes, uint32 val)
{
  bytes[0] = val & 0xFF; val >>= 8;
  bytes[1] = val & 0xFF; val >>= 8;
  bytes[2] = val & 0xFF; val >>= 8;
  bytes[3] = val & 0xFF;
}

int prx_inside_prx (struct prx *p, uint32 offset, uint32 size)
{
  if (offset >= p->size || size > p->size ||
      size > (p->size - offset)) return 0;
  return 1;
}

int prx_inside_progfile (struct elf_program *program, uint32 vaddr, uint32 size)
{
  if (vaddr < program->vaddr || size > program->filesz) return 0;

  vaddr -= program->vaddr;
  if (vaddr >= program->filesz || (program->filesz - vaddr) < size) return 0;
  return 1;
}

int prx_inside_progmem (struct elf_program *program, uint32 vaddr, uint32 size)
{
  if (vaddr < program->vaddr || size > program->memsz) return 0;

  vaddr -= program->vaddr;
  if (vaddr >= program->memsz || (program->memsz - vaddr) < size) return 0;
  return 1;
}


int prx_inside_strprogfile (struct elf_program *program, uint32 vaddr)
{
  if (vaddr < program->vaddr) return 0;

  vaddr -= program->vaddr;
  if (vaddr >= program->filesz) return 0;

  while (vaddr < program->filesz) {
    if (!program->data[vaddr]) return 1;
    vaddr++;
  }

  return 0;
}

static
int check_section_header (struct prx *p, uint32 index)
{
  struct elf_section *section = &p->sections[index];

  switch (section->type) {
  case SHT_NOBITS:
    break;
  case SHT_PRXRELOC:
  case SHT_STRTAB:
  case SHT_PROGBITS:
  case SHT_NULL:
    if (section->size) {
      if (!prx_inside_prx (p, section->offset, section->size)) {
        error (__FILE__ ": section is not inside ELF/PRX (section %d)", index);
        return 0;
      }
    }
    break;
  default:
    error (__FILE__ ": invalid section type 0x$08X (section %d)", section->type, index);
    return 0;
  }

  return 1;
}

static
int check_program_header (struct prx *p, uint32 index)
{
  struct elf_program *program = &p->programs[index];
  if (!prx_inside_prx (p, program->offset, program->filesz)) {
    error (__FILE__ ": program is not inside ELF/PRX (program %d)", index);
    return 0;
  }

  if ((index == 0) && program->type != PT_LOAD) {
    error (__FILE__ ": first program is not of the type LOAD");
    return 0;
  }

  switch (program->type) {
  case PT_LOAD:
    if (program->filesz > program->memsz) {
      error (__FILE__ ": program file size grater than than memory size (program %d)", index);
      return 0;
    }
    break;
  case PT_PRXRELOC:
  case PT_PRXRELOC2:
    if (program->memsz) {
      error (__FILE__ ": program type must not loaded (program %d)", index);
      return 0;
    }
    break;
  default:
    error (__FILE__ ": invalid program type 0x%08X (program %d)", program->type, index);
    return 0;
  }

  return 1;
}

static
int check_elf_header (struct prx *p)
{
  uint32 table_size;

  if (memcmp (p->ident, valid_ident, sizeof (valid_ident))) {
    error (__FILE__ ": invalid identification for ELF/PRX");
    return 0;
  }

  if (p->type != ELF_PRX_TYPE) {
    error (__FILE__ ": not a PRX file (0x%04X)", p->type);
    return 0;
  }

  if (p->machine != ELF_MACHINE_MIPS) {
    error (__FILE__ ": machine is not MIPS (0x%04X)", p->machine);
    return 0;
  }

  if (p->version != ELF_VERSION_CURRENT) {
    error (__FILE__ ": version is not EV_CURRENT (0x%08X)", p->version);
    return 0;
  }

  if (p->ehsize != ELF_HEADER_SIZE) {
    error (__FILE__ ": wrong ELF header size (%u)", p->ehsize);
    return 0;
  }

  if ((p->flags & ELF_PRX_FLAGS) != ELF_PRX_FLAGS) {
    error (__FILE__ ": wrong ELF flags (0x%08X)", p->flags);
    return 0;
  }

  if (p->phnum && p->phentsize != ELF_PROGRAM_HEADER_ENT_SIZE) {
    error (__FILE__ ": wrong ELF program header entity size (%u)", p->phentsize);
    return 0;
  }

  if (!p->phnum) {
    error (__FILE__ ": PRX has no programs");
    return 0;
  }

  table_size = p->phentsize;
  table_size *= (uint32) p->phnum;
  if (!prx_inside_prx (p, p->phoff, table_size)) {
    error (__FILE__ ": wrong ELF program header table offset/size");
    return 0;
  }

  if (p->shnum && p->shentsize != ELF_SECTION_HEADER_ENT_SIZE) {
    error (__FILE__ ": wrong ELF section header entity size (%u)", p->shentsize);
    return 0;
  }

  table_size = p->shentsize;
  table_size *= (uint32) p->shnum;
  if (!prx_inside_prx (p, p->shoff, table_size)) {
    error (__FILE__ ": wrong ELF section header table offset/size");
    return 0;
  }

  return 1;

}

static
int load_sections (struct prx *p)
{
  struct elf_section *sections;
  uint32 idx;
  uint32 offset;

  p->sections = NULL;
  if (p->shnum == 0) return 1;

  sections = xmalloc (p->shnum * sizeof (struct elf_section));
  p->sections = sections;

  offset = p->shoff;
  for (idx = 0; idx < p->shnum; idx++) {

    sections[idx].idxname = read_uint32_le (&p->data[offset]);
    sections[idx].type = read_uint32_le (&p->data[offset+4]);
    sections[idx].flags = read_uint32_le (&p->data[offset+8]);
    sections[idx].addr = read_uint32_le (&p->data[offset+12]);
    sections[idx].offset = read_uint32_le (&p->data[offset+16]);
    sections[idx].size = read_uint32_le (&p->data[offset+20]);
    sections[idx].link = read_uint32_le (&p->data[offset+24]);
    sections[idx].info = read_uint32_le (&p->data[offset+28]);
    sections[idx].addralign = read_uint32_le (&p->data[offset+32]);
    sections[idx].entsize = read_uint32_le (&p->data[offset+36]);

    sections[idx].data = &p->data[sections[idx].offset];

    if (!check_section_header (p, idx))
      return 0;

    offset += p->shentsize;
  }

  if (p->shstrndx > 0) {
    if (sections[p->shstrndx].type == SHT_STRTAB) {
      char *strings = (char *) sections[p->shstrndx].data;
      uint32 max_index = sections[p->shstrndx].size;
      if (max_index > 0) {

        if (strings[max_index - 1] != '\0') {
          error (__FILE__ ": string table section not terminated with null byte");
          return 0;
        }

        for (idx = 0; idx < p->shnum; idx++) {
          if (sections[idx].idxname < max_index) {
            sections[idx].name = &strings[sections[idx].idxname];
          } else {
            error (__FILE__ ": invalid section name");
            return 0;
          }
        }
      }
    }
  }

  return 1;
}

static
int load_programs (struct prx *p)
{
  struct elf_program *programs;
  uint32 idx;
  uint32 offset;

  programs = xmalloc (p->phnum * sizeof (struct elf_program));
  p->programs = programs;

  offset = p->phoff;
  for (idx = 0; idx < p->phnum; idx++) {
    programs[idx].type = read_uint32_le (&p->data[offset]);
    programs[idx].offset = read_uint32_le (&p->data[offset+4]);
    programs[idx].vaddr = read_uint32_le (&p->data[offset+8]);
    programs[idx].paddr = read_uint32_le (&p->data[offset+12]);
    programs[idx].filesz = read_uint32_le (&p->data[offset+16]);
    programs[idx].memsz = read_uint32_le (&p->data[offset+20]);
    programs[idx].flags = read_uint32_le (&p->data[offset+24]);
    programs[idx].align = read_uint32_le (&p->data[offset+28]);

    programs[idx].data = &p->data[programs[idx].offset];

    if (!check_program_header (p, idx))
      return 0;

    offset += p->phentsize;
  }

  return 1;
}

struct prx *prx_load (const char *path)
{
  struct prx *p;
  uint8 *elf_bytes;
  size_t elf_size;
  elf_bytes = read_file (path, &elf_size);

  if (!elf_bytes) return NULL;

  if (elf_size < ELF_HEADER_SIZE) {
    error (__FILE__ ": elf size too short");
    free ((void *) elf_bytes);
    return NULL;
  }

  p = xmalloc (sizeof (struct prx));
  memset (p, 0, sizeof (struct prx));
  p->size = elf_size;
  p->data = elf_bytes;

  memcpy (p->ident, p->data, ELF_HEADER_IDENT);
  p->type = read_uint16_le (&p->data[ELF_HEADER_IDENT]);
  p->machine = read_uint16_le (&p->data[ELF_HEADER_IDENT+2]);

  p->version = read_uint32_le (&p->data[ELF_HEADER_IDENT+4]);
  p->entry = read_uint32_le (&p->data[ELF_HEADER_IDENT+8]);
  p->phoff = read_uint32_le (&p->data[ELF_HEADER_IDENT+12]);
  p->shoff = read_uint32_le (&p->data[ELF_HEADER_IDENT+16]);
  p->flags = read_uint32_le (&p->data[ELF_HEADER_IDENT+20]);
  p->ehsize = read_uint16_le (&p->data[ELF_HEADER_IDENT+24]);
  p->phentsize = read_uint16_le (&p->data[ELF_HEADER_IDENT+26]);
  p->phnum = read_uint16_le (&p->data[ELF_HEADER_IDENT+28]);
  p->shentsize = read_uint16_le (&p->data[ELF_HEADER_IDENT+30]);
  p->shnum = read_uint16_le (&p->data[ELF_HEADER_IDENT+32]);
  p->shstrndx = read_uint16_le (&p->data[ELF_HEADER_IDENT+34]);

  if (!check_elf_header (p)) {
    prx_free (p);
    return NULL;
  }

  if (!load_sections (p)) {
    prx_free (p);
    return NULL;
  }

  if (!load_programs (p)) {
    prx_free (p);
    return NULL;
  }

  if (!load_relocs (p)) {
    prx_free (p);
    return NULL;
  }

  if (!load_module_info (p)) {
    prx_free (p);
    return NULL;
  }

  return p;
}

static
void free_sections (struct prx *p)
{
  if (p->sections)
    free (p->sections);
  p->sections = NULL;
}

static
void free_programs (struct prx *p)
{
  if (p->programs)
    free (p->programs);
  p->programs = NULL;
}

void prx_free (struct prx *p)
{
  free_sections (p);
  free_programs (p);
  free_relocs (p);
  free_module_info (p);
  if (p->data)
    free ((void *) p->data);
  p->data = NULL;
  free (p);
}

static
void print_sections (struct prx *p)
{
  uint32 idx;
  struct elf_section *section;
  const char *type = "";

  if (!p->shnum) return;
  report ("\nSection Headers:\n");
  report ("  [Nr]  Name                        Type       Addr     Off      Size     ES Flg Lk Inf Al\n");

  for (idx = 0; idx < p->shnum; idx++) {
    section = &p->sections[idx];
    switch (section->type) {
    case SHT_NOBITS: type = "NOBITS"; break;
    case SHT_PRXRELOC: type = "PRXRELOC"; break;
    case SHT_STRTAB: type = "STRTAB"; break;
    case SHT_PROGBITS: type = "PROGBITS"; break;
    case SHT_NULL: type = "NULL"; break;
    }
    report ("  [%2d] %-28s %-10s %08X %08X %08X %02d %s%s%s %2d %2d  %2d\n",
            idx, section->name, type, section->addr, section->offset, section->size,
            section->entsize, (section->flags & SHF_ALLOC) ? "A" : " ",
            (section->flags & SHF_EXECINSTR) ? "X" : " ", (section->flags & SHF_WRITE) ? "W" : " ",
            section->link, section->info, section->addralign);
  }
}

static
void print_programs (struct prx *p)
{
  uint32 idx;
  struct elf_program *program;
  const char *type = "";

  if (!p->phnum) return;
  report ("\nProgram Headers:\n");
  report ("  Type  Offset     VirtAddr   PhysAddr   FileSiz    MemSiz     Flg Align\n");

  for (idx = 0; idx < p->phnum; idx++) {
    program = &p->programs[idx];
    switch (program->type) {
    case PT_LOAD: type = "LOAD"; break;
    case PT_PRXRELOC: type = "REL"; break;
    case PT_PRXRELOC2: type = "REL2"; break;
    }

    report ("  %-5s 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X %s%s%s 0x%02X\n",
            type, program->offset, program->vaddr, program->paddr, program->filesz,
            program->memsz, (program->flags & PF_X) ? "X" : " ", (program->flags & PF_R) ? "R" : " ",
            (program->flags & PF_W) ? "W" : " ", program->align);
  }
}

void prx_print (struct prx *p, int prtrelocs)
{
  report ("ELF header:\n");
  report ("  Entry point address:        0x%08X\n", p->entry);
  report ("  Start of program headers:   0x%08X\n", p->phoff);
  report ("  Start of section headers:   0x%08X\n", p->shoff);
  report ("  Number of programs:           %8d\n", p->phnum);
  report ("  Number of sections:           %8d\n", p->shnum);

  print_sections (p);
  print_programs (p);
  if (prtrelocs)
    print_relocs (p);
  print_module_info (p);

  report ("\n");
}

uint32 prx_translate (struct prx *p, uint32 vaddr)
{
  uint32 idx;
  for (idx = 0; idx < p->phnum; idx++) {
    struct elf_program *program = &p->programs[idx];
    if (program->type != PT_LOAD) continue;
    if (vaddr >= program->vaddr &&
        (vaddr - program->vaddr) < program->memsz) {
      vaddr -= program->vaddr;
      if (vaddr < program->filesz)
        return vaddr + program->offset;
      else
        return 0;
    }
  }
  return 0;
}
