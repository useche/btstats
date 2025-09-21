#ifndef _TRACE_H_
#define _TRACE_H_

#include "include/bsd/queue.h"
#include <blktrace_api.h>

struct trace_file {
  struct blk_io_trace t;
  int fd;
  int eof;
  SLIST_ENTRY(trace_file) entries;
};

SLIST_HEAD(trace_file_list, trace_file);

struct trace {
  struct trace_file_list *files;
  __u64 genesis;
};

typedef int (*trace_reader_t)(const struct trace *, struct blk_io_trace *);

/* constructor and destructor */
struct trace *trace_create(const char *dev);
void trace_destroy(struct trace *dt);

/* default trace reader */
int trace_read_next(const struct trace *dt, struct blk_io_trace *t);

/* reader for devices with ata_piix controller */
int trace_ata_piix_read_next(const struct trace *dt, struct blk_io_trace *t);

/*
 * 0 - default reader
 * 1 - ata_piix reader
 */
#define N_TRCREAD 2
static const trace_reader_t reader[] = {trace_read_next,
                                        trace_ata_piix_read_next};

#endif
