#include "syncer.h"

#include "config.h"
#include "constants.h"
#include "crypto.h"
#include "murmur3.h"
#include "regexp3.h"
#include "tweetnacl.h"
#include <stdlib.h>
#include <string.h>

void send_header(char *devname, char *ver);
void send_dir(char *dir, char **ignore, char **update);
void handle_transfer(uint8_t *transfer_req, char **base_dirs);
bool check_in_base_dirs(char *dir, char **base_dirs);
bool check_regexps(char *file, char **regexps);
void bytiffy_uint32(uint8_t *dest, uint32_t val);
void bytiffy_uint64(uint8_t *dest, uint64_t val);

void syncer_run(config_data *config, char *devname, char *ver)
{
	// TODO Handle dir ending without '/'
	encrypted_begin_communication(config->name, config->enc_key);
	send_header(devname, ver);
	LOG("Sending entries\n");
	clbk_show_status("Sending entries\n");
	for (int i = 0; config->dirs[i] != NULL; i++)
		send_dir(config->dirs[i], config->ignore, config->update);
	LOG("Sending end of entries\n");
	uint8_t entry[e_padd + entry_size] = {0};
	// Iterate over directory
	entry[e_padd] = 'e';
	encrypted_send(entry, sizeof(entry));
	uint8_t transfer_req[e_padd + transfer_req_size + 1];
	// Just to make sure the 0th byte isn't 'e'
	transfer_req[e_padd] = 0;
	// And make sure it's null terminated
	transfer_req[e_padd + transfer_req_size] = 0;
	do {
		encrypted_receive(transfer_req, e_padd + transfer_req_size);
		LOG("Received request\n");
		handle_transfer(transfer_req + e_padd, config->dirs);
	} while (transfer_req[e_padd] != 'e');
	clbk_show_status("Done\n");
}

void send_header(char *devname, char *ver)
{
	LOG("Assembling header\n");
	uint8_t data[e_padd + 16] = {0};
	strncpy((char *)data + e_padd, devname, 8);
	strncpy((char *)data + e_padd + 8, ver, 8);
	LOG("Sending header\n");
	encrypted_send(data, sizeof(data));
}

void send_dir(char *dir, char **ignore, char **update)
{
	LOG("Opening %s\n", dir);
	void *dird = clbk_open_dir(dir);
	if (dird == NULL) {
		// Sometimes happens
		// Particularly on vita, the ux0:/MUSIC dir can't be opened
		clbk_show_status("Couldn't open dir ");
		clbk_show_status(dir);
		clbk_show_status(". Skipping it.\n");
		return;
	}
	struct dir_entry *dentry;
	while ((dentry = clbk_read_dir(dird)) != NULL) {
		if ((!strcmp(".", dentry->name)) ||
		    (!strcmp("..", dentry->name)))
			continue;
		LOG("processing entry %s\n", dentry->name);
		// So we can have a final \0
		static uint8_t entry[e_padd + entry_size + 1] = {0};
		// The upper bytes are 0
		bytiffy_uint32(e_padd + entry,
			       (uint32_t)(dentry->dir ? 'd' : 'f'));
		LOG("with size: %li, mtime %lli\n", dentry->size,
		    dentry->mtime);
		bytiffy_uint32(e_padd + entry + 4, dentry->size);
		bytiffy_uint64(e_padd + entry + 8, dentry->mtime);
		uint16_t newlen = strlen(dir) + strlen(dentry->name) + 1;
		if (newlen > 256)
			clbk_show_error("Filepath too long");
		strcpy((char *)(e_padd + entry + 16), dir);
		strcpy((char *)(e_padd + entry + 16 + strlen(dir)),
		       dentry->name);
		// Zero out the rest
		for (int i = e_padd + newlen + 16; i < sizeof(entry); i++)
			entry[i] = 0;
		LOG("Full path is %s, type %c\n", e_padd + entry + 16,
		    entry[e_padd]);
		if (check_regexps(e_padd + entry + 16, ignore)) {
			LOG("Skipping %s\n", e_padd + entry + 16);
			continue;
		} else if (check_regexps(e_padd + entry + 16, update)) {
			LOG("Force updating %s\n", e_padd + entry + 16);
			bytiffy_uint64(e_padd + entry + 8, 0);
		}
		encrypted_send(entry, e_padd + entry_size);
		if (dentry->dir) {
			// TODO Different types of allocations if vlas
			// aren't supported
			char path[newlen + 1];
			memcpy(path, entry + 16 + e_padd, newlen);
			// TODO Remove plattform specific file separator
			path[newlen - 1] = '/';
			path[newlen] = 0;
			send_dir(path, ignore, update);
		}
	}
	clbk_close_dir(dird);
}

void handle_transfer(uint8_t *transfer_req, char *base_dirs[])
{
	if (transfer_req[0] == 'f' || transfer_req[0] == 'h') {
		if (check_in_base_dirs((char *)transfer_req + 1, base_dirs) ==
		    false) {
			clbk_show_status((char *)transfer_req + 1);
			clbk_show_error(" not within base dir");
		}
		// Just so this isn't stack allocated
		static uint8_t transfer_buffer[e_padd + transfer_size] = {0};
		uint32_t size = clbk_file_size((char *)transfer_req + 1);
		LOG("File size is %li\n", size);
		if (transfer_req[0] == 'f') {
			bytiffy_uint32(transfer_buffer + e_padd, size);
			encrypted_send(transfer_buffer, e_padd + 4);
			LOG("Sent file size\n");
		}
		if (clbk_open((char *)transfer_req + 1) == -1) {
			clbk_show_status("Couldn't open ");
			clbk_show_status((char *)transfer_req + 1);
			clbk_show_error("");
		}
		uint32_t read;
		uint32_t h = 0;
		if (transfer_req[0] == 'h')
			clbk_show_status("Hashing ");
		else
			clbk_show_status("Sending ");
		clbk_show_status((char *)transfer_req + 1);
		clbk_show_status("\n");
		// TODO This will break if the file was swapped out under us.
		// For now, this is probably sufficient
		while ((read = clbk_read(transfer_buffer + e_padd,
					 transfer_size)) == transfer_size) {
			h = murmur3_32_step(h, transfer_buffer + e_padd, read);
			if (transfer_req[0] == 'f') {
				// The first ZEROBYTES are already zero
				encrypted_send(transfer_buffer, e_padd + read);
			}
		}
		// printf("Left %i\n", read);
		h = murmur3_32_finalize(h, transfer_buffer + e_padd, read,
					size);
		if (transfer_req[0] == 'f') {
			if (read != 0)
				encrypted_send(transfer_buffer, e_padd + read);
			LOG("Sent file %s\n", (char *)transfer_req + 1);
		}
		bytiffy_uint32(transfer_buffer + e_padd, h);
		encrypted_send(transfer_buffer, e_padd + 4);
		LOG("Sent checksum %lx\n", h);
	}
}

bool check_in_base_dirs(char *dir, char **base_dirs)
{
	for (int i = 0; base_dirs[i] != NULL; i++)
		if (strncmp((char *)dir, base_dirs[i], strlen(base_dirs[i])) ==
		    0)
			return true;
	return false;
}

bool check_regexps(char *file, char **regexps)
{
	for (int i = 0; regexps[i] != NULL; i++)
		if (regexp3(file, regexps[i]) != 0)
			return true;
	return false;
}

// tweetnacl wants a randombyte function, which isn't needed for our
// purposes
void randombytes(uint8_t *a, uint64_t b)
{
	clbk_show_error("called randombytes, usually not needed, just here for "
			"linking errors");
}

void bytiffy_uint32(uint8_t *dest, uint32_t val)
{
	(dest)[0] = (val)&0xFF;
	(dest)[1] = (val >> 8) & 0xFF;
	(dest)[2] = (val >> 16) & 0xFF;
	(dest)[3] = (val >> 24) & 0xFF;
}

void bytiffy_uint64(uint8_t *dest, uint64_t val)
{
	bytiffy_uint32(dest, val & 0xFFFFFFFF);
	bytiffy_uint32(dest, (val >> 32) & 0xFFFFFFFF);
}
