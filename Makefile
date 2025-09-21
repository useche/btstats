APP=btstats
APP_O=$(APP).o

# All .c files in statplug directory are considered plugins.
PLUG_SRCS = $(wildcard statplug/*.c)
PLUGS = $(patsubst %.c,%.o,$(PLUG_SRCS))

# .c in trace reader
TRCREAD_SRCS = $(wildcard trace_reader/*.c)
TRCREAD = $(patsubst %.c,%.o,$(TRCREAD_SRCS))

# other sources
OTHER_SRCS = utils.c vector.c hash.c sector_tree.c tree.c
OTHER_OBJS = $(patsubst %.c,%.o,$(OTHER_SRCS))

APP_DEP=$(APP).o $(PLUGS) $(TRCREAD) $(OTHER_OBJS)
SRCS=$(APP).c $(PLUG_SRCS) $(TRCREAD_SRCS) $(OTHER_SRCS)

COMPILER=gcc

# If DEBUG defined, then -ggdb used
ifdef DEBUG
OPT_OR_DBG = -g3
CC=${COMPILER}
else
OPT_OR_DBG = -O3
define CC
        @echo " [CC] $@" && ${COMPILER}
endef
endif

INCLUDE=-I. -Istatplug/ -Iinclude/ -Itrace_reader/
CFLAGS=-Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 $(OPT_OR_DBG) $(INCLUDE) -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
LDFLAGS=-lgsl -lgslcblas -lm

all: depend $(APP)

$(APP): | depend

$(APP): $(APP_DEP)
	$(CC) $(APP_DEP) $(LDFLAGS) -o $@

clean:
	rm -rf $(APP) $(APP_DEP) .depend

depend:
	@$(CC) -MM $(CFLAGS) $(SRCS) 1> .depend

ifneq ($(wildcard .depend),)
include .depend
endif
