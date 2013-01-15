/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include <stdio.h>
#include <ctype.h>

#include "output.h"
#include "utils.h"

static
void print_block(FILE *out, struct basicblock *block, int identsize, int reversecond) {
	element opel;
	int options = 0;

	if (reversecond)
		options |= OPTS_REVERSECOND;
	opel = list_head(block->operations);
	while (opel) {
		struct operation *op = element_getvalue(opel);
		if (!(op->status & OP_STAT_DEFERRED)) {
			if (op != block->jumpop)
				print_operation(out, op, identsize, options);
		}
		opel = element_next(opel);
	}
	if (block->jumpop)
		print_operation(out, block->jumpop, identsize, options);
}

static
void print_block_recursive(FILE *out, struct basicblock *block) {
	element ref;
	struct basicedge *edge;
	int identsize = block->st->identsize;
	int revcond = block->status & BLOCK_STAT_REVCOND;
	int first = TRUE;
	/* int isloop = FALSE; */

	if (g_verbosity > 1) {
		ident_line(out, identsize + 1);
		fprintf(out, "/** Block %d\n", block->node.dfsnum);
		if (block->type == BLOCK_SIMPLE) {
			struct location *loc;
			loc = block->info.simple.begin;
			while (1) {
				ident_line(out, identsize + 1);
				fprintf(out, " * %s\n", allegrex_disassemble(loc->opc, loc->address, TRUE));
				if (loc++ == block->info.simple.end)
					break;
			}
		}
		ident_line(out, identsize + 1);
		fprintf(out, " */\n");
	}

	if (block->status & BLOCK_STAT_ISSWITCHTARGET) {
		ref = list_head(block->inrefs);
		while (ref) {
			edge = element_getvalue(ref);
			if (edge->from->status & BLOCK_STAT_ISSWITCH) {
				ident_line(out, identsize);
				fprintf(out, "case %d:\n", edge->fromnum);
			}
			ref = element_next(ref);
		}
	}

	if (block->status & BLOCK_STAT_HASLABEL) {
		fprintf(out, "\n");
		ident_line(out, identsize);
		fprintf(out, "label%d:\n", block->node.dfsnum);
	}

	if (block->st->start == block && block->st->type == CONTROL_LOOP) {
		/*isloop = TRUE;*/
		ident_line(out, identsize);
		fprintf(out, "while (1) {\n");
	}

	block->mark1 = 1;
	print_block(out, block, identsize + 1, revcond);

	if (block->status & BLOCK_STAT_ISSWITCH) {
		revcond = TRUE;
		ident_line(out, identsize + 1);
		fprintf(out, "switch () {\n");
	}

	if (revcond)
		ref = list_head(block->outrefs);
	else
		ref = list_tail(block->outrefs);

	while (ref) {
		edge = element_getvalue(ref);

		switch (edge->type) {
		case EDGE_BREAK:
		case EDGE_CONTINUE:
		case EDGE_INVALID:
		case EDGE_GOTO:
		case EDGE_IFENTER:
			ident_line(out, identsize + 1);
			if (first && list_size(block->outrefs) == 2 && !(block->status & BLOCK_STAT_ISSWITCH) && edge->type != EDGE_IFENTER)
				ident_line(out, 1);
			break;

		case EDGE_CASE:
		case EDGE_NEXT:
		case EDGE_RETURN:
		case EDGE_IFEXIT:
		case EDGE_UNKNOWN:
			break;
		}

		switch (edge->type) {
		case EDGE_BREAK:
			fprintf(out, "break;\n");
			break;
		case EDGE_CONTINUE:
			fprintf(out, "continue;\n");
			break;
		case EDGE_INVALID:
		case EDGE_GOTO:
			fprintf(out, "goto label%d;\n", edge->to->node.dfsnum);
			break;
		case EDGE_UNKNOWN:
		case EDGE_IFEXIT:
			break;
		case EDGE_CASE:
			if (!edge->to->mark1)
				print_block_recursive(out, edge->to);
			break;
		case EDGE_NEXT:
		case EDGE_RETURN:
			print_block_recursive(out, edge->to);
			break;
		case EDGE_IFENTER:
			fprintf(out, "{\n");
			print_block_recursive(out, edge->to);
			ident_line(out, identsize + 1);
			fprintf(out, "}\n");
			break;
		}

		if (revcond)
			ref = element_next(ref);
		else
			ref = element_previous(ref);

		if ((block->status & BLOCK_STAT_HASELSE) && ref) {
			ident_line(out, identsize + 1);
			fprintf(out, "else\n");
		}
		first = FALSE;
	}

	if (block->ifst) {
		if (block->ifst->hasendgoto) {
			ident_line(out, identsize + 1);
			fprintf(out, "goto label%d;\n", block->ifst->end->node.dfsnum);
		} else if (block->ifst->endfollow) {
			print_block_recursive(out, block->ifst->end);
		}
	}

	if (block->status & BLOCK_STAT_ISSWITCH) {
		ident_line(out, identsize + 1);
		fprintf(out, "}\n");
	}

	if (block->st->start == block && block->st->type == CONTROL_LOOP) {
		ident_line(out, identsize);
		fprintf(out, "}\n");
		if (block->st->hasendgoto) {
			ident_line(out, identsize);
			fprintf(out, "goto label%d;\n", block->st->end->node.dfsnum);
		} else if (block->st->endfollow) {
			print_block_recursive(out, block->st->end);
		}
	}

}

static
void print_subroutine(FILE *out, struct subroutine *sub) {
	if (sub->import) {
		return;
	}

	fprintf(out, "/**\n * Subroutine at address 0x%08X\n", sub->begin->address);
	fprintf(out, " */\n");
	print_subroutine_declaration(out, sub);
	fprintf(out, "\n{\n");

	if (sub->haserror) {
		struct location *loc;
		for (loc = sub->begin;; loc++) {
			fprintf(out, "%s\n", allegrex_disassemble(loc->opc, loc->address, TRUE));
			if (loc == sub->end)
				break;
		}
	} else {
		element el;
		reset_marks(sub);

		el = list_head(sub->blocks);
		while (el) {
			struct basicblock *block = element_getvalue(el);
			if (!block->mark1)
				print_block_recursive(out, block);
			el = element_next(el);
		}
	}
	fprintf(out, "}\n\n");
}

static
void print_source(FILE *out, struct code *c, char *headerfilename) {
	uint32 i, j;
	element el;

	fprintf(out, "#include <pspsdk.h>\n");
	fprintf(out, "#include \"%s\"\n\n", headerfilename);

	for (i = 0; i < c->file->modinfo->numimports; i++) {
		struct prx_import *imp = &c->file->modinfo->imports[i];

		fprintf(out, "/*\n * Imports from library: %s\n */\n", imp->name);
		for (j = 0; j < imp->nfuncs; j++) {
			struct prx_function *func = &imp->funcs[j];
			if (func->pfunc) {
				fprintf(out, "extern ");
				print_subroutine_declaration(out, func->pfunc);
				fprintf(out, ";\n");
			}
		}
		fprintf(out, "\n");
	}

	el = list_head(c->subroutines);
	while (el) {
		struct subroutine *sub;
		sub = element_getvalue(el);

		print_subroutine(out, sub);
		el = element_next(el);
	}

}

static
void print_header(FILE *out, struct code *c, char *headerfilename) {
	uint32 i, j;
	char buffer[256];
	int pos = 0;

	while (pos < sizeof(buffer) - 1) {
		char c = headerfilename[pos];
		if (!c)
			break;
		if (c == '.')
			c = '_';
		else
			c = toupper(c);
		buffer[pos++] = c;
	}
	buffer[pos] = '\0';

	fprintf(out, "#ifndef __%s\n", buffer);
	fprintf(out, "#define __%s\n\n", buffer);

	for (i = 0; i < c->file->modinfo->numexports; i++) {
		struct prx_export *exp = &c->file->modinfo->exports[i];

		fprintf(out, "/*\n * Exports from library: %s\n */\n", exp->name);
		for (j = 0; j < exp->nfuncs; j++) {
			struct prx_function *func = &exp->funcs[j];
			if (func->name) {
				fprintf(out, "void %s (void);\n", func->name);
			} else {
				fprintf(out, "void %s_%08X (void);\n", exp->name, func->nid);
			}
		}
		fprintf(out, "\n");
	}

	fprintf(out, "#endif /* __%s */\n", buffer);
}

int print_code(struct code *c, char *prxname) {
	char buffer[64];
	char basename[32];
	FILE *cout, *hout;

	get_base_name(prxname, basename, sizeof(basename));
	sprintf(buffer, "%s.c", basename);

	cout = fopen(buffer, "w");
	if (!cout) {
		xerror(__FILE__ ": can't open file for writing `%s'", buffer);
		return 0;
	}

	sprintf(buffer, "%s.h", basename);
	hout = fopen(buffer, "w");
	if (!hout) {
		xerror(__FILE__ ": can't open file for writing `%s'", buffer);
		return 0;
	}

	print_header(hout, c, buffer);
	print_source(cout, c, buffer);

	fclose(cout);
	fclose(hout);
	return 1;
}
