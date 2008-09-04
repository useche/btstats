#ifndef _PLUGINS_H_
#define _PLUGINS_H_

#include <glib.h>
#include <blktrace_api.h>
#include <inits.h>

typedef void (*event_func_t)(const struct blk_io_trace *, void *);
struct plugin_ops 
{
	/* hash table to find, given the event int,
	   the function to call */
	GTree *event_tree;

	/* additional functions */
	void (*add)(void *data1, const void *data2);
	void (*print_results)(const void *data);	
};

struct plugin
{
	/* private data per plugin */
	void *data;
	
	/* ops */
	struct plugin_ops *ops;
};

struct plug_init_dest_funcs
{
	/* init, destroy */
	void (*init)(struct plugin *p);
	void (*destroy)(struct plugin *p);
	void (*ops_init)(struct plugin_ops *po);
	void (*ops_destroy)(struct plugin_ops *po);
};

/* functions to create & destroy a plugin set */
struct plugin_set 
{
	struct plugin *plugs;
	int n;
};

void init_plugs_ops();
void destroy_plugs_ops();

struct plugin_set *plugin_set_create();
void plugin_set_destroy(struct plugin_set *ps);
void plugin_set_print(const struct plugin_set *ps, const char *head);
void plugin_set_add_trace(struct plugin_set *ps, const struct blk_io_trace *t);
void plugin_set_add(struct plugin_set *ps1, const struct plugin_set *ps2);

#endif
