#ifndef KFWP_H
#define KFWP_H
#include <stddef.h>
#include <stdint.h>

#define KFWP_MAGIC "KFWP"
#define KFWP_HDR_SIZE 0x40
#define KFWP_SECT_SIZE 0x18
#define KFWP_F_ENCRYPTED 0x0001

#pragma pack(push, 1)
struct kfwp_header {
    char magic[4];           /* 0x00 */
    uint8_t ver_major_bcd;   /* 0x04 */
    uint8_t ver_minor_bcd;   /* 0x05 */
    uint16_t hdr_size;       /* 0x06 */
    uint32_t total_size;     /* 0x08 */
    uint32_t checksum;       /* 0x0c */
    uint32_t device_id;      /* 0x10 */
    uint16_t section_count;  /* 0x14 */
    uint16_t flags;          /* 0x16 */
    uint64_t build_time;     /* 0x18 */
    uint8_t signature[32];   /* 0x20 */
};

struct kfwp_section {
    char name[8];
    uint32_t offset;
    uint32_t size;
    uint32_t load_addr;
    uint32_t crc32;
};
#pragma pack(pop)

uint32_t kfwp_checksum(const uint8_t *image, size_t total_size, uint32_t device_id);
uint32_t kfwp_crc32(const uint8_t *data, size_t len);
void kfwp_crypt_section(uint8_t *data, size_t len, uint32_t device_id, uint64_t build_time);
int kfwp_verify(const uint8_t *image, size_t len, char *why, size_t why_len);
int kfwp_inspect(const uint8_t *image, size_t len);
size_t kfwp_pack(uint8_t *out, size_t cap, uint32_t device_id, uint64_t build_time, int encrypt,
                 const char *const names[], const uint8_t *const blobs[], const size_t sizes[],
                 const uint32_t load_addrs[], int count);

#endif
