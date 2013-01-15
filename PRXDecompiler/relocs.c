/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stdlib.h>
#include <string.h>

#include "prx.h"
#include "utils.h"



static
int cmp_relocs (const void *p1, const void *p2)
{
  const struct prx_reloc *r1 = p1;
  const struct prx_reloc *r2 = p2;
  if (r1->target < r2->target) return -1;
  if (r1->target > r2->target) return 1;
  return 0;
}

static
int cmp_relocs_by_addr (const void *p1, const void *p2)
{
  const struct prx_reloc *r1 = p1;
  const struct prx_reloc *r2 = p2;
  if (r1->vaddr < r2->vaddr) return -1;
  if (r1->vaddr > r2->vaddr) return 1;
  return 0;
}

static
int check_apply_relocs (struct prx *p)
{
  struct prx_reloc *r, *lastxhi = NULL;
  struct elf_program *offsbase;
  struct elf_program *addrbase;
  uint32 index, addend, base, temp;
  uint32 hiaddr, loaddr;


  for (index = 0; index < p->relocnum; index++) {
    r = &p->relocs[index];
    if (r->offsbase >= p->phnum) {
      error (__FILE__ ": invalid offset base for relocation (%d)", r->offsbase);
      return 0;
    }

    if (r->addrbase >= p->phnum) {
      error (__FILE__ ": invalid address base for relocation (%d)", r->offsbase);
      return 0;
    }

    offsbase = &p->programs[r->offsbase];
    addrbase = &p->programs[r->addrbase];

    r->vaddr = r->offset + offsbase->vaddr;
    if (!prx_inside_progfile (offsbase, r->vaddr, 4)) {
      error (__FILE__ ": relocation points to invalid address (0x%08X)", r->vaddr);
      return 0;
    }
  }

  for (index = 0; index < p->relocnum; index++) {
    r = &p->relocs[index];
    offsbase = &p->programs[r->offsbase];
    addrbase = &p->programs[r->addrbase];

    addend = read_uint32_le (&offsbase->data[r->offset]);

    switch (r->type) {
    case R_MIPS_NONE:
      break;
    case R_MIPS_26:
    case R_MIPSX_J26:
    case R_MIPSX_JAL26:
      r->target = (r->offset + offsbase->vaddr) & 0xF0000000;
      r->target = (((addend & 0x3FFFFFF) << 2) | r->target) + addrbase->vaddr;
      addend = (addend & ~0x3FFFFFF) | (r->target >> 2);
      if (!prx_inside_progfile (addrbase, r->target, 8)) {
        error (__FILE__ ": mips26 reference out of range at 0x%08X (0x%08X)", r->vaddr, r->target);
      }
      write_uint32_le ((uint8 *)&offsbase->data[r->offset], addend);
      break;
    case R_MIPS_HI16:
      base = index;
      while (++index < p->relocnum) {
        if (p->relocs[index].type != R_MIPS_HI16) break;
        if (p->relocs[index].offsbase != r->offsbase) {
          error (__FILE__ ": changed offset base");
          return 0;
        }
        if (p->relocs[index].addrbase != r->addrbase) {
          error (__FILE__ ": changed offset base");
          return 0;
        }
        temp = read_uint32_le (&offsbase->data[p->relocs[index].offset]) & 0xFFFF;
        if (temp != (addend & 0xFFFF)) {
          error (__FILE__ ": changed hi");
          return 0;
        }
      }

      if (index == p->relocnum) {
        error (__FILE__ ": hi16 without matching lo16");
        return 0;
      }

      if (p->relocs[index].type != R_MIPS_LO16 ||
          p->relocs[index].offsbase != r->offsbase ||
          p->relocs[index].addrbase != r->addrbase) {
        error (__FILE__ ": hi16 without matching lo16");
        return 0;
      }

      temp = read_uint32_le (&offsbase->data[p->relocs[index].offset]) & 0xFFFF;
      if (temp & 0x8000) temp |= ~0xFFFF;

      r->target = ((addend & 0xFFFF) << 16) + addrbase->vaddr + temp;
      addend = temp & 0xFFFF;
      if (!prx_inside_progmem (addrbase, r->target, 1)) {
        error (__FILE__ ": hi16 reference out of range at 0x%08X (0x%08X)", r->vaddr, r->target);
      }

      loaddr = r->target & 0xFFFF;
      hiaddr = (((r->target >> 15) + 1) >> 1) & 0xFFFF;

      while (base < index) {
        p->relocs[base].target = r->target;
        temp = (read_uint32_le (&offsbase->data[p->relocs[base].offset]) & ~0xFFFF) | hiaddr;
        write_uint32_le ((uint8 *) &offsbase->data[p->relocs[base].offset], temp);
        base++;
      }

      while (index < p->relocnum) {
        temp = read_uint32_le (&offsbase->data[p->relocs[index].offset]);
        if ((temp & 0xFFFF) != addend) break;
        if (p->relocs[index].type != R_MIPS_LO16) break;
        if (p->relocs[index].offsbase != r->offsbase) break;
        if (p->relocs[index].addrbase != r->addrbase) break;

        p->relocs[index].target = r->target;

        temp = (temp & ~0xFFFF) | loaddr;
        write_uint32_le ((uint8 *) &offsbase->data[p->relocs[index].offset], temp);
        index++;
      }
      index--;
      break;
    case R_MIPSX_HI16:
      r->target = ((addend & 0xFFFF) << 16) + addrbase->vaddr + r->addend;
      addend = (addend & ~0xFFFF) | ((((r->target >> 15) + 1) >> 1) & 0xFFFF);
      if (!prx_inside_progmem (addrbase, r->target, 1)) {
        error (__FILE__ ": xhi16 reference out of range at 0x%08X (0x%08X)", r->vaddr, r->target);
      }
      write_uint32_le ((uint8 *)&offsbase->data[r->offset], addend);
      lastxhi = r;
      break;

    case R_MIPS_16:
    case R_MIPS_LO16:
      r->target = (addend & 0xFFFF) + addrbase->vaddr;
      if (lastxhi) {
        if ((lastxhi->target & 0xFFFF) == (r->target & 0xFFFF) &&
            lastxhi->addrbase == r->addrbase &&
            lastxhi->offsbase == r->offsbase) {
          r->target = lastxhi->target;
        }
      }
      addend = (addend & ~0xFFFF) | (r->target & 0xFFFF);
      if (!prx_inside_progmem (addrbase, r->target, 1)) {
        error (__FILE__ ": lo16 reference out of range at 0x%08X (0x%08X)", r->vaddr, r->target);
      }
      write_uint32_le ((uint8 *)&offsbase->data[r->offset], addend);
      break;

    case R_MIPS_32:
      r->target = addend + addrbase->vaddr;
      addend = r->target;
      /*if (!inside_progmem (addrbase, r->target, 1)) {
        error (__FILE__ ": mips32 reference out of range at 0x%08X (0x%08X)", r->vaddr, r->target);
      }*/
      write_uint32_le ((uint8 *)&offsbase->data[r->offset], addend);
      break;

    default:
      error (__FILE__ ": invalid reference type %d", r->type);
      return 0;
    }

  }


  p->relocsbyaddr = xmalloc (p->relocnum * sizeof (struct prx_reloc));
  memcpy (p->relocsbyaddr, p->relocs, p->relocnum * sizeof (struct prx_reloc));

  qsort (p->relocs, p->relocnum, sizeof (struct prx_reloc), &cmp_relocs);
  qsort (p->relocsbyaddr, p->relocnum, sizeof (struct prx_reloc), &cmp_relocs_by_addr);

  return 1;
}


static
uint32 count_relocs_b (uint32 prgidx, const uint8 *data, uint32 size)
{
  const uint8 *end;
  uint8 part1s, part2s;
  uint32 block1s, block2s;
  uint8 block1[256], block2[256];
  uint32 temp1, temp2, part1, part2;
  uint32 count = 0, nbits;

  end = data + size;
  for (nbits = 1; (1 << nbits) < prgidx; nbits++) {
    if (nbits >= 33) {
      error (__FILE__  ": invalid number of bits for indexes");
      return 0;
    }
  }

  if (read_uint16_le (data) != 0) {
    error (__FILE__  ": invalid header for relocation");
    return 0;
  }

  part1s = data[2];
  part2s = data[3];

  block1s = data[4];
  data += 4;

  if (block1s) {
    memcpy (block1, data, block1s);
    data += block1s;
  }

  block2s = *data;
  if (block2s) {
    memcpy (block2, data, block2s);
    data += block2s;
  }


  count = 0;
  while (data < end) {
    uint32 cmd = read_uint16_le (data);
    temp1 = (cmd << (16 - part1s)) & 0xFFFF;
    temp1 = (temp1 >> (16 -part1s)) & 0xFFFF;

    data = data + 2;
    if (temp1 >= block1s) {
      error (__FILE__ ": invalid index for the first part");
      return 0;
    }
    part1 = block1[temp1];
    if ((part1 & 0x06) == 0x06) {
      error (__FILE__ ": invalid size");
      return 0;
    }

    data += part1 & 0x06;

    if ((part1 & 0x01) == 0) {
      if ((part1 & 0x06) == 2) {
        error (__FILE__ ": invalid size of part1");
        return 0;
      }
    } else {
      temp2 = (cmd << (16 - (part1s + nbits + part2s))) & 0xFFFF;
      temp2 = (temp2 >> (16 - part2s)) & 0xFFFF;
      if (temp2 >= block2s) {
        error (__FILE__ ": invalid index for the second part");
        return 0;
      }

      part2 = block2[temp2];

      switch (part1 & 0x38) {
      case 0x00:
        break;
      case 0x08:
        break;
      case 0x10:
        data += 2;
        break;
      default:
        error (__FILE__ ": invalid addendum size");
        return 0;
      }

      switch (part2) {
      case 1: case 2: case 3:
      case 4: case 5: case 6: case 7:
        count++;
        break;
      case 0:
        break;
      default:
        error (__FILE__ ": invalid relocation type %d", part2);
        return 0;
      }
    }
  }

  return count;
}

static
int load_relocs_b (struct elf_program *programs, struct prx_reloc *out, uint32 prgidx, const uint8 *data, uint32 size)
{
  const uint8 *end;
  uint32 nbits;
  uint8 part1s, part2s;
  uint32 block1s, block2s;
  uint8 block1[256], block2[256];
  uint32 temp1, temp2;
  uint32 part1, part2, lastpart2;
  uint32 addend = 0, offset = 0;
  uint32 offsbase = 0xFFFFFFFF;
  uint32 addrbase;
  uint32 count;

  end = data + size;
  for (nbits = 1; (1 << nbits) < prgidx; nbits++) {
  }

  part1s = data[2];
  part2s = data[3];

  block1s = data[4];
  data += 4;

  if (block1s) {
    memcpy (block1, data, block1s);
    data += block1s;
  }

  block2s = *data;
  if (block2s) {
    memcpy (block2, data, block2s);
    data += block2s;
  }


  count = 0;
  lastpart2 = block2s;
  while (data < end) {
    uint32 cmd = read_uint16_le (data);
    temp1 = (cmd << (16 - part1s)) & 0xFFFF;
    temp1 = (temp1 >> (16 -part1s)) & 0xFFFF;

    data += 2;
    part1= block1[temp1];

    if ((part1 & 0x01) == 0) {
      offsbase = (cmd << (16 - part1s - nbits)) & 0xFFFF;
      offsbase = (offsbase >> (16 - nbits)) & 0xFFFF;
      if (!(offsbase < prgidx)) {
        error (__FILE__ ": invalid offset base");
        return 0;
      }

      offset = cmd >> (part1s + nbits);
      if ((part1 & 0x06) == 0) continue;
      offset = read_uint32_le (data);
      data += 4;
    } else {
      temp2 = (cmd << (16 - (part1s + nbits + part2s))) & 0xFFFF;
      temp2 = (temp2 >> (16 - part2s)) & 0xFFFF;

      addrbase = (cmd << (16 - part1s - nbits)) & 0xFFFF;
      addrbase = (addrbase >> (16 - nbits)) & 0xFFFF;
      if (!(addrbase < prgidx)) {
        error (__FILE__ ": invalid address base");
        return 0;
      }
      part2 = block2[temp2];

      switch (part1 & 0x06) {
      case 0:
        if (cmd & 0x8000) {
          cmd |= ~0xFFFF;
          cmd >>= part1s + part2s + nbits;
          cmd |= ~0xFFFF;
        } else {
          cmd >>= part1s + part2s + nbits;
        }
        offset += cmd;
        break;
      case 2:
        if (cmd & 0x8000) cmd |= ~0xFFFF;
        cmd = (cmd >> (part1s + part2s + nbits)) << 16;
        cmd |= read_uint16_le (data);
        offset += cmd;
        data += 2;
        break;
      case 4:
        offset = read_uint32_le (data);
        data += 4;
        break;
      }

      if (!(offset < programs[offsbase].filesz)) {
        error (__FILE__ ": invalid relocation offset");
        return 0;
      }

      switch (part1 & 0x38) {
      case 0x00:
        addend = 0;
        break;
      case 0x08:
        if ((lastpart2 ^ 0x04) != 0) {
          addend = 0;
        }
        break;
      case 0x10:
        addend = read_uint16_le (data);
        data += 2;
        break;
      }

      lastpart2 = part2;

      out[count].addrbase = addrbase;
      out[count].offsbase = offsbase;
      out[count].offset = offset;
      out[count].extra = 0;

      switch (part2) {
      case 2:
        out[count++].type = R_MIPS_32;
        break;
      case 0:
        break;
      case 3:
        out[count++].type = R_MIPS_26;
        break;
      case 4:
        if (addend & 0x8000) addend |= ~0xFFFF;
        out[count].addend = addend;
        out[count++].type = R_MIPSX_HI16;
        break;
      case 1:
      case 5:
        out[count++].type = R_MIPS_LO16;
        break;
      case 6:
        out[count++].type = R_MIPSX_J26;
        break;
      case 7:
        out[count++].type = R_MIPSX_JAL26;
        break;
      }
    }
  }

  return count;
}


int load_relocs (struct prx *p)
{
  uint32 i, ret, count = 0;

  for (i = 0; i < p->shnum; i++) {
    struct elf_section *section = &p->sections[i];
    if (section->type == SHT_PRXRELOC) {
      count += section->size >> 3;
    }
  }
  for (i = 0; i < p->phnum; i++) {
    struct elf_program *program = &p->programs[i];
    if (program->type == PT_PRXRELOC) {
      count += program->filesz >> 3;
    } else if (program->type == PT_PRXRELOC2) {
      ret = count_relocs_b (i, program->data, program->filesz);
      if (!ret) return 0;
      count += ret;
    }
  }

  p->relocs = NULL;
  if (!count) {
    error (__FILE__ ": no relocation found");
    return 0;
  }

  p->relocnum = count;
  p->relocs = (struct prx_reloc *) xmalloc (count * sizeof (struct prx_reloc));
  memset (p->relocs, 0, count * sizeof (struct prx_reloc));

  count = 0;
  for (i = 0; i < p->shnum; i++) {
    struct elf_section *section = &p->sections[i];
    if (section->type == SHT_PRXRELOC) {
      uint32 j, secsize;
      uint32 offset;
      offset = section->offset;
      secsize = section->size >> 3;
      for (j = 0; j < secsize; j++) {
        p->relocs[count].offset = read_uint32_le (&p->data[offset]);
        p->relocs[count].type = p->data[offset + 4];
        p->relocs[count].offsbase = p->data[offset + 5];
        p->relocs[count].addrbase = p->data[offset + 6];
        p->relocs[count].extra = p->data[offset + 7];

        count++;
        offset += 8;
      }
    }
  }

  for (i = 0; i < p->phnum; i++) {
    struct elf_program *program = &p->programs[i];
    if (program->type == PT_PRXRELOC) {
      uint32 j, progsize;
      uint32 offset;
      offset = program->offset;
      progsize = program->filesz >> 3;
      for (j = 0; j < progsize; j++) {
        p->relocs[count].offset = read_uint32_le (&p->data[offset]);
        p->relocs[count].type = p->data[offset + 4];
        p->relocs[count].offsbase = p->data[offset + 5];
        p->relocs[count].addrbase = p->data[offset + 6];
        p->relocs[count].extra = p->data[offset + 7];

        count++;
        offset += 8;
      }
    } else if (program->type == PT_PRXRELOC2) {
      ret = load_relocs_b (p->programs, &p->relocs[count], i, program->data, program->filesz);
      if (!ret) {
        return 0;
      }
      count += ret;
    }
  }

  if (!check_apply_relocs (p)) return 0;

  return 1;
}

void free_relocs (struct prx *p)
{
  if (p->relocs)
    free (p->relocs);
  p->relocs = NULL;

  if (p->relocsbyaddr)
    free (p->relocsbyaddr);
  p->relocsbyaddr = NULL;
}



uint32 prx_findreloc (struct prx *p, uint32 target)
{
  uint32 first, last, i;

  first = 0;
  last = p->relocnum;
  while (first < last) {
    i = (first + last) / 2;
    if (p->relocs[i].target < target) {
      first = i + 1;
    } else {
      last = i;
    }
  }

  return first;
}

uint32 prx_findrelocbyaddr (struct prx *p, uint32 vaddr)
{
  uint32 first, last, i;

  first = 0;
  last = p->relocnum;
  while (first < last) {
    i = (first + last) / 2;
    if (p->relocsbyaddr[i].vaddr < vaddr) {
      first = i + 1;
    } else {
      last = i;
    }
  }

  return first;
}


void print_relocs (struct prx *p)
{
  uint32 i;
  report ("\nRelocs:\n");
  for (i = 0; i < p->relocnum; i++) {
    const char *type = "unk";

    switch (p->relocs[i].type) {
    case R_MIPSX_HI16:  type = "xhi16"; break;
    case R_MIPSX_J26:   type = "xj26"; break;
    case R_MIPSX_JAL26: type = "xjal26"; break;
    case R_MIPS_16:     type = "mips16"; break;
    case R_MIPS_26:     type = "mips26"; break;
    case R_MIPS_32:     type = "mips32"; break;
    case R_MIPS_HI16:   type = "hi16"; break;
    case R_MIPS_LO16:   type = "lo16"; break;
    case R_MIPS_NONE:   type = "none"; break;
    }
    report ("  Type: %8s Vaddr: 0x%08X Target: 0x%08X Addend: 0x%08X\n",
        type, p->relocs[i].vaddr, p->relocs[i].target, p->relocs[i].addend);
  }
}




