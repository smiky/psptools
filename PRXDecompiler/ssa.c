/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include "code.h"
#include "utils.h"

static
struct ssavar *alloc_variable (struct basicblock *block)
{
  struct ssavar *var;
  var = fixedpool_alloc (block->sub->code->ssavarspool);
  var->uses = list_alloc (block->sub->code->lstpool);
  return var;
}

static
void ssa_place_phis (struct subroutine *sub, list *defblocks)
{
  struct basicblock *block, *bref;
  struct basicblocknode *brefnode;
  element el, ref;
  int regno, i;

  el = list_head (sub->blocks);
  while (el) {
    block = element_getvalue (el);
    block->mark1 = 0;
    block->mark2 = 0;
    el = element_next (el);
  }

  for (regno = 1; regno < NUM_REGISTERS; regno++) {
    list worklist = defblocks[regno];
    el = list_head (worklist);
    while (el) {
      block = element_getvalue (el);
      block->mark1 = regno;
      el = element_next (el);
    }

    while (list_size (worklist) != 0) {
      block = list_removehead (worklist);
      ref = list_head (block->node.frontier);
      while (ref) {
        brefnode = element_getvalue (ref);
        bref = element_getvalue (brefnode->blockel);
        if (bref->mark2 != regno && IS_BIT_SET (bref->reg_live_out, regno)) {
          struct operation *op;

          bref->mark2 = regno;
          op = operation_alloc (bref);
          op->type = OP_PHI;
          value_append (sub, op->results, VAL_REGISTER, regno, FALSE);
          for (i = list_size (bref->inrefs); i > 0; i--)
            value_append (sub, op->operands, VAL_REGISTER, regno, FALSE);
          list_inserthead (bref->operations, op);

          if (bref->mark1 != regno) {
            bref->mark1 = regno;
            list_inserttail (worklist, bref);
          }
        }
        ref = element_next (ref);
      }
    }
  }
}

static
void ssa_search (struct basicblock *block, list *vars)
{
  element el;
  int regno, pushed[NUM_REGISTERS];

  for (regno = 1; regno < NUM_REGISTERS; regno++)
    pushed[regno] = FALSE;

  el = list_head (block->operations);
  while (el) {
    struct operation *op;
    struct ssavar *var;
    struct value *val;
    element opel, rel;

    op = element_getvalue (el);

    if (op->type != OP_PHI) {
      opel = list_head (op->operands);
      while (opel) {
        val = element_getvalue (opel);
        if (val->type == VAL_REGISTER) {
          var = list_headvalue (vars[val->val.intval]);
          val->type = VAL_SSAVAR;
          val->val.variable = var;
          if (op->type == OP_ASM)
            var->status |= VAR_STAT_ASMARG;
          list_inserttail (var->uses, op);
        }
        opel = element_next (opel);
      }
    }

    rel = list_head (op->results);
    while (rel) {
      val = element_getvalue (rel);
      if (val->type == VAL_REGISTER) {
        val->type = VAL_SSAVAR;
        var = alloc_variable (block);
        var->name.type = VAL_REGISTER;
        var->name.val.intval = val->val.intval;
        var->def = op;
        list_inserttail (block->sub->ssavars, var);
        if (!pushed[val->val.intval]) {
          pushed[val->val.intval] = TRUE;
          list_inserthead (vars[val->val.intval], var);
        } else {
          element_setvalue (list_head (vars[val->val.intval]), var);
        }
        val->val.variable = var;
      }
      rel = element_next (rel);
    }

    el = element_next (el);
  }

  el = list_head (block->outrefs);
  while (el) {
    struct basicedge *edge;
    struct basicblock *ref;
    element phiel, opel;

    edge = element_getvalue (el);
    ref = edge->to;

    phiel = list_head (ref->operations);
    while (phiel) {
      struct operation *op;
      struct ssavar *var;
      struct value *val;

      op = element_getvalue (phiel);
      if (op->type != OP_PHI) break;

      opel = list_head (op->operands);
      for (regno = edge->tonum; regno > 0; regno--)
        opel = element_next (opel);

      val = element_getvalue (opel);
      val->type = VAL_SSAVAR;
      var = val->val.variable = list_headvalue (vars[val->val.intval]);
      var->status |= VAR_STAT_PHIARG;
      list_inserttail (var->uses, op);
      phiel = element_next (phiel);
    }
    el = element_next (el);
  }

  el = list_head (block->node.children);
  while (el) {
    struct basicblocknode *childnode;
    struct basicblock *child;

    childnode = element_getvalue (el);
    child = element_getvalue (childnode->blockel);
    ssa_search (child, vars);
    el = element_next (el);
  }

  for (regno = 1; regno < NUM_REGISTERS; regno++)
    if (pushed[regno]) list_removehead (vars[regno]);
}

void build_ssa (struct subroutine *sub)
{
  list reglist[NUM_REGISTERS];
  element blockel;
  int regno;

  reglist[0] = NULL;
  for (regno = 1; regno < NUM_REGISTERS; regno++) {
    reglist[regno] = list_alloc (sub->code->lstpool);
  }

  sub->ssavars = list_alloc (sub->code->lstpool);

  blockel = list_head (sub->blocks);
  while (blockel) {
    struct basicblock *block = element_getvalue (blockel);
    for (regno = 0; regno < NUM_REGISTERS; regno++) {
      if (IS_BIT_SET (block->reg_kill, regno))
        list_inserttail (reglist[regno], block);
    }
    blockel = element_next (blockel);
  }

  ssa_place_phis (sub, reglist);
  ssa_search (sub->startblock, reglist);

  for (regno = 1; regno < NUM_REGISTERS; regno++) {
    list_free (reglist[regno]);
  }
}

void unbuild_ssa (struct subroutine *sub)
{
  element varel, valel, blockel, opel;

  blockel = list_head (sub->blocks);
  while (blockel) {
    struct basicblock *block = element_getvalue (blockel);
    opel = list_head (block->operations);
    while (opel) {
      struct operation *op = element_getvalue (opel);
      element nextopel;

      nextopel = element_next (opel);
      if (op->type == OP_PHI) {
        element_remove (opel);

        valel = list_head (op->operands);
        while (valel) {
          struct value *val = element_getvalue (valel);
          fixedpool_free (sub->code->valspool, val);
          valel = element_next (valel);
        }
        list_free (op->operands);

        fixedpool_free (sub->code->valspool, list_headvalue (op->results));
        list_free (op->results);
      } else {
        valel = list_head (op->operands);
        while (valel) {
          struct value *val = element_getvalue (valel);
          if (val->type == VAL_SSAVAR) {
            val->type = VAL_REGISTER;
            val->val.intval = val->val.variable->name.val.intval;
          }
          valel = element_next (valel);
        }

        valel = list_head (op->results);
        while (valel) {
          struct value *val = element_getvalue (valel);
          if (val->type == VAL_SSAVAR) {
            val->type = VAL_REGISTER;
            val->val.intval = val->val.variable->name.val.intval;
          }
          valel = element_next (valel);
        }

      }
      opel = nextopel;
    }
    blockel = element_next (blockel);
  }

  varel = list_head (sub->ssavars);
  while (varel) {
    struct ssavar *var = element_getvalue (varel);
    list_free (var->uses);
    fixedpool_free (sub->code->ssavarspool, var);
    varel = element_next (varel);
  }
  list_free (sub->ssavars);

  sub->ssavars = NULL;
}




