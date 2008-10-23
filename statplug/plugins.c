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

#include <stdlib.h>
#include <stdio.h>

#include <plugins.h>
#include <list_plugins.h>

#include <utils.h>

/* array of operations and function initializer */
struct plugin_ops ps_ops[N_PLUGINS];

struct plugin_set *plugin_set_create(struct plug_args *pia)
{
	int i;
	
	struct plugin_set *tmp = g_new(struct plugin_set, 1);
	tmp->plugs = g_new(struct plugin, N_PLUGINS);
	tmp->n = N_PLUGINS;
	
	/* create and initilize a new set of plugins */
	for(i = 0; i < N_PLUGINS; ++i) {
		plug_init_dest[i].init(&tmp->plugs[i], tmp, pia);
		tmp->plugs[i].ops = &ps_ops[i];
	}
	
	return tmp;
}
	
void plugin_set_destroy(struct plugin_set *ps) 
{
	int i;
	
	/* destroy the plugins in the plugin set */
	for(i = 0; i < N_PLUGINS; ++i)
		plug_init_dest[i].destroy(&ps->plugs[i]);
	
	g_free(ps->plugs);
	g_free(ps);
}

void plugin_set_print(const struct plugin_set *ps, const char *head) 
{
	int i;
	
	printf("%s\t=====================================\n",head);
	for(i = 0; i < N_PLUGINS; ++i)
		ps->plugs[i].ops->print_results(ps->plugs[i].data);
}

void plugin_set_add_trace(struct plugin_set *ps, const struct blk_io_trace *t) 
{
	int i;
	int act;
	event_func_t event_handler;
	struct plugin *p;
	
	act = t->action & 0xffff;
	for(i = 0; i < N_PLUGINS; ++i) {
		p = &ps->plugs[i];
		event_handler = g_tree_lookup(p->ops->event_tree,(gpointer)act);
		if(event_handler)
			event_handler(t,p->data);
	}
}

void plugin_set_add(struct plugin_set *ps1, const struct plugin_set *ps2) 
{
	int i;
	struct plugin *p1, *p2;
	
	for(i = 0; i < N_PLUGINS; ++i) {
		p1 = &ps1->plugs[i];
		p2 = &ps2->plugs[i];
		p1->ops->add(p1->data, p2->data);
	}
}

void init_plugs_ops() 
{
	int i;	
	for(i = 0; i < N_PLUGINS; ++i) {
		ps_ops[i].event_tree = g_tree_new(comp_int);
		if(plug_init_dest[i].ops_init)
			plug_init_dest[i].ops_init(&ps_ops[i]);
	}
}

void destroy_plugs_ops() 
{
	int i;	
	for(i = 0; i < N_PLUGINS; ++i) {
		if(plug_init_dest[i].ops_destroy)
			plug_init_dest[i].ops_destroy(&ps_ops[i]);
		g_tree_destroy(ps_ops[i].event_tree);
	}
}

