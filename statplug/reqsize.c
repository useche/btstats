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

#include <asm/types.h>
#include <glib.h>
#include <stdio.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>
#include <utils.h>

#include <reqsize.h>

static void C(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_REQSIZE(rsd,data);
	
	__u64 blks = t_blks(t);
	
	if(blks) {
		rsd->min = MIN(rsd->min, blks);
		rsd->max = MAX(rsd->max, blks);
		rsd->total_size += blks;
		rsd->reqs++;
	}
}

void reqsize_add(void *data1, const void *data2) 
{
	DECL_ASSIGN_REQSIZE(rsd1,data1);
	DECL_ASSIGN_REQSIZE(rsd2,data2);
	
	rsd1->min = MIN(rsd1->min,rsd2->min);
	rsd1->max = MAX(rsd1->max,rsd2->max);
	rsd1->total_size += rsd2->total_size;
	rsd1->reqs += rsd2->reqs;
}

void reqsize_print_results(const void *data)
{
	DECL_ASSIGN_REQSIZE(rsd,data);

	if(rsd->reqs)
		printf("Reqs. #: %lld min: %lld avg: %f max: %lld (blocks)\n",
		       rsd->reqs,
		       rsd->min,
		       ((double)rsd->total_size)/rsd->reqs,
		       rsd->max);
}

void reqsize_init(struct plugin *p, struct plugin_set *__unused)
{
	struct reqsize_data *req = p->data = g_new(struct reqsize_data,1);
	req->min = ~0;
	req->max = 0;
	req->total_size = 0;
	req->reqs = 0;
	
	__unused = NULL; /* to make gcc quite */
}

void reqsize_ops_init(struct plugin_ops *po)
{
	po->add = reqsize_add;
	po->print_results = reqsize_print_results;
	
	/* association of event int and function */
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_COMPLETE,C);
}

void reqsize_destroy(struct plugin *p)
{
	g_free(p->data);
}
