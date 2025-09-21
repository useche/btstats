#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plugins.h>
#include <blktrace_api.h>
#include <blktrace.h>
#include <utils.h>

#include <reqsize.h>
#include <list_plugins.h>
#include "include/vector.h"
#include "include/sector_tree.h"

#define DECL_ASSIGN_D2C(name,data)			\
	struct d2c_data *name = (struct d2c_data *)data

struct d2c_data
{
	__u32 outstanding;
	__u32 processed;
	
	struct sector_tree_head *prospect_ds;
	vector *dtimes;
	vector *ctimes;

	__u64 d2ctime;

	__u32 maxouts;
	
	struct reqsize_data *req_dat;

	FILE *detail_f;
};

static void D(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_D2C(d2c,data);
	
	__u64 blks = t_blks(t);
	struct sector_entry find, *res;
    find.sector = t->sector;
	
	res = RB_FIND(sector_tree_head, d2c->prospect_ds, &find);
	if(blks && res == NULL) {
		DECL_DUP(struct blk_io_trace,new_t,t);
        struct sector_entry *new_se = malloc(sizeof(struct sector_entry));
        new_se->sector = new_t->sector;
        new_se->trace = new_t;
		RB_INSERT(sector_tree_head, d2c->prospect_ds, new_se);
		d2c->outstanding++;
	}
}

static int comp_u64(const void *a, const void *b)
{
    const __u64 *ua = a;
    const __u64 *ub = b;
    if (*ua < *ub) return -1;
    if (*ua > *ub) return 1;
    return 0;
}

static void __account_reqs(struct d2c_data *d2c, int finished)
{
	d2c->outstanding--;
	if(d2c->outstanding == 0 || finished) {
		if(d2c->processed > 0) {
			unsigned int i, j;
			__u32 outs, maxouts;
			
			__u64 start, end;
			
			qsort(d2c->ctimes->data, vector_total(d2c->ctimes), sizeof(__u64), comp_u64);
			qsort(d2c->dtimes->data, vector_total(d2c->dtimes), sizeof(__u64), comp_u64);

			i = j = maxouts = outs = 0;
			while(i < (unsigned int)vector_total(d2c->dtimes)) {
				if(*((__u64*)vector_get(d2c->dtimes,i)) <
				   *((__u64*)vector_get(d2c->ctimes,j))) {
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
			start = *((__u64*)vector_get(d2c->dtimes,0));
			end = *((__u64*)vector_get(d2c->ctimes, vector_total(d2c->ctimes) - 1));

			/* adding total time */
			d2c->d2ctime += end - start;

			/* re-initialize accounters */
			d2c->processed = 0;
			
			/* empty arrays */
			vector_remove_range(d2c->dtimes, 0, vector_total(d2c->dtimes));
			vector_remove_range(d2c->ctimes, 0, vector_total(d2c->ctimes));
		}
		
		assert(d2c->processed == 0);
		assert(vector_total(d2c->dtimes) == 0 && vector_total(d2c->ctimes) == 0);
	}
}

static void C(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_D2C(d2c,data);
	
	__u64 blks = t_blks(t);
	struct sector_entry find, *dtrace_entry;
    find.sector = t->sector;
	dtrace_entry = RB_FIND(sector_tree_head, d2c->prospect_ds, &find);
	
	if(blks && dtrace_entry) {
        struct blk_io_trace *dtrace = dtrace_entry->trace;
		if(dtrace->bytes == t->bytes) {
			int e;

			d2c->processed++;
			
			/* add detail to file @detail_f */
			if(d2c->detail_f) {
				e = fprintf(d2c->detail_f,"%f %llu %llu %f\n",
					NANO_ULL_TO_DOUBLE(t->time),
					(long long unsigned int)t->sector,
					(long long unsigned int)blks,
					NANO_ULL_TO_DOUBLE(t->time - dtrace->time));
				if(e < 0) error_exit("Error writing D2C detail file\n");
			}
			
			vector_add(d2c->dtimes, &dtrace->time);
			vector_add(d2c->ctimes, (void*)&t->time);
		}
		
		RB_REMOVE(sector_tree_head, d2c->prospect_ds, dtrace_entry);
		free(dtrace);
        free(dtrace_entry);
		
		__account_reqs(d2c, 0);
	}
}

static void R(const struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_D2C(d2c,data);
	
	struct sector_entry find, *dtrace_entry;
    find.sector = t->sector;
	dtrace_entry = RB_FIND(sector_tree_head, d2c->prospect_ds, &find);

	if(dtrace_entry) {
        struct blk_io_trace *dtrace = dtrace_entry->trace;
		assert(dtrace->bytes == t->bytes);
		
		RB_REMOVE(sector_tree_head, d2c->prospect_ds, dtrace_entry);
		free(dtrace);
        free(dtrace_entry);

		__account_reqs(d2c, 0);
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
	
	__account_reqs(d2c, 1);

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
	struct d2c_data *d2c = p->data = malloc(sizeof(struct d2c_data));

	d2c->outstanding = d2c->processed = 0;
	d2c->d2ctime = d2c->maxouts = 0;

	d2c->prospect_ds = malloc(sizeof(struct sector_tree_head));
    RB_INIT(sector_tree_head, d2c->prospect_ds);
	d2c->dtimes = malloc(sizeof(vector));
    vector_init(d2c->dtimes, sizeof(__u64));
	d2c->ctimes = malloc(sizeof(vector));
    vector_init(d2c->ctimes, sizeof(__u64));
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
	
    struct sector_entry *se, *next;
    RB_FOREACH_SAFE(se, sector_tree_head, d2c->prospect_ds, next) {
        RB_REMOVE(sector_tree_head, d2c->prospect_ds, se);
        free(se->trace);
        free(se);
    }
    free(d2c->prospect_ds);
	vector_free(d2c->dtimes);
    free(d2c->dtimes);
	vector_free(d2c->ctimes);
    free(d2c->ctimes);
	if(d2c->detail_f)
		fclose(d2c->detail_f);
	free(p->data);
}

void d2c_ops_init(struct plugin_ops *po)
{
	po->add = d2c_add;
	po->print_results = d2c_print_results;
	
    struct event_entry *e1 = malloc(sizeof(struct event_entry));
    e1->event_key = __BLK_TA_COMPLETE;
    e1->event_handler = (event_func_t)C;
	RB_INSERT(event_tree_head, po->event_tree, e1);

    struct event_entry *e2 = malloc(sizeof(struct event_entry));
    e2->event_key = __BLK_TA_ISSUE;
    e2->event_handler = (event_func_t)D;
	RB_INSERT(event_tree_head, po->event_tree, e2);

    struct event_entry *e3 = malloc(sizeof(struct event_entry));
    e3->event_key = __BLK_TA_REQUEUE;
    e3->event_handler = (event_func_t)R;
	RB_INSERT(event_tree_head, po->event_tree, e3);
}
