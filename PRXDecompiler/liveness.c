/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include "code.h"
#include "utils.h"

static
void live_analysis (list worklist)
{
  while (list_size (worklist) != 0) {
    struct basicedge *edge;
    struct basicblock *block, *bref;
    struct subroutine *sub;
    int i, changed, regno;
    element ref;

    block = list_removehead (worklist);
    block->mark1 = 0;
    for (i = 0; i < NUM_REGMASK; i++)
      block->reg_live_out[i] =
        (block->reg_live_in[i] & ~(block->reg_kill[i])) | block->reg_gen[i];

    ref = list_head (block->inrefs);
    while (ref) {
      changed = FALSE;

      edge = element_getvalue (ref);
      bref = edge->from;

      for (i = 0; i < NUM_REGMASK; i++) {
        changed = changed || (block->reg_live_out[i] & (~bref->reg_live_in[i]));
        bref->reg_live_in[i] |= block->reg_live_out[i];
      }

      if (changed && !bref->mark1) {
        list_inserttail (worklist, bref);
        bref->mark1 = 1;
      }

      ref = element_next (ref);
    }

    sub = block->info.call.calltarget;
    if (block->type == BLOCK_CALL && sub) {

      for (regno = REGISTER_GPR_V0; regno <= REGISTER_GPR_V1; regno++) {
        if (IS_BIT_SET (block->reg_live_in, regno)) {
          sub->numregout = MAX (sub->numregout, regno - REGISTER_GPR_V0 + 1);
        }
      }

      if (sub->status & SUB_STAT_OPERATIONS_EXTRACTED) {
        changed = FALSE;
        for (regno = REGISTER_GPR_V0; regno <= REGISTER_GPR_V1; regno++) {
          if (IS_BIT_SET (block->reg_live_in, regno)) {
            changed = changed || !IS_BIT_SET (sub->endblock->reg_gen, regno);
            BIT_SET (sub->endblock->reg_gen, regno);
          }
        }
        if (changed && !sub->endblock->mark1) {
          list_inserttail (worklist, sub->endblock);
          sub->endblock->mark1 = 1;
        }
      }
    }

    if (block->type == BLOCK_START) {
      sub = block->sub;

      for (regno = REGISTER_GPR_A0; regno <= REGISTER_GPR_T3; regno++) {
        if (IS_BIT_SET (block->reg_live_in, regno)) {
          sub->numregargs = MAX (sub->numregargs, regno - REGISTER_GPR_A0 + 1);
        }
      }

      ref = list_head (sub->whereused);
      while (ref) {
        bref = element_getvalue (ref);
        changed = FALSE;
        for (regno = REGISTER_GPR_A0; regno <= REGISTER_GPR_T3; regno++) {
          if (IS_BIT_SET (block->reg_live_in, regno)) {
            changed = changed || !IS_BIT_SET (bref->reg_gen, regno);
            BIT_SET (bref->reg_gen, regno);
          }
        }
        if (changed && !bref->mark1) {
          list_inserttail (worklist, bref);
          bref->mark1 = 1;
        }
        ref = element_next (ref);
      }
    }
  }
}

void live_registers (struct code *c)
{
  list worklist = list_alloc (c->lstpool);
  element el = list_head (c->subroutines);

  while (el) {
    struct subroutine *sub = element_getvalue (el);
    if (!sub->import && !sub->haserror) {
      reset_marks (sub);
      sub->status |= SUB_STAT_LIVE_REGISTERS;
      sub->endblock->mark1 = 1;
      list_inserthead (worklist, sub->endblock);
    }
    el = element_next (el);
  }

  live_analysis (worklist);
  list_free (worklist);
}

void live_registers_imports (struct code *c)
{
  element el = list_head (c->subroutines);

  while (el) {
    struct subroutine *sub = element_getvalue (el);
    if (sub->import && sub->numregargs == -1) {
      element ref;

      ref = list_head (sub->whereused);
      while (ref) {
        struct basicblock *block = element_getvalue (ref);
        struct operation *op = list_tailvalue (block->operations);
        int count = 0, maxcount = 0;
        element opel;

        opel = list_head (op->info.callop.arguments);
        while (opel) {
          struct value *val = element_getvalue (opel);
          count++;

          if (list_size (val->val.variable->uses) == 1 &&
              val->val.variable->def->type != OP_START &&
              val->val.variable->def->type != OP_CALL) {
            if (maxcount < count) maxcount = count;
          }
          opel = element_next (opel);
        }

        if (sub->numregargs < maxcount)
          sub->numregargs = maxcount;

        ref = element_next (ref);
      }

      ref = list_head (sub->whereused);
      while (ref) {
        struct basicblock *block = element_getvalue (ref);
        struct subroutine *target = block->sub;

        target->status &= ~(SUB_STAT_SSA | SUB_STAT_FIXUP_CALL_ARGS);
        ref = element_next (ref);
      }
    }
    el = element_next (el);
  }

  el = list_head (c->subroutines);

  while (el) {
    struct subroutine *sub = element_getvalue (el);
    if (!sub->import && !sub->haserror &&
        !(sub->status & SUB_STAT_SSA)) {
      unbuild_ssa (sub);
      remove_call_arguments (sub);
    }
    el = element_next (el);
  }

}
