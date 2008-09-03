APP=btstats

include statplug/Makefile.plugs

DEV_TRACE=dev_trace.o
APP_DEP=$(APP).o $(PLUGS) statplug/plugins.o $(DEV_TRACE)

SRCS=dev_trace.c statplug/plugins.c $(APP).c $(PLUG_SRCS)

CFLAGS=-Wall -Wextra -pedantic `pkg-config --cflags glib-2.0` -I. -Istatplug/ -Iinclude/ -std=c99
LDFLAGS=`pkg-config --libs glib-2.0`
CC=gcc

$(APP): $(APP_DEP)
	$(CC) $(CFLAGS) $(LDFLAGS) $(APP_DEP) -o $(APP)

depend:
	makedepend -- $(CFLAGS) -- $(SRCS)

clean:
	rm -rf $(APP) $(APP_DEP)

