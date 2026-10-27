#ifndef KCRYPTO_H
#define KCRYPTO_H
#include <stddef.h>
#include <stdint.h>

/* libkcrypto - message authentication for Kestrel unlock tokens. */
uint64_t kc_mac64(const uint8_t key[16], const void *msg, size_t len);
uint64_t kc_mac64_legacy(const uint8_t key[16], const void *msg, size_t len);
void kc_hex(const uint8_t *data, size_t len, char *out);
const char *kc_version(void);

#endif
