#ifndef _INITS_H_
#define _INITS_H_

#include <plugins.h>

/* avoid circular dependecies problems */
struct plugin;
struct plugin_ops;
struct plugin_set;

/* reqsize inits and destroyers */
void reqsize_init(struct plugin *p, struct plugin_set *ps);
void reqsize_destroy(struct plugin *p);
void reqsize_ops_init(struct plugin_ops *po);

/* seek inits and destroyers */
void seek_init(struct plugin *p, struct plugin_set *ps);
void seek_destroy(struct plugin *p);
void seek_ops_init(struct plugin_ops *po);

/* d2c inits and destroyers */
void d2c_init(struct plugin *p, struct plugin_set *ps);
void d2c_destroy(struct plugin *p);
void d2c_ops_init(struct plugin_ops *po);

/* merge inits and destroyers */
void merge_init(struct plugin *p, struct plugin_set *ps);
void merge_destroy(struct plugin *p);
void merge_ops_init(struct plugin_ops *po);

/* pluging inits and destroyers */
void pluging_init(struct plugin *p, struct plugin_set *ps);
void pluging_destroy(struct plugin *p);
void pluging_ops_init(struct plugin_ops *po);

#endif
