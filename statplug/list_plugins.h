#ifndef _LIST_PLUGINS_H_
#define _LIST_PLUGINS_H_

#include <plugins.h>

/* macro to declare inits and destroyers of plugins */
#define DECLARE_PLUG_FUNCS(name)			\
	void name##_init(struct plugin *p,		\
	    struct plugin_set *ps,			\
	    struct plug_args *pia);			\
	void name##_destroy(struct plugin *p);		\
	void name##_ops_init(struct plugin_ops *po);

DECLARE_PLUG_FUNCS(reqsize);
DECLARE_PLUG_FUNCS(seek);
DECLARE_PLUG_FUNCS(d2c);
DECLARE_PLUG_FUNCS(q2c);
DECLARE_PLUG_FUNCS(i2c);
DECLARE_PLUG_FUNCS(c2d);
DECLARE_PLUG_FUNCS(merge);
DECLARE_PLUG_FUNCS(pluging);

/* list of initilizers and destroyers for each function */
enum {
	REQ_SIZE_IND = 0,
	SEEK_IND,
	D2C_IND,
	C2D_IND,
	Q2C_IND,
	I2C_IND,
	MERGE_IND,
	PLUGING_IND,
	N_PLUGINS
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
		.init = q2c_init,
		.destroy = q2c_destroy,
		.ops_init = q2c_ops_init,
		.ops_destroy = NULL
	},
	{
		.init = i2c_init,
		.destroy = i2c_destroy,
		.ops_init = i2c_ops_init,
		.ops_destroy = NULL
	},
	{
		.init = c2d_init,
		.destroy = c2d_destroy,
		.ops_init = c2d_ops_init,
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
