/*
 * kestrel-bled - Kestrel Lockworks BLE gateway daemon (replay build).
 * usage: kestrel-bled <hex-packet>...
 */
#include <stdio.h>
#include <string.h>

#include "bled.h"

#define STR2(x) #x
#define STR(x) STR2(x)
static const char k_version[] = "kestrel-bled " STR(BLED_VERSION_MAJOR) "." STR(BLED_VERSION_MINOR) "." STR(BLED_VERSION_PATCH);

static void on_message(struct ble_msg *m)
{
    printf("msg seq=%u type=%#x flags=%#x name='%s' payload=%u bytes\n",
           m->seq, m->type, m->flags, m->name, m->payload_len);
}

static size_t unhex(const char *hex, uint8_t *out, size_t cap)
{
    size_t n = 0;
    while (hex[0] && hex[1] && n < cap) {
        unsigned v;
        if (sscanf(hex, "%2x", &v) != 1)
            break;
        out[n++] = (uint8_t)v;
        hex += 2;
    }
    return n;
}

__attribute__((noinline)) static int handle_packet(const char *hex)
{
    uint8_t buf[300];
    struct ble_msg msg;

    memset(&msg, 0, sizeof(msg));
    msg.on_complete = on_message;
    size_t n = unhex(hex, buf, sizeof(buf));
    int rc = parse_packet(buf, n, &msg);
    if (rc != 0)
        printf("parse error %d\n", rc);
    return rc;
}

int main(int argc, char **argv)
{
    puts(k_version);
    for (int i = 1; i < argc; i++)
        handle_packet(argv[i]);
    return 0;
}
