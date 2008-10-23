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

#include <stdio.h>
#include <asm/types.h>

#include <plugins.h>
#include <blktrace_api.h>

#define DECL_ASSIGN_MERGE(name,data)				\
	struct merge_data *name = (struct merge_data *)data

struct merge_data
{
	__u64 ms;
	__u64 fs;
	__u64 ins;
};

static void M(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_MERGE(m,data);

	t = NULL;

	if(m->ins)
		m->ms++;
}

static void F(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_MERGE(m,data);
	
	t = NULL;
	
	if(m->ins)
		m->fs++;	
}

static void I(struct blk_io_trace *t, void *data)
{
	DECL_ASSIGN_MERGE(m,data);
	
	t = NULL;
	
	m->ins++;
}

void merge_add(void *data1, const void *data2)
{
	DECL_ASSIGN_MERGE(m1,data1);
	DECL_ASSIGN_MERGE(m2,data2);
	
	m1->ms += m2->ms;
	m1->fs += m2->fs;
	m1->ins += m2->ins;
}

void merge_print_results(const void *data)
{
	DECL_ASSIGN_MERGE(m,data);
	
	if(m->ins) {
		printf("#I: %llu #F+#M: %llu ratio: %f\n",
		       m->ins,
		       m->fs + m->ms,
		       ((double) m->fs + m->ms + m->ins)/m->ins);
	} else
		printf("#I: 0\n");
}

void merge_init(struct plugin *p, struct plugin_set *__un1, struct plug_args *__un2)
{
	struct merge_data *m = p->data = g_new(struct merge_data,1);
	m->ms = m->fs = m->ins = 0;
	
	__un1 = NULL; __un2 = NULL; /* make gcc quite */
}

void merge_ops_init(struct plugin_ops *po)
{
	po->add = merge_add;
	po->print_results = merge_print_results;
	
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_BACKMERGE,M);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_FRONTMERGE,F);
	g_tree_insert(po->event_tree,(gpointer)__BLK_TA_INSERT,I);
}

void merge_destroy(struct plugin *p)
{
	g_free(p->data);
}
