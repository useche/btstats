#include <asm/types.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>
#include <utils.h>
#include <list_plugins.h>
#include "include/hash.h"

#define DECL_ASSIGN_Q2C(name,data)				\
	struct q2c_data *name = (struct q2c_data *)data

struct q2c_data 
{
	hash_table *qs;

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

struct proc_q_arg
{
	__u64 ts;
	__u64 te;
	struct q2c_data *q2c;
};

static int proc_q(void *key, void *value, void *pqap)
{
	struct blk_io_trace *t = (struct blk_io_trace *)value;
	struct proc_q_arg *pqa = (struct proc_q_arg *)pqap;

	__u64 this_s = BIT_START(t), this_e = BIT_END(t);
	__u64 this_ts = t->time;

	if(pqa->ts <= this_s && this_e <= pqa->te) {
		if(this_ts < pqa->q2c->start)
			pqa->q2c->start = this_ts;
		pqa->q2c->processed++;
		pqa->q2c->outstanding--;
		return 1;
	} else {
		return 0;
	}
}

static void restart_ongoing(struct q2c_data *q2c)
{
		/* restart ongoing period */
		q2c->start = ~(0ULL);
		q2c->end = 0;
		q2c->processed = 0;
}

static void C(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_Q2C(q2c,data);
	struct proc_q_arg pqa = {
		BIT_START(t),
		BIT_END(t),
		q2c
	};

	if(t->time > q2c->end)
		q2c->end = t->time;

	hash_table_foreach_remove(q2c->qs,proc_q,&pqa);
	if(q2c->outstanding==0 && q2c->processed>0) {
		q2c->q2c_time += q2c->end - q2c->start;
		restart_ongoing(q2c);
	}
}

static void Q(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_Q2C(q2c,data);

	__u64 blks = t_blks(t);

	DECL_DUP(struct blk_io_trace,new_t,t);
	hash_table_insert(q2c->qs, new_t, new_t);
	q2c->outstanding++;
	q2c->q_reqs++;
	q2c->q_total_size+=blks;
	q2c->maxouts = MAX(q2c->maxouts, q2c->outstanding);
}

void q2c_add(void *data1, const void *data2) 
{
	DECL_ASSIGN_Q2C(q2c1,data1);
	DECL_ASSIGN_Q2C(q2c2,data2);

	q2c1->q2c_time += q2c2->q2c_time;
	q2c1->maxouts = MAX(q2c1->maxouts, q2c2->maxouts);
}

void q2c_print_results(const void *data)
{
	DECL_ASSIGN_Q2C(q2c,data);

	/* include all the outstanding I/Os stats if any */
	if(q2c->end > 0)
		q2c->q2c_time += q2c->end - q2c->start;

	if(q2c->q2c_time > 0) {
		double t_time_msec = ((double)q2c->q2c_time)/1e6;
		double t_req_mb = ((double)q2c->q_total_size)/(1<<11);

		printf("Q2C Total time: %f (msec)\n",
			t_time_msec);
		printf("Avg. Q2C per I/O: %f (msec)\n",
			t_time_msec/(q2c->q_reqs));
		printf("Avg. Q2C per block: %f (msec)\n",
			t_time_msec/(q2c->q_total_size));
		printf("Avg. Q2C Throughput: %f (MB/sec)\n",
			(t_req_mb)/(t_time_msec/1000));
		printf("Q2C Max outstanding: %u (reqs)\n",
			q2c->maxouts);
	} else
		printf("Not enough data for Q2C stats\n");
}

void q2c_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *__un2)
{
	struct q2c_data *q2c = p->data = malloc(sizeof(struct q2c_data));
	q2c->qs = hash_table_new(ptr_hash,
			ptr_equal,
			NULL,
			free);
	restart_ongoing(q2c);
	
	q2c->q2c_time = q2c->maxouts = 0;
	q2c->q_reqs = q2c->q_total_size = 0;
}

void q2c_ops_init(struct plugin_ops *po)
{
	po->add = q2c_add;
	po->print_results = q2c_print_results;
	
	/* association of event int and function */
    struct event_entry *e1 = malloc(sizeof(struct event_entry));
    e1->event_key = __BLK_TA_COMPLETE;
    e1->event_handler = (event_func_t)C;
	RB_INSERT(event_tree_head, po->event_tree, e1);

    struct event_entry *e2 = malloc(sizeof(struct event_entry));
    e2->event_key = __BLK_TA_QUEUE;
    e2->event_handler = (event_func_t)Q;
	RB_INSERT(event_tree_head, po->event_tree, e2);
}

void q2c_destroy(struct plugin *p)
{
	DECL_ASSIGN_Q2C(q2c,p->data);
	hash_table_destroy(q2c->qs);
	free(p->data);
}
