/*
 * lockctl - Kestrel Lockworks door controller CLI.
 * usage: lockctl <user> <door> <token>
 *
 * The unlock token is the hex MAC (libkcrypto) of a protocol challenge
 * (libkproto, loaded at runtime) keyed with the door secret (libkstore).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifndef KPROTO_STATIC
#include <dlfcn.h>
#endif

#include "kcrypto.h"
#include "kproto.h"
#include "kstore.h"

static const struct kproto_ops *load_protocol(void)
{
#ifdef KPROTO_STATIC
    return kproto_v2_entry();
#else
    void *plugin = dlopen("libkproto.so", RTLD_NOW);
    if (!plugin) {
        fprintf(stderr, "lockctl: protocol plugin missing: %s\n", dlerror());
        return NULL;
    }
    kproto_entry_fn entry = (kproto_entry_fn)dlsym(plugin, "kproto_v2_entry");
    return entry ? entry() : NULL;
#endif
}

static int compute_token(const char *user, unsigned door, char hex[17])
{
    uint8_t secret[16], mac_bytes[8];
    char challenge[128];

    if (kstore_door_secret(kstore_open(), door, secret) != 16)
        return -1;

    const struct kproto_ops *proto = load_protocol();
    if (!proto || proto->abi != 2)
        return -2;

    size_t n = proto->build_challenge(user, door, challenge, sizeof(challenge));
    uint64_t mac = kc_mac64(secret, challenge, n);
    memset(secret, 0, sizeof(secret));

    for (int i = 0; i < 8; i++)
        mac_bytes[i] = (uint8_t)(mac >> (8 * i));
    kc_hex(mac_bytes, sizeof(mac_bytes), hex);
    return 0;
}

int main(int argc, char **argv)
{
    char expected[17];

    if (argc != 4) {
        fprintf(stderr, "usage: %s <user> <door> <token>\n", argv[0]);
        return 2;
    }

    unsigned door = (unsigned)strtoul(argv[2], NULL, 10);
    int rc = compute_token(argv[1], door, expected);
    if (rc != 0) {
        printf("lockctl: cannot verify door %u (error %d)\n", door, rc);
        return 1;
    }

    if (strcasecmp(expected, argv[3]) == 0) {
        printf("ACCESS GRANTED: door %u released for %s\n", door, argv[1]);
        return 0;
    }
    printf("ACCESS DENIED: bad token for door %u\n", door);
    return 1;
}
