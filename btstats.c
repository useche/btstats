#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>

#include <blktrace_api.h>
#include <blktrace.h>

#include <trace.h>
#include <plugins.h>

#include <utils.h>

struct time_range 
{
	__u64 start;
	__u64 end;

	struct plugin_set *ps; /* used in analysis */
};

struct args
{
	GHashTable *devs_ranges;
	gboolean total;
	char *d2c_det;
	unsigned trc_rdr;
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
		"Usage: btstats [-h] [-f <file>] [-r <reader>] [-t] [-d <file>] <device>\n\n"
		"Options:\n"
		"\t-h: Show this help message and exit\n"
		"\t-f: File which list the traces and phases to analyze.\n"
		"\t-t: Print the total stats for all traces.\n"
		"\t-d: File where all the details of D2C will be stored.\n"
		"\t\t<End range> <timestamp> <Sector #> <Req. Size (blks)> <D2C time (sec)>\n"
		"\t-r: Trace reader to be used\n"
		"\t\t0: default\n"
		"\t\t1: reader for device ata_piix\n"
		"\t<device>: String of devices/ranges to analyze.\n");
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

	a->devs_ranges = g_hash_table_new(g_str_hash,g_str_equal);
	memset(curdev,0,sizeof(curdev));
	
	while(getline(&line,&len,f) > 0) {
		char **no_com = g_strsplit(line,"#",2);
		no_com[0] = g_strstrip(no_com[0]);
		
		if(strlen(no_com[0])!=0) {
			if(no_com[0][0] == '@') {
				g_stpcpy(curdev,(no_com[0]+1));
				last_start = 0;
			} else {
				double end;
				struct time_range r;
				GArray *ranges = NULL;
				char *dev = NULL;
				
				if(strlen(curdev)==0)
					error_exit("Wrong trace name\n");
				
				e = sscanf(no_com[0],"%lf",&end);
				if(!e) error_exit("Wrong range\n");
				
				r.start = DOUBLE_TO_NANO_ULL(last_start);
				r.end = end==-1?G_MAXUINT64:DOUBLE_TO_NANO_ULL(end);
				
				dev = curdev;
				ranges = g_hash_table_lookup(a->devs_ranges, dev);
				
				if(!ranges) {
					dev = g_strdup(curdev);
					ranges = g_array_new(FALSE,FALSE,sizeof(struct time_range));
					g_hash_table_insert(a->devs_ranges,dev,ranges);
				}
				
				g_array_append_val(ranges,r);
				
				last_start = end;
			}
		}
		
		g_strfreev(no_com);
		free(line);
		line = NULL;
	}
}

void parse_dev_str(char *devs, struct args *a)
{
	int i, e;
	
	gchar **raw_dev_range = g_strsplit(devs,",",-1);
	
	a->devs_ranges = g_hash_table_new(g_str_hash,g_str_equal);
	
	for(i=0; raw_dev_range[i]; ++i) {
		GArray *ranges = NULL;
		struct time_range r;
		
		char *dev = NULL;
		char **dev_pair = g_strsplit(raw_dev_range[i],"@",2);
		
		double d_start = 0;
		double d_end = -1;
		
		if(dev_pair[1]) {
			e = sscanf(dev_pair[1],"%lf:%lf",&d_start,&d_end);
			if(!e) error_exit("Wrong devices or ranges\n");
		}
		
		r.start = DOUBLE_TO_NANO_ULL(d_start);
		r.end = d_end==-1?G_MAXUINT64:DOUBLE_TO_NANO_ULL(d_end);

		dev = dev_pair[0];
		ranges = g_hash_table_lookup(a->devs_ranges,dev);

		if(!ranges) {
			dev = g_strdup(dev_pair[0]);
			ranges = g_array_new(FALSE,FALSE,sizeof(struct time_range));
			g_hash_table_insert(a->devs_ranges,dev,ranges);
		}

		g_array_append_val(ranges,r);
		
		g_strfreev(dev_pair);
	}
	
	g_strfreev(raw_dev_range);
}

void handle_args(int argc, char **argv, struct args *a) 
{
	int c, r;
	char *device = NULL;
	char *file = NULL;

	memset(a,0,sizeof(struct args));

	while (1) {
		int option_index = 0;
		static struct option long_options[] =
			{
				{"file",	required_argument,	0, 'f'},
				{"total",	no_argument,		0, 't'},
				{"help",	no_argument,		0, 'h'},
				{"d2c-detail",	required_argument,	0, 'd'},
				{"trace-read",	required_argument,	0, 'r'},
				{0,0,0,0}
			};		
		
		c = getopt_long(argc, argv, "f:thd:r:", long_options, &option_index);
		
		if (c == -1) break;
		
		switch (c) {
		case 'f':
			file = optarg;
			break;
		case 't':
			a->total = TRUE;
			break;
		case 'd':
			a->d2c_det = optarg;
			break;
		case 'r':
			r = sscanf(optarg,"%u",&a->trc_rdr);
			if (r!=1 || a->trc_rdr >= N_TRCREAD)
				usage_exit();
			break;
		default:
			usage_exit();
			break;
		}
	}
	
	if((!file && argc == optind)||
	   (file && argc != optind) ||
	   (!file && argc > optind+1))
		usage_exit();
	else
		device = argv[optind];
	
	if(device)
		parse_dev_str(device,a);
	else
		parse_file(file,a);
}

void range_finish(struct time_range *range,
		  struct plugin_set *gps,
		  struct plugin_set *ps,
		  char *dev)
{
	char head[MAX_HEAD];
	char end_range[MAX_HEAD];
	
	/* adding the current plugin set to the global ps */
	if(gps)
		plugin_set_add(gps,ps);
	
	if(range->end == G_MAXUINT64)
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

void analyze_device(char *dev, GArray *ranges,
		struct plugin_set *ps,
		struct plug_args *pa,
		trace_reader_t read_next) 
{
	unsigned i;
	struct blk_io_trace t;
	struct trace *dt;
	
	/* init all plugin sets */
	for(i = 0; i < ranges->len; ++i) {
		struct time_range *r = &g_array_index(ranges,struct time_range,i);
		pa->end_range = r->end;
		r->ps = plugin_set_create(pa);
	}
	
	/* read and collect stats */
	dt = trace_create(dev);
	while(read_next(dt,&t) && ranges->len > 0) {
		i = 0;
		while(i < ranges->len) {
			struct time_range *r = &g_array_index(ranges,struct time_range,i);
			
			if(t.time > r->end) {
				range_finish(r,ps,r->ps,dev);
				g_array_remove_index_fast(ranges,i);
			} else {
				if(r->start <= t.time)
					plugin_set_add_trace(r->ps,&t);
				
				i++;
			}
		}
	}
	trace_destroy(dt);

	/* finish the ps which range is beyond the end */
	for(i = 0; i < ranges->len; ++i) {
		struct time_range *r = &g_array_index(ranges,struct time_range,i);
		range_finish(r,ps,r->ps,dev);
	}
}

void analyze_device_hash(gpointer dev_arg, gpointer ranges_arg, gpointer ar) 
{
	char *dev = dev_arg;
	GArray *ranges = ranges_arg;
	struct plugin_set *global_plugin = ((struct analyze_args *)ar)->ps;
	struct plug_args *pa = ((struct analyze_args *)ar)->pa;
	trace_reader_t rdr = ((struct analyze_args *)ar)->reader;
	
	analyze_device(dev,ranges,global_plugin,pa,rdr);
	
	free(dev);
	g_array_free(ranges,TRUE);
}

int main(int argc, char **argv) 
{
	struct args a;
	struct plug_args pa = { .d2c_det_f = NULL };
	
	struct analyze_args ar;
	struct plugin_set *global_plugin = NULL;
	
	handle_args(argc,argv,&a);
	
	init_plugs_ops();	
	
	if(a.total)
		global_plugin = plugin_set_create(NULL);

	/* open d2c detail file */
	if(a.d2c_det) {
		pa.d2c_det_f = fopen(a.d2c_det,"w");
		if(!pa.d2c_det_f) perror_exit("Opening D2C detail file");
	}

	/* analyze each device with its ranges */
	ar.ps = global_plugin;
	ar.pa = &pa;
	ar.reader = reader[a.trc_rdr];
	g_hash_table_foreach(a.devs_ranges,analyze_device_hash,&ar);
	
	if(a.total) {
		plugin_set_print(global_plugin,"All");
		plugin_set_destroy(global_plugin);
	}
	
	if(a.d2c_det)
		fclose(pa.d2c_det_f);
	
	destroy_plugs_ops();
	
	return 0;
}
