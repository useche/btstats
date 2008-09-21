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

#include <dev_trace.h>

#include <glib.h>
#include <utils.h>
#include <unistd.h>
#include <string.h>

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
			assert(tf->t.time!=(*mintf)->t.time);
			if(tf->t.time<(*mintf)->t.time)
				*mintf = tf;
		}
	}
}

void correct_time(gpointer data, gpointer dt_arg)
{
	struct trace_file *tf = (struct trace_file *)data;
	struct dev_trace *dt = (struct dev_trace *)dt_arg;
	
	tf->t.time -= dt->genesis;
}

gboolean not_real_event(struct blk_io_trace *t)
{
	return t->action & BLK_TC_ACT(BLK_TC_NOTIFY);
}

void read_next_trace(struct trace_file *tf, __u64 genesis)
{
	int e;

	do {

		e = read(tf->fd,&tf->t,sizeof(struct blk_io_trace));
		if(e==-1) 
			perror_exit("Reading trace\n");
		else if(e==0)
			tf->eof = TRUE;
		else {
			if(e!=sizeof(struct blk_io_trace))
				error_exit("Reading trace\n");

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
				char pdu_buf[tf->t.pdu_len];
				e = read(tf->fd,pdu_buf,tf->t.pdu_len);
				if(e==-1) perror_exit("Reading trace pdu");
			}
		}

	} while(not_real_event(&tf->t));
}

void find_input_traces(struct dev_trace *trace, const char *dev)
{
	struct dirent *d;
	char pre_dev_trace[MAX_FILE_SIZE];
	
	struct trace_file *tf;
	struct trace_file *min = NULL;
	
	DIR *cur_dir = opendir(".");

	if(!cur_dir) perror_exit("Opening dir");
	
	sprintf(pre_dev_trace,"%s.blktrace.",dev);
	while((d = readdir(cur_dir))) {
		if(strstr(d->d_name,pre_dev_trace)) {
			tf = g_new(struct trace_file,1);
			trace->files = g_slist_append(trace->files,tf);
			
			tf->fd = open(d->d_name,O_RDONLY);
			if(tf->fd<0) perror_exit("Opening tracefile");
			
			tf->eof = FALSE;
			
			read_next_trace(tf,0);
		}
	}
	
	if(g_slist_length(trace->files)==0) {
		printf("%s; %s\n",dev,pre_dev_trace);
		error_exit("No such traces\n");
	}

	g_slist_foreach(trace->files,min_time,&min);
	trace->genesis = min->t.time;
	g_slist_foreach(trace->files,correct_time,trace);
	
	closedir(cur_dir);
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
	__unused = NULL; /* make gcc quite */

	struct trace_file *tf = (struct trace_file *)data;
	close(tf->fd);
	g_free(tf);
}

void dev_trace_destroy(struct dev_trace *dt) 
{
	g_slist_foreach(dt->files,free_data,NULL);
	g_slist_free(dt->files);
	g_free(dt);
}

gboolean dev_trace_read_next(const struct dev_trace *dt, struct blk_io_trace *t) 
{
	struct trace_file *min = NULL;
	
	g_slist_foreach(dt->files,min_time,&min);
	
	if(!min)
		return FALSE;
	else {
		*t = min->t;
		read_next_trace(min, dt->genesis);
		return TRUE;
	}
}

