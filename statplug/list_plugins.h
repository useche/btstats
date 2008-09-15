#ifndef _LIST_PLUGINS_H_
#define _LIST_PLUGINS_H_

#include <plugins.h>
#include <inits.h>

/* list of initilizers and destroyers for each function */
#define N_PLUGINS 5
enum {
	REQ_SIZE_IND = 0,
	SEEK_IND,
	D2C_IND,
	MERGE_IND
};
static const struct plug_init_dest_funcs plug_init_dest[] =
{
	{
		.init = reqsize_init, 
		.destroy = reqsize_destroy, 
		.ops_init = reqsize_ops_init, 
		.ops_destroy = NULL
	},
	{
		.init = seek_init,
		.destroy = seek_destroy,
		.ops_init = seek_ops_init,
		.ops_destroy = NULL
	},
	{
		.init = d2c_init,
		.destroy = d2c_destroy,
		.ops_init = d2c_ops_init,
		.ops_destroy = NULL
	},
	{
		.init = merge_init,
		.destroy = merge_destroy,
		.ops_init = merge_ops_init,
		.ops_destroy = NULL
	},
	{
		.init = pluging_init,
		.destroy = pluging_destroy,
		.ops_init = pluging_ops_init,
		.ops_destroy = NULL
	}
};

#endif
