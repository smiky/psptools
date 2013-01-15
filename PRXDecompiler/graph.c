/**
 * Author: Humberto Naves (hsnaves@gmail.com)
 */

#include "code.h"
#include "utils.h"

static
void dfs_step (struct basicblock *block, int reverse)
{
  struct basicblock *next;
  struct basicblocknode *node, *nextnode;
  list refs, out;
  element el;

  if (reverse) {
    node = &block->revnode;
    refs = block->inrefs;
    out = block->sub->revdfsblocks;
  } else {
    node = &block->node;
    refs = block->outrefs;
    out = block->sub->dfsblocks;
  }
  node->dfsnum = -1;

  el = list_head (refs);
  while (el) {
    struct basicedge *edge;
    edge = element_getvalue (el);
    if (reverse) {
      next = edge->from;
      nextnode = &next->revnode;
    } else {
      next = edge->to;
      nextnode = &next->node;
    }

    if (!nextnode->dfsnum) {
      nextnode->parent = node;
      list_inserttail (node->children, nextnode);
      dfs_step (next, reverse);
    }
    el = element_next (el);
  }

  node->dfsnum = block->sub->temp--;
  node->blockel = list_inserthead (out, block);
}

static
int cfg_dfs (struct subroutine *sub, int reverse)
{
  struct basicblock *start;
  sub->temp = list_size (sub->blocks);
  start = reverse ? sub->endblock : sub->startblock;

  dfs_step (start, reverse);
  return (sub->temp == 0);
}

int dom_isancestor (struct basicblocknode *ancestor, struct basicblocknode *node)
{
  return (ancestor->domdfsnum.first <= node->domdfsnum.first &&
          ancestor->domdfsnum.last <= node->domdfsnum.last);
}

struct basicblocknode *dom_common (struct basicblocknode *n1, struct basicblocknode *n2)
{
  while (n1 != n2) {
    while (n1->dfsnum > n2->dfsnum) {
      n1 = n1->dominator;
    }
    while (n2->dfsnum > n1->dfsnum) {
      n2 = n2->dominator;
    }
  }
  return n1;
}

static
void dom_dfs_step (struct basicblocknode *node, struct intpair *domdfsnum)
{
  struct basicblocknode *next;
  element el;

  node->domdfsnum.first = (domdfsnum->first)++;
  el = list_head (node->domchildren);
  while (el) {
    next = element_getvalue (el);
    if (!next->domdfsnum.first)
      dom_dfs_step (next, domdfsnum);
    el = element_next (el);
  }
  node->domdfsnum.last = (domdfsnum->last)--;
}

static
void cfg_dominance (struct subroutine *sub, int reverse)
{
  struct basicblock *start;
  struct basicblocknode *startnode;
  struct intpair domdfsnum;
  int changed = TRUE;
  list blocks, refs;
  element el;

  if (reverse) {
    blocks = sub->revdfsblocks;
    start = sub->endblock;
    startnode = start->revnode.dominator = &start->revnode;
  } else {
    blocks = sub->dfsblocks;
    start = sub->startblock;
    startnode = start->node.dominator = &start->node;
  }

  while (changed) {
    changed = FALSE;
    el = list_head (blocks);
    el = element_next (el);
    while (el) {
      struct basicblock *block;
      struct basicblocknode *node, *dom = NULL;
      element ref;

      block = element_getvalue (el);
      refs = (reverse) ? block->outrefs : block->inrefs;
      ref = list_head (refs);
      while (ref) {
        struct basicblock *bref;
        struct basicblocknode *brefnode;
        struct basicedge *edge;

        edge = element_getvalue (ref);
        if (reverse) {
          bref = edge->to;
          brefnode = &bref->revnode;
        } else {
          bref = edge->from;
          brefnode = &bref->node;
        }

        if (brefnode->dominator) {
          if (!dom) {
            dom = brefnode;
          } else {
            dom = dom_common (dom, brefnode);
          }
        }

        ref = element_next (ref);
      }

      node = (reverse) ? &block->revnode : &block->node;
      if (dom != node->dominator) {
        node->dominator = dom;
        changed = TRUE;
      }

      el = element_next (el);
    }
  }

  el = list_head (blocks);
  while (el) {
    struct basicblock *block;
    struct basicblocknode *node;

    block = element_getvalue (el);
    node = (reverse) ? &block->revnode : &block->node;
    list_inserttail (node->dominator->domchildren, node);

    el = element_next (el);
  }

  domdfsnum.first = 0;
  domdfsnum.last = list_size (blocks);
  dom_dfs_step (startnode, &domdfsnum);
}

static
void cfg_frontier (struct subroutine *sub, int reverse)
{
  struct basicblock *block;
  struct basicblocknode *blocknode, *runner;
  element el, ref;
  list refs;

  el = (reverse) ? list_head (sub->revdfsblocks) : list_head (sub->dfsblocks);

  while (el) {
    block = element_getvalue (el);
    if (reverse) {
      refs = block->outrefs;
      blocknode = &block->revnode;
    } else {
      refs = block->inrefs;
      blocknode = &block->node;
    }
    if (list_size (refs) >= 2) {
      ref = list_head (refs);
      while (ref) {
        struct basicedge *edge = element_getvalue (ref);
        runner = (reverse) ? &edge->to->revnode : &edge->from->node;
        while (runner != blocknode->dominator) {
          list_inserttail (runner->frontier, blocknode);
          runner = runner->dominator;
        }
        ref = element_next (ref);
      }
    }
    el = element_next (el);
  }
}

void cfg_traverse (struct subroutine *sub, int reverse)
{
  if (!reverse) {
    if (!cfg_dfs (sub, FALSE)) {
      error (__FILE__ ": unreachable code at subroutine 0x%08X", sub->begin->address);
      sub->haserror = TRUE;
      return;
    }
  } else {
    if (!cfg_dfs (sub, TRUE)) {
      error (__FILE__ ": infinite loop at subroutine 0x%08X", sub->begin->address);
      sub->haserror = TRUE;
      return;
    }
  }

  cfg_dominance (sub, reverse);
  cfg_frontier (sub, reverse);
}
