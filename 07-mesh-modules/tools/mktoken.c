/* Ground-truth token generator: links the real library sources on the host. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "kcrypto.h"
#include "kproto.h"
#include "kstore.h"

int main(int argc, char **argv)
{
    uint8_t secret[16], mac_bytes[8];
    char challenge[128], hex[17], shex[33];

    if (argc != 3)
        return 2;
    unsigned door = (unsigned)strtoul(argv[2], NULL, 10);
    if (kstore_door_secret(kstore_open(), door, secret) != 16)
        return 1;
    size_t n = kproto_v2_entry()->build_challenge(argv[1], door, challenge, sizeof(challenge));
    uint64_t mac = kc_mac64(secret, challenge, n);
    for (int i = 0; i < 8; i++)
        mac_bytes[i] = (uint8_t)(mac >> (8 * i));
    kc_hex(mac_bytes, 8, hex);
    kc_hex(secret, 16, shex);
    printf("user=%s door=%u\nsecret=%s\nchallenge=%s\nmac64=0x%016llx\ntoken=%s\n",
           argv[1], door, shex, challenge, (unsigned long long)mac, hex);
    return 0;
}
