APP=btstats
APP_O=$(APP).o

include statplug/Makefile.plugs

APP_DEP=$(APP).o $(PLUGS) dev_trace.o
SRCS=$(APP).c $(PLUG_SRCS) dev_trace.c

INCLUDE=`pkg-config --cflags glib-2.0` -I. -Istatplug/ -Iinclude/
CFLAGS=-Wall -Wextra -ggdb -std=gnu99 $(INCLUDE) -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
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
