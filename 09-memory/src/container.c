/* KFWP container parsing, verification and packing. */
#include <stdio.h>
#include <string.h>

#include "kfwp.h"

static unsigned bcd(uint8_t v)
{
    return (v >> 4) * 10u + (v & 15u);
}

__attribute__((noinline)) int kfwp_verify(const uint8_t *image, size_t len, char *why, size_t why_len)
{
    const struct kfwp_header *h = (const struct kfwp_header *)image;

    if (len < KFWP_HDR_SIZE || memcmp(h->magic, KFWP_MAGIC, 4) != 0) {
        snprintf(why, why_len, "not a KFWP container");
        return -1;
    }
    if (h->hdr_size != KFWP_HDR_SIZE || h->total_size > len) {
        snprintf(why, why_len, "bad header size/total size");
        return -2;
    }
    uint32_t sum = kfwp_checksum(image, h->total_size, h->device_id);
    if (sum != h->checksum) {
        snprintf(why, why_len, "header checksum mismatch (stored %08x, computed %08x)", h->checksum, sum);
        return -3;
    }

    const struct kfwp_section *s = (const struct kfwp_section *)(image + KFWP_HDR_SIZE);
    for (unsigned i = 0; i < h->section_count; i++) {
        uint8_t plain[4096];
        if (s[i].offset + s[i].size > h->total_size || s[i].size > sizeof(plain)) {
            snprintf(why, why_len, "section %u out of bounds", i);
            return -4;
        }
        memcpy(plain, image + s[i].offset, s[i].size);
        if (h->flags & KFWP_F_ENCRYPTED)
            kfwp_crypt_section(plain, s[i].size, h->device_id, h->build_time);
        if (kfwp_crc32(plain, s[i].size) != s[i].crc32) {
            snprintf(why, why_len, "section %.8s crc mismatch", s[i].name);
            return -5;
        }
    }
    snprintf(why, why_len, "ok");
    return 0;
}

__attribute__((noinline)) int kfwp_inspect(const uint8_t *image, size_t len)
{
    const struct kfwp_header *h = (const struct kfwp_header *)image;
    char why[96];
    int rc = kfwp_verify(image, len, why, sizeof(why));

    printf("KFWP v%u.%02u  device %08x  %u section(s)  flags %#x  build %llu\n",
           bcd(h->ver_major_bcd), bcd(h->ver_minor_bcd), h->device_id, h->section_count,
           h->flags, (unsigned long long)h->build_time);
    printf("checksum %08x  total %u bytes  verify: %s\n", h->checksum, h->total_size, why);

    const struct kfwp_section *s = (const struct kfwp_section *)(image + KFWP_HDR_SIZE);
    for (unsigned i = 0; rc != -1 && i < h->section_count; i++)
        printf("  [%u] %-8.8s off %#06x size %#06x load %#010x crc %08x\n", i, s[i].name,
               s[i].offset, s[i].size, s[i].load_addr, s[i].crc32);
    return rc;
}

__attribute__((noinline)) size_t kfwp_pack(uint8_t *out, size_t cap, uint32_t device_id, uint64_t build_time, int encrypt,
                 const char *const names[], const uint8_t *const blobs[], const size_t sizes[],
                 const uint32_t load_addrs[], int count)
{
    struct kfwp_header *h = (struct kfwp_header *)out;
    struct kfwp_section *s = (struct kfwp_section *)(out + KFWP_HDR_SIZE);
    size_t off = KFWP_HDR_SIZE + (size_t)count * KFWP_SECT_SIZE;

    memset(out, 0, off);
    memcpy(h->magic, KFWP_MAGIC, 4);
    h->ver_major_bcd = 0x03;
    h->ver_minor_bcd = 0x12;
    h->hdr_size = KFWP_HDR_SIZE;
    h->device_id = device_id;
    h->section_count = (uint16_t)count;
    h->flags = encrypt ? KFWP_F_ENCRYPTED : 0;
    h->build_time = build_time;
    for (int i = 0; i < 32; i++)
        h->signature[i] = (uint8_t)(0xA5 ^ i); /* placeholder: signed by the update server */

    for (int i = 0; i < count; i++) {
        if (off + sizes[i] > cap)
            return 0;
        strncpy(s[i].name, names[i], sizeof(s[i].name));
        s[i].offset = (uint32_t)off;
        s[i].size = (uint32_t)sizes[i];
        s[i].load_addr = load_addrs[i];
        s[i].crc32 = kfwp_crc32(blobs[i], sizes[i]);
        memcpy(out + off, blobs[i], sizes[i]);
        if (encrypt)
            kfwp_crypt_section(out + off, sizes[i], device_id, build_time);
        off += sizes[i];
    }
    h->total_size = (uint32_t)off;
    h->checksum = kfwp_checksum(out, off, device_id);
    return off;
}
