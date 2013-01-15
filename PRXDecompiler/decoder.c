/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <string.h>

#include "code.h"
#include "utils.h"

int decode_instructions(struct code *c) {
	struct location *base;
	uint32 i, numopc, size, address;
	const uint8 *code;
	int slot = FALSE;

	address = c->file->programs->vaddr;
	size = c->file->modinfo->expvaddr - 4;
	code = c->file->programs->data;

	numopc = size >> 2;

	if ((size & 0x03) || (address & 0x03)) {
		error(__FILE__ ": size/address is not multiple of 4");
		return 0;
	}

	base = (struct location *) xmalloc((numopc) * sizeof(struct location));
	memset(base, 0, (numopc) * sizeof(struct location));

	c->base = base;
	c->end = &base[numopc - 1];
	c->baddr = address;
	c->numopc = numopc;

	for (i = 0; i < numopc; i++) {
		struct location *loc = &base[i];
		uint32 tgt;

		loc->opc = code[i << 2];
		loc->opc |= code[(i << 2) + 1] << 8;
		loc->opc |= code[(i << 2) + 2] << 16;
		loc->opc |= code[(i << 2) + 3] << 24;
		loc->insn = allegrex_decode(loc->opc, FALSE);
		loc->address = address + (i << 2);

		if (loc->insn == NULL ) {
			loc->error = ERROR_INVALID_OPCODE;
			slot = FALSE;
			continue;
		}

		/*
		 if (INSN_TYPE (loc->insn->flags) != INSN_ALLEGREX) {
		 slot = FALSE;
		 continue;
		 }
		 */

		if (loc->insn->flags & (INSN_BRANCH | INSN_JUMP)) {
			if (slot)
				c->base[i - 1].error = ERROR_DELAY_SLOT;
			slot = TRUE;
		} else {
			slot = FALSE;
		}

		if (loc->insn->flags & INSN_BRANCH) {
			tgt = loc->opc & 0xFFFF;
			if (tgt & 0x8000) {
				tgt |= ~0xFFFF;
			}
			tgt += i + 1;
			if (tgt < numopc) {
				loc->target = &base[tgt];
			} else {
				loc->error = ERROR_TARGET_OUTSIDE_FILE;
			}

			if (location_gpr_used(loc) == 0 || ((loc->insn->flags & INSN_READ_GPR_T) && RS (loc->opc) == RT (loc->opc))) {
				switch (loc->insn->insn) {
				case I_BEQ:
				case I_BEQL:
				case I_BGEZ:
				case I_BGEZAL:
				case I_BGEZL:
				case I_BLEZ:
				case I_BLEZL:
					loc->branchalways = TRUE;
					break;
				case I_BGTZ:
				case I_BGTZL:
				case I_BLTZ:
				case I_BLTZAL:
				case I_BLTZALL:
				case I_BLTZL:
				case I_BNE:
				case I_BNEL:
					loc->error = ERROR_ILLEGAL_BRANCH;
					break;
				default:
					loc->branchalways = FALSE;
					break;
				}
			}

		} else if (loc->insn->insn == I_J || loc->insn->insn == I_JAL) {
			uint32 target_addr = (loc->opc & 0x3FFFFFF) << 2;
			;
			target_addr |= ((loc->address) & 0xF0000000);
			tgt = (target_addr - address) >> 2;
			if (tgt < numopc) {
				loc->target = &base[tgt];
			} else {
				loc->error = ERROR_TARGET_OUTSIDE_FILE;
			}
		}
	}

	if (slot) {
		c->base[i - 1].error = ERROR_TARGET_OUTSIDE_FILE;
	}

	return 1;
}

uint32 location_gpr_used(struct location *loc) {
	uint32 result = 0;

	if (loc->insn->flags & INSN_READ_GPR_S) {
		if (RS (loc->opc) != 0)
			result |= 1 << (RS (loc->opc));
	}

	if (loc->insn->flags & INSN_READ_GPR_T) {
		if (RT (loc->opc) != 0)
			result |= 1 << (RT (loc->opc));
	}

	if (loc->insn->flags & INSN_READ_GPR_D) {
		if (RD (loc->opc) != 0)
			result |= 1 << (RD (loc->opc));
	}

	return result;
}

uint32 location_gpr_defined(struct location *loc) {
	uint32 result = 0;

	if (loc->insn->flags & INSN_LINK) {
		result |= 1 << 31;
	}

	if (loc->insn->flags & INSN_WRITE_GPR_D) {
		if (RD (loc->opc) != 0)
			result |= 1 << (RD (loc->opc));
	}

	if (loc->insn->flags & INSN_WRITE_GPR_T) {
		if (RT (loc->opc) != 0)
			result |= 1 << (RT (loc->opc));
	}
	return result;
}

int location_branch_may_swap(struct location *branch) {
	int gpr_used, gpr_defined;

	gpr_used = location_gpr_used(branch);
	gpr_defined = location_gpr_defined(&branch[1]);
	return (!(gpr_used & gpr_defined));
}
