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
#include <stdio.h>

#include <blktrace_api.h>
#include <blktrace.h>
#include <plugins.h>

#define DECL_ASSIGN_PLUGING(name,data)				\
	struct pluging_data *name = (struct pluging_data *)data

struct pluging_data
{
	__u64 min;
	__u64 max;
	__u64 total;
	__u64 nplugs;
	
	__u64 plug_time;
	gboolean plugged;
};

static void P(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_PLUGING(plug,data);

	if(!plug->plugged) {
		plug->plug_time = t->time;
		plug->plugged = TRUE;
	}
}

static void U(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_PLUGING(plug,data);
	
	if(plug->plugged) {
		__u64 time = t->time - plug->plug_time;
		
		plug->nplugs++;
		plug->total += time;
		
		plug->min = MIN(plug->min,time);
		plug->max = MAX(plug->max,time);
		
		plug->plugged = FALSE;
		plug->plug_time = 0;
	}
}

void pluging_add(void *data1, const void *data2)
{
	DECL_ASSIGN_PLUGING(plug1,data1);
	DECL_ASSIGN_PLUGING(plug2,data2);
	
	plug1->min = MIN(plug1->min,plug2->min);
	plug1->max = MAX(plug1->max,plug2->max);
	plug1->total += plug2->total;
	plug1->nplugs += plug2->nplugs;
}

void pluging_print_results(const void *data)
{
	DECL_ASSIGN_PLUGING(plug,data);
	
	if(plug->nplugs)
		printf("Plug Time Min: %f Avg: %f Max: %f (sec)\n",
		       NANO_ULL_TO_DOUBLE(plug->min),
		       NANO_ULL_TO_DOUBLE(plug->total)/plug->nplugs,
		       NANO_ULL_TO_DOUBLE(plug->max));
	else
		printf("No plugging in this range\n");
}

void pluging_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *__un2)
{
	struct pluging_data *plug = p->data = g_new(struct pluging_data,1);
	
	plug->min = ~0;
	plug->max = 0;
	plug->total = 0;
	plug->nplugs = 0;
	plug->plug_time = 0;
	
	__un1 = NULL; __un2 = NULL;
}

void pluging_ops_init(struct plugin_ops *po)
{
	po->add = pluging_add;
	po->print_results = pluging_print_results;
	
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_PLUG,P);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_UNPLUG_IO,U);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_UNPLUG_TIMER,U);
}

void pluging_destroy(struct plugin *p)
{
	g_free(p->data);
}
