#ifndef _LIST_PLUGINS_H_
#define _LIST_PLUGINS_H_

#include <plugins.h>

/* list of initilizers and destroyers for each function */
#define N_PLUGINS 1
enum {
	REQ_SIZE_IND = 0
};
struct plug_init_dest_funcs plug_init_dest[] =
{
	{reqsize_init, reqsize_destroy, reqsize_ops_init, reqsize_ops_destroy}
};

#endif
