#ifndef __MURMUR3_H_
#define __MURMUR3_H_

#include <stdint.h>
#include <stddef.h>
uint32_t murmur3_32(const void *data, uint32_t nbytes);

// nbytes has to be divisible by 0, start of with h = 0
uint32_t murmur3_32_step(uint32_t h, const void *data, uint32_t nbytes);
uint32_t murmur3_32_finalize(uint32_t h, const void *data, uint32_t nbytes, uint32_t totalbytes);


#endif // __MURMUR3_H_
