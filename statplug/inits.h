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

#ifndef _INITS_H_
#define _INITS_H_

#include <plugins.h>

/* avoid circular dependecies problems */
struct plugin;
struct plugin_ops;
struct plugin_set;
struct plug_args;

/* reqsize inits and destroyers */
void reqsize_init(struct plugin *p, struct plugin_set *ps, struct plug_args *pia);
void reqsize_destroy(struct plugin *p);
void reqsize_ops_init(struct plugin_ops *po);

/* seek inits and destroyers */
void seek_init(struct plugin *p, struct plugin_set *ps, struct plug_args *pia);
void seek_destroy(struct plugin *p);
void seek_ops_init(struct plugin_ops *po);

/* d2c inits and destroyers */
void d2c_init(struct plugin *p, struct plugin_set *ps, struct plug_args *pia);
void d2c_destroy(struct plugin *p);
void d2c_ops_init(struct plugin_ops *po);

/* merge inits and destroyers */
void merge_init(struct plugin *p, struct plugin_set *ps, struct plug_args *pia);
void merge_destroy(struct plugin *p);
void merge_ops_init(struct plugin_ops *po);

/* pluging inits and destroyers */
void pluging_init(struct plugin *p, struct plugin_set *ps, struct plug_args *pia);
void pluging_destroy(struct plugin *p);
void pluging_ops_init(struct plugin_ops *po);

#endif
