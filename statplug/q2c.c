#include <asm/types.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>
#include <utils.h>
#include <list_plugins.h>

#define DECL_ASSIGN_Q2C(name, data) \
	struct q2c_data *name = (struct q2c_data *)data

struct q2c_data {
	GHashTable *qs;

	/* ongoing active period */
	__u64 start;
	__u64 end;
	__u32 processed;
	__u32 outstanding;

	/* total active time */
	__u64 q2c_time;
	__u32 maxouts;

	/* req size data */
	__u64 q_reqs;
	__u64 q_total_size;
};

struct proc_q_arg {
	__u64 ts;
	__u64 te;
	struct q2c_data *q2c;
};

static bool proc_q(gpointer __unused, gpointer tp, gpointer pqap)
{
	struct blk_io_trace *t = (struct blk_io_trace *)tp;
	struct proc_q_arg *pqa = (struct proc_q_arg *)pqap;

	__u64 this_s = BIT_START(t), this_e = BIT_END(t);
	__u64 this_ts = t->time;

	if (pqa->ts <= this_s && this_e <= pqa->te) {
		if (this_ts < pqa->q2c->start)
			pqa->q2c->start = this_ts;
		pqa->q2c->processed++;
		pqa->q2c->outstanding--;
		return true;
	} else {
		return false;
	}
}

static void restart_ongoing(struct q2c_data *q2c)
{
	/* restart ongoing period */
	q2c->start = ~(0ULL);
	q2c->end = 0;
	q2c->processed = 0;
}

static void C(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_Q2C(q2c, data);
	struct proc_q_arg pqa = { BIT_START(t), BIT_END(t), q2c };

	if (t->time > q2c->end)
		q2c->end = t->time;

	g_hash_table_foreach_remove(q2c->qs, proc_q, &pqa);
	if (q2c->outstanding == 0 && q2c->processed > 0) {
		q2c->q2c_time += q2c->end - q2c->start;
		restart_ongoing(q2c);
	}
}

static void Q(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_Q2C(q2c, data);

	__u64 blks = t_blks(t);

	DECL_DUP(struct blk_io_trace, new_t, t);
	g_hash_table_insert(q2c->qs, new_t, new_t);
	q2c->outstanding++;
	q2c->q_reqs++;
	q2c->q_total_size += blks;
	q2c->maxouts = MAX(q2c->maxouts, q2c->outstanding);
}

void q2c_add(void *data1, const void *data2)
{
	DECL_ASSIGN_Q2C(q2c1, data1);
	DECL_ASSIGN_Q2C(q2c2, data2);

	q2c1->q2c_time += q2c2->q2c_time;
	q2c1->maxouts = MAX(q2c1->maxouts, q2c2->maxouts);
}

void q2c_print_results(const void *data)
{
	DECL_ASSIGN_Q2C(q2c, data);

	/* include all the outstanding I/Os stats if any */
	if (q2c->end > 0)
		q2c->q2c_time += q2c->end - q2c->start;

	if (q2c->q2c_time > 0) {
		double t_time_msec = ((double)q2c->q2c_time) / 1e6;
		double t_req_mb = ((double)q2c->q_total_size) / (1 << 11);

		printf("Q2C Total time: %f (msec)\n", t_time_msec);
		printf("Avg. Q2C per I/O: %f (msec)\n",
		       t_time_msec / (q2c->q_reqs));
		printf("Avg. Q2C per block: %f (msec)\n",
		       t_time_msec / (q2c->q_total_size));
		printf("Avg. Q2C Throughput: %f (MB/sec)\n",
		       (t_req_mb) / (t_time_msec / 1000));
		printf("Q2C Max outstanding: %u (reqs)\n", q2c->maxouts);
	} else
		printf("Not enough data for Q2C stats\n");
}

void q2c_init(struct plugin *p, struct plugin_set *__un1,
	      struct plug_args *__un2)
{
	struct q2c_data *q2c = p->data = g_new(struct q2c_data, 1);
	q2c->qs = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
					g_free);
	restart_ongoing(q2c);

	q2c->q2c_time = q2c->maxouts = 0;
	q2c->q_reqs = q2c->q_total_size = 0;
}

void q2c_ops_init(struct plugin_ops *po)
{
	po->add = q2c_add;
	po->print_results = q2c_print_results;

	/* association of event int and function */
	g_tree_insert(po->event_tree, (gpointer)__BLK_TA_COMPLETE, C);
	g_tree_insert(po->event_tree, (gpointer)__BLK_TA_QUEUE, Q);
}

void q2c_destroy(struct plugin *p)
{
	DECL_ASSIGN_Q2C(q2c, p->data);
	g_hash_table_destroy(q2c->qs);
	g_free(p->data);
}
