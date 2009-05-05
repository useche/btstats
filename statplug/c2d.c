/*
 * Author: Luis Useche (May 2009)
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

#define NOT_NUM	(~(0U))

#define DECL_ASSIGN_C2D(name,data)		\
	struct c2d_data *name = (struct c2d_data *)data

struct c2d_data 
{
	__u64 min;
	__u64 max;
	__u64 total;
	__u32 total_gaps;

	__u32 outstanding;
	__u64 last_C;

	__u64 prospect_time;

	struct reqsize_data *req_dat;
};

static void D(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_C2D(c2d,data);

	if(c2d->outstanding++ == 0 && c2d->last_C != NOT_NUM)
		c2d->prospect_time = t->time - c2d->last_C;
}

static void R(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_C2D(c2d,data);
	t = NULL;

	if(--c2d->outstanding == 0)
		c2d->prospect_time = NOT_NUM;
}

static void C(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_C2D(c2d,data);

	if(c2d->outstanding > 0 && --c2d->outstanding == 0) {
		c2d->last_C = t->time;
		if(c2d->prospect_time != NOT_NUM) {
			c2d->total += c2d->prospect_time;
			c2d->min = MIN(c2d->min,c2d->prospect_time);
			c2d->max = MAX(c2d->max,c2d->prospect_time);
			c2d->prospect_time = NOT_NUM;
			c2d->total_gaps++;
		}
	}
}

void c2d_add(void *data1, const void *data2) 
{
	DECL_ASSIGN_C2D(c2d1,data1);
	DECL_ASSIGN_C2D(c2d2,data2);
	
	c2d1->total += c2d2->total;
	c2d1->total_gaps += c2d2->total_gaps;
	c2d1->min = MIN(c2d1->min,c2d2->min);
	c2d1->max = MAX(c2d1->max,c2d2->max);
}

void c2d_print_results(const void *data)
{
	DECL_ASSIGN_C2D(c2d,data);
	
	if(c2d->total)
		printf("C2D Total: %f min: %f avg: %f max: %f (sec)\n",
		       NANO_ULL_TO_DOUBLE(c2d->total),
		       NANO_ULL_TO_DOUBLE(c2d->min),
		       NANO_ULL_TO_DOUBLE(c2d->total)/c2d->total_gaps,
		       NANO_ULL_TO_DOUBLE(c2d->max));
	else
		printf("C2D Total: 0\n");
}

void c2d_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *__un2)
{
	struct c2d_data *c2d = p->data = g_new0(struct c2d_data,1);
	c2d->min = NOT_NUM;
	c2d->last_C = NOT_NUM;
	c2d->prospect_time = NOT_NUM;

	__un1 = NULL; __un2 = NULL; /* to make gcc quite */
}

void c2d_ops_init(struct plugin_ops *po)
{
	po->add = c2d_add;
	po->print_results = c2d_print_results;
	
	/* association of event int and function */
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_ISSUE,D);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_COMPLETE,C);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_REQUEUE,R);
}

void c2d_destroy(struct plugin *p)
{
	g_free(p->data);
}
