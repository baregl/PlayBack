#ifndef __CONFIG_H_
#define __CONFIG_H_
#include <stdint.h>

// Returns -1 for config errors, -2 if it is too long, 0 for success
int config_parse(char *file);

void clbk_config_entry(char *key, char *val);
#endif // __CONFIG_H_
