/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include "code.h"
#include "utils.h"

const uint32 regmask_localvars[NUM_REGMASK] = { 0x43FFFFFE, 0x00000003 };

static
void mark_ssavar (struct ssavar *var, enum ssavartype type, int num)
{
  element useel, phiel;
  struct value *val;

  var->info = num;
  var->type = type;
  useel = list_head (var->uses);
  while (useel) {
    struct operation *use = element_getvalue (useel);
    if (use->type == OP_PHI) {
      phiel = list_head (use->operands);
      while (phiel) {
        struct value *val = element_getvalue (phiel);
        if (val->val.variable->type == SSAVAR_UNK) {
          mark_ssavar (val->val.variable, type, num);
        }
        phiel = element_next (phiel);
      }
      val = list_headvalue (use->results);
      if (val->val.variable->type == SSAVAR_UNK)
        mark_ssavar (val->val.variable, type, num);
    }
    useel = element_next (useel);
  }

  if (var->def->type == OP_PHI) {
    phiel = list_head (var->def->operands);
    while (phiel) {
      struct value *val = element_getvalue (phiel);
      if (val->val.variable->type == SSAVAR_UNK) {
        mark_ssavar (val->val.variable, type, num);
      }
      phiel = element_next (phiel);
    }
  }
}


static
int check_regs (list l)
{
  struct value *val;
  element operel;
  int reg;

  operel = list_head (l);
  while (operel) {
    val = element_getvalue (operel);
    operel = element_next (operel);

    if (val->type == VAL_REGISTER) {
      reg = val->val.intval;
    } else if (val->type == VAL_SSAVAR) {
      reg = val->val.variable->name.val.intval;
    } else continue;

    if (!IS_BIT_SET (regmask_localvars, reg)) return TRUE;
  }

  return FALSE;
}

static
void check_special_regs (struct subroutine *sub)
{
  element blockel;
  element opel;

  blockel = list_head (sub->blocks);
  while (blockel) {
    struct basicblock *block = element_getvalue (blockel);
    opel = list_head (block->operations);
    while (opel) {
      struct operation *op = element_getvalue (opel);
      if (op->type == OP_INSTRUCTION || op->type == OP_MOVE) {
        if (check_regs (op->operands) || check_regs (op->results)) {
          op->status |= OP_STAT_SPECIALREGS;
        }
      }
      opel = element_next (opel);
    }
    blockel = element_next (blockel);
  }
}

void extract_variables (struct subroutine *sub)
{
  element varel;
  int count = 0;

  check_special_regs (sub);

  varel = list_head (sub->ssavars);
  while (varel) {
    struct ssavar *var = element_getvalue (varel);
    struct operation *op = var->def;

    if (var->type == SSAVAR_UNK) {
      if (IS_BIT_SET (regmask_localvars, var->name.val.intval)) {
        if (op->type == OP_START) {
          mark_ssavar (var, SSAVAR_ARGUMENT, var->name.val.intval);
        } else if (op->type == OP_CALL && var->name.val.intval != REGISTER_GPR_V0 &&
                   var->name.val.intval != REGISTER_GPR_V1) {
          mark_ssavar (var, SSAVAR_INVALID, 0);
        } else {

          if (op->type == OP_MOVE || op->type == OP_INSTRUCTION) {
            if (!(var->status & (VAR_STAT_PHIARG | VAR_STAT_ASMARG))) {
              if (list_size (var->uses) <= 1) {
                op->status |= OP_STAT_DEFERRED;
              }
            }

            if (op->type == OP_INSTRUCTION) {
              if (op->info.iop.loc->insn->flags & (INSN_LOAD | INSN_STORE))
                op->status &= ~OP_STAT_DEFERRED;
              else if ((op->info.iop.loc->insn->flags & (INSN_BRANCH)) &&
                       !op->info.iop.loc->branchalways)
                op->status &= ~OP_STAT_DEFERRED;
            }
          }

          if (op->status & OP_STAT_SPECIALREGS) {
            op->status &= ~OP_STAT_DEFERRED;
          }

          if (op->status & OP_STAT_DEFERRED) {
            var->type = SSAVAR_TEMP;
            var->info = 0;
          } else {
            mark_ssavar (var, SSAVAR_LOCAL, ++count);
          }
        }
      } else {
        mark_ssavar (var, SSAVAR_ARGUMENT, var->name.val.intval);
      }
    }
    varel = element_next (varel);
  }
}
