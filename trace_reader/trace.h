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

#ifndef _TRACE_H_
#define _TRACE_H_

#include <blktrace_api.h>
#include <glib.h>

struct trace_file 
{
	struct blk_io_trace t;
	int fd;
	gboolean eof;
};

struct trace 
{
	GSList *files;
	__u64 genesis;
};

typedef gboolean (*trace_reader_t)(const struct trace *, struct blk_io_trace *);

/* constructor and destructor */
struct trace *trace_create(const char *dev);
void trace_destroy(struct trace *dt);

/* default trace reader */
gboolean trace_read_next(const struct trace *dt, struct blk_io_trace *t);

/* reader for devices with ata_piix controller */
gboolean trace_ata_piix_read_next(const struct trace *dt, struct blk_io_trace *t);

/*
 * 0 - default reader
 * 1 - ata_piix reader
 */
#define N_TRCREAD 2
static const trace_reader_t reader[] =
{
	trace_read_next,
	trace_ata_piix_read_next
};

#endif
