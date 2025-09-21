#include <asm/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>
#include <utils.h>

#define DECL_ASSIGN_PLUGING(name,data)				\
	struct pluging_data *name = (struct pluging_data *)data

struct pluging_data
{
	__u64 min;
	__u64 max;
	__u64 total;
	__u64 nplugs;
	
	__u64 plug_time;
	int plugged;
};

static void P(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_PLUGING(plug,data);

	if(!plug->plugged) {
		plug->plug_time = t->time;
		plug->plugged = 1;
	}
}

static void U(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_PLUGING(plug,data);
	
	if(plug->plugged) {
		__u64 time = t->time - plug->plug_time;
		
		plug->nplugs++;
		plug->total += time;
		
		plug->min = MIN(plug->min,time);
		plug->max = MAX(plug->max,time);
		
		plug->plugged = 0;
		plug->plug_time = 0;
	}
}

void pluging_add(void *data1, const void *data2)
{
	DECL_ASSIGN_PLUGING(plug1,data1);
	DECL_ASSIGN_PLUGING(plug2,data2);
	
	plug1->min = MIN(plug1->min,plug2->min);
	plug1->max = MAX(plug1->max,plug2->max);
	plug1->total += plug2->total;
	plug1->nplugs += plug2->nplugs;
}

void pluging_print_results(const void *data)
{
	DECL_ASSIGN_PLUGING(plug,data);
	
	if(plug->nplugs)
		printf("Plug Time Min: %f Avg: %f Max: %f (sec)\n",
		       NANO_ULL_TO_DOUBLE(plug->min),
		       NANO_ULL_TO_DOUBLE(plug->total)/plug->nplugs,
		       NANO_ULL_TO_DOUBLE(plug->max));
	else
		printf("No plugging in this range\n");
}

void pluging_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *__un2)
{
	struct pluging_data *plug = p->data = malloc(sizeof(struct pluging_data));
	
	plug->min = ~0;
	plug->max = 0;
	plug->total = 0;
	plug->nplugs = 0;
	plug->plug_time = 0;
}

void pluging_ops_init(struct plugin_ops *po)
{
	po->add = pluging_add;
	po->print_results = pluging_print_results;
	
    struct event_entry *e1 = malloc(sizeof(struct event_entry));
    e1->event_key = __BLK_TA_PLUG;
    e1->event_handler = (event_func_t)P;
	RB_INSERT(event_tree_head, po->event_tree, e1);

    struct event_entry *e2 = malloc(sizeof(struct event_entry));
    e2->event_key = __BLK_TA_UNPLUG_IO;
    e2->event_handler = (event_func_t)U;
	RB_INSERT(event_tree_head, po->event_tree, e2);

    struct event_entry *e3 = malloc(sizeof(struct event_entry));
    e3->event_key = __BLK_TA_UNPLUG_TIMER;
    e3->event_handler = (event_func_t)U;
	RB_INSERT(event_tree_head, po->event_tree, e3);
}

void pluging_destroy(struct plugin *p)
{
	free(p->data);
}
