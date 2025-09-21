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
#include "include/bsd/queue.h"
#include "include/sector_tree.h"

#define DECL_ASSIGN_D2C(name,data)			\
	struct d2c_data *name = (struct d2c_data *)data

struct time_entry {
    __u64 time;
    SLIST_ENTRY(time_entry) entries;
};

SLIST_HEAD(time_list, time_entry);

struct d2c_data
{
	__u32 outstanding;
	__u32 processed;
	
	struct sector_tree_head *prospect_ds;
	struct time_list *dtimes;
	struct time_list *ctimes;

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

static void __account_reqs(struct d2c_data *d2c, int finished)
{
	d2c->outstanding--;
	if(d2c->outstanding == 0 || finished) {
		if(d2c->processed > 0) {
			__u32 outs, maxouts;
			
			__u64 start, end;
			
            struct time_entry *d_it, *c_it;
            d_it = SLIST_FIRST(d2c->dtimes);
            c_it = SLIST_FIRST(d2c->ctimes);

			outs = maxouts = 0;
			while(d_it != NULL && c_it != NULL) {
				if(d_it->time < c_it->time) {
					outs++;
					maxouts = MAX(outs,maxouts);
                    d_it = SLIST_NEXT(d_it, entries);
				} else {
					outs--;
                    c_it = SLIST_NEXT(c_it, entries);
				}
			}
			
			d2c->maxouts = MAX(d2c->maxouts,maxouts);
			
			/* getting d2c time */
			start = SLIST_FIRST(d2c->dtimes)->time;
            struct time_entry *last = SLIST_FIRST(d2c->ctimes);
            while(SLIST_NEXT(last, entries) != NULL)
                last = SLIST_NEXT(last, entries);
			end = last->time;

			/* adding total time */
			d2c->d2ctime += end - start;

			/* re-initialize accounters */
			d2c->processed = 0;
			
			/* empty arrays */
            struct time_entry *te;
            while(!SLIST_EMPTY(d2c->dtimes)) {
                te = SLIST_FIRST(d2c->dtimes);
                SLIST_REMOVE_HEAD(d2c->dtimes, entries);
                free(te);
            }
            while(!SLIST_EMPTY(d2c->ctimes)) {
                te = SLIST_FIRST(d2c->ctimes);
                SLIST_REMOVE_HEAD(d2c->ctimes, entries);
                free(te);
            }
		}
		
		assert(d2c->processed == 0);
		assert(SLIST_EMPTY(d2c->dtimes) && SLIST_EMPTY(d2c->ctimes));
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
			
            struct time_entry *d_te = malloc(sizeof(struct time_entry));
            d_te->time = dtrace->time;
            SLIST_INSERT_HEAD(d2c->dtimes, d_te, entries);

            struct time_entry *c_te = malloc(sizeof(struct time_entry));
            c_te->time = t->time;
            SLIST_INSERT_HEAD(d2c->ctimes, c_te, entries);
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
	d2c->dtimes = malloc(sizeof(struct time_list));
    SLIST_INIT(d2c->dtimes);
	d2c->ctimes = malloc(sizeof(struct time_list));
    SLIST_INIT(d2c->ctimes);
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

    struct time_entry *te;
    while(!SLIST_EMPTY(d2c->dtimes)) {
        te = SLIST_FIRST(d2c->dtimes);
        SLIST_REMOVE_HEAD(d2c->dtimes, entries);
        free(te);
    }
    free(d2c->dtimes);
    while(!SLIST_EMPTY(d2c->ctimes)) {
        te = SLIST_FIRST(d2c->ctimes);
        SLIST_REMOVE_HEAD(d2c->ctimes, entries);
        free(te);
    }
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
