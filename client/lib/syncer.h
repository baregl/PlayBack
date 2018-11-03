#ifndef __SYNCER_H_
#define __SYNCER_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// #define DEBUG
#ifdef DEBUG
#include <stdio.h>
#define LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

// The one which is null determines the type
struct dir_entry {
	bool dir;
	// relative
	char *name;
	// 0 if folder
	uint32_t size;
	uint64_t mtime;
};

// Implementation should open tcp connection befor calling syncer_run
// If connection is closed prematurely, display error
void clbk_send(uint8_t *data, uint32_t length);
uint32_t clbk_receive(uint8_t *data, uint32_t length);
// Open file for reading, close previous
// Return -1 if it failed
int clbk_open(char *path);
uint32_t clbk_read(uint8_t *data, uint32_t length);

// The path is absolute & ends with '/'
void *clbk_open_dir(char *path);
void clbk_close_dir(void *dird);
// allocate fodlre_entry statically
// return null for last entry
struct dir_entry *clbk_read_dir(void *dird);
uint32_t clbk_file_size(char *path);

void clbk_delay(uint8_t ms);
// Doesn't return
void clbk_show_error(char *msg);
// The newline is explicitly passed
void clbk_show_status(char *status);
// Should be true randomness, is the protection against replay attacks
void clbk_get_random(uint8_t *data, uint8_t len);

// When returning, close the open file & TCP connection
// Dirs array ends with a NULL pointer
void syncer_run(char *dirs[], char *devname, char *devid, char *ver, char *key);
#endif // __SYNCER_H_
