#include <stdio.h>
#include <stdlib.h>
#include <asm/types.h>

#include <plugins.h>
#include <blktrace_api.h>
#include <utils.h>

#define DECL_ASSIGN_MERGE(name,data)				\
	struct merge_data *name = (struct merge_data *)data

struct merge_data
{
	__u64 ms;
	__u64 fs;
	__u64 ins;
};

static void M(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_MERGE(m,data);

	if(m->ins)
		m->ms++;
}

static void F(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_MERGE(m,data);

	if(m->ins)
		m->fs++;
}

static void I(const struct blk_io_trace *t, void *data)
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
		       (long long unsigned int)m->ins,
		       (long long unsigned int)(m->fs + m->ms),
		       ((double) m->fs + m->ms + m->ins)/m->ins);
	} else
		printf("#I: 0\n");
}

void merge_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *__un2)
{
	struct merge_data *m = p->data = malloc(sizeof(struct merge_data));
	m->ms = m->fs = m->ins = 0;
}

void merge_ops_init(struct plugin_ops *po)
{
	po->add = merge_add;
	po->print_results = merge_print_results;

    struct event_entry *e1 = malloc(sizeof(struct event_entry));
    e1->event_key = __BLK_TA_BACKMERGE;
    e1->event_handler = (event_func_t)M;
	RB_INSERT(event_tree_head, po->event_tree, e1);

    struct event_entry *e2 = malloc(sizeof(struct event_entry));
    e2->event_key = __BLK_TA_FRONTMERGE;
    e2->event_handler = (event_func_t)F;
	RB_INSERT(event_tree_head, po->event_tree, e2);

    struct event_entry *e3 = malloc(sizeof(struct event_entry));
    e3->event_key = __BLK_TA_INSERT;
    e3->event_handler = (event_func_t)I;
	RB_INSERT(event_tree_head, po->event_tree, e3);
}

void merge_destroy(struct plugin *p)
{
	free(p->data);
}
