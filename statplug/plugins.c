#include <stdio.h>
#include <stdlib.h>
#include <blktrace_api.h>
#include <plugins.h>
#include <utils.h>

#include "list_plugins.h"

static struct plugin_ops *ps_ops;

int event_entry_cmp(const struct event_entry *a, const struct event_entry *b)
{
    return a->event_key - b->event_key;
}

RB_GENERATE(event_tree_head, event_entry, entry, event_entry_cmp);

void init_plugs_ops()
{
	int i;
	ps_ops = malloc(sizeof(struct plugin_ops) * N_PLUGINS);
	for(i = 0; i<N_PLUGINS; ++i) {
		ps_ops[i].event_tree = malloc(sizeof(struct event_tree_head));
		RB_INIT(event_tree_head, ps_ops[i].event_tree);
		if(plug_init_dest[i].ops_init)
			plug_init_dest[i].ops_init(&ps_ops[i]);
	}
}

void destroy_plugs_ops()
{
	int i;
	for(i = 0; i<N_PLUGINS; ++i) {
		if(plug_init_dest[i].ops_destroy)
			plug_init_dest[i].ops_destroy(&ps_ops[i]);

        struct event_entry *var, *next;
        RB_FOREACH_SAFE(var, event_tree_head, ps_ops[i].event_tree, next) {
            RB_REMOVE(event_tree_head, ps_ops[i].event_tree, var);
            free(var);
        }
		free(ps_ops[i].event_tree);
	}
	free(ps_ops);
}

struct plugin_set *plugin_set_create(struct plug_args *pia)
{
	int i;
	struct plugin_set *tmp = malloc(sizeof(struct plugin_set));
	tmp->n = N_PLUGINS;
	tmp->plugs = malloc(sizeof(struct plugin) * N_PLUGINS);

	for(i=0;i<N_PLUGINS;i++) {
		tmp->plugs[i].ops = &ps_ops[i];
		plug_init_dest[i].init(&tmp->plugs[i], tmp, pia);
	}

	return tmp;
}

void plugin_set_destroy(struct plugin_set *ps)
{
	int i;
	for(i=0; i<ps->n; ++i)
		plug_init_dest[i].destroy(&ps->plugs[i]);
	free(ps->plugs);
	free(ps);
}

void plugin_set_print(const struct plugin_set *ps, const char *head)
{
	int i;

	printf("\nStats for %s\n",head);
	for(i=0;i<ps->n;i++)
		if(ps_ops[i].print_results)
			ps_ops[i].print_results(ps->plugs[i].data);
}

void plugin_set_add_trace(struct plugin_set *ps, const struct blk_io_trace *t)
{
	int i;
	const __u32 act = t->action & 0x0fffffff;

	for(i=0; i<ps->n; ++i) {
		struct plugin *p = &ps->plugs[i];
		struct event_entry find, *res;
        find.event_key = act;

		res = RB_FIND(event_tree_head, p->ops->event_tree, &find);
		if(res)
			res->event_handler(t,p->data);
	}
}

void plugin_set_add(struct plugin_set *ps1, const struct plugin_set *ps2)
{
	int i;
	for(i=0;i<ps1->n;i++)
		if(ps_ops[i].add)
			ps_ops[i].add(ps1->plugs[i].data,ps2->plugs[i].data);
}
