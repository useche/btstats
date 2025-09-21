#include <asm/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <blktrace.h>
#include <blktrace_api.h>
#include <list_plugins.h>
#include <plugins.h>
#include <utils.h>

#include <reqsize.h>

#define DECL_ASSIGN_SEEK(name, data)                                           \
  struct seek_data *name = (struct seek_data *)data

struct seek_data {
  __u64 lastpos;
  struct reqsize_data *req_dat;

  __u64 max;
  __u64 min;
  __u64 total;
  __u64 seeks;
};

static void C(const struct blk_io_trace *t, void *data) {
  DECL_ASSIGN_SEEK(seek, data);

  __u64 blks = t_blks(t);

  if (seek->lastpos != UINT64_MAX) {
    if (seek->lastpos != t->sector) {
      __u64 distance = seek->lastpos > t->sector ? seek->lastpos - t->sector
                                                 : t->sector - seek->lastpos;
      seek->total += distance;
      seek->max = MAX(seek->max, distance);
      seek->min = MIN(seek->min, distance);
      seek->seeks++;
    }
  }

  seek->lastpos = t->sector + blks;
}

void seek_add(void *data1, const void *data2) {
  DECL_ASSIGN_SEEK(seek1, data1);
  DECL_ASSIGN_SEEK(seek2, data2);

  seek1->min = MIN(seek1->min, seek2->min);
  seek1->max = MAX(seek2->max, seek2->max);
  seek1->total += seek2->total;
  seek1->seeks += seek2->seeks;
}

void seek_print_results(const void *data) {
  DECL_ASSIGN_SEEK(seek, data);

  if (seek->seeks) {
    printf("Seq.: %.2f%%\n",
           (1 - (((double)seek->seeks) / seek->req_dat->total_size)) * 100);
    printf(
        "Seeks #: %llu min: %llu avg: %f max: %llu (blks)\n",
        (long long unsigned int)seek->seeks, (long long unsigned int)seek->min,
        ((double)seek->total) / seek->seeks, (long long unsigned int)seek->max);
  }
}

void seek_init(struct plugin *p, struct plugin_set *ps,
               struct plug_args *__un) {
  struct seek_data *seek = p->data = calloc(1, sizeof(struct seek_data));
  seek->lastpos = UINT64_MAX;
  seek->max = 0;
  seek->min = ~0;
  seek->total = 0;
  seek->seeks = 0;
  seek->req_dat = (struct reqsize_data *)ps->plugs[REQ_SIZE_IND].data;
}

void seek_destroy(struct plugin *p) { free(p->data); }

void seek_ops_init(struct plugin_ops *po) {
  po->add = seek_add;
  po->print_results = seek_print_results;

  struct event_entry *e = malloc(sizeof(struct event_entry));
  e->event_key = __BLK_TA_COMPLETE;
  e->event_handler = (event_func_t)C;
  RB_INSERT(event_tree_head, po->event_tree, e);
}
