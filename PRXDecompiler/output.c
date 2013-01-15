/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stdio.h>
#include <string.h>

#include "output.h"
#include "allegrex.h"
#include "utils.h"

void get_base_name (char *filename, char *basename, size_t len)
{
  char *temp;

  temp = strrchr (filename, '/');
  if (temp) filename = &temp[1];

  strncpy (basename, filename, len - 1);
  basename[len - 1] = '\0';
  temp = strchr (basename, '.');
  if (temp) *temp = '\0';
}

void ident_line (FILE *out, int size)
{
  int i;
  for (i = 0; i < size; i++)
    fprintf (out, "  ");
}


void print_subroutine_name (FILE *out, struct subroutine *sub)
{
  if (sub->export) {
    if (sub->export->name) {
      fprintf (out, "%s", sub->export->name);
    } else {
      fprintf (out, "%s_%08X", sub->export->libname, sub->export->nid);
    }
  } else if (sub->import) {
    if (sub->import->name) {
      fprintf (out, "%s", sub->import->name);
    } else {
      fprintf (out, "%s_%08X", sub->import->libname, sub->import->nid);
    }
  } else {
    fprintf (out, "sub_%05X", sub->begin->address);
  }
}

void print_subroutine_declaration (FILE *out, struct subroutine *sub)
{
  int i;
  if (sub->numregout > 0)
    fprintf (out, "int ");
  else
    fprintf (out, "void ");

  print_subroutine_name (out, sub);

  fprintf (out, " (");
  for (i = 0; i < sub->numregargs; i++) {
    if (i != 0) fprintf (out, ", ");
    fprintf (out, "int arg%d", i + 1);
  }
  fprintf (out, ")");
}

#define ISSPACE(x) ((x) == '\t' || (x) == '\r' || (x) == '\n' || (x) == '\v' || (x) == '\f')

static
int valid_string (struct prx *file, uint32 vaddr)
{
  uint32 off = prx_translate (file, vaddr);
  int len = 0;
  if (off) {
    for (; off < file->size; off++) {
      uint8 ch = file->data[off];
      if (ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' ||
          ch == '\f' || (ch >= 32 && ch < 127))
        len++;
      else
        break;
    }
  }
  return len > 3;
}

static
void print_string (FILE *out, struct prx *file, uint32 vaddr)
{
  uint32 off = prx_translate (file, vaddr);

  fprintf (out, "\"");
  for (; off < file->size; off++) {
    uint8 ch = file->data[off];
    if (ch >= 32 && ch < 127) {
      fprintf (out, "%c", ch);
    } else {
      switch (ch) {
      case '\t': fprintf (out, "\\t"); break;
      case '\r': fprintf (out, "\\r"); break;
      case '\n': fprintf (out, "\\n"); break;
      case '\v': fprintf (out, "\\v"); break;
      case '\f': fprintf (out, "\\f"); break;
      default:
        fprintf (out, "\"");
        return;
      }
    }
  }
}


void print_value (FILE *out, struct value *val, int options)
{
  struct ssavar *var;
  int isstring = FALSE;

  switch (val->type) {
  case VAL_CONSTANT: fprintf (out, "0x%08X", val->val.intval); break;
  case VAL_SSAVAR:
    var = val->val.variable;
    if (CONST_TYPE (var->status) != VAR_STAT_NOTCONSTANT &&
        !(options & OPTS_RESULT)) {
      struct prx *file;
      file = var->def->block->sub->code->file;
      if (var->def->status & OP_STAT_HASRELOC) {
        isstring = valid_string (file, var->value);
      }
      if (isstring)
        print_string (out, file, var->value);
      else
        fprintf (out, "0x%08X", var->value);

    } else {
      switch (var->type) {
      case SSAVAR_ARGUMENT:
        if (var->name.val.intval >= REGISTER_GPR_A0 &&
            var->name.val.intval <= REGISTER_GPR_T3) {
          fprintf (out, "arg%d", var->name.val.intval - REGISTER_GPR_A0 + 1);
        } else {
          print_value (out, &var->name, options);
        }
        break;
      case SSAVAR_LOCAL:
        fprintf (out, "var%d", var->info);
        break;
      case SSAVAR_TEMP:
        options = OPTS_NORESULT;
        if (((struct value *) list_headvalue (var->def->results))->val.variable != var)
          options |= OPTS_SECONDRESULT;
        if (var->def->type != OP_MOVE)
          fprintf (out, "(");
        print_operation (out, var->def, 0, options);
        if (var->def->type != OP_MOVE)
          fprintf (out, ")");
        break;
      default:
        print_value (out, &var->name, options);
        fprintf (out, "/* Invalid block %d %d */",
            var->def->block->node.dfsnum,
            var->def->type);
        break;
      }
    }
    break;
  case VAL_REGISTER:
    if (val->val.intval == REGISTER_HI)      fprintf (out, "hi");
    else if (val->val.intval == REGISTER_LO) fprintf (out, "lo");
    else fprintf (out, "%s", gpr_names[val->val.intval]);
    break;
  default:
    fprintf (out, "UNK");
  }
}

static
void print_asm_reglist (FILE *out, list regs, int identsize, int options)
{
  element el;

  fprintf (out, "\n");
  ident_line (out, identsize);
  fprintf (out, "  : ");

  el = list_head (regs);
  while (el) {
    struct value *val = element_getvalue (el);
    if (el != list_head (regs))
      fprintf (out, ", ");
    fprintf (out, "\"=r\"(");
    print_value (out, val, 0);
    fprintf (out, ")");
    el = element_next (el);
  }
}

static
void print_asm (FILE *out, struct operation *op, int identsize, int options)
{
  struct location *loc;

  ident_line (out, identsize);
  fprintf (out, "__asm__ (");
  for (loc = op->info.asmop.begin; ; loc++) {
    if (loc != op->info.asmop.begin) {
      fprintf (out, "\n");
      ident_line (out, identsize);
      fprintf (out, "         ");
    }
    fprintf (out, "\"%s;\"", allegrex_disassemble (loc->opc, loc->address, FALSE));
    if (loc == op->info.asmop.end) break;
  }
  if (list_size (op->results) != 0 || list_size (op->operands) != 0) {
    print_asm_reglist (out, op->results, identsize, options);
    if (list_size (op->operands) != 0) {
      print_asm_reglist (out, op->operands, identsize, options);
    }
  }

  fprintf (out, ");\n");
}

static
void print_binaryop (FILE *out, struct operation *op, const char *opsymbol, int options)
{
  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }
  print_value (out, list_headvalue (op->operands), 0);
  fprintf (out, " %s ", opsymbol);
  print_value (out, list_tailvalue (op->operands), 0);
}

static
void print_revbinaryop (FILE *out, struct operation *op, const char *opsymbol, int options)
{
  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }
  print_value (out, list_tailvalue (op->operands), 0);
  fprintf (out, " %s ", opsymbol);
  print_value (out, list_headvalue (op->operands), 0);
}

static
void print_complexop (FILE *out, struct operation *op, const char *opsymbol, int options)
{
  element el;

  if (list_size (op->results) != 0 && !(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }

  fprintf (out, "%s (", opsymbol);
  el = list_head (op->operands);
  while (el) {
    struct value *val;
    val = element_getvalue (el);
    if (val->type == VAL_SSAVAR) {
      if (val->val.variable->type == SSAVAR_INVALID) break;
    }
    if (el != list_head (op->operands))
      fprintf (out, ", ");
    print_value (out, val, 0);
    el = element_next (el);
  }
  fprintf (out, ")");
}

static
void print_call (FILE *out, struct operation *op, int options)
{
  element el;

  if (list_size (op->info.callop.retvalues) != 0 && !(options & OPTS_NORESULT)) {
    el = list_head (op->info.callop.retvalues);
    while (el) {
      print_value (out, element_getvalue (el), OPTS_RESULT);
      fprintf (out, " ");
      el = element_next (el);
    }
    fprintf (out, "= ");
  }

  if (op->block->info.call.calltarget) {
    print_subroutine_name (out, op->block->info.call.calltarget);
  } else {
    fprintf (out, "(*");
    print_value (out, list_headvalue (op->block->info.call.from->jumpop->operands), 0);
    fprintf (out, ")");
  }

  fprintf (out, " (");

  el = list_head (op->info.callop.arguments);
  while (el) {
    struct value *val;
    val = element_getvalue (el);
    if (val->type == VAL_SSAVAR) {
      if (val->val.variable->type == SSAVAR_INVALID) break;
    }
    if (el != list_head (op->info.callop.arguments))
      fprintf (out, ", ");
    print_value (out, val, 0);
    el = element_next (el);
  }
  fprintf (out, ")");
}

static
void print_return (FILE *out, struct operation *op, int options)
{
  element el;

  fprintf (out, "return");
  el = list_head (op->info.endop.arguments);
  while (el) {
    struct value *val;
    val = element_getvalue (el);
    fprintf (out, " ");
    print_value (out, val, 0);
    el = element_next (el);
  }
}

static
void print_ext (FILE *out, struct operation *op, int options)
{
  struct value *val1, *val2, *val3;
  element el;
  uint32 mask;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el);

  mask = 0xFFFFFFFF >> (32 - val3->val.intval);
  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }

  fprintf (out, "(");
  print_value (out, val1, 0);
  fprintf (out, " >> %d)", val2->val.intval);
  fprintf (out, " & 0x%08X", mask);
}

static
void print_ins (FILE *out, struct operation *op, int options)
{
  struct value *val1, *val2, *val3, *val4;
  element el;
  uint32 mask;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el); el = element_next (el);
  val4 = element_getvalue (el);

  mask = 0xFFFFFFFF >> (32 - val4->val.intval);
  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }

  fprintf (out, "(");
  print_value (out, val2, 0);
  fprintf (out, " & 0x%08X) | (", ~(mask << val3->val.intval));
  print_value (out, val1, 0);
  fprintf (out, " & 0x%08X)", mask);
}

static
void print_nor (FILE *out, struct operation *op, int options)
{
  struct value *val1, *val2;
  int simple = 0;

  val1 = list_headvalue (op->operands);
  val2 = list_tailvalue (op->operands);

  if (val1->val.intval == 0 || val2->val.intval == 0) {
    simple = 1;
    if (val1->val.intval == 0) val1 = val2;
  }

  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }

  if (!simple) {
    fprintf (out, "!(");
    print_value (out, val1, 0);
    fprintf (out, " | ");
    print_value (out, val2, 0);
    fprintf (out, ")");
  } else {
    fprintf (out, "!");
    print_value (out, val1, 0);
  }
}

static
void print_movnz (FILE *out, struct operation *op, int ismovn, int options)
{
  struct value *val1, *val2, *val3;
  struct value *result;
  element el;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el);
  result = list_headvalue (op->results);

  if (!(options & OPTS_NORESULT)) {
    print_value (out, result, OPTS_RESULT);
    fprintf (out, " = ");
  }

  if (ismovn)
    fprintf (out, "(");
  else
    fprintf (out, "!(");
  print_value (out, val2, 0);
  fprintf (out, ") ? ");
  print_value (out, val1, 0);
  fprintf (out, " : ");
  print_value (out, val3, 0);
}

static
void print_mult (FILE *out, struct operation *op, int options)
{
  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " ");
    print_value (out, list_tailvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }
  if (options & OPTS_SECONDRESULT)
    fprintf (out, "hi (");

  print_value (out, list_headvalue (op->operands), 0);
  fprintf (out, " * ");
  print_value (out, list_tailvalue (op->operands), 0);

  if (options & OPTS_SECONDRESULT)
    fprintf (out, ")");
}

static
void print_madd (FILE *out, struct operation *op, int options)
{
  struct value *val1, *val2, *val3, *val4;
  element el = list_head (op->operands);

  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el); el = element_next (el);
  val4 = element_getvalue (el);

  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " ");
    print_value (out, list_tailvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }

  print_value (out, val1, 0);
  fprintf (out, " * ");
  print_value (out, val2, 0);

  fprintf (out, " + (");
  print_value (out, val3, 0);
  fprintf (out, " ");
  print_value (out, val4, 0);
  fprintf (out, ")");
}


static
void print_msub (FILE *out, struct operation *op, int options)
{
  struct value *val1, *val2, *val3, *val4;
  element el = list_head (op->operands);

  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el); el = element_next (el);
  val3 = element_getvalue (el); el = element_next (el);
  val4 = element_getvalue (el);

  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " ");
    print_value (out, list_tailvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }

  fprintf (out, "(");
  print_value (out, val3, 0);
  fprintf (out, " ");
  print_value (out, val4, 0);
  fprintf (out, ") - ");

  print_value (out, val1, 0);
  fprintf (out, " * ");
  print_value (out, val2, 0);

}

static
void print_div (FILE *out, struct operation *op, int options)
{
  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " ");
    print_value (out, list_tailvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }
  print_value (out, list_headvalue (op->operands), 0);

  if (options & OPTS_SECONDRESULT)
    fprintf (out, " %% ");
  else
    fprintf (out, " / ");

  print_value (out, list_tailvalue (op->operands), 0);
}

static
void print_slt (FILE *out, struct operation *op, int isunsigned, int options)
{
  struct value *val1, *val2;
  struct value *result;
  element el;

  el = list_head (op->operands);
  val1 = element_getvalue (el); el = element_next (el);
  val2 = element_getvalue (el);
  result = list_headvalue (op->results);

  if (!(options & OPTS_NORESULT)) {
    print_value (out, result, OPTS_RESULT);
    fprintf (out, " = ");
  }

  fprintf (out, "(");

  print_value (out, val1, 0);
  fprintf (out, " < ");
  print_value (out, val2, 0);
  fprintf (out, ")");
}

static
void print_signextend (FILE *out, struct operation *op, int isbyte, int options)
{
  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }

  if (isbyte)
    fprintf (out, "(char) ");
  else
    fprintf (out, "(short) ");

  print_value (out, list_headvalue (op->operands), 0);
}

static
void print_memory_address (FILE *out, struct operation *op, int size, int isunsigned, int options)
{
  struct value *val;
  uint32 address;
  const char *type;

  if (size == 0) {
    if (isunsigned) type = "unsigned char *";
    else type = "char *";
  } else if (size == 1) {
    if (isunsigned) type = "unsigned short *";
    else type = "short *";
  } else if (size == 2) {
    type = "int *";
  }

  val = list_headvalue (op->operands);
  if (val->type == VAL_SSAVAR) {
    if (CONST_TYPE (val->val.variable->status) != VAR_STAT_NOTCONSTANT) {
      address = val->val.variable->value;
      val = list_tailvalue (op->operands);
      address += val->val.intval;
      fprintf (out, "*((%s) 0x%08X)", type, address);
      return;
    }
  }

  fprintf (out, "((%s) ", type);
  print_value (out, val, 0);
  val = list_tailvalue (op->operands);
  fprintf (out, ")[%d]", val->val.intval >> size);
}

static
void print_load (FILE *out, struct operation *op, int size, int isunsigned, int options)
{
  if (!(options & OPTS_NORESULT)) {
    print_value (out, list_headvalue (op->results), OPTS_RESULT);
    fprintf (out, " = ");
  }
  print_memory_address (out, op, size, isunsigned, options);
}

static
void print_store (FILE *out, struct operation *op, int size, int isunsigned, int options)
{
  struct value *val = element_getvalue (element_next (list_head (op->operands)));
  print_memory_address (out, op, size, isunsigned, options);
  fprintf (out, " = ");
  print_value (out, val, 0);
}

static
void print_condition (FILE *out, struct operation *op, int options)
{
  fprintf (out, "if (");
  if (options & OPTS_REVERSECOND) fprintf (out, "!(");
  print_value (out, list_headvalue (op->operands), 0);
  switch (op->info.iop.insn) {
  case I_BNE:
    fprintf (out, " != ");
    break;
  case I_BEQ:
    fprintf (out, " == ");
    break;
  case I_BGEZ:
  case I_BGEZAL:
    fprintf (out, " >= 0");
    break;
  case I_BGTZ:
    fprintf (out, " > 0");
    break;
  case I_BLEZ:
    fprintf (out, " <= 0");
    break;
  case I_BLTZ:
  case I_BLTZAL:
    fprintf (out, " < 0");
    break;
  default:
    break;
  }
  if (list_size (op->operands) == 2)
    print_value (out, list_tailvalue (op->operands), 0);

  if (options & OPTS_REVERSECOND) fprintf (out, ")");
  fprintf (out, ")");
}



void print_operation (FILE *out, struct operation *op, int identsize, int options)
{
  struct location *loc;
  int nosemicolon = FALSE;

  if (op->type == OP_ASM) {
    print_asm (out, op, identsize, options);
    return;
  }

  loc = op->info.iop.loc;
  if (op->type == OP_INSTRUCTION) {
    if (op->info.iop.loc->insn->flags & (INSN_JUMP))
      return;
    if (loc->branchalways) return;
  } else if (op->type == OP_NOP || op->type == OP_START || op->type == OP_PHI) {
    return;
  }

  ident_line (out, identsize);

  if ((op->status & (OP_STAT_CONSTANT | OP_STAT_DEFERRED)) == OP_STAT_CONSTANT) {
    struct value *val = list_headvalue (op->results);
    if (!(options & OPTS_NORESULT)) {
      print_value (out, val, OPTS_RESULT);
      fprintf (out, " = ");
    }
    print_value (out, val, 0);
  } else {
    if (op->type == OP_INSTRUCTION) {
      switch (op->info.iop.insn) {
      case I_ADD:
      case I_ADDU: print_binaryop (out, op, "+", options);     break;
      case I_SUB:
      case I_SUBU: print_binaryop (out, op, "-", options);     break;
      case I_XOR:  print_binaryop (out, op, "^", options);     break;
      case I_AND:  print_binaryop (out, op, "&", options);     break;
      case I_OR:   print_binaryop (out, op, "|", options);     break;
      case I_SRAV:
      case I_SRLV: print_revbinaryop (out, op, ">>", options); break;
      case I_SLLV: print_revbinaryop (out, op, "<<", options); break;
      case I_ROTV: print_complexop (out, op, "ROTV", options); break;
      case I_INS:  print_ins (out, op, options);               break;
      case I_EXT:  print_ext (out, op, options);               break;
      case I_MIN:  print_complexop (out, op, "MIN", options);  break;
      case I_MAX:  print_complexop (out, op, "MAX", options);  break;
      case I_BITREV: print_complexop (out, op, "BITREV", options); break;
      case I_CLZ:  print_complexop (out, op, "CLZ", options);  break;
      case I_CLO:  print_complexop (out, op, "CLO", options);  break;
      case I_NOR:  print_nor (out, op, options);               break;
      case I_MOVN: print_movnz (out, op, TRUE, options);       break;
      case I_MOVZ: print_movnz (out, op, FALSE, options);      break;
      case I_MULT:
      case I_MULTU: print_mult (out, op, options);             break;
      case I_MADD:
      case I_MADDU: print_madd (out, op, options);             break;
      case I_MSUB:
      case I_MSUBU: print_msub (out, op, options);             break;
      case I_DIV:
      case I_DIVU: print_div (out, op, options);               break;
      case I_SLT:  print_slt (out, op, FALSE, options);        break;
      case I_SLTU: print_slt (out, op, TRUE, options);         break;
      case I_LW:   print_load (out, op, 2, FALSE, options);    break;
      case I_LB:   print_load (out, op, 0, FALSE, options);    break;
      case I_LBU:  print_load (out, op, 0, TRUE, options);     break;
      case I_LH:   print_load (out, op, 1, FALSE, options);    break;
      case I_LHU:  print_load (out, op, 1, TRUE, options);     break;
      case I_LL:   print_complexop (out, op, "LL", options);   break;
      case I_LWL:  print_complexop (out, op, "LWL", options);  break;
      case I_LWR:  print_complexop (out, op, "LWR", options);  break;
      case I_SW:   print_store (out, op, 2, FALSE, options);   break;
      case I_SH:   print_store (out, op, 1, FALSE, options);   break;
      case I_SB:   print_store (out, op, 0, FALSE, options);   break;
      case I_SC:   print_complexop (out, op, "SC", options);   break;
      case I_SWL:  print_complexop (out, op, "SWL", options);  break;
      case I_SWR:  print_complexop (out, op, "SWR", options);  break;
      case I_SEB:  print_signextend (out, op, TRUE, options);  break;
      case I_SEH:  print_signextend (out, op, TRUE, options);  break;
      default:
        if (loc->insn->flags & INSN_BRANCH) {
          print_condition (out, op, options);
          nosemicolon = TRUE;
        }
        break;
      }
    } else if (op->type == OP_MOVE) {
      if (!(options & OPTS_NORESULT)) {
        print_value (out, list_headvalue (op->results), OPTS_RESULT);
        fprintf (out, " = ");
      }
      print_value (out, list_headvalue (op->operands), 0);
    } else if (op->type == OP_CALL) {
      print_call (out, op, options);
    } else if (op->type == OP_END) {
      print_return (out, op, options);
    /*} else if (op->type == OP_PHI) {
      print_complexop (out, op, "PHI", options);*/
    }
  }

  if (!(options & OPTS_NORESULT)) {
    if (nosemicolon) fprintf (out, "\n");
    else fprintf (out, ";\n");
  }
}



