#include <dev_trace.h>

#include <glib.h>
#include <utils.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <assert.h>

#include <blktrace.h>
#include <blktrace_api.h>

void read_next_trace(struct trace_file *tf) 
{
	int e;
	
	e = read(tf->fd,&tf->t,sizeof(struct blk_io_trace));
	if(e==-1)
		perror_exit("Reading trace");
	else if(e==0)
		tf->eof = TRUE;
	else {
		assert(e==sizeof(struct blk_io_trace));
		if(tf->t.pdu_len) {
			char pdu_buf[tf->t.pdu_len];
			read(tf->fd,pdu_buf,tf->t.pdu_len);
			if(e==-1) perror_exit("Reading trace pdu");
		}
	}
}

void find_input_traces(struct dev_trace *trace, const char *dev)
{
	struct dirent *d;
	char pre_dev_trace[MAX_FILE_SIZE];
	
	struct trace_file *tf;
	
	DIR *cur_dir = opendir(".");

	if(!cur_dir) perror_exit("Opening dir");
	
	sprintf(pre_dev_trace,"%s.blktrace.",dev);
	while((d = readdir(cur_dir))) {
		if(strstr(d->d_name,pre_dev_trace)) {
			tf = g_new(struct trace_file,1);
			trace->files = g_slist_append(trace->files,tf);
			
			tf->fd = open(d->d_name,O_RDONLY);
			if(tf->fd<0) perror_exit("Opening file");
			
			tf->eof = FALSE;
			
			read_next_trace(tf);
		}
	}
}

struct dev_trace *dev_trace_create(const char *dev)
{
	struct dev_trace *dt = g_new(struct dev_trace,1);
	dt->files = NULL;	
	find_input_traces(dt,dev);
	
	return dt;
}

void free_data(gpointer data, gpointer __unused) 
{
	struct trace_file *tf = (struct trace_file *)data;
	close(tf->fd);
	g_free(tf);

	tf = __unused; /* useless. Just to make gcc quite */
}

void dev_trace_destroy(struct dev_trace *dt) 
{
	g_slist_foreach(dt->files,free_data,NULL);
	g_slist_free(dt->files);
	g_free(dt);
}

void min_time(gpointer data, gpointer min) 
{
	struct trace_file *tf = (struct trace_file *)data;
	struct trace_file **mintf = (struct trace_file **)min;

	if(!tf->eof) {
		if(!(*mintf)) {
			*mintf = tf;
		} else {
			assert(tf->t.time!=(*mintf)->t.time);
			if(tf->t.time<(*mintf)->t.time)
				*mintf = tf;
		}
	}
}

gboolean dev_trace_read_next(const struct dev_trace *dt, struct blk_io_trace *t) 
{
	struct trace_file *min = NULL;
	
	g_slist_foreach(dt->files,min_time,&min);
	
	if(!min)
		return FALSE;
	else {
		*t = min->t;
		read_next_trace(min);
		return TRUE;
	}
}

