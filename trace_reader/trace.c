#include <trace.h>

#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <asm/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>

#include "utils.h"
#include <blktrace.h>
#include <blktrace_api.h>

#define CORRECT_ENDIAN(v)                                                      \
  do {                                                                         \
    if (sizeof(v) == sizeof(__u32))                                            \
      v = be32_to_cpu(v);                                                      \
    else if (sizeof(v) == sizeof(__u64))                                       \
      v = be64_to_cpu(v);                                                      \
    else                                                                       \
      error_exit("Wrong endian conversion");                                   \
  } while (0)

static int native_trace = -1;

void min_time(struct trace_file *tf, struct trace_file **mintf) {
  if (!tf->eof) {
    if (!(*mintf)) {
      *mintf = tf;
    } else {
      if (tf->t.time < (*mintf)->t.time)
        *mintf = tf;
    }
  }
}

void correct_time(struct trace_file *tf, struct trace *dt) {
  tf->t.time -= dt->genesis;
}

int not_real_event(struct blk_io_trace *t) {
  return (t->action & BLK_TC_ACT(BLK_TC_NOTIFY)) ||
         (t->action & BLK_TC_ACT(BLK_TC_DISCARD)) ||
         (t->action & BLK_TC_ACT(BLK_TC_DRV_DATA));
}

void read_next(struct trace_file *tf, __u64 genesis) {
  int e;

  do {
    e = read(tf->fd, &tf->t, sizeof(struct blk_io_trace));
    if (e == 0) {
      tf->eof = 1;
      break;
    } else if (e == -1 || e != sizeof(struct blk_io_trace)) {
      perror_exit("Reading trace\n");
    } else {
      /* verify trace and check endianess */
      if (native_trace < 0)
        native_trace = check_data_endianness(tf->t.magic);

      assert(native_trace >= 0);
      if (!native_trace) {
        CORRECT_ENDIAN(tf->t.magic);
        CORRECT_ENDIAN(tf->t.sequence);
        CORRECT_ENDIAN(tf->t.time);
        CORRECT_ENDIAN(tf->t.sector);
        CORRECT_ENDIAN(tf->t.bytes);
        CORRECT_ENDIAN(tf->t.action);
        CORRECT_ENDIAN(tf->t.pid);
        CORRECT_ENDIAN(tf->t.device);
        CORRECT_ENDIAN(tf->t.cpu);
        CORRECT_ENDIAN(tf->t.error);
        CORRECT_ENDIAN(tf->t.pdu_len);
      }

      if (verify_trace(&tf->t))
        error_exit("Bad trace!\n");

      /* updating to relative time right away */
      tf->t.time -= genesis;

      if (tf->t.pdu_len) {
        e = lseek(tf->fd, tf->t.pdu_len, SEEK_CUR);
        if (e == -1)
          perror_exit("Skipping pdu");
      }
    }
  } while (not_real_event(&tf->t));
}

void find_input_traces(struct trace *trace, const char *dev) {
  struct dirent *d;
  char pre_trace[FILENAME_MAX];
  char file_path[FILENAME_MAX];

  struct trace_file *tf;
  struct trace_file *min = NULL;

  char *basen, *dirn;
  char *basec = strdup(dev);
  char *dirc = strdup(dev);

  basen = basename(basec);
  dirn = dirname(dirc);
  DIR *cur_dir = opendir(dirn);

  if (!cur_dir)
    perror_exit("Opening dir");

  sprintf(pre_trace, "%s.blktrace.", basen);
  while ((d = readdir(cur_dir))) {
    if (strstr(d->d_name, pre_trace) == d->d_name) {
      tf = malloc(sizeof(struct trace_file));
      SLIST_INSERT_HEAD(trace->files, tf, entries);

      sprintf(file_path, "%s/%s", dirn, d->d_name);

      tf->fd = open(file_path, O_RDONLY);
      if (tf->fd < 0)
        perror_exit("Opening tracefile");

      tf->eof = 0;

      read_next(tf, 0);
    }
  }

  int count = 0;
  SLIST_FOREACH(tf, trace->files, entries) { count++; }
  if (count == 0)
    error_exit("No such traces: %s\n", dev);

  SLIST_FOREACH(tf, trace->files, entries) { min_time(tf, &min); }
  trace->genesis = min->t.time;
  SLIST_FOREACH(tf, trace->files, entries) { correct_time(tf, trace); }

  closedir(cur_dir);
  free(basec);
  free(dirc);
}

struct trace *trace_create(const char *dev) {
  struct trace *dt = malloc(sizeof(struct trace));
  dt->files = malloc(sizeof(struct trace_file_list));
  SLIST_INIT(dt->files);
  find_input_traces(dt, dev);

  return dt;
}

void free_data(struct trace_file *tf) {
  close(tf->fd);
  free(tf);
}

void trace_destroy(struct trace *dt) {
  struct trace_file *tf;
  while (!SLIST_EMPTY(dt->files)) {
    tf = SLIST_FIRST(dt->files);
    SLIST_REMOVE_HEAD(dt->files, entries);
    free_data(tf);
  }
  free(dt->files);
  free(dt);
}

int trace_read_next(const struct trace *dt, struct blk_io_trace *t) {
  struct trace_file *min = NULL;
  struct trace_file *tf;

  SLIST_FOREACH(tf, dt->files, entries) { min_time(tf, &min); }

  if (!min)
    return 0;
  else {
    *t = min->t;
    read_next(min, dt->genesis);
    return 1;
  }
}
