/*
 * Kestrel Lockworks license verification.
 *
 * Key format: XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX (hex, case-insensitive),
 * read as four 32-bit words and checked against the FNV-1a hash of the user.
 */
#include <stdint.h>

#include "license.h"

static inline uint32_t rotl32(uint32_t v, int n)
{
    return (v << n) | (v >> (32 - n));
}

__attribute__((noinline)) static uint32_t user_hash(const char *user)
{
    uint32_t h = 0x811C9DC5u;

    for (const unsigned char *p = (const unsigned char *)user; *p; ++p) {
        unsigned char c = *p;
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char)(c - 'A' + 'a');
        h ^= c;
        h *= 0x01000193u;
    }
    return h;
}

__attribute__((noinline)) static int parse_key(const char *key, uint32_t w[4])
{
    for (int word = 0; word < 4; word++) {
        uint32_t v = 0;

        for (int i = 0; i < 8; i++) {
            char c = *key++;
            uint32_t nib;
            if (c >= '0' && c <= '9')
                nib = (uint32_t)(c - '0');
            else if (c >= 'a' && c <= 'f')
                nib = (uint32_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                nib = (uint32_t)(c - 'A' + 10);
            else
                return 0;
            v = (v << 4) | nib;
        }
        w[word] = v;

        if (word < 3 && *key++ != '-')
            return 0;
    }
    return *key == '\0';
}

/* The four core equations every valid key satisfies. */
__attribute__((noinline)) static int check_words(uint32_t h, const uint32_t w[4])
{
    if (((w[0] * 0x9E3779B1u) ^ rotl32(w[1], 13)) != (h ^ 0xC0FFEE11u))
        return TIER_INVALID;
    if ((((w[1] + 3u * w[2]) ^ (w[3] >> 7)) & 0xFFFFu) != ((rotl32(h, 7) + 0x1337u) & 0xFFFFu))
        return TIER_INVALID;
    if ((rotl32(w[2] ^ w[0], 5) - w[3]) != (h * 0x2545F491u))
        return TIER_INVALID;
    if (((w[3] ^ 0xA5A5A5A5u) * 0x01000193u + w[0]) != ~h)
        return TIER_INVALID;

    /* Enterprise keys: the word sum carries the 0x4B4 marker in its top 12 bits. */
    if (((w[0] + w[1] + w[2] + w[3]) >> 20) != 0x4B4u)
        return TIER_STANDARD;

    /* Lifetime keys carry a second checksum over words 0 and 3. */
    if ((w[3] ^ 0xA5A5A5A5u) * 0x01000193u == h - w[0])
        return TIER_LIFETIME;

    return TIER_ENTERPRISE;
}

__attribute__((noinline)) int verify_license(const char *user, const char *key)
{
    uint32_t w[4];

    if (!parse_key(key, w))
        return TIER_INVALID;
    return check_words(user_hash(user), w);
}
