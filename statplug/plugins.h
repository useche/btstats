#ifndef _PLUGINS_H_
#define _PLUGINS_H_

#include "include/bsd/tree.h"
#include <blktrace_api.h>

typedef void (*event_func_t)(const struct blk_io_trace *, void *);

struct event_entry {
  int event_key;
  event_func_t event_handler;
  RB_ENTRY(event_entry) entry;
};

RB_HEAD(event_tree_head, event_entry);
RB_PROTOTYPE(event_tree_head, event_entry, entry, event_entry_cmp);

struct plugin_ops {
  /* red-black tree with key = int of event,
     value = the function to call */
  struct event_tree_head *event_tree;

  /* additional functions */
  void (*add)(void *data1, const void *data2);
  void (*print_results)(const void *data);
};

struct plugin {
  /* private data per plugin */
  void *data;

  /* ops */
  struct plugin_ops *ops;
};

/* functions to create & destroy a plugin set */
struct plugin_set {
  struct plugin *plugs;
  int n;
};

struct plug_args {
  /* d2c args */
  char *d2c_det_f;
  __u64 end_range;

  /* i2c args */
  char *i2c_oio_f;
  char *i2c_oio_hist_f;
};

struct plug_init_dest_funcs {
  /* init, destroy */
  void (*init)(struct plugin *p, struct plugin_set *ps, struct plug_args *pia);
  void (*destroy)(struct plugin *p);
  void (*ops_init)(struct plugin_ops *po);
  void (*ops_destroy)(struct plugin_ops *po);
};

void init_plugs_ops();
void destroy_plugs_ops();

/* plugin set methods */
struct plugin_set *plugin_set_create(struct plug_args *pia);
void plugin_set_destroy(struct plugin_set *ps);
void plugin_set_print(const struct plugin_set *ps, const char *head);
void plugin_set_add_trace(struct plugin_set *ps, const struct blk_io_trace *t);
void plugin_set_add(struct plugin_set *ps1, const struct plugin_set *ps2);

#endif
