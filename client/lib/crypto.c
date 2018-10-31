#include "constants.h"
#include "crypto.h"
#include "syncer.h"
#include "tweetnacl.h"
#include <stdbool.h>
#include <string.h>
#if (crypto_secretbox_KEYBYTES > crypto_hash_BYTES) ||                         \
    (crypto_secretbox_NONCEBYTES > crypto_hash_BYTES)
#error "Bytes don't match"
#endif

static const char *magic = "PLYSYNC2";
uint8_t k[crypto_secretbox_KEYBYTES];
// Both are always increased by 2, recv starts with zero bit at 0, send with
// zero bit at 1,
uint8_t n_send[crypto_secretbox_NONCEBYTES];
uint8_t n_recv[crypto_secretbox_NONCEBYTES];
void receive_exact(uint8_t *data, uint32_t length);

void encrypted_begin_communication(char *id, char *key)
{
	LOG("Sending identification\n");
	// Identification
	uint8_t data[16];
	strncpy((char *)data, magic, 8);
	strncpy((char *)data + 8, id, 8);
	clbk_send(data, sizeof(data));

	LOG("Generating key\n");
	// Key generation
	uint8_t h[crypto_hash_BYTES];
	if (crypto_hash(h, (const unsigned char *)key, strlen(key)) != 0)
		clbk_show_error("Couldn't generate key hash");
	for (int i = 0; i < crypto_secretbox_KEYBYTES; i++)
		k[i] = h[i];

	LOG("Getting nonce\n");
	// Nonce receiving
	uint8_t noncea[crypto_secretbox_NONCEBYTES];
	receive_exact(noncea, sizeof(noncea));

	LOG("Getting encrypted nonce\n");
	uint8_t rmsg[crypto_secretbox_ZEROBYTES + crypto_secretbox_NONCEBYTES] =
	    {0};
	receive_exact(rmsg + crypto_secretbox_BOXZEROBYTES,
		      sizeof(rmsg) - crypto_secretbox_BOXZEROBYTES);
	uint8_t dmsg[sizeof(rmsg)];
	if (crypto_secretbox_open(dmsg, rmsg, sizeof(rmsg), noncea, k) != 0)
		clbk_show_error("Couldn't decrypt nonce b from server");
	uint8_t *nonceb = dmsg + crypto_secretbox_ZEROBYTES;

	// Generating own nonce with additional randomness provided by the
	// server (in the form of nonceb)
	{
		LOG("Generating client side nonce\n");
		uint8_t hash_in[crypto_secretbox_NONCEBYTES * 2];
		for (int i = 0; i < crypto_secretbox_NONCEBYTES; i++)

			hash_in[i] = nonceb[i];
		clbk_get_random(hash_in + crypto_secretbox_NONCEBYTES,
				crypto_secretbox_NONCEBYTES);

		uint8_t hnonce[crypto_hash_BYTES];
		if (crypto_hash(hnonce, hash_in, sizeof(hash_in)))
			clbk_show_error(
			    "Couldn't generate new nonce c by hashing");
		for (int i = 0; i < crypto_secretbox_NONCEBYTES; i++) {
			uint8_t b = hnonce[i];
			n_send[i] = b;
			n_recv[i] = b;
		}
		n_recv[0] &= ~(0b1);
		n_send[0] |= 0b1;
		LOG("Sending client side nonce\n");
		// Send the new nonce
		uint8_t msg[crypto_secretbox_ZEROBYTES +
			    crypto_secretbox_NONCEBYTES] = {0};
		uint8_t smsg[sizeof(msg)];
		for (int i = 0; i < crypto_secretbox_NONCEBYTES; i++)
			msg[i + crypto_secretbox_ZEROBYTES] = n_recv[i];
		if (crypto_secretbox(smsg, msg, sizeof(msg), nonceb, k) != 0)
			clbk_show_error("Couldn't encrypt nonce c");
		clbk_send(smsg + crypto_secretbox_BOXZEROBYTES,
			  sizeof(smsg) - crypto_secretbox_BOXZEROBYTES);
	}

	LOG("Verifying ok message\n");
	// Verify that the server can do crypto and isn't simply making a replay
	// attack
	{
		uint8_t rmsg[crypto_secretbox_ZEROBYTES + 2] = {0};
		uint8_t dmsg[sizeof(rmsg)];
		receive_exact(rmsg + crypto_secretbox_BOXZEROBYTES,
			      sizeof(rmsg) - crypto_secretbox_BOXZEROBYTES);
		if (crypto_secretbox_open(dmsg, rmsg, sizeof(rmsg), n_recv,
					  k) != 0)
			clbk_show_error("Couldn't decrypt final ok, for "
					"protection against replay attacks");
		if (!((dmsg[crypto_secretbox_ZEROBYTES] == 'O') &&
		      (dmsg[crypto_secretbox_ZEROBYTES + 1] == 'K')))
			// This was already authenticated. This is probably an
			// error in the server, nothing malicious
			clbk_show_error("Wrong content for last message");
	}
}

// Increase nonce by 2 (to get the next even/odd nonce)
void increase_nonce(uint8_t *n)
{
	int inc = 2;
	// An attempt to get approx. constant time
	for (int i = 0; i < crypto_secretbox_NONCEBYTES; i++) {
		n[i] += inc;
		if (n[i] != 0 && !((i == 0) && (n[0] == 1)))
			inc = 0;
		else if (inc == 2)
			inc = 1;
	}
}

// First crypto_secretbox_ZEROBYTES have to be 0
void encrypted_send(uint8_t *data, int size)
{
	increase_nonce(n_send);
	uint8_t enc[size];
	// BOXZEROBYTES are never sent over the wire
	if (crypto_secretbox(enc, data, size, n_send, k) != 0)
		clbk_show_error("Couldn't encrypt data to send");
	clbk_send(enc + crypto_secretbox_BOXZEROBYTES,
		  size - crypto_secretbox_BOXZEROBYTES);
}

// First crypto_secretbox_ZEROBYTES are going to be 0
void encrypted_receive(uint8_t *data, int size)
{
	increase_nonce(n_recv);
	uint8_t enc[size];
	// BOXZEROBYTES are never sent over the wire
	receive_exact(enc + crypto_secretbox_BOXZEROBYTES,
		      size - crypto_secretbox_BOXZEROBYTES);
	for (int i = 0; i < crypto_secretbox_BOXZEROBYTES; i++)
		enc[i] = 0;
	if (crypto_secretbox_open(data, enc, size, n_recv, k) != 0)
		clbk_show_error("Couldn't decrypt received data");
}

void receive_exact(uint8_t *data, uint32_t length)
{
	uint32_t repeat_count = data_timeout;
	while (repeat_count != 0) {
		uint32_t read = clbk_receive(data, length);
		data += read;
		length -= read;
		if (length == 0) {
			return;
		}
		if (read == 0) {
			repeat_count--;
			clbk_delay(1);
		}
	}
	clbk_show_error("Timeout for receive");
}
