/*
 * From qLibc
 *
 * Copyright (c) 2010-2015 Seungyoung Kim. 2018 baregl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
  */
#include "murmur3.h"
#include "syncer.h"

uint32_t murmur3_32(const void *data, uint32_t nbytes)
{
	if (data == NULL || nbytes == 0)
		return 0;

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	const int nblocks = nbytes / 4;
	const uint32_t *blocks = (const uint32_t *) (data);
	const uint8_t *tail = (const uint8_t *) (data + (nblocks * 4));

	uint32_t h = 0;

	int i;
	uint32_t k;
	for (i = 0; i < nblocks; i++) {
		k = blocks[i];

		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;

		h ^= k;
		h = (h << 13) | (h >> (32 - 13));
		h = (h * 5) + 0xe6546b64;
	}

	k = 0;
	switch (nbytes & 3) {
		case 3:
			k ^= tail[2] << 16;
			/* fall through */
		case 2:
			k ^= tail[1] << 8;
			/* fall through */
		case 1:
			k ^= tail[0];
			k *= c1;
			k = (k << 15) | (k >> (32 - 15));
			k *= c2;
			h ^= k;
	};

	h ^= nbytes;

	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

// nbytes has to be divisible by 0, start of with h = 0
uint32_t murmur3_32_step(uint32_t h, const void *data, uint32_t nbytes)
{
	if (data == NULL || nbytes == 0 || (nbytes % 4)) {
		LOG("Invalid data to hash\n");
		return -1;
	}

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	const int nblocks = nbytes / 4;
	const uint32_t *blocks = (const uint32_t *) (data);

	int i;
	uint32_t k;
	for (i = 0; i < nblocks; i++) {
		k = blocks[i];

		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;

		h ^= k;
		h = (h << 13) | (h >> (32 - 13));
		h = (h * 5) + 0xe6546b64;
	}
	return h;

}

uint32_t murmur3_32_finalize(uint32_t h, const void *data, uint32_t nbytes, uint32_t totalbytes)
{
	if (data == NULL) {
		LOG("Invalid data to finalize\n");
		return -1;

	}

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	const int nblocks = nbytes / 4;
	const uint32_t *blocks = (const uint32_t *) (data);
	const uint8_t *tail = (const uint8_t *) (data + (nblocks * 4));

	int i;
	uint32_t k;
	for (i = 0; i < nblocks; i++) {
		k = blocks[i];

		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;

		h ^= k;
		h = (h << 13) | (h >> (32 - 13));
		h = (h * 5) + 0xe6546b64;
	}

	k = 0;
	switch (nbytes & 3) {
		case 3:
			k ^= tail[2] << 16;
			/* fall through */
		case 2:
			k ^= tail[1] << 8;
			/* fall through */
		case 1:
			k ^= tail[0];
			k *= c1;
			k = (k << 15) | (k >> (32 - 15));
			k *= c2;
			h ^= k;
	};

	h ^= totalbytes;

	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}
