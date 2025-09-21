#include <asm/types.h>
#include <stdio.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>

#define DECL_ASSIGN_PLUGING(name, data) \
	struct pluging_data *name = (struct pluging_data *)data

struct pluging_data {
	__u64 min;
	__u64 max;
	__u64 total;
	__u64 nplugs;

	__u64 plug_time;
	gboolean plugged;
};

static void P(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_PLUGING(plug, data);

	if (!plug->plugged) {
		plug->plug_time = t->time;
		plug->plugged = TRUE;
	}
}

static void U(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_PLUGING(plug, data);

	if (plug->plugged) {
		__u64 time = t->time - plug->plug_time;

		plug->nplugs++;
		plug->total += time;

		plug->min = MIN(plug->min, time);
		plug->max = MAX(plug->max, time);

		plug->plugged = FALSE;
		plug->plug_time = 0;
	}
}

void pluging_add(void *data1, const void *data2)
{
	DECL_ASSIGN_PLUGING(plug1, data1);
	DECL_ASSIGN_PLUGING(plug2, data2);

	plug1->min = MIN(plug1->min, plug2->min);
	plug1->max = MAX(plug1->max, plug2->max);
	plug1->total += plug2->total;
	plug1->nplugs += plug2->nplugs;
}

void pluging_print_results(const void *data)
{
	DECL_ASSIGN_PLUGING(plug, data);

	if (plug->nplugs)
		printf("Plug Time Min: %f Avg: %f Max: %f (sec)\n",
		       NANO_ULL_TO_DOUBLE(plug->min),
		       NANO_ULL_TO_DOUBLE(plug->total) / plug->nplugs,
		       NANO_ULL_TO_DOUBLE(plug->max));
	else
		printf("No plugging in this range\n");
}

void pluging_init(struct plugin *p, struct plugin_set *__un1,
		  struct plug_args *__un2)
{
	p->data = g_new(struct pluging_data, 1);
	struct pluging_data *plug = static_cast<struct pluging_data*>(p->data);

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

	g_tree_insert(po->event_tree, (gpointer)__BLK_TA_PLUG, reinterpret_cast<gpointer>(P));
	g_tree_insert(po->event_tree, (gpointer)__BLK_TA_UNPLUG_IO, reinterpret_cast<gpointer>(U));
	g_tree_insert(po->event_tree, (gpointer)__BLK_TA_UNPLUG_TIMER, reinterpret_cast<gpointer>(U));
}

void pluging_destroy(struct plugin *p)
{
	g_free(p->data);
}
