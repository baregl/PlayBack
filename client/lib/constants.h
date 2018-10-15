#ifndef __CONSTANTS_H_
#define __CONSTANTS_H_

// Hack for array size
enum constants {
	header_size = 36,
	entry_size = 272,
	queue_max_depth = 64,
	transfer_req_size = 257,
	transfer_size = 1048576,
	config_size = 2048,
	data_timeout = 500,
};

#endif // __CONSTANTS_H_
