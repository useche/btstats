#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/utils.h"

char **str_split(const char *string, const char *delimiter, int max_tokens) {
  if (max_tokens < 1)
    max_tokens = 1; // At least one token
  char *s = strdup(string);
  char **result = malloc(sizeof(char *) * (max_tokens + 1));
  int i = 0;
  char *p = s;
  char *next_p;

  while (i < max_tokens - 1 && (next_p = strstr(p, delimiter))) {
    *next_p = '\0';
    result[i++] = strdup(p);
    p = next_p + strlen(delimiter);
  }
  result[i++] = strdup(p);
  result[i] = NULL;
  free(s);
  return result;
}

void str_freev(char **strv) {
  if (!strv)
    return;
  for (int i = 0; strv[i] != NULL; i++) {
    free(strv[i]);
  }
  free(strv);
}

char *str_strip(char *str) {
  if (!str)
    return NULL;
  char *end;

  // Trim leading space
  while (isspace((unsigned char)*str))
    str++;

  if (*str == 0) // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;

  // Write new null terminator
  *(end + 1) = 0;

  return str;
}

void get_filename(char *buf, const char *prefix, const char *suffix,
                  __u64 num) {
  sprintf(buf, "%s_%s_%llu.dat", prefix, suffix, (long long unsigned int)num);
}
