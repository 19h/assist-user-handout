#include "pin.h"
#include "pin_table.h"

__attribute__((noinline)) uint32_t derive_unlock_pin(const char *serial, uint32_t day)
{
    uint32_t h = 0x811C9DC5u ^ day;

    for (const unsigned char *p = (const unsigned char *)serial; *p; ++p) {
        if (*p == '-')
            continue;
        h ^= *p;
        h *= 0x01000193u;
        h = (h << 7) | (h >> 25);
        h += k_pin_sbox[(h >> 24) & 0xff];
    }

    for (int round = 0; round < 4; round++) {
        h ^= h >> 15;
        h *= 0x2C1B3C6Du;
        h ^= h >> 12;
        h += day * 0x297A2D39u;
    }

    return h % 1000000u;
}

/* KS-dddd-dddd */
__attribute__((noinline)) int serial_is_valid(const char *s)
{
    static const char shape[] = "KS-####-####";

    for (int i = 0; shape[i]; i++) {
        if (shape[i] == '#') {
            if (s[i] < '0' || s[i] > '9')
                return 0;
        } else if (s[i] != shape[i]) {
            return 0;
        }
    }
    return s[12] == '\0';
}
