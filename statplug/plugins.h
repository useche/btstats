#include <blktrace_api.h>
#include "inits.h"

typedef void (*event_func_t)(struct blk_io_trace *, void *);
struct plugin_ops 
{
	/* event handlers */
	event_func_t C;
	event_func_t D;
	event_func_t I;
	event_func_t B;
	event_func_t M;
	event_func_t F;
	event_func_t G;
	event_func_t S;
	event_func_t P;
	event_func_t U;
	event_func_t UT;
	event_func_t R;
	event_func_t A;
	event_func_t m;

	/* additional functions */
	void (*add)(void *data1, void *data2);
	void (*print_results)(void *data);	
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
	void (*ops_init)(struct plugin_ops *po);
	void (*destroy)(struct plugin *p);
};

/* list of initilizers and destroyers for each function */
const int n_plugins = 2;
struct plug_init_dest_funcs plug_init_dest[] =
{
	{reqsize_init, reqsize_ops_init, reqsize_destroy},
	{d2c_init, d2c_ops_init, d2c_destroy}
};

/* array of operations and function initializer */
struct plugin_ops ps_ops[n_plugins];

/* functions to create & destroy a plugin set */
struct plugin_set 
{
	struct plugin *plugs;
	int n;
};

struct plugin_set *create_plugin_set();
void destroy_plugin_set(struct plugin_set *ps);
void initialize_plugs_ops();
