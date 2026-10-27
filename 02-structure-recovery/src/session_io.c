/* Session persistence, failover and diagnostics for kestrel-sessiond. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "session.h"

__attribute__((noinline)) const char *session_state_name(enum kst_state st)
{
    switch (st) {
    case KST_IDLE:          return "IDLE";
    case KST_HELLO_SENT:    return "HELLO_SENT";
    case KST_CHALLENGED:    return "CHALLENGED";
    case KST_AUTHENTICATED: return "AUTHENTICATED";
    case KST_LOCKED_OUT:    return "LOCKED_OUT";
    case KST_CLOSED:        return "CLOSED";
    }
    return "?";
}

__attribute__((noinline)) struct kst_session *session_clone_for_failover(const struct kst_session *src, const char *addr)
{
    struct kst_session *s = session_new(src->user, addr, src->peer ? src->peer->port : 0);
    if (!s)
        return NULL;

    s->flags = src->flags & ~KST_F_ENCRYPTED;
    if (src->cipher)
        session_enable_cipher(s, src->cipher->key);
    s->door_mask = src->door_mask;
    s->proto_version = src->proto_version;
    s->on_event = src->on_event;
    s->event_ctx = src->event_ctx;
    s->flags |= KST_F_REMOTE;
    return s;
}

/* Snapshot line: "<user> <addr> <port> <flags> <door_mask> <lock_level>" */
__attribute__((noinline)) struct kst_session *session_restore(const char *line)
{
    char user[32], addr[46];
    unsigned port, flags, doors, level;

    if (sscanf(line, "%31s %45s %u %x %x %u", user, addr, &port, &flags, &doors, &level) != 6)
        return NULL;

    struct kst_session *s = session_new(user, addr, (uint16_t)port);
    if (!s)
        return NULL;
    s->flags = flags;
    s->door_mask = doors;
    s->lock_level = (uint8_t)level;
    if (level > 2)
        s->state = KST_LOCKED_OUT;
    session_touch(s, s->created_at);
    return s;
}

__attribute__((noinline)) void session_dump(const struct kst_session *s, FILE *out)
{
    fprintf(out, "session #%u [%s] user=%s flags=%#x doors=%#x\n",
            s->id, session_state_name(s->state), s->user, s->flags, s->door_mask);
    fprintf(out, "  peer=%s:%u rtt=%ums proto=v%u retries=%u lock_level=%u failed=%u\n",
            s->peer ? s->peer->addr : "-", s->peer ? s->peer->port : 0,
            s->peer ? s->peer->rtt_ms : 0, s->proto_version, s->retries, s->lock_level,
            s->failed_attempts);
    fprintf(out, "  created=%" PRIu64 " last_seen=%" PRIu64 " rx=%zu/%zu cipher=%s nonce=%08x\n",
            s->created_at, s->last_seen, s->rx_len, s->rx_cap,
            s->cipher ? "on" : "off", s->nonce);
}
