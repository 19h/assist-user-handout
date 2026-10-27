/*
 * KCFG - Kestrel binary site configuration.
 *   "KCFG" | u8 version | u8 flags | u16 count | entry* | u16 crc16-ccitt(all previous bytes)
 *   entry: u8 type | u8 key_len | key | value
 *     1 int32 (LE) | 2 string (u16 len + bytes, obfuscated) | 3 bool (u8)
 *     4 blob (u16 len + bytes) | 5 ipv4 (4 bytes, network order)
 *   String bytes are XORed with (0x5C + i) & 0xff, i = index within the string.
 */
#include <stdio.h>
#include <string.h>

#include "kcfg.h"

__attribute__((noinline)) uint16_t kcfg_crc16(const uint8_t *p, size_t n)
{
    uint16_t crc = 0xFFFF;
    while (n--) {
        crc ^= (uint16_t)(*p++ << 8);
        for (int k = 0; k < 8; k++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

static void deobfuscate(char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        s[i] ^= (char)((0x5C + i) & 0xFF);
}

__attribute__((noinline)) int kcfg_parse(const uint8_t *buf, size_t len, struct kcfg_entry *out, int max)
{
    if (len < 10 || memcmp(buf, "KCFG", 4) != 0 || buf[4] != 2)
        return -1;
    uint16_t stored = (uint16_t)(buf[len - 2] | buf[len - 1] << 8);
    if (kcfg_crc16(buf, len - 2) != stored)
        return -2;

    int count = buf[6] | buf[7] << 8;
    size_t off = 8;
    int n = 0;
    for (int i = 0; i < count && n < max; i++, n++) {
        struct kcfg_entry *e = &out[n];
        memset(e, 0, sizeof(*e));
        e->type = buf[off++];
        uint8_t klen = buf[off++];
        if (klen >= sizeof(e->key) || off + klen > len - 2)
            return -3;
        memcpy(e->key, buf + off, klen);
        off += klen;
        switch (e->type) {
        case KCFG_INT:
            memcpy(&e->i, buf + off, 4);
            off += 4;
            break;
        case KCFG_STRING:
        case KCFG_BLOB: {
            uint16_t vlen = (uint16_t)(buf[off] | buf[off + 1] << 8);
            off += 2;
            if (vlen >= sizeof(e->s) || off + vlen > len - 2)
                return -4;
            memcpy(e->s, buf + off, vlen);
            e->len = vlen;
            if (e->type == KCFG_STRING)
                deobfuscate(e->s, vlen);
            off += vlen;
            break;
        }
        case KCFG_BOOL:
            e->i = buf[off++] != 0;
            break;
        case KCFG_IPV4:
            memcpy(e->s, buf + off, 4);
            e->len = 4;
            off += 4;
            break;
        default:
            return -5;
        }
    }
    return n;
}

__attribute__((noinline)) void kcfg_print(const struct kcfg_entry *e)
{
    switch (e->type) {
    case KCFG_INT:    printf("%s = %d\n", e->key, e->i); break;
    case KCFG_STRING: printf("%s = \"%s\"\n", e->key, e->s); break;
    case KCFG_BOOL:   printf("%s = %s\n", e->key, e->i ? "true" : "false"); break;
    case KCFG_IPV4:   printf("%s = %u.%u.%u.%u\n", e->key, (uint8_t)e->s[0], (uint8_t)e->s[1], (uint8_t)e->s[2], (uint8_t)e->s[3]); break;
    case KCFG_BLOB:
        printf("%s = <", e->key);
        for (unsigned i = 0; i < e->len; i++)
            printf("%02x", (uint8_t)e->s[i]);
        printf(">\n");
        break;
    }
}
