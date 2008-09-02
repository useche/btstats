#include <blktrace_api.h>

struct dev_trace 
{
	char *dev;
	int ncpu;
};

struct dev_trace *dev_trace_create(const char *dev);
void dev_trace_destroy(struct dev_trace *dt);
gboolean dev_trace_read_next(const struct dev_trace *dt, struct blk_io_trace *t);
