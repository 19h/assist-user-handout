/* KFWP integrity primitives. */
#include "kfwp.h"

static inline uint32_t rotl32(uint32_t v, int n)
{
    return (v << n) | (v >> (32 - n));
}

/*
 * Header checksum: Adler-style sums seeded with 0x4B53, over 0x10..end,
 * excluding the 32-byte signature at 0x20..0x3f, folded with the device id.
 */
__attribute__((noinline)) uint32_t kfwp_checksum(const uint8_t *image, size_t total_size, uint32_t device_id)
{
    uint32_t a = 0x4B53, b = 0;

    for (size_t i = 0x10; i < total_size; i++) {
        if (i >= 0x20 && i < 0x40)
            continue;
        a = (a + image[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return ((b << 16) | a) ^ rotl32(device_id, 8);
}

__attribute__((noinline)) uint32_t kfwp_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/* Section encryption: XOR stream keyed by device id and build time. */
__attribute__((noinline)) void kfwp_crypt_section(uint8_t *data, size_t len, uint32_t device_id, uint64_t build_time)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t k = (uint8_t)(device_id >> (8 * (i % 4))) ^ (uint8_t)(build_time >> (8 * (i % 8))) ^ (uint8_t)(i * 0x1D);
        data[i] ^= k;
    }
}
