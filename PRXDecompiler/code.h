/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#ifndef __CODE_H
#define __CODE_H

#include "prx.h"
#include "allegrex.h"
#include "alloc.h"
#include "lists.h"
#include "types.h"


#define RT(op) ((op >> 16) & 0x1F)
#define RS(op) ((op >> 21) & 0x1F)
#define RD(op) ((op >> 11) & 0x1F)
#define SA(op) ((op >> 6)  & 0x1F)
#define IMM(op) ((signed short) (op & 0xFFFF))
#define IMMU(op) ((unsigned short) (op & 0xFFFF))

/* Subroutine decompilation status */
#define SUB_STAT_EXTRACTED                 1
#define SUB_STAT_CFG_EXTRACTED             2
#define SUB_STAT_OPERATIONS_EXTRACTED      4
#define SUB_STAT_LIVE_REGISTERS            8
#define SUB_STAT_CFG_TRAVERSE             16
#define SUB_STAT_CFG_TRAVERSE_REV         32
#define SUB_STAT_FIXUP_CALL_ARGS          64
#define SUB_STAT_SSA                     128
#define SUB_STAT_CONSTANTS_EXTRACTED     256
#define SUB_STAT_VARIABLES_EXTRACTED     512
#define SUB_STAT_STRUCTURES_EXTRACTED   1024

/* Operation status */
#define OP_STAT_DEFERRED            1
#define OP_STAT_HASRELOC            2
#define OP_STAT_CONSTANT            4
#define OP_STAT_SPECIALREGS         8

/* Block status */
#define BLOCK_STAT_HASLABEL         1
#define BLOCK_STAT_ISSWITCH         2
#define BLOCK_STAT_ISSWITCHTARGET   4
#define BLOCK_STAT_REVCOND          8
#define BLOCK_STAT_HASELSE         16

/* Variable status */
#define VAR_STAT_NOTCONSTANT       0
#define VAR_STAT_UNKCONSTANT       1
#define VAR_STAT_CONSTANT          2
#define VAR_STAT_PHIARG            4
#define VAR_STAT_ASMARG            8

#define CONST_TYPE(status) ((status) & 3)
#define CONST_SETTYPE(status, type) (status) = ((status) & (~3)) | (type)


/* Register values */
#define REGISTER_GPR_ZERO  0
#define REGISTER_GPR_AT    1
#define REGISTER_GPR_V0    2
#define REGISTER_GPR_V1    3
#define REGISTER_GPR_A0    4
#define REGISTER_GPR_A1    5
#define REGISTER_GPR_A2    6
#define REGISTER_GPR_A3    7
#define REGISTER_GPR_T0    8
#define REGISTER_GPR_T1    9
#define REGISTER_GPR_T2   10
#define REGISTER_GPR_T3   11
#define REGISTER_GPR_T4   12
#define REGISTER_GPR_T5   13
#define REGISTER_GPR_T6   14
#define REGISTER_GPR_T7   15
#define REGISTER_GPR_S0   16
#define REGISTER_GPR_S1   17
#define REGISTER_GPR_S2   18
#define REGISTER_GPR_S3   19
#define REGISTER_GPR_S4   20
#define REGISTER_GPR_S5   21
#define REGISTER_GPR_S6   22
#define REGISTER_GPR_S7   23
#define REGISTER_GPR_T8   24
#define REGISTER_GPR_T9   25
#define REGISTER_GPR_K0   26
#define REGISTER_GPR_K1   27
#define REGISTER_GPR_GP   28
#define REGISTER_GPR_SP   29
#define REGISTER_GPR_FP   30
#define REGISTER_GPR_RA   31
#define REGISTER_LO       32
#define REGISTER_HI       33
#define NUM_REGISTERS     34
#define NUM_REGMASK   ((NUM_REGISTERS + 31) >> 4)


#define MAX_SUB_ARGS  8

#define IS_BIT_SET(flags, bit) ((1 << ((bit) & 31)) & ((flags)[(bit) >> 5]))
#define BIT_SET(flags, bit) ((flags)[(bit) >> 5]) |= 1 << ((bit) & 31)


extern const uint32 regmask_call_gen[NUM_REGMASK];
extern const uint32 regmask_call_kill[NUM_REGMASK];
extern const uint32 regmask_subend_gen[NUM_REGMASK];
extern const uint32 regmask_localvars[NUM_REGMASK];


/* Possible reachable status */
enum locationreachable {
  LOCATION_UNREACHABLE = 0,  /* Location is not reachable by any means */
  LOCATION_REACHABLE,        /* Location is reachable by some path */
  LOCATION_DELAY_SLOT        /* Location is a delay slot of a reachable branch/jump */
};

/* If an error was detected one a location */
enum locationerror {
  ERROR_NONE = 0,              /* No error */
  ERROR_INVALID_OPCODE,        /* Opcode is not recognized */
  ERROR_DELAY_SLOT,            /* Branch/jump inside a delay slot */
  ERROR_TARGET_OUTSIDE_FILE,   /* Branch/jump target outside the code */
  ERROR_ILLEGAL_BRANCH         /* Branch with a condition that can never occur, such as `bne  $0, $0, target' */
};

/* Represents a location in the code */
struct location {
  uint32 opc;                                /* The opcode (little-endian) */
  uint32 address;                            /* The virtual address of the location */

  const struct allegrex_instruction *insn;   /* The decoded instruction or null (illegal opcode) */
  struct location *target;                   /* A possible target of a branch/jump */

  list references;                           /* Number of references to this target inside the same subroutine */
  int  branchalways;                         /* True if this location is a branch that always occurs */
  enum locationreachable reachable;          /* Reachable status */
  enum locationerror  error;                 /* Error status */

  struct subroutine *sub;                    /* Owner subroutine */
  struct basicblock *block;                  /* Basic block mark (used when extracting basic blocks) */
  struct codeswitch *cswitch;                /* Code switch mark */
};

/* Represents a switch in the code */
struct codeswitch {
  struct prx_reloc *jumpreloc;
  struct prx_reloc *switchreloc;
  struct location  *location;       /* The location that loads the base address of the switch */
  struct location  *jumplocation;   /* The location of the jump instruction */
  list   references;                /* A list of possible target locations (without repeating) */
  int    count;                     /* How many possible targets this switch have */
  int    checked;                   /* Is this switch valid? */
};

/* A subroutine */
struct subroutine {
  struct code *code;                /* The owner code of this subroutine */
  struct prx_function *export;      /* Is this a function export? */
  struct prx_function *import;      /* Is this a function import? */

  struct location *begin;           /* Where the subroutine begins */
  struct location *end;             /* Where the subroutine ends */

  struct basicblock *startblock;    /* Points to the START basic block of this subroutine */
  struct basicblock *firstblock;    /* Points to the first SIMPLE basic block of this subroutine */
  struct basicblock *endblock;      /* Points to the END basic block of this subroutine */
  list   blocks;                    /* A list of the basic blocks of this subroutine */
  list   dfsblocks, revdfsblocks;   /* Blocks ordered in DFS and Reverse-DFS order */

  list   whereused;                 /* A list of basic blocks calling this subroutine */
  list   callblocks;                /* Inner blocks of type CALL */
  list   ssavars;

  uint32 stacksize;
  int    numregargs, numregout;

  int    haserror, status;          /* Subroutine decompilation status */
  int    temp;
};

/* Represents a pair of integers */
struct intpair {
  int first, last;
};


/* Abstract node in DFS and DOM trees (or reverse DFS and DOM trees) */
struct basicblocknode {
  int dfsnum;                        /* The Depth-First search number */
  struct intpair domdfsnum;          /* To determine ancestry information in the dominator tree */
  struct basicblocknode *dominator;  /* The dominator node */
  struct basicblocknode *parent;     /* The parent node (in the depth-first search) */
  element blockel;                   /* An element inside the list (dfsblocks or revdfsblocks) */
  list children;                     /* Children in the DFS tree */
  list domchildren;                  /* Children in the dominator tree */
  list frontier;                     /* The dominator frontier */
};

/* The type of the basic block */
enum basicblocktype {
  BLOCK_START = 0,      /* The first basic block in a subroutine */
  BLOCK_SIMPLE,         /* A simple block */
  BLOCK_CALL,           /* A block that represents a call */
  BLOCK_END             /* The last basic block */
};

/* The basic block */
struct basicblock {
  enum basicblocktype type;                /* The type of the basic block */
  element blockel;                         /* An element inside the list sub->blocks */
  union {
    struct {
      struct location *begin;              /* The start of the simple block */
      struct location *end;                /* The end of the simple block */
      struct location *jumploc;            /* The jump/branch location inside the block */
    } simple;
    struct {
      struct subroutine *calltarget;       /* The target of the call */
      struct basicblock *from;
    } call;
  } info;

  uint32 reg_gen[NUM_REGMASK], reg_kill[NUM_REGMASK];
  uint32 reg_live_in[NUM_REGMASK], reg_live_out[NUM_REGMASK];

  list   operations;
  struct operation *jumpop;

  struct subroutine *sub;                  /* The owner subroutine */

  struct basicblocknode node;              /* Node info for DFS and DOM trees */
  struct basicblocknode revnode;           /* Node info for the reverse DFS and DOM trees */

  list   inrefs, outrefs;                  /* A list of in- and out-edges of this block */

  struct ctrlstruct *st, *ifst, *loopst;
  int    blockcond, status;

  int    mark1, mark2;
};

enum edgetype {
  EDGE_UNKNOWN = 0,
  EDGE_GOTO,
  EDGE_INVALID,
  EDGE_CONTINUE,
  EDGE_BREAK,
  EDGE_CASE,
  EDGE_NEXT,
  EDGE_IFENTER,
  EDGE_IFEXIT,
  EDGE_RETURN
};

struct basicedge {
  enum edgetype type;
  struct basicblock *from, *to;
  element fromel, toel;
  int fromnum, tonum;
};

enum valuetype {
  VAL_CONSTANT = 0,
  VAL_REGISTER,
  VAL_SSAVAR
};

struct value {
  enum valuetype type;
  union {
    uint32 intval;
    struct ssavar *variable;
  } val;
};


enum ssavartype {
  SSAVAR_UNK = 0,
  SSAVAR_LOCAL,
  SSAVAR_ARGUMENT,
  SSAVAR_TEMP,
  SSAVAR_INVALID
};

struct ssavar {
  enum ssavartype type;
  int status;
  int mark;

  struct value name;
  uint32 info, value;

  struct operation *def;
  list   uses;
};

enum operationtype {
  OP_START,
  OP_END,
  OP_CALL,
  OP_INSTRUCTION,
  OP_MOVE,
  OP_ASM,
  OP_NOP,
  OP_PHI
};

struct operation {
  enum operationtype type;
  struct basicblock *block;

  union {
    struct {
      enum allegrex_insn insn;
      struct location *loc;
    } iop;
    struct {
      struct location *begin, *end;
    } asmop;
    struct {
      list arguments;
      list retvalues;
    } callop;
    struct {
      list arguments;
    } endop;
  } info;

  int  status;

  list results;
  list operands;
};

enum ctrltype {
  CONTROL_LOOP,
  CONTROL_SWITCH,
  CONTROL_IF,
  CONTROL_MAIN
};

enum looptype {
  LOOP_WHILE,
  LOOP_REPEAT,
  LOOP_FOR
};

struct ctrlstruct {
  enum ctrltype type;
  struct basicblock *start;
  struct basicblock *end;
  struct ctrlstruct *parent;
  int    hasendgoto;
  int    endfollow;
  int    identsize;

  union {
    struct {
      list  edges;
    } loopctrl;
  } info;
};


/* Represents the entire PRX code */
struct code {
  struct prx *file;        /* The PRX file */

  uint32 baddr, numopc;    /* The code segment base address and number of opcodes */
  struct location *base;   /* The code segment start */
  struct location *end;    /* The code segment end */

  list subroutines;        /* The list of subroutines */

  listpool  lstpool;
  fixedpool switchpool;
  fixedpool subspool;
  fixedpool blockspool;
  fixedpool edgespool;
  fixedpool ssavarspool;
  fixedpool opspool;
  fixedpool valspool;
  fixedpool ctrlspool;
};


struct code* code_analyse (struct prx *p);
void code_free (struct code *c);

int decode_instructions (struct code *c);
uint32 location_gpr_used (struct location *loc);
uint32 location_gpr_defined (struct location *loc);
int location_branch_may_swap (struct location *branch);

void extract_switches (struct code *c);
void extract_subroutines (struct code *c);

void extract_cfg (struct subroutine *sub);
void cfg_traverse (struct subroutine *sub, int reverse);
int dom_isancestor (struct basicblocknode *ancestor, struct basicblocknode *node);
struct basicblocknode *dom_common (struct basicblocknode *n1, struct basicblocknode *n2);


struct operation *operation_alloc (struct basicblock *block);
struct value *value_append (struct subroutine *sub, list l, enum valuetype type, uint32 value, int prepend);
void extract_operations (struct subroutine *sub);
void fixup_call_arguments (struct subroutine *sub);
void remove_call_arguments (struct subroutine *sub);

void live_registers (struct code *c);
void live_registers_imports (struct code *c);

void build_ssa (struct subroutine *sub);
void unbuild_ssa (struct subroutine *sub);

void abi_check (struct subroutine *sub);

void propagate_constants (struct subroutine *sub);
void extract_variables (struct subroutine *sub);


void reset_marks (struct subroutine *sub);
void extract_structures (struct subroutine *sub);

#endif /* __CODE_H */
