/*
 * kestrel-provision - Kestrel Lockworks factory provisioning tool.
 *
 * Computes the daily unlock PIN for a lock serial. All human-readable strings
 * are stored encrypted and only decoded into stack buffers that are wiped
 * right after use.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kstr.h"
#include "kstr_table.h"
#include "pin.h"

__attribute__((noinline)) static void say(int id)
{
    char buf[KSTR_BUF];

    kstr_decode(id, buf);
    puts(buf);
    kstr_wipe(buf);
}

/* The factory master override: a hidden serial that bypasses PIN derivation. */
__attribute__((noinline)) static int is_master_override(const char *serial)
{
    char buf[KSTR_BUF];
    int hit;

    kstr_decode(KSTR_MASTER, buf);
    hit = strcmp(serial, buf) == 0;
    kstr_wipe(buf);
    return hit;
}

__attribute__((noinline)) static void print_pin(const char *serial, unsigned day, unsigned pin)
{
    char fmt[KSTR_BUF];

    kstr_decode(KSTR_PIN_FMT, fmt);
    printf(fmt, serial, day, pin);
    kstr_wipe(fmt);
}

__attribute__((noinline)) static void print_service_info(void)
{
    say(KSTR_AUDIT_LOG);
    say(KSTR_ENDPOINT);
}

int main(int argc, char **argv)
{
    say(KSTR_BANNER);

    if (argc < 3) {
        say(KSTR_USAGE);
        return 2;
    }

    const char *serial = argv[1];
    unsigned day = (unsigned)strtoul(argv[2], NULL, 10);

    if (is_master_override(serial)) {
        say(KSTR_OVERRIDE);
        return 0;
    }

    if (!serial_is_valid(serial)) {
        say(KSTR_BAD_SERIAL);
        return 1;
    }

    print_service_info();
    print_pin(serial, day, derive_unlock_pin(serial, day));
    return 0;
}
