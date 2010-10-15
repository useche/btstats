#include <asm/types.h>
#include <glib.h>
#include <stdio.h>
#include <assert.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>
#include <utils.h>
#include <list_plugins.h>
#include <reqsize.h>

#define DECL_ASSIGN_I2C(name,data)				\
	struct i2c_data *name = (struct i2c_data *)data

struct i2c_data 
{
	GTree *is;
	FILE *oio_f;

	__u32 outstanding;
	__u32 maxouts;
};

static void write_outs(struct i2c_data *i2c, struct blk_io_trace *t)
{
	if(i2c->oio_f)
		fprintf(i2c->oio_f, "%f %u\n",
				NANO_ULL_TO_DOUBLE(t->time),
				i2c->outstanding);
}

static void C(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_I2C(i2c,data);
	struct blk_io_trace *i;

	i = g_tree_lookup(i2c->is, &t->sector);
	if(i!=NULL) {
		i2c->outstanding--;
		g_tree_remove(i2c->is, &t->sector);
		
		write_outs(i2c,t);
	}
}

static void I(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_I2C(i2c,data);

	if(g_tree_lookup(i2c->is, &t->sector)==NULL) {
		DECL_DUP(struct blk_io_trace,new_t,t);
		g_tree_insert(i2c->is,&new_t->sector,new_t);
		i2c->outstanding++;
		i2c->maxouts = MAX(i2c->maxouts, i2c->outstanding);

		write_outs(i2c,t);
	}
}

void i2c_add(void *data1, const void *data2) 
{
	DECL_ASSIGN_I2C(i2c1,data1);
	DECL_ASSIGN_I2C(i2c2,data2);

	i2c1->maxouts = MAX(i2c1->maxouts, i2c2->maxouts);
}

void i2c_print_results(const void *data)
{
	DECL_ASSIGN_I2C(i2c,data);

	printf("I2C max outstanding: %u (reqs)\n",i2c->maxouts);
}

void i2c_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *pa)
{
	char filename[FILENAME_MAX];
	struct i2c_data *i2c = p->data = g_new(struct i2c_data,1);

	i2c->is = g_tree_new(comp_int64);
	i2c->outstanding = 0;
	i2c->maxouts = 0;

	i2c->oio_f = NULL;
	if(pa->i2c_oio_f) {
		get_filename(filename, "i2c_oio", pa->i2c_oio_f, pa->end_range);
		i2c->oio_f = fopen(filename,"w");
		if(!i2c->oio_f) perror_exit("Opening I2C detail file");
	}

	__un1 = NULL; /* to make gcc quite */
}

void i2c_ops_init(struct plugin_ops *po)
{
	po->add = i2c_add;
	po->print_results = i2c_print_results;
	
	/* association of event int and function */
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_COMPLETE,C);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_INSERT,I);
}

void i2c_destroy(struct plugin *p)
{
	DECL_ASSIGN_I2C(i2c,p->data);

	if(i2c->oio_f)
		fclose(i2c->oio_f);

	g_free(p->data);
}
