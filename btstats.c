#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>

#include <blktrace_api.h>
#include <blktrace.h>

#include <trace.h>
#include <plugins.h>

#include "include/utils.h"
#include "include/bsd/queue.h"
#include "include/bsd/tree.h"

// MAX_HEAD is used for the head string in range_finish.
// 256 seems like a reasonable size.
#define MAX_HEAD 256

struct time_range
{
	__u64 start;
	__u64 end;
	struct plugin_set *ps; /* used in analysis */
    SLIST_ENTRY(time_range) entries;
};

SLIST_HEAD(time_range_list, time_range);

struct dev_range_entry {
    char *dev_name;
    struct time_range_list *ranges;
    RB_ENTRY(dev_range_entry) entry;
};

static int dev_range_cmp(const struct dev_range_entry *a, const struct dev_range_entry *b) {
    return strcmp(a->dev_name, b->dev_name);
}

RB_HEAD(dev_range_tree, dev_range_entry);
RB_PROTOTYPE(dev_range_tree, dev_range_entry, entry, dev_range_cmp);
RB_GENERATE(dev_range_tree, dev_range_entry, entry, dev_range_cmp);

struct args
{
	struct dev_range_tree *devs_ranges;
	bool total;
	char *d2c_det;
	unsigned trc_rdr;
	char *i2c_oio;
	char *i2c_oio_hist;
};

struct analyze_args
{
	struct plugin_set *ps;
	struct plug_args *pa;
	trace_reader_t reader;
};

void usage_exit()
{
	error_exit(
		"Usage: btstats [-h] [-f <file>] [-r <reader>] [-t] [-d <file>] [-i <file>] [<trace> .. <trace>]\n\n"
		"Options:\n"
		"\t-h: Show this help message and exit\n"
		"\t-f: File which list the traces and phases to analyze.\n"
		"\t-t: Print the total stats for all traces.\n"
		"\t-d: File sufix where all the details of D2C will be stored.\n"
		"\t\t<timestamp> <Sector #> <Req. Size (blks)> <D2C time (sec)>\n"
		"\t-i: File sufix where all the changes in OIO for I2C are logged.\n"
		"\t-s: File sufix where the histogram of OIO for I2C is printed.\n"
		"\t-r: Trace reader to be used\n"
		"\t\t0: default\n"
		"\t\t1: reader for driver ata_piix\n"
		"\t<trace>: String of device/range to analyze. Exclusive with -f.\n");
}

void parse_file(char *filename, struct args *a)
{
	char *line = NULL;
	size_t len;
	char curdev[FILENAME_MAX];
	double last_start = -1;

	int e;
	FILE *f = fopen(filename,"r");
	if(!f) perror_exit("Ranges file");

	a->devs_ranges = malloc(sizeof(struct dev_range_tree));
    RB_INIT(dev_range_tree, a->devs_ranges);
	memset(curdev,0,sizeof(curdev));

	while(getline(&line,&len,f) > 0) {
		char **no_com = str_split(line,"#",2);
		no_com[0] = str_strip(no_com[0]);

		if(strlen(no_com[0])!=0) {
			if(no_com[0][0] == '@') {
				strncpy(curdev,(no_com[0]+1), FILENAME_MAX -1);
				last_start = 0;
			} else {
				double end;
				struct time_range *r = malloc(sizeof(struct time_range));
				struct dev_range_entry find, *res;
                find.dev_name = curdev;

				if(strlen(curdev)==0)
					error_exit("Wrong trace name\n");

				e = sscanf(no_com[0],"%lf",&end);
				if(!e) error_exit("Wrong range\n");

				r->start = DOUBLE_TO_NANO_ULL(last_start);
				r->end = end==-1?UINT64_MAX:DOUBLE_TO_NANO_ULL(end);

				res = RB_FIND(dev_range_tree, a->devs_ranges, &find);

				if(!res) {
                    res = malloc(sizeof(struct dev_range_entry));
                    res->dev_name = strdup(curdev);
                    res->ranges = malloc(sizeof(struct time_range_list));
                    SLIST_INIT(res->ranges);
					RB_INSERT(dev_range_tree, a->devs_ranges, res);
				}

                SLIST_INSERT_HEAD(res->ranges, r, entries);

				last_start = end;
			}
		}

		str_freev(no_com);
		free(line);
		line = NULL;
	}
}

void parse_dev_str(char **devs, struct args *a)
{
	int i, e;

	a->devs_ranges = malloc(sizeof(struct dev_range_tree));
    RB_INIT(dev_range_tree, a->devs_ranges);

	for(i=0; devs[i]; ++i) {
		struct time_range *r = malloc(sizeof(struct time_range));
        struct dev_range_entry find, *res;

		char **dev_pair = str_split(devs[i],"@",2);
        find.dev_name = dev_pair[0];

		double d_start = 0;
		double d_end = -1;

		if(dev_pair[1]) {
			e = sscanf(dev_pair[1],"%lf:%lf",&d_start,&d_end);
			if(!e) error_exit("Wrong devices or ranges\n");
		}

		r->start = DOUBLE_TO_NANO_ULL(d_start);
		r->end = d_end==-1?UINT64_MAX:DOUBLE_TO_NANO_ULL(d_end);

		res = RB_FIND(dev_range_tree, a->devs_ranges, &find);

		if(!res) {
			res = malloc(sizeof(struct dev_range_entry));
            res->dev_name = strdup(dev_pair[0]);
            res->ranges = malloc(sizeof(struct time_range_list));
            SLIST_INIT(res->ranges);
			RB_INSERT(dev_range_tree, a->devs_ranges, res);
		}

        SLIST_INSERT_HEAD(res->ranges, r, entries);

		str_freev(dev_pair);
	}
}

void handle_args(int argc, char **argv, struct args *a)
{
	int c, r;
	char *file = NULL;

	memset(a,0,sizeof(struct args));

	while (1) {
		int option_index = 0;
		static struct option long_options[] =
			{
				{"file",		required_argument,	0, 'f'},
				{"total",		no_argument,		0, 't'},
				{"help",		no_argument,		0, 'h'},
				{"d2c-detail",		required_argument,	0, 'd'},
				{"trace-read",		required_argument,	0, 'r'},
				{"i2c-oio",		required_argument,	0, 'i'},
				{"i2c-oio-hist",	required_argument,	0, 's'},
				{0,0,0,0}
			};

		c = getopt_long(argc, argv, "f:thd:r:i:s:", long_options, &option_index);

		if (c == -1) break;

		switch (c) {
		case 'f':
			file = optarg;
			break;
		case 't':
			a->total = true;
			break;
		case 'd':
			a->d2c_det = optarg;
			break;
		case 'r':
			r = sscanf(optarg,"%u",&a->trc_rdr);
			if (r!=1 || a->trc_rdr >= N_TRCREAD)
				usage_exit();
			break;
		case 'i':
			a->i2c_oio = optarg;
			break;
		case 's':
			a->i2c_oio_hist = optarg;
			break;
		default:
			usage_exit();
			break;
		}
	}

	if((file && argc != optind)||
	   (!file && argc == optind))
		usage_exit();

	if(file)
		parse_file(file,a);
	else
		parse_dev_str(&argv[optind],a);
}

void range_finish(struct time_range *range,
		  struct plugin_set *gps,
		  struct plugin_set *ps,
		  char *dev)
{
	char head[MAX_HEAD];
	char end_range[MAX_HEAD / 2];

	/* adding the current plugin set to the global ps */
	if(gps)
		plugin_set_add(gps,ps);

	if(range->end == UINT64_MAX)
		sprintf(end_range,"%s","inf");
	else
		sprintf(end_range,"%.4f",NANO_ULL_TO_DOUBLE(range->end));

	sprintf(head,"%s[%.4f:%s]",
		dev,
		NANO_ULL_TO_DOUBLE(range->start),
		end_range);


	plugin_set_print(ps,head);
	plugin_set_destroy(ps);
}

void analyze_device(char *dev, struct time_range_list *ranges,
		struct plugin_set *ps,
		struct plug_args *pa,
		trace_reader_t read_next)
{
	struct blk_io_trace t;
	struct trace *dt;
    struct time_range *r;

	/* init all plugin sets */
    SLIST_FOREACH(r, ranges, entries) {
		pa->end_range = r->end;
		r->ps = plugin_set_create(pa);
	}

	/* read and collect stats */
	dt = trace_create(dev);
	while(read_next(dt,&t) && !SLIST_EMPTY(ranges)) {
        struct time_range *r_safe;
        SLIST_FOREACH_SAFE(r, ranges, entries, r_safe) {
			if(t.time > r->end) {
				range_finish(r,ps,r->ps,dev);
                SLIST_REMOVE(ranges, r, time_range, entries);
                free(r);
			} else {
				if(r->start <= t.time)
					plugin_set_add_trace(r->ps,&t);
			}
		}
	}
	trace_destroy(dt);

	/* finish the ps which range is beyond the end */
    SLIST_FOREACH(r, ranges, entries) {
		range_finish(r,ps,r->ps,dev);
	}
}

void analyze_device_hash(struct dev_range_entry *dre, void *ar)
{
	struct plugin_set *global_plugin = ((struct analyze_args *)ar)->ps;
	struct plug_args *pa = ((struct analyze_args *)ar)->pa;
	trace_reader_t rdr = ((struct analyze_args *)ar)->reader;

	analyze_device(dre->dev_name, dre->ranges, global_plugin, pa, rdr);

	free(dre->dev_name);
    struct time_range *r;
    while(!SLIST_EMPTY(dre->ranges)) {
        r = SLIST_FIRST(dre->ranges);
        SLIST_REMOVE_HEAD(dre->ranges, entries);
        free(r);
    }
    free(dre->ranges);
    free(dre);
}

int main(int argc, char **argv)
{
	struct args a;
	struct plug_args pa;

	struct analyze_args ar;
	struct plugin_set *global_plugin = NULL;

	handle_args(argc,argv,&a);

	init_plugs_ops();

	if(a.total)
		global_plugin = plugin_set_create(NULL);

	/* populate plugin arguments */
	pa.d2c_det_f = a.d2c_det;
	pa.i2c_oio_f = a.i2c_oio;
	pa.i2c_oio_hist_f = a.i2c_oio_hist;

	/* analyze each device with its ranges */
	ar.ps = global_plugin;
	ar.pa = &pa;
	ar.reader = reader[a.trc_rdr];
    struct dev_range_entry *dre, *dre_safe;
    RB_FOREACH_SAFE(dre, dev_range_tree, a.devs_ranges, dre_safe) {
        analyze_device_hash(dre, &ar);
    }

	if(a.total) {
		plugin_set_print(global_plugin,"All");
		plugin_set_destroy(global_plugin);
	}

	destroy_plugs_ops();

	return 0;
}
