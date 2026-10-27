#ifndef KSTORE_H
#define KSTORE_H
#include <stdint.h>

/* libkstore - door secret keyring for Kestrel lock controllers. */
struct kstore;

struct kstore *kstore_open(void);
int kstore_door_secret(const struct kstore *ks, unsigned door, uint8_t out[16]);
unsigned kstore_door_count(const struct kstore *ks);
const char *kstore_version(void);

#endif
