APP=seg_pat_size
APP_DEP=$(APP).o
CFLAGS=-Wall -Wextra -pedantic `pkg-config --cflags glib-2.0` -std=c99 -fgnu89-inline
LDFLAGS=-lgsl -lgslcblas -laio -lrt `pkg-config --libs glib-2.0`
CC=gcc

$(APP): $(APP_DEP)
	$(CC) $(CFLAGS) $(LDFLAGS) $(APP_DEP) -o $(APP)

clean:
	rm -rf $(APP) $(APP_DEP)
