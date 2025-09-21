#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h> // for __u64
#include <asm/types.h>

#define error_exit(format, ...) \
  do { \
    fprintf(stderr, format, ##__VA_ARGS__); \
    exit(EXIT_FAILURE); \
  } while(0)

#define perror_exit(s) \
  do { \
    perror(s); \
    exit(EXIT_FAILURE); \
  } while(0)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define DECL_DUP(type, new, old) type *new = malloc(sizeof(type)); memcpy(new, old, sizeof(type))

char **str_split(const char *str, const char *delimiter, int max_tokens);
void str_freev(char **strv);
char *str_strip(char *str);
void get_filename(char *buf, const char *prefix, const char *suffix, __u64 num);

#endif
