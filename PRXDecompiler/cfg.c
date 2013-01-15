/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include "code.h"
#include "utils.h"

static
struct basicblock *alloc_block (struct subroutine *sub, int insert)
{
  struct basicblock *block;
  block = fixedpool_alloc (sub->code->blockspool);

  block->inrefs = list_alloc (sub->code->lstpool);
  block->outrefs = list_alloc (sub->code->lstpool);
  block->node.children = list_alloc (sub->code->lstpool);
  block->revnode.children = list_alloc (sub->code->lstpool);
  block->node.domchildren = list_alloc (sub->code->lstpool);
  block->revnode.domchildren = list_alloc (sub->code->lstpool);
  block->node.frontier = list_alloc (sub->code->lstpool);
  block->revnode.frontier = list_alloc (sub->code->lstpool);
  block->sub = sub;
  if (insert) {
    block->blockel = list_inserttail (sub->blocks, block);
  } else {
    block->blockel = element_alloc (sub->code->lstpool, block);
  }

  return block;
}

static
void extract_blocks (struct subroutine *sub)
{
  struct location *begin, *next;
  struct basicblock *block;
  int prevlikely = FALSE;

  sub->blocks = list_alloc (sub->code->lstpool);
  sub->revdfsblocks = list_alloc (sub->code->lstpool);
  sub->dfsblocks = list_alloc (sub->code->lstpool);

  block = alloc_block (sub, TRUE);
  block->type = BLOCK_START;
  sub->startblock = block;

  begin = sub->begin;

  while (1) {
    block = alloc_block (sub, TRUE);
    if (sub->firstblock) sub->firstblock = block;

    if (!begin) break;
    next = begin;

    block->type = BLOCK_SIMPLE;
    block->info.simple.begin = begin;

    for (; next != sub->end; next++) {
      if (prevlikely) {
        prevlikely = FALSE;
        break;
      }

      if (next->references && (next != begin)) {
        next--;
        break;
      }

      if (next->insn->flags & (INSN_JUMP | INSN_BRANCH)) {
        block->info.simple.jumploc = next;
        if (next->insn->flags & INSN_BRANCHLIKELY)
          prevlikely = TRUE;
        if (!(next->insn->flags & INSN_BRANCHLIKELY) &&
            !next[1].references && location_branch_may_swap (next)) {
          next++;
        }
        break;
      }
    }
    block->info.simple.end = next;

    do {
      begin->block = block;
    } while (begin++ != next);

    begin = NULL;
    while (next++ != sub->end) {
      if (next->reachable == LOCATION_DELAY_SLOT) {
        if (!prevlikely) {
          begin = next;
          break;
        }
      } else if (next->reachable == LOCATION_REACHABLE) {
        if (next != &block->info.simple.end[1])
          prevlikely = FALSE;
        begin = next;
        break;
      }
    }
  }
  block->type = BLOCK_END;
  sub->endblock = block;
}


static
void make_link (struct basicblock *from, struct basicblock *to)
{
  struct basicedge *edge = fixedpool_alloc (from->sub->code->edgespool);

  edge->from = from;
  edge->fromnum = list_size (from->outrefs);
  edge->to = to;
  edge->tonum = list_size (to->inrefs);

  edge->fromel = list_inserttail (from->outrefs, edge);
  edge->toel = list_inserttail (to->inrefs, edge);
}

static
struct basicblock *make_link_and_insert (struct basicblock *from, struct basicblock *to, element el)
{
  struct basicblock *block = alloc_block (from->sub, FALSE);
  element_insertbefore (el, block->blockel);
  make_link (from, block);
  make_link (block, to);
  return block;
}

static
void make_call (struct basicblock *call, struct basicblock *from, struct location *loc)
{
  call->type = BLOCK_CALL;
  list_inserttail (call->sub->callblocks, call);
  call->info.call.from = from;
  if (loc->target) {
    call->info.call.calltarget = loc->target->sub;
    list_inserttail (loc->target->sub->whereused, call);
  }
}


static
void link_blocks (struct subroutine *sub)
{
  struct basicblock *block, *next;
  struct basicblock *target;
  struct location *loc;
  element el;

  el = list_head (sub->blocks);

  while (el) {
    block = element_getvalue (el);
    if (block->type == BLOCK_END) break;
    if (block->type == BLOCK_START) {
      el = element_next (el);
      make_link (block, element_getvalue (el));
      continue;
    }

    el = element_next (el);
    next = element_getvalue (el);


    if (block->info.simple.jumploc) {
      loc = block->info.simple.jumploc;
      if (loc->insn->flags & INSN_BRANCH) {
        if (!loc->branchalways) {
          if (loc->insn->flags & INSN_BRANCHLIKELY) {
            make_link (block, loc[2].block);
          } else {
            make_link (block, next);
          }
        }

        if (loc == block->info.simple.end) {
          struct basicblock *slot = alloc_block (sub, FALSE);
          element_insertbefore (el, slot->blockel);

          slot->type = BLOCK_SIMPLE;
          slot->info.simple.begin = &block->info.simple.end[1];
          slot->info.simple.end = slot->info.simple.begin;
          make_link (block, slot);
          block = slot;
        }

        if (loc->insn->flags & INSN_LINK) {
          target = make_link_and_insert (block, loc[2].block, el);
          make_call (target, block, loc);
        } else if (loc->target->sub->begin == loc->target) {
          target = make_link_and_insert (block, sub->endblock, el);
          make_call (target, block, loc);
        } else {
          make_link (block, loc->target->block);
        }

      } else {
        if (loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D)) {
          target = make_link_and_insert (block, next, el);
          make_call (target, block, loc);
        } else {
          if (loc->target) {
            if (loc->target->sub->begin == loc->target) {
              target = make_link_and_insert (block, sub->endblock, el);
              make_call (target, block, loc);
            } else {
              make_link (block, loc->target->block);
            }
          } else {
            element ref;
            if (loc->cswitch && loc->cswitch->jumplocation == loc) {
              block->status |= BLOCK_STAT_ISSWITCH;
              ref = list_head (loc->cswitch->references);
              while (ref) {
                struct location *switchtarget = element_getvalue (ref);
                make_link (block, switchtarget->block);
                switchtarget->block->status |= BLOCK_STAT_ISSWITCHTARGET;
                ref = element_next (ref);
              }
            } else
              make_link (block, sub->endblock);
          }
        }
      }
    } else {
      make_link (block, next);
    }
  }
}

void extract_cfg (struct subroutine *sub)
{
  extract_blocks (sub);
  link_blocks (sub);
}
