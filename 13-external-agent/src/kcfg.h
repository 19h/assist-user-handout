#pragma once
#include <stddef.h>
#include <stdint.h>

enum { KCFG_INT = 1, KCFG_STRING = 2, KCFG_BOOL = 3, KCFG_BLOB = 4, KCFG_IPV4 = 5 };

struct kcfg_entry {
    uint8_t type;
    char key[32];
    int32_t i;
    char s[128];
    uint16_t len;
};

uint16_t kcfg_crc16(const uint8_t *p, size_t n);
int kcfg_parse(const uint8_t *buf, size_t len, struct kcfg_entry *out, int max);
void kcfg_print(const struct kcfg_entry *e);
