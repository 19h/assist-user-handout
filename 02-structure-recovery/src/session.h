#ifndef KST_SESSION_H
#define KST_SESSION_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define KST_SESSION_MAGIC 0x5345534Bu /* "KSES" */

enum kst_state {
    KST_IDLE = 0,
    KST_HELLO_SENT = 1,
    KST_CHALLENGED = 2,
    KST_AUTHENTICATED = 3,
    KST_LOCKED_OUT = 4,
    KST_CLOSED = 5,
};

enum kst_flags {
    KST_F_ENCRYPTED = 0x01,
    KST_F_ADMIN = 0x02,
    KST_F_REMOTE = 0x04,
    KST_F_AUDIT = 0x08,
};

enum kst_event {
    KST_EV_OPENED = 1,
    KST_EV_AUTH_OK = 2,
    KST_EV_AUTH_FAIL = 3,
    KST_EV_LOCKOUT = 4,
    KST_EV_DOORS = 5,
    KST_EV_CLOSED = 6,
};

struct kst_peer {
    char addr[46];
    uint16_t port;
    uint32_t rtt_ms;
};

struct kst_cipher {
    uint8_t key[32];
    uint8_t iv[12];
    uint64_t counter;
};

struct kst_session;
typedef void (*kst_event_fn)(struct kst_session *s, int event, void *ctx);

struct kst_session {
    uint32_t magic;             /* 0x00 */
    uint32_t id;                /* 0x04 */
    enum kst_state state;       /* 0x08 */
    uint32_t flags;             /* 0x0c */
    char user[32];              /* 0x10 */
    uint64_t created_at;        /* 0x30 */
    uint64_t last_seen;         /* 0x38 */
    struct kst_peer *peer;      /* 0x40 */
    struct kst_cipher *cipher;  /* 0x48 */
    uint32_t failed_attempts;   /* 0x50 */
    uint32_t door_mask;         /* 0x54 */
    uint8_t *rx_buf;            /* 0x58 */
    size_t rx_len;              /* 0x60 */
    size_t rx_cap;              /* 0x68 */
    kst_event_fn on_event;      /* 0x70 */
    void *event_ctx;            /* 0x78 */
    struct kst_session *next;   /* 0x80 */
    uint16_t proto_version;     /* 0x88 */
    uint8_t retries;            /* 0x8a */
    uint8_t lock_level;         /* 0x8b */
    uint32_t nonce;             /* 0x8c */
};                              /* sizeof == 0x90 */

struct kst_session *session_new(const char *user, const char *addr, uint16_t port);
struct kst_session *session_clone_for_failover(const struct kst_session *src, const char *addr);
struct kst_session *session_restore(const char *line);
void session_free(struct kst_session *s);

int session_feed(struct kst_session *s, const void *data, size_t len);
void session_send_hello(struct kst_session *s);
int session_authenticate(struct kst_session *s, uint32_t response);
int session_grant_doors(struct kst_session *s, uint32_t mask);
void session_enable_cipher(struct kst_session *s, const uint8_t key[32]);
void session_touch(struct kst_session *s, uint64_t now);
struct kst_session *session_find(uint32_t id);
int session_expire_idle(uint64_t now, uint64_t max_idle);
void session_dump(const struct kst_session *s, FILE *out);
const char *session_state_name(enum kst_state st);

#endif
