APP=btstats

include statplug/Makefile.plugs

DEV_TRACE=dev_trace.o
APP_DEP=$(APP).o $(PLUGS) statplug/plugins.o $(DEV_TRACE)

SRCS=dev_trace.c statplug/plugins.c $(APP).c $(PLUG_SRCS)

CFLAGS=-Wall -Wextra `pkg-config --cflags glib-2.0` -I. -Istatplug/ -Iinclude/ -std=c99 -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
LDFLAGS=`pkg-config --libs glib-2.0`
CC=gcc

#TODO depend to be added
$(APP): $(APP_DEP)
	$(CC) $(LDFLAGS) $(APP_DEP) -o $(APP)

depend:
	makedepend -- $(CFLAGS) -- $(SRCS)

clean:
	rm -rf $(APP) $(APP_DEP)

