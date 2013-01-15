/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include "code.h"
#include "utils.h"


const uint32 regmask_call_gen[NUM_REGMASK] =   { 0xAC000000, 0x00000000 };
const uint32 regmask_call_kill[NUM_REGMASK] =  { 0x0300FFFE, 0x00000003 };
const uint32 regmask_subend_gen[NUM_REGMASK] = { 0xF0FF0000, 0x00000000 };


#define BLOCK_GPR_KILL() \
  if (regno != 0) {                   \
    BIT_SET (block->reg_kill, regno); \
  }

#define BLOCK_GPR_GEN() \
  if (!IS_BIT_SET (block->reg_kill, regno) && regno != 0 && \
      !IS_BIT_SET (block->reg_gen, regno)) {                \
    BIT_SET (block->reg_gen, regno);                        \
  }

#define ASM_GPR_KILL() \
  BLOCK_GPR_KILL ()                                              \
  if (!IS_BIT_SET (asm_kill, regno) && regno != 0) {             \
    BIT_SET (asm_kill, regno);                                   \
    value_append (sub, op->results, VAL_REGISTER, regno, FALSE); \
  }

#define ASM_GPR_GEN() \
  BLOCK_GPR_GEN ()                                                \
  if (!IS_BIT_SET (asm_kill, regno) && regno != 0 &&              \
      !IS_BIT_SET (asm_gen, regno)) {                             \
    BIT_SET (asm_gen, regno);                                     \
    value_append (sub, op->operands, VAL_REGISTER, regno, FALSE); \
  }


struct operation *operation_alloc (struct basicblock *block)
{
  struct operation *op;
  struct code *c = block->sub->code;

  op = fixedpool_alloc (c->opspool);
  op->block = block;
  op->operands = list_alloc (c->lstpool);
  op->results = list_alloc (c->lstpool);
  return op;
}

static
void reset_operation (struct operation *op)
{
  struct value *val;
  struct code *c = op->block->sub->code;

  while (list_size (op->results)) {
    val = list_removehead (op->results);
    fixedpool_free (c->valspool, val);
  }

  while (list_size (op->operands)) {
    val = list_removehead (op->operands);
    fixedpool_free (c->valspool, val);
  }
}

static
void simplify_reg_zero (list l)
{
  struct value *val;
  element el;

  el = list_head (l);
  while (el) {
    val = element_getvalue (el);
    if (val->type == VAL_REGISTER && val->val.intval == 0) {
      val->type = VAL_CONSTANT;
    }
    el = element_next (el);
  }
}

static
void simplify_operation (struct operation *op)
{
  struct value *val;
  struct code *c = op->block->sub->code;

  if (op->type != OP_INSTRUCTION) return;

  if (list_size (op->results) == 1 && !(op->info.iop.loc->insn->flags & (INSN_LOAD | INSN_JUMP))) {
    val = list_headvalue (op->results);
    if (val->val.intval == 0) {
      reset_operation (op);
      op->type = OP_NOP;
      return;
    }
  }
  simplify_reg_zero (op->results);
  simplify_reg_zero (op->operands);

  switch (op->info.iop.insn) {
  case I_ADDU:
  case I_ADD:
  case I_OR:
  case I_XOR:
    val = list_headvalue (op->operands);
    if (val->val.intval == 0) {
      val = list_removehead (op->operands);
      fixedpool_free (c->valspool, val);
      op->type = OP_MOVE;
    } else {
      val = list_tailvalue (op->operands);
      if (val->val.intval == 0) {
        val = list_removetail (op->operands);
        fixedpool_free (c->valspool, val);
        op->type = OP_MOVE;
      }
    }
    break;
  case I_AND:
    val = list_headvalue (op->operands);
    if (val->val.intval == 0) {
      val = list_removetail (op->operands);
      fixedpool_free (c->valspool, val);
      op->type = OP_MOVE;
    } else {
      val = list_tailvalue (op->operands);
      if (val->val.intval == 0) {
        val = list_removehead (op->operands);
        fixedpool_free (c->valspool, val);
        op->type = OP_MOVE;
      }
    }

    break;
  case I_BITREV:
  case I_SEB:
  case I_SEH:
  case I_WSBH:
  case I_WSBW:
    val = list_headvalue (op->operands);
    if (val->val.intval == 0) {
      op->type = OP_MOVE;
    }
    break;
  case I_SLLV:
  case I_SRLV:
  case I_SRAV:
  case I_ROTV:
    val = list_headvalue (op->operands);
    if (val->val.intval == 0) {
      val = list_removehead (op->operands);
      fixedpool_free (c->valspool, val);
      op->type = OP_MOVE;
    }
    break;
  case I_SUB:
  case I_SUBU:
    val = list_tailvalue (op->operands);
    if (val->val.intval == 0) {
      val = list_removetail (op->operands);
      fixedpool_free (c->valspool, val);
      op->type = OP_MOVE;
    }
    break;
  case I_MOVN:
    val = element_getvalue (element_next (list_head (op->operands)));
    if (val->val.intval == 0) {
      reset_operation (op);
      op->type = OP_NOP;
    }
    break;
  case I_MOVZ:
    val = element_getvalue (element_next (list_head (op->operands)));
    if (val->val.intval == 0) {
      val = list_removetail (op->operands);
      fixedpool_free (c->valspool, val);
      val = list_removetail (op->operands);
      fixedpool_free (c->valspool, val);
      op->type = OP_MOVE;
    }
    break;
  default:
    break;
  }

  return;
}

struct value *value_append (struct subroutine *sub, list l, enum valuetype type, uint32 value, int prepend)
{
  struct value *val;
  val = fixedpool_alloc (sub->code->valspool);
  val->type = type;
  val->val.intval = value;
  if (prepend) list_inserthead (l, val);
  else list_inserttail (l, val);
  return val;
}

void extract_operations (struct subroutine *sub)
{
  struct operation *op;
  struct basicblock *block;
  struct location *loc;
  struct prx *file;
  uint32 asm_gen[NUM_REGMASK], asm_kill[NUM_REGMASK];
  int i, regno, lastasm, relocnum;
  element el;

  file = sub->code->file;
  el = list_head (sub->blocks);
  while (el) {
    block = element_getvalue (el);
    block->operations = list_alloc (sub->code->lstpool);

    for (i = 0; i < NUM_REGMASK; i++)
      block->reg_gen[i] = block->reg_kill[i] = 0;

    switch (block->type) {
    case BLOCK_SIMPLE:
      lastasm = FALSE;
      relocnum = prx_findrelocbyaddr (file, block->info.simple.begin->address);

      for (i = 0; i < NUM_REGMASK; i++)
        asm_gen[i] = asm_kill[i] = 0;

      for (loc = block->info.simple.begin; ; loc++) {
        int hasreloc = FALSE;

        if (relocnum < file->relocnum) {
          if (file->relocsbyaddr[relocnum].vaddr == loc->address) {
            hasreloc = TRUE;
            relocnum++;
          }
        }

        if (INSN_TYPE (loc->insn->flags) == INSN_ALLEGREX) {
          enum allegrex_insn insn;

          if (lastasm)
            list_inserttail (block->operations, op);
          lastasm = FALSE;

          op = operation_alloc (block);
          op->type = OP_INSTRUCTION;
          if (hasreloc)
            op->status |= OP_STAT_HASRELOC;
          op->info.iop.loc = loc;

          if (loc->insn->flags & INSN_READ_GPR_S) {
            regno = RS (loc->opc);
            BLOCK_GPR_GEN ()
            value_append (sub, op->operands, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & INSN_READ_GPR_T) {
            regno = RT (loc->opc);
            BLOCK_GPR_GEN ()
            value_append (sub, op->operands, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & INSN_READ_GPR_D) {
            regno = RD (loc->opc);
            BLOCK_GPR_GEN ()
            value_append (sub, op->operands, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & INSN_READ_LO) {
            regno = REGISTER_LO;
            BLOCK_GPR_GEN ()
            value_append (sub, op->operands, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & INSN_READ_HI) {
            regno = REGISTER_HI;
            BLOCK_GPR_GEN ()
            value_append (sub, op->operands, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & (INSN_LOAD | INSN_STORE)) {
            value_append (sub, op->operands, VAL_CONSTANT, IMM (loc->opc), FALSE);
          }

          switch (loc->insn->insn) {
          case I_ADDI:
            insn = I_ADD;
            value_append (sub, op->operands, VAL_CONSTANT, IMM (loc->opc), FALSE);
            break;
          case I_ADDIU:
            insn = I_ADDU;
            value_append (sub, op->operands, VAL_CONSTANT, IMM (loc->opc), FALSE);
            break;
          case I_ORI:
            insn = I_OR;
            value_append (sub, op->operands, VAL_CONSTANT, IMMU (loc->opc), FALSE);
            break;
          case I_XORI:
            insn = I_XOR;
            value_append (sub, op->operands, VAL_CONSTANT, IMMU (loc->opc), FALSE);
            break;
          case I_ANDI:
            insn = I_AND;
            value_append (sub, op->operands, VAL_CONSTANT, IMMU (loc->opc), FALSE);
            break;
          case I_LUI:
            op->type = OP_MOVE;
            value_append (sub, op->operands, VAL_CONSTANT, ((unsigned int) IMMU (loc->opc)) << 16, FALSE);
            break;
          case I_MFLO:
          case I_MFHI:
          case I_MTLO:
          case I_MTHI:
            op->type = OP_MOVE;
            break;
          case I_SLTI:
            insn = I_SLT;
            value_append (sub, op->operands, VAL_CONSTANT, IMM (loc->opc), FALSE);
            break;
          case I_SLTIU:
            insn = I_SLTU;
            value_append (sub, op->operands, VAL_CONSTANT, IMM (loc->opc), FALSE);
            break;
          case I_EXT:
            insn = I_EXT;
            value_append (sub, op->operands, VAL_CONSTANT, SA (loc->opc), FALSE);
            value_append (sub, op->operands, VAL_CONSTANT, RD (loc->opc) + 1, FALSE);
            break;
          case I_INS:
            insn = I_INS;
            value_append (sub, op->operands, VAL_CONSTANT, SA (loc->opc), FALSE);
            value_append (sub, op->operands, VAL_CONSTANT, RD (loc->opc) - SA (loc->opc) + 1, FALSE);
            break;
          case I_ROTR:
            insn = I_ROTV;
            value_append (sub, op->operands, VAL_CONSTANT, SA (loc->opc), TRUE);
            break;
          case I_SLL:
            insn = I_SLLV;
            value_append (sub, op->operands, VAL_CONSTANT, SA (loc->opc), TRUE);
            break;
          case I_SRA:
            insn = I_SRAV;
            value_append (sub, op->operands, VAL_CONSTANT, SA (loc->opc), TRUE);
            break;
          case I_SRL:
            insn = I_SRLV;
            value_append (sub, op->operands, VAL_CONSTANT, SA (loc->opc), TRUE);
            break;
          case I_BEQL:
            insn = I_BEQ;
            break;
          case I_BGEZL:
            insn = I_BGEZ;
            break;
          case I_BGTZL:
            insn = I_BGTZ;
            break;
          case I_BLEZL:
            insn = I_BLEZ;
            break;
          case I_BLTZL:
            insn = I_BLTZ;
            break;
          case I_BLTZALL:
            insn = I_BLTZAL;
            break;
          case I_BNEL:
            insn = I_BNE;
            break;
          default:
            insn = loc->insn->insn;
          }
          op->info.iop.insn = insn;

          if (loc->insn->flags & INSN_WRITE_GPR_T) {
            regno = RT (loc->opc);
            BLOCK_GPR_KILL ()
            value_append (sub, op->results, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & INSN_WRITE_GPR_D) {
            regno = RD (loc->opc);
            BLOCK_GPR_KILL ()
            value_append (sub, op->results, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & INSN_WRITE_LO) {
            regno = REGISTER_LO;
            BLOCK_GPR_KILL ()
            value_append (sub, op->results, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & INSN_WRITE_HI) {
            regno = REGISTER_HI;
            BLOCK_GPR_KILL ()
            value_append (sub, op->results, VAL_REGISTER, regno, FALSE);
          }

          if (loc->insn->flags & INSN_LINK) {
            regno = REGISTER_GPR_RA;
            BLOCK_GPR_KILL ()
            value_append (sub, op->results, VAL_REGISTER, regno, FALSE);
          }

          simplify_operation (op);
          if (op->info.iop.loc->insn->flags & (INSN_JUMP | INSN_BRANCH))
            block->jumpop = op;
          list_inserttail (block->operations, op);

        } else {
          if (!lastasm) {
            op = operation_alloc (block);
            op->info.asmop.begin = op->info.asmop.end = loc;
            op->type = OP_ASM;
            for (i = 0; i < NUM_REGMASK; i++)
              asm_gen[i] = asm_kill[i] = 0;
          } else {
            op->info.asmop.end = loc;
          }
          lastasm = TRUE;

          if (loc->insn->flags & INSN_READ_LO) {
            regno = REGISTER_LO;
            ASM_GPR_GEN ()
          }

          if (loc->insn->flags & INSN_READ_HI) {
            regno = REGISTER_HI;
            ASM_GPR_GEN ()
          }

          if (loc->insn->flags & INSN_READ_GPR_D) {
            regno = RD (loc->opc);
            ASM_GPR_GEN ()
          }

          if (loc->insn->flags & INSN_READ_GPR_T) {
            regno = RT (loc->opc);
            ASM_GPR_GEN ()
          }

          if (loc->insn->flags & INSN_READ_GPR_S) {
            regno = RS (loc->opc);
            ASM_GPR_GEN ()
          }

          if (loc->insn->flags & INSN_WRITE_GPR_T) {
            regno = RT (loc->opc);
            ASM_GPR_KILL ()
          }

          if (loc->insn->flags & INSN_WRITE_GPR_D) {
            regno = RD (loc->opc);
            ASM_GPR_KILL ()
          }

          if (loc->insn->flags & INSN_WRITE_LO) {
            regno = REGISTER_LO;
            ASM_GPR_KILL ()
          }

          if (loc->insn->flags & INSN_WRITE_HI) {
            regno = REGISTER_HI;
            ASM_GPR_KILL ()
          }

          if (loc->insn->flags & INSN_LINK) {
            regno = REGISTER_GPR_RA;
            ASM_GPR_KILL ()
          }
        }

        if (loc == block->info.simple.end) {
          if (lastasm)
            list_inserttail (block->operations, op);
          break;
        }
      }
      break;

    case BLOCK_CALL:
      op = operation_alloc (block);
      op->type = OP_CALL;
      op->info.callop.arguments = list_alloc (sub->code->lstpool);
      op->info.callop.retvalues = list_alloc (sub->code->lstpool);
      list_inserttail (block->operations, op);

      for (regno = 1; regno <= NUM_REGISTERS; regno++) {
        if (IS_BIT_SET (regmask_call_gen, regno)) {
          BLOCK_GPR_GEN ()
          value_append (sub, op->operands, VAL_REGISTER, regno, FALSE);
        }
        if (IS_BIT_SET (regmask_call_kill, regno)) {
          BLOCK_GPR_KILL ()
          value_append (sub, op->results, VAL_REGISTER, regno, FALSE);
        }
      }
      break;

    case BLOCK_START:
      op = operation_alloc (block);
      op->type = OP_START;

      for (regno = 1; regno < NUM_REGISTERS; regno++) {
        BLOCK_GPR_KILL ()
        value_append (sub, op->results, VAL_REGISTER, regno, FALSE);
      }
      list_inserttail (block->operations, op);
      break;

    case BLOCK_END:
      op = operation_alloc (block);
      op->type = OP_END;
      op->info.endop.arguments = list_alloc (sub->code->lstpool);

      for (regno = 1; regno < NUM_REGISTERS; regno++) {
        if (IS_BIT_SET (regmask_subend_gen, regno)) {
          BLOCK_GPR_GEN ()
          value_append (sub, op->operands, VAL_REGISTER, regno, FALSE);
        }
      }

      list_inserttail (block->operations, op);
      break;
    }

    el = element_next (el);
  }
}


void fixup_call_arguments (struct subroutine *sub)
{
  struct operation *op;
  struct basicblock *block;
  struct value *val;
  element el;
  int regno, regend;

  el = list_head (sub->blocks);
  while (el) {
    block = element_getvalue (el);
    if (block->type == BLOCK_CALL) {
      struct subroutine *target;
      op = list_tailvalue (block->operations);
      target = block->info.call.calltarget;

      regend = REGISTER_GPR_T4;
      if (target)
        if (target->numregargs != -1)
          regend = REGISTER_GPR_A0 + target->numregargs;

      for (regno = REGISTER_GPR_A0; regno < regend; regno++) {
        val = value_append (sub, op->operands, VAL_REGISTER, regno, FALSE);
        list_inserttail (op->info.callop.arguments, val);
      }

      if (target) regend = REGISTER_GPR_V0 + target->numregout;
      else regend = REGISTER_GPR_A0;

      for (regno = REGISTER_GPR_V0; regno < regend; regno++) {
        val = value_append (sub, op->results, VAL_REGISTER, regno, FALSE);
        list_inserttail (op->info.callop.retvalues, val);
      }
    } else if (block->type == BLOCK_END) {
      op = list_tailvalue (block->operations);
      regend = REGISTER_GPR_V0 + sub->numregout;

      for (regno = REGISTER_GPR_V0; regno < regend; regno++) {
        val = value_append (sub, op->operands, VAL_REGISTER, regno, FALSE);
        list_inserttail (op->info.endop.arguments, val);
      }
    }
    el = element_next (el);
  }
}

void remove_call_arguments (struct subroutine *sub)
{
  struct operation *op;
  struct basicblock *block;
  struct value *val;
  element el;

  el = list_head (sub->blocks);
  while (el) {
    block = element_getvalue (el);
    if (block->type == BLOCK_CALL) {
      op = list_tailvalue (block->operations);
      while (list_size (op->info.callop.arguments) != 0) {
        val = list_removetail (op->info.callop.arguments);
        list_removetail (op->operands);
        fixedpool_free (sub->code->valspool, val);
      }
      while (list_size (op->info.callop.retvalues) != 0) {
        val = list_removetail (op->info.callop.retvalues);
        list_removetail (op->results);
        fixedpool_free (sub->code->valspool, val);
      }
    } else if (block->type == BLOCK_END) {
      op = list_tailvalue (block->operations);
      while (list_size (op->info.endop.arguments) != 0) {
        val = list_removetail (op->info.endop.arguments);
        list_removetail (op->operands);
        fixedpool_free (sub->code->valspool, val);
      }
    }
    el = element_next (el);
  }
}
