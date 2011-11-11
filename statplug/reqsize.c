#include <asm/types.h>
#include <glib.h>
#include <stdio.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>
#include <utils.h>

#include <reqsize.h>

static void C(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_REQSIZE(rsd,data);
	
	__u64 blks = t_blks(t);
	
	if(blks) {
		rsd->min = MIN(rsd->min, blks);
		rsd->max = MAX(rsd->max, blks);
		rsd->total_size += blks;
		rsd->reqs++;
		if(!IS_WRITE(t)) {
			rsd->reads++;
		}
	}
}

void reqsize_add(void *data1, const void *data2) 
{
	DECL_ASSIGN_REQSIZE(rsd1,data1);
	DECL_ASSIGN_REQSIZE(rsd2,data2);
	
	rsd1->min = MIN(rsd1->min,rsd2->min);
	rsd1->max = MAX(rsd1->max,rsd2->max);
	rsd1->total_size += rsd2->total_size;
	rsd1->reqs += rsd2->reqs;
	rsd1->reads += rsd2->reads;
}

void reqsize_print_results(const void *data)
{
	DECL_ASSIGN_REQSIZE(rsd,data);

	if(rsd->reqs)
		printf("Reqs. #: %lld Reads: %lld (%.1f%%) Size:(min: %lld avg: %f max: %lld (blks))\n",
		       rsd->reads,
		       100*((double)rsd->reads)/rsd->reqs,
		       rsd->reqs,
		       rsd->min,
		       ((double)rsd->total_size)/rsd->reqs,
		       rsd->max);
}

void reqsize_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *__un2)
{
	struct reqsize_data *req = p->data = g_new(struct reqsize_data,1);
	req->min = ~0;
	req->max = 0;
	req->total_size = 0;
	req->reqs = 0;
	req->reads = 0;
	
	__un1 = NULL; __un2 = NULL; /* to make gcc quite */
}

void reqsize_ops_init(struct plugin_ops *po)
{
	po->add = reqsize_add;
	po->print_results = reqsize_print_results;
	
	/* association of event int and function */
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_COMPLETE,C);
}

void reqsize_destroy(struct plugin *p)
{
	g_free(p->data);
}
