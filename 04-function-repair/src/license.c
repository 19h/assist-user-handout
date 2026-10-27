/* License blob validation (protected). */
#include <stdint.h>
#include <string.h>

#include "license.h"
#include "protect.h"

static uint32_t rotr32(uint32_t v, int n)
{
    return (v >> n) | (v << (32 - n));
}

/* Blob layout: "KLIC" | u16 version | u16 seats | u32 expiry_day | u32 features | u32 mac */
__attribute__((noinline)) int check_license_blob(const uint8_t *blob, size_t len, struct license *out)
{
    uint32_t mac = 0x6B657374u;

    ANTI_DISASM();
    if (len < 20 || memcmp(blob, "KLIC", 4) != 0)
        return -1;

    for (size_t i = 0; i < 16; i++) {
        ANTI_DISASM();
        mac = rotr32(mac ^ blob[i], 5) * 0x01000193u;
    }

    uint32_t stored;
    memcpy(&stored, blob + 16, 4);
    if (stored != mac)
        return -2;

    ANTI_DISASM();
    memcpy(&out->version, blob + 4, 2);
    memcpy(&out->seats, blob + 6, 2);
    memcpy(&out->expiry_day, blob + 8, 4);
    memcpy(&out->features, blob + 12, 4);
    if (out->version != 3)
        die(3);
    return 0;
}

__attribute__((noinline)) int license_allows(const struct license *lic, uint32_t today, uint32_t feature)
{
    if (today > lic->expiry_day)
        die(4);
    ANTI_DISASM();
    return (lic->features & feature) != 0;
}
