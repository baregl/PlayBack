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
	config_size = 2048,
	data_timeout = 5000,
	crypto_max_size = 0x11000,
};

#endif // __CONSTANTS_H_
