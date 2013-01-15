/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */


#include "code.h"
#include "utils.h"

static
uint32 get_constant_value (struct value *val)
{
  if (val->type == VAL_SSAVAR)
    return val->val.variable->value;
  else
    return val->val.intval;
}

static
void combine_constants (struct ssavar *out, struct value *val)
{
  uint32 constant;
  if (CONST_TYPE (out->status) == VAR_STAT_NOTCONSTANT) return;

  if (val->type == VAL_REGISTER) {
    CONST_SETTYPE (out->status, VAR_STAT_NOTCONSTANT);
  }

  if (val->type == VAL_SSAVAR) {
    if (CONST_TYPE (val->val.variable->status) == VAR_STAT_UNKCONSTANT)
      return;
    if (CONST_TYPE (val->val.variable->status) == VAR_STAT_NOTCONSTANT) {
      CONST_SETTYPE (out->status, VAR_STAT_NOTCONSTANT);
      return;
    }
  }

  constant = get_constant_value (val);
  if (CONST_TYPE (out->status) == VAR_STAT_UNKCONSTANT) {
    CONST_SETTYPE (out->status, VAR_STAT_CONSTANT);
    out->value = constant;
  } else {
    if (out->value != constant)
      CONST_SETTYPE (out->status, VAR_STAT_NOTCONSTANT);
  }
}

void propagate_constants (struct subroutine *sub)
{
  list worklist = list_alloc (sub->code->lstpool);
  element varel;

  varel = list_head (sub->ssavars);
  while (varel) {
    struct ssavar *var = element_getvalue (varel);
    struct operation *op = var->def;
    CONST_SETTYPE (var->status, VAR_STAT_UNKCONSTANT);
    if (op->type == OP_ASM ||
        op->type == OP_CALL ||
        op->type == OP_START ||
        !(IS_BIT_SET (regmask_localvars, var->name.val.intval)))
      CONST_SETTYPE (var->status, VAR_STAT_NOTCONSTANT);
    else {
      var->mark = 1;
      list_inserttail (worklist, var);
    }
    varel = element_next (varel);
  }

  while (list_size (worklist) != 0) {
    struct ssavar *aux, *var = list_removehead (worklist);
    struct ssavar temp;
    struct value *val;
    struct operation *op;
    element opel;

    var->mark = 0;
    op = var->def;
    op->status &= ~OP_STAT_CONSTANT;

    if (CONST_TYPE (var->status) == VAR_STAT_NOTCONSTANT) continue;

    if (op->type == OP_PHI) {
      temp.status = VAR_STAT_UNKCONSTANT;

      opel = list_head (op->operands);
      while (opel) {
        val = element_getvalue (opel);
        combine_constants (&temp, val);
        opel = element_next (opel);
      }
    } else {
      temp.status = VAR_STAT_CONSTANT;

      opel = list_head (op->operands);
      while (opel) {
        val = element_getvalue (opel);
        if (val->type == VAL_SSAVAR) {
          aux = val->val.variable;
          if (CONST_TYPE (aux->status) == VAR_STAT_NOTCONSTANT) {
            temp.status = VAR_STAT_NOTCONSTANT;
            break;
          } else if (CONST_TYPE (aux->status) == VAR_STAT_UNKCONSTANT)
            temp.status = VAR_STAT_UNKCONSTANT;
        }
        opel = element_next (opel);
      }

      if (temp.status == VAR_STAT_CONSTANT) {
        if (op->type == OP_MOVE) {
          val = list_headvalue (op->operands);
          temp.value = get_constant_value (val);
          op->status |= OP_STAT_CONSTANT;
        } else if (op->type == OP_INSTRUCTION) {
          uint32 val1, val2;
          switch (op->info.iop.insn) {
          case I_ADD:
          case I_ADDU:
            val1 = get_constant_value (list_headvalue (op->operands));
            val2 = get_constant_value (list_tailvalue (op->operands));
            op->status |= OP_STAT_CONSTANT;
            temp.value = val1 + val2;
            break;
          case I_OR:
            val1 = get_constant_value (list_headvalue (op->operands));
            val2 = get_constant_value (list_tailvalue (op->operands));
            op->status |= OP_STAT_CONSTANT;
            temp.value = val1 | val2;
            break;
          default:
            temp.status = VAR_STAT_NOTCONSTANT;
            break;
          }
        }
      }
    }

    if (temp.status != CONST_TYPE (var->status)) {
      element useel;
      useel = list_head (var->uses);
      while (useel) {
        struct operation *use = element_getvalue (useel);
        if (use->type == OP_INSTRUCTION || use->type == OP_MOVE || use->type == OP_PHI) {
          varel = list_head (use->results);
          while (varel) {
            val = element_getvalue (varel);
            if (val->type == VAL_SSAVAR) {
              aux = val->val.variable;
              if (!(aux->mark)) {
                aux->mark = 1;
                list_inserttail (worklist, aux);
              }
            }
            varel = element_next (varel);
          }
        }
        useel = element_next (useel);
      }
    }
    CONST_SETTYPE (var->status, temp.status);
    var->value = temp.value;
  }

  list_free (worklist);


  varel = list_head (sub->ssavars);
  while (varel) {
    struct ssavar *var = element_getvalue (varel);
    struct operation *op = var->def;
    element useel;

    if (CONST_TYPE (var->status) == VAR_STAT_CONSTANT) {
      op->status |= OP_STAT_DEFERRED;
      useel = list_head (var->uses);
      while (useel) {
        struct operation *use = element_getvalue (useel);
        if (use->type == OP_PHI) {
          struct value *val = list_headvalue (use->results);
          if (val->type != VAL_SSAVAR) break;
          if (CONST_TYPE (val->val.variable->status) != VAR_STAT_CONSTANT)
            break;
        } else if (use->type == OP_ASM) break;
        useel = element_next (useel);
      }
      if (useel) {
        op->status &= ~OP_STAT_DEFERRED;
      }
    }

    varel = element_next (varel);
  }

}


