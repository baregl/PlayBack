#ifndef __CONSTANTS_H_
#define __CONSTANTS_H_

// Hack for array size
enum constants {
	// hash is separate
	header_size = 32,
	entry_size = 272,
	queue_max_depth = 64,
	transfer_req_size = 257,
	transfer_size = 0x10000,
	// Is a max size for dynamic memory allocation
	config_max_size = 50000,
	data_timeout = 5000,
	// Should be a bigger than transfer size for the signing stuff
	// Just a bit of a buffer
	crypto_max_size = 0x11000,
};

#endif // __CONSTANTS_H_
