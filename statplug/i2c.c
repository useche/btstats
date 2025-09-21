#include <asm/types.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include <gsl/gsl_histogram.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>
#include <utils.h>
#include <list_plugins.h>
#include <reqsize.h>
#include "include/sector_tree.h"

#define DECL_ASSIGN_I2C(name,data)				\
	struct i2c_data *name = (struct i2c_data *)data

#define OIO_ALLOC	(8)

#define N_BINS		(21)
#define BINS_SEP	(8)

struct oio_data
{
#define READ	0
#define WRITE	1
	gsl_histogram *op[2];
	__u64 time;
};

struct i2c_data 
{
	struct sector_tree_head *is;
	FILE *oio_f;

	__u32 outstanding;
	__u32 maxouts;

	/* oio hist */
	struct oio_data *oio;
	__u32 oio_size;
	__u64 oio_prev_time;
	FILE *oio_hist_f;
};

static void write_outs(struct i2c_data *i2c, const struct blk_io_trace *t)
{
	if(i2c->oio_f)
		fprintf(i2c->oio_f, "%f %u\n",
				NANO_ULL_TO_DOUBLE(t->time),
				i2c->outstanding);
}

static gsl_histogram *new_hist()
{
	gsl_histogram *h;

	/* allocate bins and set ranges uniformally */
	h = gsl_histogram_alloc(N_BINS);
	gsl_histogram_set_ranges_uniform(h,0,N_BINS*BINS_SEP);

	/* last bin should hold everything greater
	 * than (N_BINS-1)*BINS_SEP */
	h->range[h->n] = DBL_MAX;

	return h;
}

static void init_oio_data(struct oio_data *oio, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		oio[i].time = 0;
		oio[i].op[READ] = new_hist();
		oio[i].op[WRITE] = new_hist();
	}
}

static void add_to_matrix(struct sector_entry *se, struct i2c_data *i2c)
{
    struct blk_io_trace *t = se->trace;
	gsl_histogram_increment(i2c->oio[i2c->outstanding].op[IS_WRITE(t)], (double)(t->bytes/BLK_SIZE));
}

static void oio_change(struct i2c_data *i2c, const struct blk_io_trace *t, int inc)
{
	/* allocate oio space if the one I had is over */
	if(i2c->outstanding + 1 >= i2c->oio_size) {
		i2c->oio = realloc(i2c->oio, (i2c->oio_size + OIO_ALLOC)*sizeof(struct oio_data));
		init_oio_data(i2c->oio + i2c->oio_size, OIO_ALLOC);
		i2c->oio_size += OIO_ALLOC;
	}

	/* increase the time */
	if(i2c->oio_prev_time != UINT64_MAX) {
		i2c->oio[i2c->outstanding].time += t->time - i2c->oio_prev_time;
	}
	i2c->oio_prev_time = t->time;

	if(inc) {
		i2c->outstanding++;
	} else {
		i2c->outstanding--;
	}
	i2c->maxouts = MAX(i2c->maxouts, i2c->outstanding);

    struct sector_entry *se;
    RB_FOREACH(se, sector_tree_head, i2c->is) {
        add_to_matrix(se, i2c);
    }
	
	write_outs(i2c, t);
}

static void C(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_I2C(i2c,data);
    struct sector_entry find, *res;
    find.sector = t->sector;

	res = RB_FIND(sector_tree_head, i2c->is, &find);
	if(res !=NULL) {
		RB_REMOVE(sector_tree_head, i2c->is, res);
		free(res->trace);
		free(res);
		
		oio_change(i2c, t, 0);
	}
}

static void I(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_I2C(i2c,data);
    struct sector_entry find, *res;
    find.sector = t->sector;

	res = RB_FIND(sector_tree_head, i2c->is, &find);
	if(res == NULL) {
		DECL_DUP(struct blk_io_trace,new_t,t);
        struct sector_entry *new_se = malloc(sizeof(struct sector_entry));
        new_se->sector = new_t->sector;
        new_se->trace = new_t;
		RB_INSERT(sector_tree_head, i2c->is, new_se);

		oio_change(i2c, t, 1);
	}
}

static void add_histogram(gsl_histogram *h1, gsl_histogram *h2)
{
	unsigned int i;

	for (i = 0; i < h1->n && i < h2->n; i++) {
		h1->bin[i] += h2->bin[i];
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
		i2c1->oio = realloc(i2c1->oio, i2c2->oio_size*sizeof(struct oio_data));
		init_oio_data(i2c1->oio + diff, diff);
		i2c1->oio_size = i2c2->oio_size;
	}

	for (i = 0; i <= i2c2->maxouts; i++) {
		i2c1->oio[i].time += i2c2->oio[i].time;
		add_histogram(i2c1->oio[i].op[READ],i2c2->oio[i].op[READ]);
		add_histogram(i2c1->oio[i].op[WRITE],i2c2->oio[i].op[WRITE]);
	}
}

void i2c_print_results(const void *data)
{
	DECL_ASSIGN_I2C(i2c,data);
	double p;
	double avg = 0;
	__u32 i;
	__u64 tot_time = 0;

	for (i = 0; i <= i2c->maxouts; i++) {
		tot_time += i2c->oio[i].time;
	}

	for (i = 0; i <= i2c->maxouts; i++) {
		p = ((double)i2c->oio[i].time)/((double)tot_time);

		if(i2c->oio_hist_f)
			fprintf(i2c->oio_hist_f, "%u\t%.2lf\n",i,100*p);

		avg += p*i;
	}

	/* print all histograms */
	if(i2c->oio_hist_f) {
		for (i = 1; i <= i2c->maxouts; i++) {
			fprintf(i2c->oio_hist_f, "\nread: %d\n",i);
			gsl_histogram_fprintf(i2c->oio_hist_f,
					i2c->oio[i].op[READ], "%g", "%g");
			fprintf(i2c->oio_hist_f, "\nwrite: %d\n",i);
			gsl_histogram_fprintf(i2c->oio_hist_f,
					i2c->oio[i].op[WRITE], "%g", "%g");
		}
	}

	printf("I2C Max. OIO: %u, Avg: %.2lf\n",i2c->maxouts,avg);
}

void i2c_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *pa)
{
	char filename[FILENAME_MAX];
	struct i2c_data *i2c = p->data = malloc(sizeof(struct i2c_data));

	i2c->is = malloc(sizeof(struct sector_tree_head));
    RB_INIT(sector_tree_head, i2c->is);
	i2c->outstanding = 0;
	i2c->maxouts = 0;

	i2c->oio_f = NULL;
	if(pa->i2c_oio_f) {
		get_filename(filename, "i2c_oio", pa->i2c_oio_f, pa->end_range);
		i2c->oio_f = fopen(filename,"w");
		if(!i2c->oio_f) perror_exit("Opening I2C detail file");
	}

	i2c->oio_hist_f = NULL;
	if(pa->i2c_oio_hist_f) {
		get_filename(filename, "i2c_oio_hist", pa->i2c_oio_hist_f, pa->end_range);
		i2c->oio_hist_f = fopen(filename,"w");
		if(!i2c->oio_hist_f) perror_exit("Opening I2C detail file");
	}

	i2c->oio = NULL;
	i2c->oio_size = 0;
	i2c->oio_prev_time = UINT64_MAX;
}

void i2c_ops_init(struct plugin_ops *po)
{
	po->add = i2c_add;
	po->print_results = i2c_print_results;
	
	/* association of event int and function */
    struct event_entry *e1 = malloc(sizeof(struct event_entry));
    e1->event_key = __BLK_TA_COMPLETE;
    e1->event_handler = (event_func_t)C;
	RB_INSERT(event_tree_head, po->event_tree, e1);

    struct event_entry *e2 = malloc(sizeof(struct event_entry));
    e2->event_key = __BLK_TA_INSERT;
    e2->event_handler = (event_func_t)I;
	RB_INSERT(event_tree_head, po->event_tree, e2);
}

void i2c_destroy(struct plugin *p)
{
	unsigned int i;

	DECL_ASSIGN_I2C(i2c,p->data);

	if(i2c->oio_f)
		fclose(i2c->oio_f);

	if(i2c->oio_hist_f)
		fclose(i2c->oio_hist_f);

	for (i = 0; i <= i2c->maxouts; i++) {
		gsl_histogram_free(i2c->oio[i].op[READ]);
		gsl_histogram_free(i2c->oio[i].op[WRITE]);
	}
	free(i2c->oio);

    struct sector_entry *se, *next;
    RB_FOREACH_SAFE(se, sector_tree_head, i2c->is, next) {
        RB_REMOVE(sector_tree_head, i2c->is, se);
        free(se->trace);
        free(se);
    }
    free(i2c->is);

	free(p->data);
}
