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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#define MAX_FILE_SIZE 256
#define MAX_HEAD 256
#define TENT_OUTS_RQS 32

/* perror and exit */
#define perror_exit(msg)			\
	do {					\
		perror(msg);			\
		exit(EXIT_FAILURE);		\
	} while(0)

#define error_exit(msg)				\
	do {					\
		fprintf(stderr,msg);		\
		exit(EXIT_FAILURE);		\
	} while(0)

#define DECL_DUP(type,name,src)			\
	type *name = g_new(type,1);		\
	memcpy(name,src,sizeof(type));		\
	

inline static int comp_int(gconstpointer a,gconstpointer b) 
{
	int x = (int)a;
	int y = (int)b;
	
	return x-y;
}

inline static int comp_int64(gconstpointer a,gconstpointer b) 
{
	__u64 x = *((__u64 *)a);
	__u64 y = *((__u64 *)b);
	
	if(x==y)
		return 0;		
	else
		return x>y?1:-1;		
}
