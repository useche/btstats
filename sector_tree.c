#include "include/sector_tree.h"

int sector_entry_cmp(const struct sector_entry *a, const struct sector_entry *b) {
    if (a->sector < b->sector) return -1;
    if (a->sector > b->sector) return 1;
    return 0;
}

RB_GENERATE(sector_tree_head, sector_entry, entry, sector_entry_cmp);
