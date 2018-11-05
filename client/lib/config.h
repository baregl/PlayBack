#ifndef __CONFIG_H_
#define __CONFIG_H_
#include <stdint.h>

typedef struct {
	char *name;
	char *server;
	char *enc_key;
	char **dirs;
	int dirs_len;
} config_data;

//  Returns -1 for config errors, -2 if it is too long, 0 for success
config_data *config_parse(char *file);

// Return 0 if found, -1 otherwise
// The value is already on the heap
int clbk_config_entry(char *key, char *val);

#endif // __CONFIG_H_
