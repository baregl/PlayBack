#include "syncer.h"

#include "constants.h"
#include "murmur3.h"
#include <stdlib.h>
#include <string.h>

#define bytiffy_uint32(dest, value)                                            \
	(dest)[0] = (value)&0xFF;                                              \
	(dest)[1] = (value >> 8) & 0xFF;                                       \
	(dest)[2] = (value >> 16) & 0xFF;                                      \
	(dest)[3] = (value >> 24) & 0xFF;

void add_entry(uint8_t *entry);
void send_header(char *passwd, char *devname, char *devid, char *ver);
void send_dir(char *dir);
void handle_transfer(uint8_t *transfer_req, char **base_dirs);
bool check_in_base_dirs(char *dir, char **base_dirs);

void syncer_run(char **dirs, char *devname, char *devid, char *ver,
		char *passwd)
{
	// TODO Handle dir ending without '/'
	send_header(passwd, devname, devid, ver);
	LOG("Sending entries\n");
	clbk_show_status("Sending entries\n");
	for (int i = 0; dirs[i] != NULL; i++)
		send_dir(dirs[i]);
	LOG("Sending end of entries\n");
	uint8_t entry[entry_size] = {};
	// Iterate over directory
	entry[0] = 'e';
	add_entry(entry);
	uint8_t transfer_req[transfer_req_size + 1];
	// Just to make sure the 0th byte isn't 'e'
	transfer_req[0] = 0;
	// And make sure it's null terminated
	transfer_req[transfer_req_size] = 0;
	do {
		if (clbk_receive(transfer_req, transfer_req_size) !=
		    transfer_req_size) {
			clbk_show_error("Transfer Request too small, probably "
					"connection interrupted");
		}
		LOG("Received request\n");
		handle_transfer(transfer_req, dirs);
	} while (transfer_req[0] != 'e');
	clbk_show_status("Done\n");
}

void add_entry(uint8_t *entry)
{
	static uint8_t queue[entry_size * queue_max_depth];
	static uint8_t queue_pos = 0;
	memcpy(queue + (queue_pos++ * entry_size), entry, entry_size);
	if ((entry[0] == 'e') || (queue_pos == queue_max_depth)) {
		clbk_send(queue, queue_pos * entry_size);
		queue_pos = 0;
	}
}

void send_header(char *passwd, char *devname, char *devid, char *ver)
{
	LOG("Assembling header\n");
	uint32_t pass = murmur3_32(passwd, strlen(passwd));
	uint8_t data[header_size] = {'P', 'L', 'Y', 'S', 'Y', 'N', 'C', '1'};
	strncpy((char *)data + 8, devname, 8);
	strncpy((char *)data + 16, devid, 8);
	strncpy((char *)data + 24, ver, 8);
	data[32] = pass & 0xFF;
	data[33] = (pass >> 8) & 0xFF;
	data[34] = (pass >> 16) & 0xFF;
	data[35] = (pass >> 24) & 0xFF;
	LOG("Sending header\n");
	clbk_send(data, sizeof(data));
}
void send_dir(char *dir)
{
	LOG("Opening %s\n", dir);
	void *dird = clbk_open_dir(dir);
	if (dird == NULL) {
		clbk_show_status("Couldn't open dir");
		clbk_show_error(dir);
	}
	struct dir_entry *dentry;
	while ((dentry = clbk_read_dir(dird)) != NULL) {
		if ((!strcmp(".", dentry->name)) ||
		    (!strcmp("..", dentry->name)))
			continue;
		LOG("sending entry %s\n", dentry->name);
		// So we can have a final \0
		static uint8_t entry[entry_size + 1];
		entry[0] = dentry->dir ? 'd' : 'f';
		entry[1] = 0;
		entry[2] = 0;
		entry[3] = 0;
		LOG("with size: %li, mtime %lli\n", dentry->size,
		    dentry->mtime);
		bytiffy_uint32(entry + 4, dentry->size);
		bytiffy_uint32(entry + 8, (dentry->mtime & 0xFFFFFFFF));
		bytiffy_uint32(entry + 12,
			       ((dentry->mtime >> 32) & 0xFFFFFFFF));
		uint16_t newlen = strlen(dir) + strlen(dentry->name) + 1;
		if (newlen > 256) {
			clbk_show_error("Filepath too long");
			while (1)
				;
		}
		// We null the rest of the entry here
		strncpy((char *)entry + 16, dir, entry_size - 16);
		strcpy((char *)entry + 16 + strlen(dir), dentry->name);
		entry[newlen + 16] = '\0';
		LOG("Full path is %s, type %c\n", entry + 16, entry[0]);
		add_entry(entry);
		if (dentry->dir) {
			// TODO Different types of allocations if vlas aren't
			// supported
			char path[newlen + 1];
			memcpy(path, entry + 16, newlen);
			// TODO Remove plattform specific file seperator
			path[newlen - 1] = '/';
			path[newlen] = 0;
			send_dir(path);
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
		static uint8_t transfer_buffer[transfer_size];
		uint32_t size = clbk_file_size((char *)transfer_req + 1);
		LOG("File size is %li\n", size);
		if (transfer_req[0] == 'f') {
			bytiffy_uint32(transfer_buffer, size);
			clbk_send(transfer_buffer, 4);
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
		while ((read = clbk_read(transfer_buffer,
					 sizeof(transfer_buffer))) ==
		       sizeof(transfer_buffer)) {
			h = murmur3_32_step(h, transfer_buffer, read);
			if (transfer_req[0] == 'f')
				clbk_send(transfer_buffer, read);
		}
		h = murmur3_32_finalize(h, transfer_buffer, read, size);
		if (transfer_req[0] == 'f') {
			clbk_send(transfer_buffer, read);
			LOG("Sent file %s\n", (char *)transfer_req + 1);
		}
		bytiffy_uint32(transfer_buffer, h);
		clbk_send(transfer_buffer, 4);
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
