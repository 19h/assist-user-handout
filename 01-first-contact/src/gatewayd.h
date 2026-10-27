#ifndef GATEWAYD_H
#define GATEWAYD_H
#include <stddef.h>
#include <stdint.h>

/* config.c */
#define CFG_MAX 64
struct cfg_entry { char section[24]; char key[32]; char value[96]; };
struct config { struct cfg_entry e[CFG_MAX]; int n; };
int config_load(struct config *c, const char *path);
void config_defaults(struct config *c);
const char *config_get(const struct config *c, const char *section, const char *key, const char *dflt);

/* logbuf.c */
void log_init(const char *path);
void log_msg(const char *fmt, ...);
void log_flush(void);

/* crypto.c */
uint32_t crc32_ieee(const uint8_t *p, size_t n);
void sha256(const uint8_t *data, size_t len, uint8_t out[32]);
void xtea_encrypt_blocks(uint8_t *buf, size_t len, const uint32_t key[4]);
size_t base64_encode(const uint8_t *in, size_t n, char *out);

/* telemetry.c */
int telemetry_flush(const struct config *c, uint32_t uptime, uint32_t door_bits);
uint32_t pairing_code(const char *device_secret, uint32_t nonce);

/* commands.c */
int dispatch_command(struct config *c, const char *line);

#endif
