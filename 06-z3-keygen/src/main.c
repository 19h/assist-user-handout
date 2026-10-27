/* kestrel-keycheck - offline license validation for Kestrel Lockworks Fleet Manager. */
#include <stdio.h>

#include "license.h"

static const char *const tier_names[] = {"INVALID", "STANDARD", "ENTERPRISE", "LIFETIME"};

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <user> <XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX>\n", argv[0]);
        return 2;
    }

    int tier = verify_license(argv[1], argv[2]);
    if (tier == TIER_INVALID) {
        puts("License rejected.");
        return 1;
    }

    printf("License accepted for %s: %s tier\n", argv[1], tier_names[tier]);
    if (tier == TIER_ENTERPRISE)
        puts("Fleet Manager Enterprise features unlocked: remote unlock, audit export, door groups.");
    if (tier == TIER_LIFETIME)
        puts("Lifetime license: all features, all future versions.");
    return 0;
}
