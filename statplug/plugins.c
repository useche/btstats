#include <stdlib.h>

#include "plugins.h"

struct plugin_set *create_plugin_set()
{
	int i;
	
	struct plugin_set *tmp = g_new0(struct plugin_set, 1);
	tmp->plugs = g_new0(struct plugin, n_plugins);
	tmp->n = n_plugins;
	
	/* create and initilize a new set of plugins */
	for(i = 0; i < n_plugins; ++i) {
		plug_init_dest[i].init(&tmp->plugs[i]);
		tmp->plugs[i].ops = ps_ops[i];
	}
	
	return tmp;
}
	
void destroy_plugin_set(struct plugin_set *ps) 
{
	int i;
	
	/* destroy the plugins in the plugin set */
	for(i = 0; i < n_plugins; ++i)
		tmp->plugs[i].destroy(&tmp->plugs[i]);
	
	g_free(tmp->plugs);
	g_free(ps);
}

void initialize_plugs_ops() 
{
	int i;	
	for(i = 0; i < n_plugins; ++i)
		plug_init_dest[i].ops_init(ps_ops[i]);
}

