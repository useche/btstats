# Author: Luis Useche (August 2008)
# Email: luis@cs.fiu.edu
# 
# BSD License
# Copyright (c) 2008, Luis Useche
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials
#       provided with the distribution.
#
#     * Neither the name of Luis Useche nor the names of its
#       contributors may be used to endorse or promote products
#       derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

APP=btstats
APP_O=$(APP).o

# All .c files in statplug directory are considered plugins.
PLUGS = $(patsubst %.c,%.o,$(wildcard statplug/*.c))
PLUG_SRCS = $(wildcard statplug/*.c)

APP_DEP=$(APP).o $(PLUGS) dev_trace.o
SRCS=$(APP).c $(PLUG_SRCS) dev_trace.c

# If DEBUG defined, then -ggdb used
ifdef DEBUG
OPT_OR_DBG = -ggdb
else
OPT_OR_DBG = -O3
endif

INCLUDE=`pkg-config --cflags glib-2.0` -I. -Istatplug/ -Iinclude/
CFLAGS=-Wall -Wextra -std=gnu99 $(OPT_OR_DBG) $(INCLUDE) -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
LDFLAGS=`pkg-config --libs glib-2.0`
CC=gcc

all: depend $(APP)

$(APP): | depend

$(APP): $(APP_DEP)
	$(CC) $(LDFLAGS) $(APP_DEP) -o $@

clean:
	rm -rf $(APP) $(APP_DEP) .depend

depend:
	@$(CC) -MM $(CFLAGS) $(SRCS) 1> .depend

ifneq ($(wildcard .depend),)
include .depend
endif
