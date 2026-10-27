/*
 * kestrel-licensed - Kestrel Lockworks licensed command runner.
 * usage: kestrel-licensed <license-hex> <command> [arg]
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "commands.h"
#include "license.h"
#include "protect.h"

static int unhex(const char *hex, uint8_t *out, size_t cap)
{
    size_t n = strlen(hex) / 2;
    if (n > cap)
        return -1;
    for (size_t i = 0; i < n; i++) {
        unsigned v;
        if (sscanf(hex + 2 * i, "%2x", &v) != 1)
            return -1;
        out[i] = (uint8_t)v;
    }
    return (int)n;
}

int main(int argc, char **argv)
{
    uint8_t blob[64];
    struct license lic;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <license-hex> <command> [arg]\n", argv[0]);
        return 2;
    }
    int len = unhex(argv[1], blob, sizeof(blob));
    if (len < 0)
        die(1);
    if (check_license_blob(blob, (size_t)len, &lic) != 0) {
        puts("invalid license");
        die(2);
    }
    return dispatch(&lic, argv[2], argc > 3 ? argv[3] : "");
}
