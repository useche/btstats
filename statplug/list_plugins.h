#ifndef _LIST_PLUGINS_H_
#define _LIST_PLUGINS_H_

#include <plugins.h>
#include <inits.h>

/* list of initilizers and destroyers for each function */
#define N_PLUGINS 2
enum {
	REQ_SIZE_IND = 0,
	SEEK_IND
};
static const struct plug_init_dest_funcs plug_init_dest[] =
{
	{
		.init = reqsize_init, 
		.destroy = reqsize_destroy, 
		.ops_init = reqsize_ops_init, 
		.ops_destroy = reqsize_ops_destroy
	},
	{
		.init = seek_init,
		.destroy = seek_destroy,
		.ops_init = seek_ops_init,
		.ops_destroy = seek_ops_destroy
	}
};

#endif
