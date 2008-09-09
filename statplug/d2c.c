#include <assert.h>

#include <plugins.h>
#include <blktrace_api.h>
#include <utils.h>

#include <reqsize.h>
#include <list_plugins.h>

#define DECL_ASSIGN_D2C(name,data)			\
	struct d2c_data *name = (struct d2c_data *)data

struct d2c_data
{
	__u32 outstanding;
	__u32 processed;
	__u32 processed_blks;
	
	GTree *prospect_ds;
	GArray *dtimes;
	GArray *ctimes;
	
	struct reqsize_data *req_dat;
};

static void D(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_D2C(d2c,data);
	
	__u64 blks = BYTES_TO_BLKS(t->bytes);
	
	if(blks && g_tree_lookup(d2c->prospect_ds,&t->sector)==NULL) {
		DECL_DUP(struct blk_io_trace,new_t,t);
		g_tree_insert(d2c->prospect_ds,&new_t->sector,new_t);
		d2c->outstanding++;
	}
}

static void C(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_D2C(d2c,data);
	
	__u64 blks = BYTES_TO_BLKS(t->bytes);
	struct blk_io_trace *dtrace = g_tree_lookup(d2c->prospect_ds,&t->sector);
	
	if(blks && dtrace) {
		if(dtrace->bytes == t->bytes) {
			d2c->processed++;
			d2c->processed_blks += blks;
			
			g_array_append_val(d2c->dtimes,dtrace->time);
			g_array_append_val(d2c->ctimes,t->time);
		}
		
		g_tree_remove(d2c->prospect_ds,&dtrace->sector);
		g_free(dtrace);
		
		/* TODO: account the values */
	}
}

static void R(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_D2C(d2c,data);
	
	struct blk_io_trace *dtrace = g_tree_lookup(d2c->prospect_ds,&t->sector);

	if(dtrace) {
		assert(dtrace->bytes == t->bytes);
		
		g_tree_remove(d2c->prospect_ds,&dtrace->sector);
		g_free(dtrace);		
		/* TODO: account the values */
	}
}

void d2c_add(void *data1, const void *data2)
{
	
}

void d2c_print_results(const void *data)
{
	
}


void d2c_init(struct plugin *p, struct plugin_set *ps)
{
	struct d2c_data *d2c = p->data = g_new(struct d2c_data,1);
	d2c->outstanding = d2c->processed = d2c->processed_blks = 0;
	d2c->prospect_ds = g_tree_new(comp_int64);
	d2c->dtimes = g_array_sized_new(FALSE,FALSE,sizeof(__u64),TENT_OUTS_RQS);
	d2c->ctimes = g_array_sized_new(FALSE,FALSE,sizeof(__u64),TENT_OUTS_RQS);
	d2c->req_dat = ps->plugs[REQ_SIZE_IND].data;
}

void d2c_destroy(struct plugin *p)
{
	DECL_ASSIGN_D2C(d2c,p->data);
	
	g_tree_destroy(d2c->prospect_ds);
	g_array_free(d2c->dtimes,FALSE);
	g_array_free(d2c->ctimes,FALSE);
	g_free(p->data);
}

void d2c_ops_init(struct plugin_ops *po)
{
	po->add = d2c_add;
	po->print_results = d2c_print_results;
	
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_COMPLETE,C);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_ISSUE,D);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_REQUEUE,R);
}
