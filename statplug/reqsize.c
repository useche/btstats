#include <asm/types.h>
#include <glib.h>
#include <stdio.h>
#include <blktrace_api.h>
#include "plugins.h"

#define BLK_SHIFT 9
#define BYTES_TO_BLKS(byts) ((byts)>>BLK_SHIFT)

struct reqsize_data 
{
	__u64 min;
	__u64 max;
	__u64 total_size;
	__u64 reqs;
};

void C(struct blk_io_trace *t, void *data) 
{
	int blks = BYTES_TO_BLKS(data->bytes);
	
	if(blks) {
		data->min = MIN(data->min, blks);
		data->max = MAX(data->max, blks);
		data->total_size += blks;
		data->reqs++;
	}
}

void add(void *data1, void *data2) 
{
	data1->min = MIN(data1->min,data2->min);
	data1->max = MAX(data1->max,data2->max);
	data1->total_size += data2->total_size;
	data1->reqs += data2->reqs;
}

void print_results(void *data)
{
	printf("Reqs. #: %d min: %d avg: %f max: %d\n",
	       data->reqs,
	       data->min,
	       ((double)data->total_size)/data->reqs,
	       data->max);
}

void reqsize_init(struct plugin *p) 
{
	p->data = g_new(struct reqsize_data,1);
}

void reqsize_ops_init(struct plugin_ops *po)
{
	po->add = add;
	po->print_results = print_results;
	
	po->event_ht = g_hash_table_new(g_int_hash, g_hash_equal);
	/* association of event int and function */
	g_hash_table_insert(po->event_ht,BLK_TA_COMPLETE,C);
}

void reqsize_ops_destroy(struct plugin_ops *po) 
{
	g_hash_table_destroy(po->event_ht);
}

void reqsize_destroy(struct plugin *p)
{
	free(p->data);
}
