/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include "code.h"
#include "utils.h"

static struct ctrlstruct *alloc_ctrlstruct(struct basicblock *block, enum ctrltype type) {
	struct ctrlstruct *st = fixedpool_alloc(block->sub->code->ctrlspool);
	st->start = block;
	st->type = type;
	return st;
}

static
int mark_backward(struct basicblock *start, list worklist, int num) {
	struct basicblock *block;
	element ref;
	int count = 0;

	while (list_size(worklist) != 0) {
		block = list_removetail(worklist);
		if (block->mark1 == num)
			continue;
		block->mark1 = num;
		count++;

		ref = list_head(block->inrefs);
		while (ref) {
			struct basicedge *edge = element_getvalue(ref);
			struct basicblock *next = edge->from;
			ref = element_next(ref);

			if (next->node.dfsnum < start->node.dfsnum)
				continue;

			if (next->node.dfsnum >= block->node.dfsnum)
				if (!dom_isancestor(&block->node, &next->node) || !dom_isancestor(&block->revnode, &next->revnode))
					continue;

			if (next->mark1 != num) {
				list_inserthead(worklist, next);
			}
		}
	}

	return count;
}

static
void mark_forward(struct basicblock *start, struct ctrlstruct *loop, int num, int count) {
	element el, ref;

	el = start->node.blockel;
	while (el && count) {
		struct basicblock *block = element_getvalue(el);
		if (block->mark1 == num) {
			block->loopst = loop;
			count--;
			ref = list_head(block->outrefs);
			while (ref) {
				struct basicedge *edge = element_getvalue(ref);
				struct basicblock *next = edge->to;
				if (next->mark1 != num) {
					/* edge->type = EDGE_GOTO;
					 next->status |= BLOCK_STAT_HASLABEL; */
					if (!loop->end)
						loop->end = next;
					if (list_size(loop->end->inrefs) < list_size(next->inrefs))
						loop->end = next;
				}
				ref = element_next(ref);
			}
		}

		el = element_next(el);
	}
}

static
void mark_loop(struct ctrlstruct *loop, int num) {
	element el;
	list worklist;
	int count;

	worklist = list_alloc(loop->start->sub->code->lstpool);
	el = list_head(loop->info.loopctrl.edges);
	while (el) {
		struct basicedge *edge = element_getvalue(el);
		struct basicblock *block = edge->from;

		list_inserttail(worklist, block);
		el = element_next(el);
	}
	count = mark_backward(loop->start, worklist, num);

	mark_forward(loop->start, loop, num, count);
	if (loop->end) {
		/* loop->end->status &= ~BLOCK_STAT_HASLABEL; */
		el = list_head(loop->end->inrefs);
		while (el) {
			struct basicedge *edge = element_getvalue(el);
			if (edge->from->loopst == loop)
				edge->type = EDGE_BREAK;
			el = element_next(el);
		}
	}

	list_free(worklist);
}

static
void extract_loops(struct subroutine *sub) {
	struct basicblock *block;
	struct basicedge *edge;
	struct ctrlstruct *loop;
	element el, ref;
	int num = 0;

	el = list_head(sub->dfsblocks);
	while (el) {
		block = element_getvalue(el);

		loop = NULL;
		ref = list_head(block->inrefs);
		while (ref) {
			edge = element_getvalue(ref);
			if (edge->from->node.dfsnum >= block->node.dfsnum) {
				edge->type = EDGE_CONTINUE;
				if (!dom_isancestor(&block->node, &edge->from->node)) {
					error(__FILE__ ": graph of sub 0x%08X is not reducible (using goto)", sub->begin->address);
					edge->type = EDGE_INVALID;
					edge->to->status |= BLOCK_STAT_HASLABEL;
				} else if (block->loopst == edge->from->loopst) {
					if (!loop) {
						loop = alloc_ctrlstruct(block, CONTROL_LOOP);
						loop->info.loopctrl.edges = list_alloc(sub->code->lstpool);
					}
					list_inserttail(loop->info.loopctrl.edges, edge);
				}/* else {
				 edge->type = EDGE_GOTO;
				 edge->to->status |= BLOCK_STAT_HASLABEL;
				 }*/
			}
			ref = element_next(ref);
		}
		if (loop)
			mark_loop(loop, ++num);
		el = element_next(el);
	}
}

/*static
void extract_returns_step(struct basicblock *block) {
	element ref;
	struct basicedge *edge;

	block->status &= ~BLOCK_STAT_HASLABEL;
	ref = list_head(block->inrefs);
	while (ref) {
		edge = element_getvalue(ref);
		edge->type = EDGE_RETURN;
		if (list_size(edge->from->outrefs) == 1) {
			extract_returns_step(edge->from);
		}
		ref = element_next(ref);
	}
}*/

/* never used. */
/*static
void extract_returns(struct subroutine *sub) {
	extract_returns_step(sub->endblock);
}*/

static
void structure_search(struct basicblock *block, struct ctrlstruct *parentst, int blockcond) {
	block->st = parentst;
	block->mark1 = 1;
	block->blockcond = blockcond;

	if (block->loopst) {
		if (block->loopst->start == block) {
			if (block->loopst->end) {
				if (block->loopst->end->mark1) {
					if (parentst->end != block->loopst->end) {
						block->loopst->hasendgoto = TRUE;
						block->loopst->end->status |= BLOCK_STAT_HASLABEL;
					}
				} else {
					block->loopst->endfollow = TRUE;
					structure_search(block->loopst->end, parentst, blockcond);
				}
			}

			block->st = block->loopst;
			block->st->parent = parentst;
			block->st->identsize = parentst->identsize + 1;
		}
	}

	if (block->status & BLOCK_STAT_ISSWITCH) {
		struct ctrlstruct *nst;
		element ref;

		nst = alloc_ctrlstruct(block, CONTROL_SWITCH);
		nst->end = element_getvalue(block->revnode.dominator->blockel);
		nst->parent = block->st;
		nst->identsize = block->st->identsize + 1;
		block->ifst = nst;

		ref = list_head(block->outrefs);
		while (ref) {
			struct basicedge *edge = element_getvalue(ref);

			if (edge->type == EDGE_UNKNOWN) {
				edge->type = EDGE_CASE;
				if (!edge->to->mark1) {
					structure_search(edge->to, nst, edge->fromnum);
				} else {
					if (edge->to->st != nst) {
						edge->type = EDGE_GOTO;
						edge->to->status |= BLOCK_STAT_HASLABEL;
					}
				}
			}
			ref = element_next(ref);
		}
	} else if (list_size(block->outrefs) == 2) {
		struct basicblock *end;
		struct basicedge *edge1, *edge2;

		end = element_getvalue(block->revnode.dominator->blockel);
		edge1 = list_tailvalue(block->outrefs);
		edge2 = list_headvalue(block->outrefs);

		if (edge1->to != end && edge1->to->mark1 && edge1->type == EDGE_UNKNOWN) {
			edge1->type = EDGE_GOTO;
			edge1->to->status |= BLOCK_STAT_HASLABEL;
		}

		if (edge2->to != end && edge2->to->mark1 && edge2->type == EDGE_UNKNOWN) {
			edge2->type = EDGE_GOTO;
			edge2->to->status |= BLOCK_STAT_HASLABEL;
		}

		if (edge1->type == EDGE_UNKNOWN && edge2->type == EDGE_UNKNOWN) {
			struct ctrlstruct *nst = alloc_ctrlstruct(block, CONTROL_IF);
			nst->end = end;
			nst->parent = block->st;
			nst->identsize = block->st->identsize + 1;
			block->ifst = nst;

			if (!end->mark1) {
				nst->endfollow = TRUE;
				end->mark1 = TRUE;
			} else {
				if (block->st->end != end) {
					nst->hasendgoto = TRUE;
					end->status |= BLOCK_STAT_HASLABEL;
				}
			}

			if (edge1->to == end) {
				edge1->type = EDGE_IFEXIT;
			} else {
				edge1->type = EDGE_IFENTER;
				structure_search(edge1->to, nst, TRUE);
			}

			if (edge2->to == end) {
				edge2->type = EDGE_IFEXIT;
			} else {
				if (edge2->to->mark1) {
					edge2->type = EDGE_GOTO;
					edge2->to->status |= BLOCK_STAT_HASLABEL;
				} else {
					edge2->type = EDGE_IFENTER;
					structure_search(edge2->to, nst, FALSE);
				}
			}

			if (edge2->type != EDGE_IFEXIT) {
				if (edge1->type == EDGE_IFEXIT)
					block->status |= BLOCK_STAT_REVCOND;
				else
					block->status |= BLOCK_STAT_HASELSE;
			}

			if (nst->endfollow) {
				end->mark1 = 0;
				structure_search(end, block->st, blockcond);
			}

		} else if (edge1->type == EDGE_UNKNOWN || edge2->type == EDGE_UNKNOWN) {
			struct basicedge *edge;

			if (edge1->type == EDGE_UNKNOWN) {
				block->status |= BLOCK_STAT_REVCOND;
				edge = edge1;
			} else {
				edge = edge2;
			}

			if (edge->to == block->st->end && block->st->type == CONTROL_IF) {
				edge->type = EDGE_IFEXIT;
			} else {
				if (edge->to->mark1) {
					edge->type = EDGE_GOTO;
					edge->to->status |= BLOCK_STAT_HASLABEL;
				} else {
					edge->type = EDGE_NEXT;
					structure_search(edge->to, block->st, blockcond);
				}
			}
		}
	} else {
		struct basicedge *edge;
		edge = list_headvalue(block->outrefs);
		if (edge) {
			if (edge->type == EDGE_UNKNOWN) {
				if (edge->to == block->st->end && block->st->type == CONTROL_IF) {
					edge->type = EDGE_IFEXIT;
				} else {
					if (edge->to->mark1) {
						edge->type = EDGE_GOTO;
						edge->to->status |= BLOCK_STAT_HASLABEL;
					} else {
						edge->type = EDGE_NEXT;
						structure_search(edge->to, block->st, blockcond);
					}
				}
			}
		}
	}
}

void reset_marks(struct subroutine *sub) {
	element el = list_head(sub->blocks);
	while (el) {
		struct basicblock *block = element_getvalue(el);
		block->mark1 = block->mark2 = 0;
		el = element_next(el);
	}
}

void extract_structures(struct subroutine *sub) {
	element el;
	struct ctrlstruct *st = fixedpool_alloc(sub->code->ctrlspool);

	st->type = CONTROL_MAIN;
	st->start = sub->startblock;
	st->end = sub->endblock;

	reset_marks(sub);
	extract_loops(sub);

	/* extract_returns (sub); */

	reset_marks(sub);
	el = list_head(sub->blocks);
	while (el) {
		struct basicblock *block = element_getvalue(el);
		if (!block->mark1)
			structure_search(block, st, 0);
		el = element_next(el);
	}
}
