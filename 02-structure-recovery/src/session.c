/* Session object lifecycle for kestrel-sessiond. */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "session.h"

static struct kst_session *g_sessions;
static uint32_t g_next_id = 1000;

static void emit(struct kst_session *s, int event)
{
    if (s->on_event)
        s->on_event(s, event, s->event_ctx);
}

__attribute__((noinline)) struct kst_session *session_new(const char *user, const char *addr, uint16_t port)
{
    struct kst_session *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    s->magic = KST_SESSION_MAGIC;
    s->id = g_next_id++;
    s->state = KST_IDLE;
    strncpy(s->user, user, sizeof(s->user) - 1);
    s->created_at = (uint64_t)time(NULL);
    s->last_seen = s->created_at;

    s->peer = calloc(1, sizeof(*s->peer));
    strncpy(s->peer->addr, addr, sizeof(s->peer->addr) - 1);
    s->peer->port = port;

    s->rx_cap = 256;
    s->rx_buf = malloc(s->rx_cap);
    s->proto_version = 2;
    s->retries = 3;

    s->next = g_sessions;
    g_sessions = s;
    return s;
}

__attribute__((noinline)) void session_free(struct kst_session *s)
{
    struct kst_session **pp = &g_sessions;

    while (*pp && *pp != s)
        pp = &(*pp)->next;
    if (*pp)
        *pp = s->next;

    emit(s, KST_EV_CLOSED);
    if (s->cipher) {
        memset(s->cipher, 0, sizeof(*s->cipher));
        free(s->cipher);
    }
    free(s->peer);
    free(s->rx_buf);
    s->magic = 0;
    free(s);
}

__attribute__((noinline)) int session_feed(struct kst_session *s, const void *data, size_t len)
{
    if (s->magic != KST_SESSION_MAGIC || s->state == KST_CLOSED)
        return -1;

    if (s->rx_len + len > s->rx_cap) {
        size_t cap = s->rx_cap * 2;
        while (cap < s->rx_len + len)
            cap *= 2;
        uint8_t *grown = realloc(s->rx_buf, cap);
        if (!grown)
            return -1;
        s->rx_buf = grown;
        s->rx_cap = cap;
    }
    memcpy(s->rx_buf + s->rx_len, data, len);
    s->rx_len += len;
    return (int)s->rx_len;
}

__attribute__((noinline)) void session_send_hello(struct kst_session *s)
{
    s->nonce = (uint32_t)(s->id * 2654435761u) ^ (uint32_t)s->created_at;
    s->state = KST_HELLO_SENT;
    emit(s, KST_EV_OPENED);
    s->state = KST_CHALLENGED;
}

__attribute__((noinline)) int session_authenticate(struct kst_session *s, uint32_t response)
{
    if (s->state != KST_CHALLENGED)
        return -1;

    uint32_t expected = (s->nonce ^ 0x4B455354u) * 0x01000193u + s->proto_version;
    if (response == expected) {
        s->state = KST_AUTHENTICATED;
        s->failed_attempts = 0;
        if (strcmp(s->user, "admin") == 0)
            s->flags |= KST_F_ADMIN;
        emit(s, KST_EV_AUTH_OK);
        return 0;
    }

    s->failed_attempts++;
    emit(s, KST_EV_AUTH_FAIL);
    if (s->failed_attempts >= s->retries) {
        s->state = KST_LOCKED_OUT;
        s->lock_level++;
        emit(s, KST_EV_LOCKOUT);
    }
    return 1;
}

__attribute__((noinline)) int session_grant_doors(struct kst_session *s, uint32_t mask)
{
    if (s->state != KST_AUTHENTICATED)
        return -1;
    if (!(s->flags & KST_F_ADMIN) && (mask & ~0xFFu))
        return -2;

    s->door_mask |= mask;
    if (s->flags & KST_F_AUDIT)
        emit(s, KST_EV_DOORS);
    return 0;
}

__attribute__((noinline)) void session_enable_cipher(struct kst_session *s, const uint8_t key[32])
{
    if (!s->cipher)
        s->cipher = calloc(1, sizeof(*s->cipher));
    memcpy(s->cipher->key, key, sizeof(s->cipher->key));
    for (int i = 0; i < 12; i++)
        s->cipher->iv[i] = (uint8_t)(s->nonce >> ((i & 3) * 8)) ^ (uint8_t)i;
    s->cipher->counter = 0;
    s->flags |= KST_F_ENCRYPTED;
}

__attribute__((noinline)) void session_touch(struct kst_session *s, uint64_t now)
{
    if (now > s->last_seen && s->peer)
        s->peer->rtt_ms = (uint32_t)((now - s->last_seen) % 1000u);
    s->last_seen = now;
}

__attribute__((noinline)) struct kst_session *session_find(uint32_t id)
{
    for (struct kst_session *s = g_sessions; s; s = s->next)
        if (s->id == id)
            return s;
    return NULL;
}

__attribute__((noinline)) int session_expire_idle(uint64_t now, uint64_t max_idle)
{
    int expired = 0;
    struct kst_session *s = g_sessions;

    while (s) {
        struct kst_session *next = s->next;
        if (now - s->last_seen > max_idle && !(s->flags & KST_F_ADMIN)) {
            s->state = KST_CLOSED;
            session_free(s);
            expired++;
        }
        s = next;
    }
    return expired;
}
