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

#ifndef _LIST_PLUGINS_H_
#define _LIST_PLUGINS_H_

#include <plugins.h>
#include <inits.h>

/* list of initilizers and destroyers for each function */
#define N_PLUGINS 5
enum {
	REQ_SIZE_IND = 0,
	SEEK_IND,
	D2C_IND,
	MERGE_IND,
	PLUGING_IND
};
static const struct plug_init_dest_funcs plug_init_dest[] =
{
	{
		.init = reqsize_init, 
		.destroy = reqsize_destroy, 
		.ops_init = reqsize_ops_init, 
		.ops_destroy = NULL
	},
	{
		.init = seek_init,
		.destroy = seek_destroy,
		.ops_init = seek_ops_init,
		.ops_destroy = NULL
	},
	{
		.init = d2c_init,
		.destroy = d2c_destroy,
		.ops_init = d2c_ops_init,
		.ops_destroy = NULL
	},
	{
		.init = merge_init,
		.destroy = merge_destroy,
		.ops_init = merge_ops_init,
		.ops_destroy = NULL
	},
	{
		.init = pluging_init,
		.destroy = pluging_destroy,
		.ops_init = pluging_ops_init,
		.ops_destroy = NULL
	}
};

#endif
