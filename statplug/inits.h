#include "plugins.h"

/* avoid circular dependecies problems */
struct plugin;
struct plugin_ops;

/* reqsize inits and destroyers */
void reqsize_init(struct plugin *p);
void reqsize_destroy(struct plugin *p);
void reqsize_ops_init(struct plugin_ops *po);
void reqsize_ops_destroy(struct plugin_ops *po);

