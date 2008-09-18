/*
 * Author: Luis Useche (August 2008)
 * Email: luis@cs.fiu.edu
 *
 * BSD License
 * Copyright (c) 2008, Luis Useche
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 *     * Neither the name of Luis Useche nor the names of its
 *       contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>

#include <blktrace_api.h>
#include <blktrace.h>

#include <dev_trace.h>
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
};

void usage_exit() 
{
	error_exit("btstats [-f <file>] [-t] [<device>]\n\n"
		   "\t-f: File which list the traces and phases to analyze.\n"
		   "\t-t: Print the total stats for all traces.\n"
		   "\t<device>: String of devices/ranges to analyze.\n");
}

void parse_file(char *filename, struct args *a) 
{
	char *line = NULL;
	size_t len;
	char curdev[MAX_FILE_SIZE];
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
	int c;
	char *device = NULL;
	char *file = NULL;

	bzero(a,sizeof(struct args));

	while (1) {
		int option_index = 0;
		static struct option long_options[] =
			{
				{"file", required_argument, 0,	 'f'},
				{"total",  no_argument, 0,	 't'},
				{"help",  no_argument, 0,	 'h'},
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

void analyze_device(char *dev, GArray *ranges, struct plugin_set *ps) 
{
	unsigned i;
	struct blk_io_trace t;
	struct dev_trace *dt;
	
	/* init all plugin sets */
	for(i = 0; i < ranges->len; ++i)
		g_array_index(ranges,struct time_range,i).ps = plugin_set_create();
	
	/* read and collect stats */
	dt = dev_trace_create(dev);
	while(dev_trace_read_next(dt,&t)) {
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

	/* finish the ps which ranges are beyond the end */
	for(i = 0; i < ranges->len; ++i) {
		struct time_range *r = &g_array_index(ranges,struct time_range,i);
		range_finish(r,ps,r->ps,dev);
	}
}

void analyze_device_hash(gpointer dev_arg, gpointer ranges_arg, gpointer global_plugin) 
{
	char *dev = dev_arg;
	GArray *ranges = ranges_arg;
	
	analyze_device(dev,ranges,global_plugin);
	
	free(dev);
	g_array_free(ranges,TRUE);
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
