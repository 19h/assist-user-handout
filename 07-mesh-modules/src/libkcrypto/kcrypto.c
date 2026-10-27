/*
 * libkcrypto: KMAC-64, a SipHash-2-4 construction with Kestrel-specific
 * initialisation constants, plus a legacy v1 MAC kept for old controllers.
 */
#include <string.h>

#include "kcrypto.h"
#include "kestrel_export.h"

#define ROTL64(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

#define SIPROUND                                                   \
    do {                                                           \
        v0 += v1; v1 = ROTL64(v1, 13); v1 ^= v0; v0 = ROTL64(v0, 32); \
        v2 += v3; v3 = ROTL64(v3, 16); v3 ^= v2;                   \
        v0 += v3; v3 = ROTL64(v3, 21); v3 ^= v0;                   \
        v2 += v1; v1 = ROTL64(v1, 17); v1 ^= v2; v2 = ROTL64(v2, 32); \
    } while (0)

static uint64_t load64(const uint8_t *p)
{
    uint64_t v;
    memcpy(&v, p, 8);
    return v;
}

KESTREL_API uint64_t kc_mac64(const uint8_t key[16], const void *msg, size_t len)
{
    const uint8_t *in = msg;
    uint64_t k0 = load64(key), k1 = load64(key + 8);
    uint64_t v0 = 0x6b657374726c6f63ULL ^ k0; /* Kestrel constants, not SipHash's */
    uint64_t v1 = 0x6b6d61632d763221ULL ^ k1;
    uint64_t v2 = 0x646f6f726c6f636bULL ^ k0;
    uint64_t v3 = 0x746f6b656e2d3634ULL ^ k1;
    uint64_t b = (uint64_t)len << 56;
    size_t full = len & ~(size_t)7;

    for (size_t i = 0; i < full; i += 8) {
        uint64_t m = load64(in + i);
        v3 ^= m;
        SIPROUND;
        SIPROUND;
        v0 ^= m;
    }
    for (size_t i = 0; i < (len & 7); i++)
        b |= (uint64_t)in[full + i] << (8 * i);

    v3 ^= b;
    SIPROUND;
    SIPROUND;
    v0 ^= b;
    v2 ^= 0xff;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;
}

/* v1 controllers: FNV-1a over key||msg. Still exported, no longer used. */
KESTREL_API uint64_t kc_mac64_legacy(const uint8_t key[16], const void *msg, size_t len)
{
    const uint8_t *in = msg;
    uint64_t h = 0xcbf29ce484222325ULL;

    for (int i = 0; i < 16; i++)
        h = (h ^ key[i]) * 0x100000001b3ULL;
    for (size_t i = 0; i < len; i++)
        h = (h ^ in[i]) * 0x100000001b3ULL;
    return h;
}

KESTREL_API void kc_hex(const uint8_t *data, size_t len, char *out)
{
    static const char digits[] = "0123456789abcdef";

    for (size_t i = 0; i < len; i++) {
        out[2 * i] = digits[data[i] >> 4];
        out[2 * i + 1] = digits[data[i] & 15];
    }
    out[2 * len] = '\0';
}

KESTREL_API const char *kc_version(void)
{
    return "kcrypto 2.1.0 (KMAC-64)";
}
