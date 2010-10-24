#include <assert.h>
#include <stdio.h>

#include <plugins.h>
#include <blktrace_api.h>
#include <blktrace.h>
#include <utils.h>

#include <reqsize.h>
#include <list_plugins.h>

#define DECL_ASSIGN_D2C(name,data)			\
	struct d2c_data *name = (struct d2c_data *)data

struct d2c_data
{
	__u32 outstanding;
	__u32 processed;
	
	GTree *prospect_ds;
	GArray *dtimes;
	GArray *ctimes;

	__u64 d2ctime;

	__u32 maxouts;
	
	struct reqsize_data *req_dat;

	FILE *detail_f;
};

static void D(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_D2C(d2c,data);
	
	__u64 blks = t_blks(t);
	
	if(blks && g_tree_lookup(d2c->prospect_ds,&t->sector)==NULL) {
		DECL_DUP(struct blk_io_trace,new_t,t);
		g_tree_insert(d2c->prospect_ds,&new_t->sector,new_t);
		d2c->outstanding++;
	}
}

static void __account_reqs(struct d2c_data *d2c) 
{
	d2c->outstanding--;
	if(d2c->outstanding == 0) {
		if(d2c->processed > 0) {
			unsigned i, j;
			__u32 outs, maxouts;
			
			__u64 start, end;
			
			g_array_sort(d2c->ctimes,comp_int64);
			g_array_sort(d2c->dtimes,comp_int64);

			i = j = maxouts = outs = 0;
			while(i < d2c->dtimes->len) {
				if(g_array_index(d2c->dtimes,__u64,i) <
				   g_array_index(d2c->ctimes,__u64,j)) {
					outs++;
					maxouts = MAX(outs,maxouts);
					i++;
				} else {
					outs--;
					j++;
				}
			}
			
			d2c->maxouts = MAX(d2c->maxouts,maxouts);
			
			/* getting d2c time */
			start = g_array_index(d2c->dtimes,__u64,0);
			end = g_array_index(d2c->ctimes,__u64,d2c->ctimes->len - 1);

			/* adding total time */
			d2c->d2ctime += end - start;

			/* re-initialize accounters */
			d2c->processed = 0;
			
			/* empty arrays */
			g_array_remove_range(d2c->dtimes,
					     0,
					     d2c->dtimes->len);
			g_array_remove_range(d2c->ctimes,
					     0,
					     d2c->ctimes->len);
		}
		
		assert(d2c->processed == 0);
		assert(d2c->dtimes->len == 0 && d2c->ctimes->len == 0);
	}
}

static void C(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_D2C(d2c,data);
	
	__u64 blks = t_blks(t);
	struct blk_io_trace *dtrace = g_tree_lookup(d2c->prospect_ds,&t->sector);
	
	if(blks && dtrace) {
		if(dtrace->bytes == t->bytes) {
			int e;

			d2c->processed++;
			
			/* add detail to file @detail_f */
			if(d2c->detail_f) {
				e = fprintf(d2c->detail_f,"%f %llu %llu %f\n",
					NANO_ULL_TO_DOUBLE(t->time),
					t->sector,
					blks,
					NANO_ULL_TO_DOUBLE(t->time - dtrace->time));
				if(e < 0) error_exit("Error writing D2C detail file\n");
			}
			
			g_array_append_val(d2c->dtimes,dtrace->time);
			g_array_append_val(d2c->ctimes,t->time);
		}
		
		g_tree_remove(d2c->prospect_ds,&dtrace->sector);
		g_free(dtrace);
		
		__account_reqs(d2c);
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

		__account_reqs(d2c);
	}
}

void d2c_add(void *data1, const void *data2)
{
	DECL_ASSIGN_D2C(d2c1,data1);
	DECL_ASSIGN_D2C(d2c2,data2);
	
	d2c1->d2ctime += d2c2->d2ctime;
}

void d2c_print_results(const void *data)
{
	DECL_ASSIGN_D2C(d2c,data);
	
	if(d2c->d2ctime > 0) {
		double t_time_msec = ((double)d2c->d2ctime)/1e6;
		double t_req_mb = ((double)d2c->req_dat->total_size)/(1<<11);

		printf("D2C Total time: %f (msec)\n",
			t_time_msec);
		printf("Avg. D2C per I/O: %f (msec)\n",
			t_time_msec/(d2c->req_dat->reqs));
		printf("Avg. D2C per block: %f (msec)\n",
			t_time_msec/(d2c->req_dat->total_size));
		printf("Avg. D2C Throughput: %f (MB/sec)\n",
			(t_req_mb)/(t_time_msec/1000));
		printf("D2C Max outstanding: %u (reqs)\n",
			d2c->maxouts);
	} else
		printf("Not enough data for D2C stats\n");
}

void d2c_init(struct plugin *p, struct plugin_set *ps, struct plug_args *pia)
{
	char filename[FILENAME_MAX];
	struct d2c_data *d2c = p->data = g_new(struct d2c_data,1);

	d2c->outstanding = d2c->processed = 0;
	d2c->d2ctime = d2c->maxouts = 0;

	d2c->prospect_ds = g_tree_new(comp_int64);
	d2c->dtimes = g_array_sized_new(FALSE,FALSE,sizeof(__u64),TENT_OUTS_RQS);
	d2c->ctimes = g_array_sized_new(FALSE,FALSE,sizeof(__u64),TENT_OUTS_RQS);
	d2c->req_dat = ps->plugs[REQ_SIZE_IND].data;
	
	/* open d2c detail file */
	d2c->detail_f = NULL;
	if(pia->d2c_det_f) {
		get_filename(filename, "d2c", pia->d2c_det_f, pia->end_range);
		d2c->detail_f = fopen(filename,"w");
		if(!d2c->detail_f) perror_exit("Opening D2C detail file");
	}
}

void d2c_destroy(struct plugin *p)
{
	DECL_ASSIGN_D2C(d2c,p->data);
	
	g_tree_destroy(d2c->prospect_ds);
	g_array_free(d2c->dtimes,FALSE);
	g_array_free(d2c->ctimes,FALSE);
	if(d2c->detail_f)
		fclose(d2c->detail_f);
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
