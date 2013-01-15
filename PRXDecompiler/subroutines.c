/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include "code.h"
#include "utils.h"


static
void mark_reachable (struct code *c, struct location *loc)
{
  uint32 remaining = 1 + ((c->end->address - loc->address) >> 2);
  for (; remaining--; loc++) {
    if (loc->reachable == LOCATION_REACHABLE) break;
    loc->reachable = LOCATION_REACHABLE;

    if (!loc->insn) return;

    if (loc->insn->flags & INSN_JUMP) {
      if (remaining > 0) {
        if (loc[1].reachable != LOCATION_REACHABLE)
          loc[1].reachable = LOCATION_DELAY_SLOT;
      }

      if (loc->target)
        mark_reachable (c, loc->target);

      if (loc->cswitch) {
        if (loc->cswitch->checked) {
          element el = list_head (loc->cswitch->references);
          while (el) {
            struct location *target = element_getvalue (el);
            mark_reachable (c, target);
            if (!target->references)
              target->references = list_alloc (c->lstpool);
            if (target->cswitch != loc->cswitch)
              list_inserttail (target->references, loc);
            target->cswitch = loc->cswitch;
            el = element_next (el);
          }
        }
      }

      if ((remaining == 0) || !(loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D)))
        return;

      loc++;
      remaining--;
    } else if (loc->insn->flags & INSN_BRANCH) {
      if (remaining > 0) {
        if (loc[1].reachable != LOCATION_REACHABLE)
          loc[1].reachable = LOCATION_DELAY_SLOT;
      }

      if (loc->target) {
        mark_reachable (c, loc->target);
      }

      if ((remaining == 0) || (loc->branchalways && !(loc->insn->flags & INSN_LINK)))
        return;

      loc++;
      remaining--;
    }
  };
}

static
void new_subroutine (struct code *c, struct location *loc, struct prx_function *imp, struct prx_function *exp)
{
  struct subroutine *sub = loc->sub;

  if (!sub) {
    sub = fixedpool_alloc (c->subspool);
    sub->begin = loc;
    sub->code = c;
    sub->whereused = list_alloc (c->lstpool);
    sub->callblocks = list_alloc (c->lstpool);
    loc->sub = sub;
  }
  if (imp) sub->import = imp;
  if (exp) sub->export = exp;

  if (sub->import && sub->export) {
    sub->haserror = TRUE;
    error (__FILE__ ": location 0x%08X is both import and export", loc->address);
  }

  mark_reachable (c, loc);
}

static
void extract_from_relocs (struct code *c)
{
  uint32 i, tgt;

  i = prx_findreloc (c->file, c->baddr);
  for (; i < c->file->relocnum; i++) {
    struct location *loc;
    struct prx_reloc *rel = &c->file->relocs[i];

    tgt = (rel->target - c->baddr) >> 2;
    if (tgt >= c->numopc) continue;

    if (rel->target & 0x03) {
      error (__FILE__ ": relocation not word aligned 0x%08X", rel->target);
      continue;
    }

    loc = &c->base[tgt];

    if (rel->type == R_MIPSX_JAL26) {
      new_subroutine (c, loc, NULL, NULL);
    } else if (rel->type == R_MIPS_26) {
      struct location *calledfrom;
      uint32 ctgt;

      if (rel->vaddr < c->baddr) continue;
      if (rel->vaddr & 0x03) {
        error (__FILE__ ": relocation address not word aligned 0x%08X", rel->vaddr);
        continue;
      }

      ctgt = (rel->vaddr - c->baddr) >> 2;
      if (ctgt >= c->numopc) continue;

      calledfrom = &c->base[ctgt];
      if (calledfrom->insn->insn == I_JAL) {
        new_subroutine (c, loc, NULL, NULL);
      }
    } else if (rel->type == R_MIPS_32) {
      if (!loc->cswitch)
        new_subroutine (c, loc, NULL, NULL);
    } else if (rel->type == R_MIPS_HI16 || rel->type == R_MIPSX_HI16) {
      /* TODO: is this OK to do? */
      if (!loc->cswitch)
        new_subroutine (c, loc, NULL, NULL);
    }
  }
}

static
void extract_from_exports (struct code *c)
{
  uint32 i, j, tgt;

  for (i = 0; i < c->file->modinfo->numexports; i++) {
    struct prx_export *exp;

    exp = &c->file->modinfo->exports[i];
    for (j = 0; j < exp->nfuncs; j++) {
      struct prx_function *func = &exp->funcs[j];
      struct location *loc;

      tgt = (func->vaddr - c->baddr) >> 2;
      if (func->vaddr < c->baddr ||
          tgt >= c->numopc) {
        error (__FILE__ ": invalid exported function");
        continue;
      }

      loc = &c->base[tgt];
      new_subroutine (c, loc, NULL, func);
      func->pfunc = loc->sub;
    }
  }
}

static
void extract_from_imports (struct code *c)
{
  uint32 i, j, tgt;

  for (i = 0; i < c->file->modinfo->numimports; i++) {
    struct prx_import *imp;

    imp = &c->file->modinfo->imports[i];
    for (j = 0; j < imp->nfuncs; j++) {
      struct prx_function *func = &imp->funcs[j];
      struct location *loc;

      tgt = (func->vaddr - c->baddr) >> 2;
      if (func->vaddr < c->baddr ||
          tgt >= c->numopc) {
        error (__FILE__ ": invalid imported function");
        continue;
      }

      loc = &c->base[tgt];
      new_subroutine (c, loc, func, NULL);
      func->pfunc = loc->sub;
      loc->sub->numregargs = func->numargs;
    }
  }
}

static
struct subroutine *find_sub (struct code *c, struct location *loc)
{
  do {
    if (loc->sub) return loc->sub;
  } while (loc-- != c->base);
  return NULL;
}

static
void extract_hidden_subroutines (struct code *c)
{
  struct subroutine *cursub = NULL;
  uint32 i;
  int changed = TRUE;

  /* TODO: improve the hidden subroutine detection algorithm */
  while (changed) {
    changed = FALSE;
    for (i = 0; i < c->numopc; i++) {
      struct location *loc = &c->base[i];
      if (loc->sub) cursub = loc->sub;
      if (loc->reachable == LOCATION_UNREACHABLE) continue;

      if (loc->target) {
        struct location *target;
        struct subroutine *targetsub;

        target = loc->target;
        targetsub = find_sub (c, target);

        if (!target->sub && (targetsub != cursub)) {
          report (__FILE__ ": hidden subroutine at 0x%08X (called by 0x%08X)\n", target->address, loc->address);
          new_subroutine (c, target, NULL, NULL);
          changed = TRUE;
        }
      }
    }
  }
}



static
void delimit_borders (struct code *c)
{
  struct subroutine *prevsub = NULL;
  uint32 i;

  for (i = 0; i < c->numopc; i++) {
    if (c->base[i].sub) {
      list_inserttail (c->subroutines, c->base[i].sub);
      if (prevsub) {
        prevsub->end = &c->base[i - 1];
      }
      prevsub = c->base[i].sub;
    } else {
      c->base[i].sub = prevsub;
    }
  }
  if (prevsub) {
    prevsub->end = &c->base[i - 1];
  }
}


static
void check_switches (struct subroutine *sub)
{
  struct location *loc;
  loc = sub->begin;
  do {
    if (!loc->cswitch) continue;
    if (loc->cswitch->jumplocation == loc) {
      element el;
      int haserror = FALSE;

      el = list_head (loc->cswitch->references);
      while (el) {
        struct location *target = element_getvalue (el);
        if (target->sub != loc->sub) haserror = TRUE;
        target->cswitch = NULL;
        el = element_next (el);
      }

      if (haserror) {
        report (__FILE__ ": invalid switch at 0x%08X\n", loc->address);
        fixedpool_free (sub->code->switchpool, loc->cswitch);
        loc->cswitch = NULL;
      } else {
        loc->cswitch->checked = TRUE;
        if (loc->reachable == LOCATION_REACHABLE) {
          loc->reachable = LOCATION_UNREACHABLE;
          mark_reachable (sub->code, loc);
        }
      }
    }
  } while (loc++ != sub->end);
}

static
void check_subroutine (struct subroutine *sub)
{
  struct location *loc;
  loc = sub->begin;

  if ((sub->end->address - sub->begin->address) < 4) {
    error (__FILE__ ": subroutine is too short: 0x%08X", sub->begin->address);
    sub->haserror = TRUE;
    return;
  }

  do {
    if (loc->reachable == LOCATION_UNREACHABLE) continue;

    if (loc->error != ERROR_NONE) {
      switch (loc->error) {
      case ERROR_INVALID_OPCODE:
        error (__FILE__ ": invalid opcode 0x%08X at 0x%08X (sub: 0x%08X)", loc->opc, loc->address, sub->begin->address);
        sub->haserror = TRUE;
        break;
      case ERROR_TARGET_OUTSIDE_FILE:
        error (__FILE__ ": branch/jump outside file at 0x%08X (sub: 0x%08X)", loc->address, sub->begin->address);
        sub->haserror = TRUE;
        break;
      case ERROR_DELAY_SLOT:
        error (__FILE__ ": delay slot error at 0x%08X (sub: 0x%08X)", loc->address, sub->begin->address);
        sub->haserror = TRUE;
        break;
      case ERROR_ILLEGAL_BRANCH:
        error (__FILE__ ": illegal branch at 0x%08X (sub: 0x%08X)", loc->address, sub->begin->address);
        sub->haserror = TRUE;
        break;
      case ERROR_NONE:
        break;
      }
    }

    if (sub->haserror) continue;


    if (loc->target) {
      if (!loc->target->references)
        loc->target->references = list_alloc (sub->code->lstpool);
      list_inserttail (loc->target->references, loc);
    }
  } while (loc++ != sub->end);
  loc--;

  if (loc->reachable == LOCATION_UNREACHABLE) return;
  loc--;

  if ((loc->insn->flags & (INSN_JUMP | INSN_LINK | INSN_WRITE_GPR_D)) == INSN_JUMP)
    return;

  if ((loc->insn->flags & (INSN_BRANCH | INSN_LINK)) == INSN_BRANCH && loc->branchalways)
    return;

  error (__FILE__ ": subroutine at 0x%08X (end 0x%08X) has no finish", sub->begin->address, sub->end->address);
  sub->haserror = TRUE;
}

void extract_subroutines (struct code *c)
{
  element el;

  c->subroutines = list_alloc (c->lstpool);

  extract_from_exports (c);
  extract_from_imports (c);
  extract_from_relocs (c);

  if (!c->base->sub) {
    error (__FILE__ ": creating artificial subroutine at address 0x%08X", c->baddr);
    new_subroutine (c, c->base, NULL, NULL);
  }

  extract_hidden_subroutines (c);
  delimit_borders (c);


  el = list_head (c->subroutines);
  while (el) {
    struct subroutine *sub = element_getvalue (el);
    if (!sub->import) {
      check_switches (sub);
      check_subroutine (sub);

      if (!sub->haserror) {
        sub->status |= SUB_STAT_EXTRACTED;
        extract_cfg (sub);
      }

      if (!sub->haserror) {
        sub->status |= SUB_STAT_CFG_EXTRACTED;
        extract_operations (sub);
      }

      if (!sub->haserror) {
        sub->status |= SUB_STAT_OPERATIONS_EXTRACTED;
      }
    }
    el = element_next (el);
  }
}

