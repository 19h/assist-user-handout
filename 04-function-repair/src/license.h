#ifndef LICENSE_H
#define LICENSE_H
#include <stddef.h>
#include <stdint.h>

#define FEAT_REMOTE_UNLOCK 0x01u
#define FEAT_AUDIT_EXPORT  0x02u
#define FEAT_DOOR_GROUPS   0x04u
#define FEAT_SERVICE_MODE  0x80u

struct license {
    uint16_t version;
    uint16_t seats;
    uint32_t expiry_day;
    uint32_t features;
};

int check_license_blob(const uint8_t *blob, size_t len, struct license *out);
int license_allows(const struct license *lic, uint32_t today, uint32_t feature);

#endif
