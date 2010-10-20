#include <asm/types.h>
#include <glib.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>
#include <utils.h>
#include <list_plugins.h>
#include <reqsize.h>

#define DECL_ASSIGN_I2C(name,data)				\
	struct i2c_data *name = (struct i2c_data *)data

#define OIO_ALLOC	(8)

struct i2c_data 
{
	GTree *is;
	FILE *oio_f;

	__u32 outstanding;
	__u32 maxouts;

	/* oio hist */
	__u64 *oio;
	__u32 oio_size;
	__u64 oio_prev_time;
};

static void write_outs(struct i2c_data *i2c, struct blk_io_trace *t)
{
	if(i2c->oio_f)
		fprintf(i2c->oio_f, "%f %u\n",
				NANO_ULL_TO_DOUBLE(t->time),
				i2c->outstanding);
}

static void oio_change(struct i2c_data *i2c, struct blk_io_trace *t, int inc)
{
	/* allocate oio space if the one I had is over */
	if(i2c->outstanding == i2c->oio_size) {
		i2c->oio = realloc(i2c->oio, (i2c->oio_size + OIO_ALLOC)*sizeof(__u64));
		memset(i2c->oio + i2c->oio_size, 0, OIO_ALLOC*sizeof(__u64));
		i2c->oio_size += OIO_ALLOC;
	}

	/* increase the time */
	if(i2c->oio_prev_time != UINT64_MAX) {
		i2c->oio[i2c->outstanding] += t->time - i2c->oio_prev_time;
	}
	i2c->oio_prev_time = t->time;

	if(inc) {
		i2c->outstanding++;
	} else {
		i2c->outstanding--;
	}
	
	write_outs(i2c, t);
}

static void C(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_I2C(i2c,data);

	if(g_tree_lookup(i2c->is, &t->sector)!=NULL) {
		g_tree_remove(i2c->is, &t->sector);
		
		oio_change(i2c, t, FALSE);
	}
}

static void I(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_I2C(i2c,data);

	if(g_tree_lookup(i2c->is, &t->sector)==NULL) {
		DECL_DUP(struct blk_io_trace,new_t,t);
		g_tree_insert(i2c->is,&new_t->sector,new_t);

		oio_change(i2c, t, TRUE);
		i2c->maxouts = MAX(i2c->maxouts, i2c->outstanding);
	}
}

void i2c_add(void *data1, const void *data2) 
{
	DECL_ASSIGN_I2C(i2c1,data1);
	DECL_ASSIGN_I2C(i2c2,data2);
	__u32 i;

	i2c1->maxouts = MAX(i2c1->maxouts, i2c2->maxouts);
	i2c1->oio_prev_time = MAX(i2c1->oio_prev_time, i2c2->oio_prev_time);

	if(i2c1->oio_size < i2c2->oio_size) {
		__u32 diff = i2c2->oio_size - i2c1->oio_size;
		i2c1->oio = realloc(i2c1->oio, i2c2->oio_size*sizeof(__u64));
		memset(i2c1->oio + diff, 0, diff*sizeof(__u64));
		i2c1->oio_size = i2c2->oio_size;
	}

	for (i = 0; i < i2c2->maxouts; i++) {
		i2c1->oio[i] += i2c2->oio[i];
	}
}

void i2c_print_results(const void *data)
{
	DECL_ASSIGN_I2C(i2c,data);
	double p[i2c->maxouts];
	double avg = 0;
	__u32 i;
	__u64 tot_time = 0;

	for (i = 0; i <= i2c->maxouts; i++) {
		tot_time += i2c->oio[i];
	}

	for (i = 0; i < i2c->maxouts; i++) {
		p[i] = ((double)i2c->oio[i])/((double)tot_time);
		avg += p[i]*i;
	}

	printf("I2C Max. OIO: %u, Avg: %.2lf\n",i2c->maxouts,avg);
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

	i2c->oio = NULL;
	i2c->oio_size = 0;
	i2c->oio_prev_time = UINT64_MAX;

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

	free(i2c->oio);

	g_free(p->data);
}
