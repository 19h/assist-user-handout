#include <stdint.h>

#include "kstr.h"
#include "kstr_table.h"

/* 64-bit LCG keystream, one 8-byte word per step. Decoding writes whole
 * words, so the plaintext appears in memory eight bytes at a time. */
__attribute__((noinline)) void kstr_decode(int id, char out[KSTR_BUF])
{
    const struct kstr_blob *blob = &k_strings[id];
    uint64_t *dst = (uint64_t *)out;
    uint64_t k = blob->key;

    for (uint32_t i = 0; i < blob->nwords; i++) {
        k = k * 6364136223846793005ULL + 1442695040888963407ULL;
        dst[i] = blob->words[i] ^ (k ^ (k >> 29));
    }
}

__attribute__((noinline)) void kstr_wipe(char buf[KSTR_BUF])
{
    volatile uint64_t *p = (volatile uint64_t *)buf;

    for (int i = 0; i < KSTR_BUF / 8; i++)
        p[i] = 0;
}
