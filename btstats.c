#include <glib.h>
#include <stdio.h>
#include <getopt.h>
#include <strings.h>

#include <blktrace_api.h>

#include <dev_trace.h>
#include <plugins.h>

#include <utils.h>

struct time_range 
{
	__u64 start;
	__u64 end;
};

struct args
{
	char *file;
	char *device;
	gboolean total;
};

void usage_exit() 
{
	error_exit("btstats [-f <file>] [-t] [<device>]\n\n"
		   "\t-f: File which list the traces and phases to analyze.\n"
		   "\t-t: Print the total stats for all traces.\n"
		   "\t<device>: String of devices/ranges to analyze.\n");
}

void handle_args(int argc, char **argv, struct args *a) 
{
	int c;
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
			a->file = optarg;
			break;
		case 't':
			a->total = TRUE;
			break;
		default:
			usage_exit();
			break;
		}
	}
	
	if((!a->file && argc == optind)||
	   (a->file && argc != optind) ||
	   (!a->file && argc > optind+1))
		usage_exit();
	else
		a->device = argv[optind];
}
	
void analyze_device(char *dev, 
		    struct time_range range[],
		    int nrange,
		    struct plugin_set *ps) 
{
	int i;
	struct blk_io_trace t;
	struct dev_trace *dt;
	struct plugin_set *r_ps[nrange];
	
	/* init all plugin sets */
	for(i = 0; i < nrange; ++i)
		r_ps[i] = plugin_set_create();
	
	/* read and collect stats */
	dt = dev_trace_create(dev);
	while(dev_trace_read_next(dt,&t)) {
		for(i = 0; i < nrange; ++i) {
			if(range[i].start <= t.time && t.time < range[i].end)
				plugin_set_add_trace(r_ps[i],&t);
		}
	}
	
	/* print and destroy plugin sets */
	for(i = 0; i < nrange; ++i) {
		char head[HEAD_MAX];
		
		/* adding the current plugin set to the global ps */
		if(ps)
			plugin_set_add(ps,r_ps[i]);
		
		sprintf(head,"%s [%lld:%lld]",
			dev,
			range[i].start,
			range[i].end);
		
		plugin_set_print(r_ps[i],head);
		plugin_set_destroy(r_ps[i]);
	}
}

int main(int argc, char **argv) 
{
	int i;
	struct args a;
	
	int ndevs = 1;
	
	struct plugin_set *global_plugin = NULL;
	
	/* TODO: parse ranges, for now from beginnig to end */
	struct time_range global_range[] = {{0,~0}};
	
	handle_args(argc,argv,&a);
	
	init_plugs_ops();	
	
	if(a.total)
		global_plugin = plugin_set_create();	
	
	for(i = 0; i < ndevs; ++i)
		analyze_device(a.device,global_range,1,global_plugin);
	
	if(a.total) {
		plugin_set_print(global_plugin,"All");
		plugin_set_destroy(global_plugin);
	}
	
	destroy_plugs_ops();
	
	return 0;
}
