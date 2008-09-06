#include <glib.h>
#include <stdio.h>
#include <getopt.h>
#include <strings.h>

#include <blktrace_api.h>
#include <blktrace.h>

#include <dev_trace.h>
#include <plugins.h>

#include <utils.h>

struct trace_ps 
{
	struct blk_io_trace *trace;
	struct plugin_set **ps;
	struct plugin_set *gps;
	char *dev;
	int cur_ps;
};

struct time_range 
{
	__u64 start;
	__u64 end;
};

struct dev_ranges
{
	char *dev;
	GSList *ranges;
};

struct args
{
	GHashTable *devs_ranges;
	gboolean total;
};

void usage_exit() 
{
	error_exit("btstats [-f <file>] [-t] [<device>]\n\n"
		   "\t-f: File which list the traces and phases to analyze.\n"
		   "\t-t: Print the total stats for all traces.\n"
		   "\t<device>: String of devices/ranges to analyze.\n");
}

void parse_file(char *file, struct args *a) 
{
}

void parse_dev_str(char *devs, struct args *a)
{
	int i, e;
	
	gchar **raw_dev_range = g_strsplit(devs,",",-1);
	
	a->devs_ranges = g_hash_table_new(g_str_hash,g_str_equal);
	
	for(i=0; raw_dev_range[i]; ++i) {
		GSList *ranges;
		struct time_range *r;
		
		char **dev_pair = g_strsplit(raw_dev_range[i],"@",2);
		
		double d_start = 0;
		double d_end = -1;
		
		if(dev_pair[1]) {
			e = sscanf(dev_pair[1],"%lf:%lf",&d_start,&d_end);
			if(!e) error_exit("Wrong devices and ranges");
		}
		
		r = g_new0(struct time_range,1);
		r->start = DOUBLE_TO_NANO_ULL(d_start);
		r->end = d_end==-1?G_MAXUINT64:DOUBLE_TO_NANO_ULL(d_end);
		
		ranges = g_hash_table_lookup(a->devs_ranges,dev_pair[0]);
		ranges = g_slist_append(ranges,r);
		g_hash_table_replace(a->devs_ranges,dev_pair[0],ranges);
	}
	
	g_strfreev(raw_dev_range);
}

void handle_args(int argc, char **argv, struct args *a) 
{
	int c;
	char *device = NULL;
	char *file = NULL;

	bzero(a,sizeof(struct args));

	while (1) {
		int option_index = 0;
		static struct option long_options[] =
			{
				{"file", required_argument, 0,   'f'},
				{"total",  no_argument, 0,       't'},
				{"help",  no_argument, 0,        'h'},
				{0,0,0,0}
			};		
		
		c = getopt_long(argc, argv, "f:th", long_options, &option_index);
		
		if (c == -1) break;
		
		switch (c) {
		case 'f':
			file = optarg;
			break;
		case 't':
			a->total = TRUE;
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

void analyze_dev_range(gpointer r_arg, gpointer t_arg)
{
	struct time_range *range = (struct time_range *)r_arg;
	struct trace_ps *tps = (struct trace_ps *)t_arg;

	if(range->start <= tps->trace->time && tps->trace->time < range->end)
		plugin_set_add_trace(tps->ps[tps->cur_ps],tps->trace);
	
	tps->cur_ps++;
}

void ranges_finish(gpointer r_arg, gpointer t_arg)
{
	char head[HEAD_MAX];
	struct time_range *range = (struct time_range *)r_arg;
	struct trace_ps *tps = (struct trace_ps *)t_arg;
	
	/* adding the current plugin set to the global ps */
	if(tps->gps)
		plugin_set_add(tps->gps,tps->ps[tps->cur_ps]);
	
	sprintf(head,"%s [%lld.%lld:%lld.%lld]",
		tps->dev,
		SECONDS(range->start),
		NANO_SECONDS(range->start),
		SECONDS(range->end),
		NANO_SECONDS(range->end));
	
	plugin_set_print(tps->ps[tps->cur_ps],head);
	plugin_set_destroy(tps->ps[tps->cur_ps]);	
	
	tps->cur_ps++;
}

void analyze_device(char *dev, GSList *ranges, struct plugin_set *ps) 
{
	int i;
	struct blk_io_trace t;
	struct dev_trace *dt;
	int nrange = g_slist_length(ranges);
	struct plugin_set *r_ps[nrange];
	struct trace_ps tps;
	
	/* init all plugin sets */
	for(i = 0; i < nrange; ++i)
		r_ps[i] = plugin_set_create();
	
	/* read and collect stats */
	dt = dev_trace_create(dev);
	while(dev_trace_read_next(dt,&t)) {
		tps.trace = &t;
		tps.ps = r_ps;
		tps.cur_ps = 0;
		tps.gps = NULL;
		tps.dev = NULL;
		g_slist_foreach(ranges,analyze_dev_range,&tps);
	}
	
	/* print and destroy plugin sets */
	tps.trace = NULL;
	tps.ps = r_ps;
	tps.cur_ps = 0;
	tps.gps = ps;
	tps.dev = dev;
	g_slist_foreach(ranges,ranges_finish,&tps);
}

void analyze_device_hash(gpointer dev, gpointer ranges, gpointer global_plugin) 
{
	analyze_device(dev,ranges,global_plugin);
}

int main(int argc, char **argv) 
{
	struct args a;
	
	struct plugin_set *global_plugin = NULL;
	
	handle_args(argc,argv,&a);
	
	init_plugs_ops();	
	
	if(a.total)
		global_plugin = plugin_set_create();	

	/* analyze each device with its ranges */
	g_hash_table_foreach(a.devs_ranges,analyze_device_hash,global_plugin);
	
	if(a.total) {
		plugin_set_print(global_plugin,"All");
		plugin_set_destroy(global_plugin);
	}
	
	destroy_plugs_ops();
	
	return 0;
}
