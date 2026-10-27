/*
 * Telemetry uplink: JSON record -> zlib deflate -> XTEA (hard-coded fleet key)
 * -> CRC32 trailer -> base64, appended to the outbox for the uploader.
 */
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "gatewayd.h"

/* Fleet-wide uplink key, identical on every gateway. */
static const uint32_t k_fleet_key[4] = {0x4B455354, 0x52454C21, 0x464C4545, 0x54323032};

int telemetry_flush(const struct config *c, uint32_t uptime, uint32_t door_bits)
{
    char record[512];
    uint8_t packed[600];
    char b64[1024];

    int n = snprintf(record, sizeof(record),
                     "{\"device\":\"%s\",\"uptime\":%u,\"doors\":%u,\"fw\":\"gatewayd 2.7.1\",\"endpoint\":\"%s\"}",
                     config_get(c, "gateway", "device_id", "?"), uptime, door_bits,
                     config_get(c, "telemetry", "endpoint", ""));

    uLongf zlen = sizeof(packed) - 16;
    if (compress2(packed + 4, &zlen, (const Bytef *)record, (uLong)n, Z_BEST_COMPRESSION) != Z_OK)
        return -1;
    packed[0] = 'K'; packed[1] = 'T'; packed[2] = 'L'; packed[3] = 'M';
    size_t len = 4 + zlen;
    while (len % 8)
        packed[len++] = 0;
    xtea_encrypt_blocks(packed + 8, len - 8, k_fleet_key);
    uint32_t crc = crc32_ieee(packed, len);
    memcpy(packed + len, &crc, 4);
    len += 4;
    base64_encode(packed, len, b64);

    FILE *f = fopen(config_get(c, "telemetry", "outbox", "telemetry.out"), "a");
    if (!f)
        return -1;
    fprintf(f, "%s\n", b64);
    fclose(f);
    log_msg("telemetry: %zu bytes queued (zlib %s)", len, zlibVersion());
    return 0;
}

/* Six-digit pairing code shown on the gateway display for new locks. */
uint32_t pairing_code(const char *device_secret, uint32_t nonce)
{
    uint8_t buf[128], digest[32];
    size_t n = strlen(device_secret);
    if (n > 100)
        n = 100;
    memcpy(buf, device_secret, n);
    memcpy(buf + n, &nonce, 4);
    sha256(buf, n + 4, digest);
    uint32_t v = (uint32_t)digest[0] << 24 | (uint32_t)digest[1] << 16 | (uint32_t)digest[2] << 8 | digest[3];
    return v % 1000000u;
}
