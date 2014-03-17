#include <stdio.h>
#include <asm/types.h>

#include <plugins.h>
#include <blktrace_api.h>

#define DECL_ASSIGN_MERGE(name,data)				\
	struct merge_data *name = (struct merge_data *)data

struct merge_data
{
	__u64 ms;
	__u64 fs;
	__u64 ins;
};

static void M(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_MERGE(m,data);

	if(m->ins)
		m->ms++;
}

static void F(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_MERGE(m,data);
	
	if(m->ins)
		m->fs++;	
}

static void I(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_MERGE(m,data);

	m->ins++;
}

void merge_add(void *data1, const void *data2)
{
	DECL_ASSIGN_MERGE(m1,data1);
	DECL_ASSIGN_MERGE(m2,data2);
	
	m1->ms += m2->ms;
	m1->fs += m2->fs;
	m1->ins += m2->ins;
}

void merge_print_results(const void *data)
{
	DECL_ASSIGN_MERGE(m,data);
	
	if(m->ins) {
		printf("#I: %llu #F+#M: %llu ratio: %f\n",
		       m->ins,
		       m->fs + m->ms,
		       ((double) m->fs + m->ms + m->ins)/m->ins);
	} else
		printf("#I: 0\n");
}

void merge_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *__un2)
{
	struct merge_data *m = p->data = g_new(struct merge_data,1);
	m->ms = m->fs = m->ins = 0;
}

void merge_ops_init(struct plugin_ops *po)
{
	po->add = merge_add;
	po->print_results = merge_print_results;
	
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_BACKMERGE,M);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_FRONTMERGE,F);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_INSERT,I);
}

void merge_destroy(struct plugin *p)
{
	g_free(p->data);
}
