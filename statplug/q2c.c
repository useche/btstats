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
#include "include/bsd/tree.h"

#define DECL_ASSIGN_Q2C(name,data)				\
	struct q2c_data *name = (struct q2c_data *)data

struct q_entry {
    struct blk_io_trace *trace;
    RB_ENTRY(q_entry) entry;
};

int q_entry_cmp(const struct q_entry *a, const struct q_entry *b) {
    if (a->trace->sector < b->trace->sector) return -1;
    if (a->trace->sector > b->trace->sector) return 1;
    return 0;
}

RB_HEAD(q_tree_head, q_entry);
RB_PROTOTYPE(q_tree_head, q_entry, entry, q_entry_cmp);
RB_GENERATE(q_tree_head, q_entry, entry, q_entry_cmp);

struct q2c_data 
{
	struct q_tree_head *qs;

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

static void C(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_Q2C(q2c,data);
    struct q_entry find, *res;
    find.trace = (struct blk_io_trace *)t;

	res = RB_FIND(q_tree_head, q2c->qs, &find);
	if(res) {
        q2c->processed++;
        q2c->outstanding--;
		RB_REMOVE(q_tree_head, q2c->qs, res);
		free(res->trace);
		free(res);
	}

	if(t->time > q2c->end)
		q2c->end = t->time;

	if(q2c->outstanding==0 && q2c->processed>0) {
		q2c->q2c_time += q2c->end - q2c->start;
		q2c->start = ~(0ULL);
		q2c->end = 0;
		q2c->processed = 0;
	}
}

static void Q(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_Q2C(q2c,data);

	__u64 blks = t_blks(t);

	DECL_DUP(struct blk_io_trace,new_t,t);
    struct q_entry *new_qe = malloc(sizeof(struct q_entry));
    new_qe->trace = new_t;
	RB_INSERT(q_tree_head, q2c->qs, new_qe);
	q2c->outstanding++;
	q2c->q_reqs++;
	q2c->q_total_size+=blks;
	q2c->maxouts = MAX(q2c->maxouts, q2c->outstanding);
    if (q2c->start == ~(0ULL))
        q2c->start = t->time;
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
	q2c->qs = malloc(sizeof(struct q_tree_head));
    RB_INIT(q_tree_head, q2c->qs);
	q2c->start = ~(0ULL);
	q2c->end = 0;
	q2c->processed = 0;
    q2c->outstanding = 0;
	
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
    struct q_entry *qe, *next;
    RB_FOREACH_SAFE(qe, q_tree_head, q2c->qs, next) {
        RB_REMOVE(q_tree_head, q2c->qs, qe);
        free(qe->trace);
        free(qe);
    }
    free(q2c->qs);
	free(p->data);
}
