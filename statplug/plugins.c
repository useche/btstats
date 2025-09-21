#include <stdio.h>
#include <stdlib.h>

#include <list_plugins.h>
#include <plugins.h>

#include "utils.h"

/* array of operations and function initializer */
struct plugin_ops ps_ops[N_PLUGINS];

static int event_entry_cmp(const void *a, const void *b) {
  const struct event_entry *e1 = a;
  const struct event_entry *e2 = b;
  return e1->event_key - e2->event_key;
}

RB_GENERATE(event_tree_head, event_entry, entry, event_entry_cmp);

struct plugin_set *plugin_set_create(struct plug_args *pia) {
  int i;

  struct plugin_set *tmp = malloc(sizeof(struct plugin_set));
  tmp->plugs = malloc(sizeof(struct plugin) * N_PLUGINS);
  tmp->n = N_PLUGINS;

  /* create and initilize a new set of plugins */
  for (i = 0; i < N_PLUGINS; ++i) {
    plug_init_dest[i].init(&tmp->plugs[i], tmp, pia);
    tmp->plugs[i].ops = &ps_ops[i];
  }

  return tmp;
}

void plugin_set_destroy(struct plugin_set *ps) {
  int i;

  /* destroy the plugins in the plugin set */
  for (i = 0; i < N_PLUGINS; ++i)
    plug_init_dest[i].destroy(&ps->plugs[i]);

  free(ps->plugs);
  free(ps);
}

void plugin_set_print(const struct plugin_set *ps, const char *head) {
  int i;

  printf("%s\t=====================================\n", head);
  for (i = 0; i < N_PLUGINS; ++i)
    ps->plugs[i].ops->print_results(ps->plugs[i].data);
}

void plugin_set_add_trace(struct plugin_set *ps, const struct blk_io_trace *t) {
  int i;
  long act;
  struct plugin *p;

  act = t->action & 0xffff;
  for (i = 0; i < N_PLUGINS; ++i) {
    p = &ps->plugs[i];
    struct event_entry find, *res;
    find.event_key = act;

    res = RB_FIND(event_tree_head, p->ops->event_tree, &find);
    if (res)
      res->event_handler(t, p->data);
  }
}

void plugin_set_add(struct plugin_set *ps1, const struct plugin_set *ps2) {
  int i;
  struct plugin *p1, *p2;

  for (i = 0; i < N_PLUGINS; ++i) {
    p1 = &ps1->plugs[i];
    p2 = &ps2->plugs[i];
    p1->ops->add(p1->data, p2->data);
  }
}

void init_plugs_ops() {
  int i;
  for (i = 0; i < N_PLUGINS; ++i) {
    ps_ops[i].event_tree = malloc(sizeof(struct event_tree_head));
    RB_INIT(event_tree_head, ps_ops[i].event_tree);
    if (plug_init_dest[i].ops_init)
      plug_init_dest[i].ops_init(&ps_ops[i]);
  }
}

void destroy_plugs_ops() {
  int i;
  for (i = 0; i < N_PLUGINS; ++i) {
    struct event_entry *var, *next;
    if (plug_init_dest[i].ops_destroy)
      plug_init_dest[i].ops_destroy(&ps_ops[i]);
    RB_FOREACH_SAFE(var, event_tree_head, ps_ops[i].event_tree, next) {
      RB_REMOVE(event_tree_head, ps_ops[i].event_tree, var);
      free(var);
    }
    free(ps_ops[i].event_tree);
  }
}
