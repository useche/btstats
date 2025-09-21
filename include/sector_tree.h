#ifndef SECTOR_TREE_H
#define SECTOR_TREE_H

#include "bsd/tree.h"
#include <blktrace_api.h>

struct sector_entry {
  __u64 sector;
  struct blk_io_trace *trace;
  RB_ENTRY(sector_entry) entry;
};

RB_HEAD(sector_tree_head, sector_entry);
int sector_entry_cmp(const struct sector_entry *a,
                     const struct sector_entry *b);
RB_PROTOTYPE(sector_tree_head, sector_entry, entry, sector_entry_cmp);

#endif
