#include <glib.h>
#include <stdio.h>

#include <blktrace_api.h>

#include <dev_trace.h>
#include <plugins.h>

#include <utils.h>

struct time_range 
{
	double start;
	double end;
};

void analyze_device(char *dev, 
		    struct time_range range[],
		    struct plugin_set *ps) 
{
	int i;
	struct blk_io_trace t;
	struct dev_trace *dt;
	int nrange = sizeof(range)/sizeof(struct time_range);
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
		plugin_set_add(ps,r_ps[i]);
		
		sprintf(head,"%s [%.2f:%.2f]",
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
	
	char **devs = argv;
	int ndevs = argc;
	
	struct plugin_set *global_plugin = NULL;
	
	/* TODO: parse ranges, for now from beginnig to end */
	struct time_range global_range[] = {{0,-1}};
	
	/* TODO: parse application arguments */
	
	init_plugs_ops();	
	
	for(i = 0; i < ndevs; ++i)
		analyze_device(devs[i],global_range,global_plugin);
	
	if(global_plugin) {
		plugin_set_print(global_plugin,"All");
		plugin_set_destroy(global_plugin);
	}
	
	destroy_plugs_ops();
	
	return 0;
}
