#ifndef _DEV_TRACE_H_
#define _DEV_TRACE_H_

#include <blktrace_api.h>
#include <glib.h>

struct trace_file 
{
	struct blk_io_trace t;
	int fd;
	gboolean eof;
};

struct dev_trace 
{
	char *dev;
	GSList *files;
};

struct dev_trace *dev_trace_create(const char *dev);
void dev_trace_destroy(struct dev_trace *dt);
gboolean dev_trace_read_next(const struct dev_trace *dt, struct blk_io_trace *t);

#endif
