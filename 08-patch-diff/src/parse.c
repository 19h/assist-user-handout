/* BLE advertisement / command packet parser for kestrel-bled. */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "bled.h"

__attribute__((noinline)) static void log_reject(const char *why, uint16_t seq)
{
#if BLED_VERSION_MINOR >= 1
    fprintf(stderr, "bled[%u]: packet rejected: %s\n", seq, why);
#else
    fprintf(stderr, "bled: rejected packet %u (%s)\n", seq, why);
#endif
}

/*
 * Packet: B1 | type | seq(le16) | { tag | len | value[len] }*
 */
__attribute__((noinline)) int parse_packet(const uint8_t *buf, size_t len, struct ble_msg *m)
{
    if (len < 4 || buf[0] != BLED_MAGIC) {
        log_reject("bad magic", 0);
        return -EINVAL;
    }
    m->type = buf[1];
    m->seq = (uint16_t)(buf[2] | (buf[3] << 8));

    size_t off = 4;
    while (off + 2 <= len) {
        uint8_t tag = buf[off];
        uint8_t l = buf[off + 1];
        off += 2;
        if (off + l > len) {
            log_reject("truncated tlv", m->seq);
            return -EINVAL;
        }

        switch (tag) {
        case TLV_FLAGS:
            if (l != 1) {
                log_reject("bad flags", m->seq);
                return -EINVAL;
            }
            m->flags = buf[off];
            break;

        case TLV_NAME: {
#if BLED_VERSION_MINOR >= 1
            if (l == 0 || l > sizeof(m->name) - 1) {
                log_reject("device name too long", m->seq);
                return -EMSGSIZE;
            }
#else
            int8_t room = (int8_t)(l - 1);
            if (room >= (int8_t)sizeof(m->name)) {
                log_reject("device name too long", m->seq);
                return -EMSGSIZE;
            }
#endif
            memcpy(m->name, buf + off, l);
            m->name[l < sizeof(m->name) ? l : sizeof(m->name) - 1] = '\0';
            break;
        }

#if BLED_VERSION_MINOR >= 1
        case TLV_TX_POWER:
            if (l != 1) {
                log_reject("bad tx power", m->seq);
                return -EINVAL;
            }
            m->tx_power = (int8_t)buf[off];
            break;
#endif

        case TLV_PAYLOAD:
            if (l > sizeof(m->payload)) {
                log_reject("payload too large", m->seq);
                return -EMSGSIZE;
            }
            memcpy(m->payload, buf + off, l);
            m->payload_len = l;
            break;

        default:
            break; /* unknown tags are skipped */
        }
        off += l;
    }

    if (m->on_complete)
        m->on_complete(m);
    return 0;
}
