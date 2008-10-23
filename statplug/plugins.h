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

#ifndef _PLUGINS_H_
#define _PLUGINS_H_

#include <glib.h>
#include <blktrace_api.h>
#include <inits.h>

typedef void (*event_func_t)(const struct blk_io_trace *, void *);
struct plugin_ops 
{
	/* hash table with key = int of event,
	   value = the function to call */
	GTree *event_tree;

	/* additional functions */
	void (*add)(void *data1, const void *data2);
	void (*print_results)(const void *data);	
};

struct plugin
{
	/* private data per plugin */
	void *data;
	
	/* ops */
	struct plugin_ops *ops;
};

/* functions to create & destroy a plugin set */
struct plugin_set 
{
	struct plugin *plugs;
	int n;
};

struct plug_args
{
	char *d2c_file_detail;
};

struct plug_init_dest_funcs
{
	/* init, destroy */
	void (*init)(struct plugin *p,
		struct plugin_set *ps,
		struct plug_args *pia);
	void (*destroy)(struct plugin *p);
	void (*ops_init)(struct plugin_ops *po);
	void (*ops_destroy)(struct plugin_ops *po);
};

void init_plugs_ops();
void destroy_plugs_ops();

/* plugin set methods */
struct plugin_set *plugin_set_create(struct plug_args *pia);
void plugin_set_destroy(struct plugin_set *ps);
void plugin_set_print(const struct plugin_set *ps, const char *head);
void plugin_set_add_trace(struct plugin_set *ps, const struct blk_io_trace *t);
void plugin_set_add(struct plugin_set *ps1, const struct plugin_set *ps2);

#endif
