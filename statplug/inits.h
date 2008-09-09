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
void reqsize_ops_destroy(struct plugin_ops *po);

/* seek inits and destroyers */
void seek_init(struct plugin *p, struct plugin_set *ps);
void seek_destroy(struct plugin *p);
void seek_ops_init(struct plugin_ops *po);
void seek_ops_destroy(struct plugin_ops *po);

#endif
