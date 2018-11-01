#include "syncer.h"

#include "constants.h"
#include "crypto.h"
#include "murmur3.h"
#include "tweetnacl.h"
#include <stdlib.h>
#include <string.h>

#define bytiffy_uint32(dest, value)                                            \
	(dest)[0] = (value)&0xFF;                                              \
	(dest)[1] = (value >> 8) & 0xFF;                                       \
	(dest)[2] = (value >> 16) & 0xFF;                                      \
	(dest)[3] = (value >> 24) & 0xFF;

void send_header(char *devname, char *ver);
void send_dir(char *dir);
void handle_transfer(uint8_t *transfer_req, char **base_dirs);
bool check_in_base_dirs(char *dir, char **base_dirs);

void syncer_run(char **dirs, char *devname, char *devid, char *ver, char *key)
{
	// TODO Handle dir ending without '/'
	encrypted_begin_communication(devid, key);
	send_header(devname, ver);
	LOG("Sending entries\n");
	clbk_show_status("Sending entries\n");
	for (int i = 0; dirs[i] != NULL; i++)
		send_dir(dirs[i]);
	LOG("Sending end of entries\n");
	uint8_t entry[crypto_secretbox_ZEROBYTES + entry_size] = {0};
	// Iterate over directory
	entry[crypto_secretbox_ZEROBYTES] = 'e';
	encrypted_send(entry, sizeof(entry));
	uint8_t
	    transfer_req[crypto_secretbox_ZEROBYTES + transfer_req_size + 1];
	// Just to make sure the 0th byte isn't 'e'
	transfer_req[crypto_secretbox_ZEROBYTES] = 0;
	// And make sure it's null terminated
	transfer_req[crypto_secretbox_ZEROBYTES + transfer_req_size] = 0;
	do {
		encrypted_receive(transfer_req, crypto_secretbox_ZEROBYTES +
						    transfer_req_size);
		LOG("Received request\n");
		handle_transfer(transfer_req + crypto_secretbox_ZEROBYTES,
				dirs);
	} while (transfer_req[crypto_secretbox_ZEROBYTES] != 'e');
	clbk_show_status("Done\n");
}

void send_header(char *devname, char *ver)
{
	LOG("Assembling header\n");
	uint8_t data[crypto_secretbox_ZEROBYTES + 16];
	strncpy((char *)data + crypto_secretbox_ZEROBYTES, devname, 8);
	strncpy((char *)data + crypto_secretbox_ZEROBYTES + 8, ver, 8);
	LOG("Sending header\n");
	encrypted_send(data, sizeof(data));
}

void send_dir(char *dir)
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
		LOG("sending entry %s\n", dentry->name);
		// So we can have a final \0
		static uint8_t
		    entry[crypto_secretbox_ZEROBYTES + entry_size + 1];
		entry[crypto_secretbox_ZEROBYTES + 0] = dentry->dir ? 'd' : 'f';
		entry[crypto_secretbox_ZEROBYTES + 1] = 0;
		entry[crypto_secretbox_ZEROBYTES + 2] = 0;
		entry[crypto_secretbox_ZEROBYTES + 3] = 0;
		LOG("with size: %li, mtime %lli\n", dentry->size,
		    dentry->mtime);
		bytiffy_uint32(crypto_secretbox_ZEROBYTES + entry + 4,
			       dentry->size);
		bytiffy_uint32(crypto_secretbox_ZEROBYTES + entry + 8,
			       (dentry->mtime & 0xFFFFFFFF));
		bytiffy_uint32(crypto_secretbox_ZEROBYTES + entry + 12,
			       ((dentry->mtime >> 32) & 0xFFFFFFFF));
		uint16_t newlen = strlen(dir) + strlen(dentry->name) + 1;
		if (newlen > 256) {
			clbk_show_error("Filepath too long");
			while (1)
				;
		}
		// We null the rest of the entry here
		strncpy((char *)(crypto_secretbox_ZEROBYTES + entry + 16), dir,
			entry_size - 16);
		strcpy((char *)(crypto_secretbox_ZEROBYTES + entry + 16 +
				strlen(dir)),
		       dentry->name);
		// Zero out the rest
		for (int i = crypto_secretbox_ZEROBYTES + newlen + 16;
		     i < sizeof(entry); i++)
			entry[i] = 0;
		LOG("Full path is %s, type %c\n",
		    crypto_secretbox_ZEROBYTES + entry + 16,
		    entry[0 + crypto_secretbox_ZEROBYTES]);
		encrypted_send(entry, crypto_secretbox_ZEROBYTES + entry_size);
		if (dentry->dir) {
			// TODO Different types of allocations if vlas aren't
			// supported
			char path[newlen + 1];
			memcpy(path, entry + 16 + crypto_secretbox_ZEROBYTES,
			       newlen);
			// TODO Remove plattform specific file separator
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
		static uint8_t
		    transfer_buffer[crypto_secretbox_ZEROBYTES + transfer_size];
		uint32_t size = clbk_file_size((char *)transfer_req + 1);
		LOG("File size is %li\n", size);
		if (transfer_req[0] == 'f') {
			for (int i = 0; i < crypto_secretbox_ZEROBYTES; i++)
				transfer_buffer[i] = 0;
			bytiffy_uint32(
			    transfer_buffer + crypto_secretbox_ZEROBYTES, size);
			encrypted_send(transfer_buffer,
				       crypto_secretbox_ZEROBYTES + 4);
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
		while ((read = clbk_read(transfer_buffer +
					     crypto_secretbox_ZEROBYTES,
					 transfer_size)) == transfer_size) {
			h = murmur3_32_step(
			    h, transfer_buffer + crypto_secretbox_ZEROBYTES,
			    read);
			if (transfer_req[0] == 'f') {
				// The first ZEROBYTES are already zero
				encrypted_send(transfer_buffer,
					       crypto_secretbox_ZEROBYTES +
						   read);
			}
		}
		printf("Left %i\n", read);
		h = murmur3_32_finalize(
		    h, transfer_buffer + crypto_secretbox_ZEROBYTES, read,
		    size);
		if (transfer_req[0] == 'f') {
			encrypted_send(transfer_buffer,
				       crypto_secretbox_ZEROBYTES + read);
			LOG("Sent file %s\n", (char *)transfer_req + 1);
		}
		bytiffy_uint32(transfer_buffer + crypto_secretbox_ZEROBYTES, h);
		encrypted_send(transfer_buffer, crypto_secretbox_ZEROBYTES + 4);
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

// tweetnacl wants a randombyte function, which isn't needed for our purposes
void randombytes(uint8_t *a, uint64_t b)
{
	clbk_show_error("called randombytes, usually not needed, just here for "
			"linking errors");
}
