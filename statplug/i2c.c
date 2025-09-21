#include <asm/types.h>
#include <glib.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include <float.h>

#include <gsl/gsl_histogram.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>
#include <utils.h>
#include <list_plugins.h>
#include <reqsize.h>

#define DECL_ASSIGN_I2C(name, data) \
	struct i2c_data *name = (struct i2c_data *)data

#define OIO_ALLOC (8)

#define N_BINS (21)
#define BINS_SEP (8)

struct oio_data {
#define READ 0
#define WRITE 1
	gsl_histogram *op[2];
	__u64 time;
};

struct i2c_data {
	GTree *is;
	FILE *oio_f;

	__u32 outstanding;
	__u32 maxouts;

	/* oio hist */
	struct oio_data *oio;
	__u32 oio_size;
	__u64 oio_prev_time;
	FILE *oio_hist_f;
};

static void write_outs(struct i2c_data *i2c, struct blk_io_trace *t)
{
	if (i2c->oio_f)
		fprintf(i2c->oio_f, "%f %u\n", NANO_ULL_TO_DOUBLE(t->time),
			i2c->outstanding);
}

static gsl_histogram *new_hist()
{
	gsl_histogram *h;

	/* allocate bins and set ranges uniformally */
	h = gsl_histogram_alloc(N_BINS);
	gsl_histogram_set_ranges_uniform(h, 0, N_BINS * BINS_SEP);

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

static gboolean add_to_matrix(__u64 *__unused, struct blk_io_trace *t,
			      struct i2c_data *i2c)
{
	gsl_histogram_increment(i2c->oio[i2c->outstanding].op[IS_WRITE(t)],
				(double)(t->bytes / BLK_SIZE));

	return FALSE;
}

static void oio_change(struct i2c_data *i2c, struct blk_io_trace *t, int inc)
{
	/* allocate oio space if the one I had is over */
	if (i2c->outstanding + 1 >= i2c->oio_size) {
		i2c->oio = static_cast<struct oio_data*>(realloc(i2c->oio, (i2c->oio_size + OIO_ALLOC) *
						     sizeof(struct oio_data)));
		init_oio_data(i2c->oio + i2c->oio_size, OIO_ALLOC);
		i2c->oio_size += OIO_ALLOC;
	}

	/* increase the time */
	if (i2c->oio_prev_time != UINT64_MAX) {
		i2c->oio[i2c->outstanding].time += t->time - i2c->oio_prev_time;
	}
	i2c->oio_prev_time = t->time;

	if (inc) {
		i2c->outstanding++;
	} else {
		i2c->outstanding--;
	}
	i2c->maxouts = MAX(i2c->maxouts, i2c->outstanding);

	g_tree_foreach(i2c->is, (GTraverseFunc)add_to_matrix, i2c);

	write_outs(i2c, t);
}

static void C(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_I2C(i2c, data);

	if (g_tree_lookup(i2c->is, &t->sector) != NULL) {
		g_tree_remove(i2c->is, &t->sector);

		oio_change(i2c, t, FALSE);
	}
}

static void I(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_I2C(i2c, data);

	if (g_tree_lookup(i2c->is, &t->sector) == NULL) {
		DECL_DUP(struct blk_io_trace, new_t, t);
		g_tree_insert(i2c->is, &new_t->sector, new_t);

		oio_change(i2c, t, TRUE);
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
	DECL_ASSIGN_I2C(i2c1, data1);
	DECL_ASSIGN_I2C(i2c2, data2);
	__u32 i;

	i2c1->maxouts = MAX(i2c1->maxouts, i2c2->maxouts);
	i2c1->oio_prev_time = MAX(i2c1->oio_prev_time, i2c2->oio_prev_time);

	if (i2c1->oio_size < i2c2->oio_size) {
		__u32 diff = i2c2->oio_size - i2c1->oio_size;
		i2c1->oio = static_cast<struct oio_data*>(realloc(i2c1->oio,
				    i2c2->oio_size * sizeof(struct oio_data)));
		init_oio_data(i2c1->oio + diff, diff);
		i2c1->oio_size = i2c2->oio_size;
	}

	for (i = 0; i <= i2c2->maxouts; i++) {
		i2c1->oio[i].time += i2c2->oio[i].time;
		add_histogram(i2c1->oio[i].op[READ], i2c2->oio[i].op[READ]);
		add_histogram(i2c1->oio[i].op[WRITE], i2c2->oio[i].op[WRITE]);
	}
}

void i2c_print_results(const void *data)
{
	DECL_ASSIGN_I2C(i2c, data);
	double p;
	double avg = 0;
	__u32 i;
	__u64 tot_time = 0;

	for (i = 0; i <= i2c->maxouts; i++) {
		tot_time += i2c->oio[i].time;
	}

	for (i = 0; i <= i2c->maxouts; i++) {
		p = ((double)i2c->oio[i].time) / ((double)tot_time);

		if (i2c->oio_hist_f)
			fprintf(i2c->oio_hist_f, "%u\t%.2lf\n", i, 100 * p);

		avg += p * i;
	}

	/* print all histograms */
	if (i2c->oio_hist_f) {
		for (i = 1; i <= i2c->maxouts; i++) {
			fprintf(i2c->oio_hist_f, "\nread: %d\n", i);
			gsl_histogram_fprintf(i2c->oio_hist_f,
					      i2c->oio[i].op[READ], "%g", "%g");
			fprintf(i2c->oio_hist_f, "\nwrite: %d\n", i);
			gsl_histogram_fprintf(i2c->oio_hist_f,
					      i2c->oio[i].op[WRITE], "%g",
					      "%g");
		}
	}

	printf("I2C Max. OIO: %u, Avg: %.2lf\n", i2c->maxouts, avg);
}

void i2c_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *pa)
{
	char filename[FILENAME_MAX];
	p->data = g_new(struct i2c_data, 1);
	struct i2c_data *i2c = static_cast<struct i2c_data*>(p->data);

	i2c->is = g_tree_new(comp_int64);
	i2c->outstanding = 0;
	i2c->maxouts = 0;

	i2c->oio_f = NULL;
	if (pa->i2c_oio_f) {
		get_filename(filename, "i2c_oio", pa->i2c_oio_f, pa->end_range);
		i2c->oio_f = fopen(filename, "w");
		if (!i2c->oio_f)
			perror_exit("Opening I2C detail file");
	}

	i2c->oio_hist_f = NULL;
	if (pa->i2c_oio_hist_f) {
		get_filename(filename, "i2c_oio_hist", pa->i2c_oio_hist_f,
			     pa->end_range);
		i2c->oio_hist_f = fopen(filename, "w");
		if (!i2c->oio_hist_f)
			perror_exit("Opening I2C detail file");
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
	g_tree_insert(po->event_tree, (gpointer)__BLK_TA_COMPLETE, reinterpret_cast<gpointer>(C));
	g_tree_insert(po->event_tree, (gpointer)__BLK_TA_INSERT, reinterpret_cast<gpointer>(I));
}

void i2c_destroy(struct plugin *p)
{
	unsigned int i;

	DECL_ASSIGN_I2C(i2c, p->data);

	if (i2c->oio_f)
		fclose(i2c->oio_f);

	if (i2c->oio_hist_f)
		fclose(i2c->oio_hist_f);

	for (i = 0; i <= i2c->maxouts; i++) {
		gsl_histogram_free(i2c->oio[i].op[READ]);
		gsl_histogram_free(i2c->oio[i].op[WRITE]);
	}
	free(i2c->oio);

	g_free(p->data);
}
