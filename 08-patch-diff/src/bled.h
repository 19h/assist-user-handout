#ifndef BLED_H
#define BLED_H
#include <stddef.h>
#include <stdint.h>

#define BLED_MAGIC 0xB1

enum {
    TLV_FLAGS = 0x01,
    TLV_NAME = 0x09,
#if BLED_VERSION_MINOR >= 1
    TLV_TX_POWER = 0x0A, /* new in 1.1 */
#endif
    TLV_PAYLOAD = 0xFF,
};

struct ble_msg {
    uint8_t type;
    uint8_t flags;
    uint16_t seq;
    char name[32];
    void (*on_complete)(struct ble_msg *m);
#if BLED_VERSION_MINOR >= 1
    int8_t tx_power;
#endif
    uint8_t payload[64];
    uint32_t payload_len;
};

int parse_packet(const uint8_t *buf, size_t len, struct ble_msg *m);

#endif
