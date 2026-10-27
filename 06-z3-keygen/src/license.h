#ifndef LICENSE_H
#define LICENSE_H
#include <stdint.h>

enum license_tier {
    TIER_INVALID = 0,
    TIER_STANDARD = 1,
    TIER_ENTERPRISE = 2,
    TIER_LIFETIME = 3,
};

/* Returns the license tier a key unlocks for a user name. */
int verify_license(const char *user, const char *key);

#endif
