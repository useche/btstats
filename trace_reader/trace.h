#ifndef _TRACE_H_
#define _TRACE_H_

#include <blktrace_api.h>
#include <glib.h>
#include <stdbool.h>

struct trace_file {
	struct blk_io_trace t;
	int fd;
	bool eof;
};

struct trace {
	GSList *files;
	__u64 genesis;
};

typedef bool (*trace_reader_t)(const struct trace *, struct blk_io_trace *);

/* constructor and destructor */
struct trace *trace_create(const char *dev);
void trace_destroy(struct trace *dt);

/* default trace reader */
bool trace_read_next(const struct trace *dt, struct blk_io_trace *t);

/* reader for devices with ata_piix controller */
bool trace_ata_piix_read_next(const struct trace *dt,
				  struct blk_io_trace *t);

/*
 * 0 - default reader
 * 1 - ata_piix reader
 */
#define N_TRCREAD 2
static const trace_reader_t reader[] = { trace_read_next,
					 trace_ata_piix_read_next };

#endif
