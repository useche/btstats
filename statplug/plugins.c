#include <stdlib.h>

#include "plugins.h"

struct plugin_set *plugin_set_create()
{
	int i;
	
	struct plugin_set *tmp = g_new(struct plugin_set, 1);
	tmp->plugs = g_new(struct plugin, n_plugins);
	tmp->n = n_plugins;
	
	/* create and initilize a new set of plugins */
	for(i = 0; i < n_plugins; ++i) {
		plug_init_dest[i].init(&tmp->plugs[i]);
		tmp->plugs[i].ops = ps_ops[i];
	}
	
	return tmp;
}
	
void plugin_set_destroy(struct plugin_set *ps) 
{
	int i;
	
	/* destroy the plugins in the plugin set */
	for(i = 0; i < n_plugins; ++i)
		ps->plugs[i]->destroy(&ps->plugs[i]);
	
	g_free(ps->plugs);
	g_free(ps);
}

void plugin_set_print(const char *head, const struct plugin_set *ps) 
{
	int i;
	
	printf("%s ==============================\n",head);
	for(i = 0; i < n_plugins; ++i)
		ps->plugs[i]->print_results(&ps->plugs[i]);
}

void plugin_set_add_trace(struct plugin_set *ps, const struct blk_io_trace *t) 
{
	int i;
	event_func_t event_handler;
	struct plugin *p;
	
	for(i = 0; i < n_plugins; ++i) {
		p = ps->plugs[i];
		event_handler = g_hash_table_lookup(p->ops->event_ht,t->action);
		if(event_handler)
			event_handler(t,p->data);
	}
}

void init_plugs_ops() 
{
	int i;	
	for(i = 0; i < n_plugins; ++i)
		plug_init_dest[i].ops_init(ps_ops[i]);
}

void destroy_plugs_ops() 
{
	int i;	
	for(i = 0; i < n_plugins; ++i)
		plug_init_dest[i].ops_destroy(ps_ops[i]);
}
