#include <trace.h>

#include <glib.h>
#include <utils.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>

#include <asm/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <assert.h>

#include <blktrace.h>
#include <blktrace_api.h>

#define CORRECT_ENDIAN(v)					\
	do {							\
		if(sizeof(v) == sizeof(__u32))			\
			v = be32_to_cpu(v);			\
		else if(sizeof(v) == sizeof(__u64))		\
			v = be64_to_cpu(v);			\
		else						\
			error_exit("Wrong endian conversion");	\
	} while(0)

static int native_trace = -1;

void min_time(gpointer data, gpointer min)
{
	struct trace_file *tf = (struct trace_file *)data;
	struct trace_file **mintf = (struct trace_file **)min;

	if(!tf->eof) {
		if(!(*mintf)) {
			*mintf = tf;
		} else {
			if(tf->t.time<(*mintf)->t.time)
				*mintf = tf;
		}
	}
}

void correct_time(gpointer data, gpointer dt_arg)
{
	struct trace_file *tf = (struct trace_file *)data;
	struct trace *dt = (struct trace *)dt_arg;
	
	tf->t.time -= dt->genesis;
}

gboolean not_real_event(struct blk_io_trace *t)
{
	return	(t->action & BLK_TC_ACT(BLK_TC_NOTIFY))	||
		(t->action & BLK_TC_ACT(BLK_TC_DISCARD))||
		(t->action & BLK_TC_ACT(BLK_TC_DRV_DATA));
}

void read_next(struct trace_file *tf, __u64 genesis)
{
	int e;

	do {
		e = read(tf->fd,&tf->t,sizeof(struct blk_io_trace));
		if(e==0) {
			tf->eof = TRUE;
			break;
		} else if(e==-1 || e!=sizeof(struct blk_io_trace)) {
			perror_exit("Reading trace\n");
		} else {
			/* verify trace and check endianess */
			if(native_trace<0)
				native_trace = check_data_endianness(tf->t.magic);
			
			assert(native_trace>=0);
			if(!native_trace) {
				CORRECT_ENDIAN(tf->t.magic);
				CORRECT_ENDIAN(tf->t.sequence);
				CORRECT_ENDIAN(tf->t.time);
				CORRECT_ENDIAN(tf->t.sector);
				CORRECT_ENDIAN(tf->t.bytes);
				CORRECT_ENDIAN(tf->t.action);
				CORRECT_ENDIAN(tf->t.pid);
				CORRECT_ENDIAN(tf->t.device);
				CORRECT_ENDIAN(tf->t.cpu);
				CORRECT_ENDIAN(tf->t.error);
				CORRECT_ENDIAN(tf->t.pdu_len);
			}
			
			if(verify_trace(&tf->t))
				error_exit("Bad trace!\n");
			
			/* updating to relative time right away */
			tf->t.time -= genesis;
			
			if(tf->t.pdu_len) {
				e = lseek(tf->fd,tf->t.pdu_len,SEEK_CUR);
				if(e==-1) perror_exit("Skipping pdu");
			}
		}
	} while(not_real_event(&tf->t));
}

void find_input_traces(struct trace *trace, const char *dev)
{
	struct dirent *d;
	char pre_trace[FILENAME_MAX];
	char file_path[FILENAME_MAX];
	
	struct trace_file *tf;
	struct trace_file *min = NULL;

	char *basen, *dirn;
	char *basec = strdup(dev);
	char *dirc = strdup(dev);
	
	basen = basename(basec);
	dirn = dirname(dirc);
	DIR *cur_dir = opendir(dirn);

	if(!cur_dir) perror_exit("Opening dir");
	
	sprintf(pre_trace,"%s.blktrace.",basen);
	while((d = readdir(cur_dir))) {
		if(strstr(d->d_name,pre_trace)==d->d_name) {
			tf = g_new(struct trace_file,1);
			trace->files = g_slist_prepend(trace->files,tf);
			
			sprintf(file_path,"%s/%s", dirn, d->d_name);

			tf->fd = open(file_path,O_RDONLY);
			if(tf->fd<0) perror_exit("Opening tracefile");
			
			tf->eof = FALSE;
			
			read_next(tf,0);
		}
	}
	
	if(g_slist_length(trace->files)==0)
		error_exit("No such traces: %s\n", dev);

	g_slist_foreach(trace->files,min_time,&min);
	trace->genesis = min->t.time;
	g_slist_foreach(trace->files,correct_time,trace);
	
	closedir(cur_dir);
	free(basec);
	free(dirc);
}

struct trace *trace_create(const char *dev)
{
	struct trace *dt = g_new(struct trace,1);
	dt->files = NULL;	
	find_input_traces(dt,dev);
	
	return dt;
}

void free_data(gpointer data, gpointer __unused) 
{
	__unused = NULL; /* make gcc quite */

	struct trace_file *tf = (struct trace_file *)data;
	close(tf->fd);
	g_free(tf);
}

void trace_destroy(struct trace *dt) 
{
	g_slist_foreach(dt->files,free_data,NULL);
	g_slist_free(dt->files);
	g_free(dt);
}

gboolean trace_read_next(const struct trace *dt, struct blk_io_trace *t) 
{
	struct trace_file *min = NULL;
	
	g_slist_foreach(dt->files,min_time,&min);
	
	if(!min)
		return FALSE;
	else {
		*t = min->t;
		read_next(min, dt->genesis);
		return TRUE;
	}
}

