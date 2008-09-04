#include <stdlib.h>
#include <stdio.h>

#include <plugins.h>
#include <list_plugins.h>

/* array of operations and function initializer */
struct plugin_ops ps_ops[N_PLUGINS];

struct plugin_set *plugin_set_create()
{
	int i;
	
	struct plugin_set *tmp = g_new(struct plugin_set, 1);
	tmp->plugs = g_new(struct plugin, N_PLUGINS);
	tmp->n = N_PLUGINS;
	
	/* create and initilize a new set of plugins */
	for(i = 0; i < N_PLUGINS; ++i) {
		plug_init_dest[i].init(&tmp->plugs[i]);
		tmp->plugs[i].ops = &ps_ops[i];
	}
	
	return tmp;
}
	
void plugin_set_destroy(struct plugin_set *ps) 
{
	int i;
	
	/* destroy the plugins in the plugin set */
	for(i = 0; i < N_PLUGINS; ++i)
		plug_init_dest[i].destroy(&ps->plugs[i]);
	
	g_free(ps->plugs);
	g_free(ps);
}

void plugin_set_print(const struct plugin_set *ps, const char *head) 
{
	int i;
	
	printf("%s ==============================\n",head);
	for(i = 0; i < N_PLUGINS; ++i)
		ps->plugs[i].ops->print_results(ps->plugs[i].data);
}

void plugin_set_add_trace(struct plugin_set *ps, const struct blk_io_trace *t) 
{
	int i;
	int act;
	event_func_t event_handler;
	struct plugin *p;
	
	for(i = 0; i < N_PLUGINS; ++i) {
		p = &ps->plugs[i];
		act = t->action & 0xffff;
		event_handler = g_tree_lookup(p->ops->event_tree,(gpointer)act);
		if(event_handler)
			event_handler(t,p->data);
	}
}

void plugin_set_add(struct plugin_set *ps1, const struct plugin_set *ps2) 
{
	int i;
	struct plugin *p1, *p2;
	
	for(i = 0; i < N_PLUGINS; ++i) {
		p1 = &ps1->plugs[i];
		p2 = &ps2->plugs[i];
		p1->ops->add(p1->data, p2->data);
	}
}

void init_plugs_ops() 
{
	int i;	
	for(i = 0; i < N_PLUGINS; ++i)
		plug_init_dest[i].ops_init(&ps_ops[i]);
}

void destroy_plugs_ops() 
{
	int i;	
	for(i = 0; i < N_PLUGINS; ++i)
		plug_init_dest[i].ops_destroy(&ps_ops[i]);
}

