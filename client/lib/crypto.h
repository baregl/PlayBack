#include <stdint.h>
void encrypted_begin_communication(char *id, char *key);

// First crypto_secretbox_NONCEBYTES have to be 0
void encrypted_send(uint8_t *data, int size);
// First crypto_secretbox_NONCEBYTES are going to be 0
void encrypted_receive(uint8_t *data, int size);
