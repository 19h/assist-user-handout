/* libkstore: door secrets, stored encrypted and decrypted per request. */
#include "kestrel_export.h"
#include "kstore.h"
#include "kstore_table.h"

struct kstore {
    uint32_t magic;
    uint32_t doors;
    uint64_t key;
};

static const struct kstore k_store = {0x4B53544Fu, 16, KSTORE_KEY};

static uint8_t keystream_byte(uint64_t key, unsigned door, unsigned i)
{
    uint64_t x = key ^ (door * 0x9E3779B97F4A7C15ULL) ^ (i * 0xD1B54A32D192ED03ULL);

    x ^= x >> 31;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    return (uint8_t)(x >> 56);
}

KESTREL_API struct kstore *kstore_open(void)
{
    return (struct kstore *)&k_store;
}

KESTREL_API int kstore_door_secret(const struct kstore *ks, unsigned door, uint8_t out[16])
{
    if (!ks || ks->magic != 0x4B53544Fu || door == 0 || door > ks->doors)
        return -1;
    for (unsigned i = 0; i < 16; i++)
        out[i] = k_door_table[door - 1][i] ^ keystream_byte(ks->key, door, i);
    return 16;
}

KESTREL_API unsigned kstore_door_count(const struct kstore *ks)
{
    return ks ? ks->doors : 0;
}

KESTREL_API const char *kstore_version(void)
{
    return "kstore 1.4.2";
}
