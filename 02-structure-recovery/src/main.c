/*
 * kestrel-sessiond - Kestrel Lockworks gateway session daemon (offline replay build).
 *
 * Replays a session script from stdin so the state machine can be exercised
 * without a network:  HELLO <user> <addr> <port> | AUTH <id> <response> |
 * DOORS <id> <mask> | KEY <id> | FAILOVER <id> <addr> | RESTORE <line> |
 * DATA <id> <text> | EXPIRE <now> <max_idle> | DUMP
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "session.h"

static void on_session_event(struct kst_session *s, int event, void *ctx)
{
    FILE *log = ctx;
    fprintf(log, "[event] session #%u -> %d (%s)\n", s->id, event, session_state_name(s->state));
}

static int g_verbose = 1;

static struct kst_session *accept_client(const char *user, const char *addr, unsigned port)
{
    struct kst_session *s = session_new(user, addr, (uint16_t)port);
    if (!s)
        return NULL;
    s->on_event = on_session_event;
    s->event_ctx = stderr;
    s->flags |= KST_F_AUDIT;
    session_send_hello(s);
    session_touch(s, s->created_at);
    if (g_verbose)
        session_dump(s, stderr);
    printf("challenge #%u nonce=%08x\n", s->id, s->nonce);
    return s;
}

int main(void)
{
    char line[256];

    while (fgets(line, sizeof(line), stdin)) {
        char cmd[16], a[64], b[64];
        unsigned x = 0, y = 0;
        struct kst_session *s;

        line[strcspn(line, "\n")] = '\0';
        if (sscanf(line, "%15s", cmd) != 1)
            continue;

        if (!strcmp(cmd, "HELLO") && sscanf(line, "%*s %63s %63s %u", a, b, &x) == 3) {
            accept_client(a, b, x);
        } else if (!strcmp(cmd, "AUTH") && sscanf(line, "%*s %u %x", &x, &y) == 2 && (s = session_find(x))) {
            printf("auth #%u -> %d\n", x, session_authenticate(s, y));
        } else if (!strcmp(cmd, "DOORS") && sscanf(line, "%*s %u %x", &x, &y) == 2 && (s = session_find(x))) {
            printf("doors #%u -> %d\n", x, session_grant_doors(s, y));
        } else if (!strcmp(cmd, "KEY") && sscanf(line, "%*s %u", &x) == 1 && (s = session_find(x))) {
            uint8_t key[32];
            for (int i = 0; i < 32; i++)
                key[i] = (uint8_t)(i * 7 + x);
            session_enable_cipher(s, key);
        } else if (!strcmp(cmd, "FAILOVER") && sscanf(line, "%*s %u %63s", &x, a) == 2 && (s = session_find(x))) {
            struct kst_session *c = session_clone_for_failover(s, a);
            printf("failover #%u -> #%u\n", x, c ? c->id : 0);
        } else if (!strcmp(cmd, "RESTORE")) {
            struct kst_session *r = session_restore(line + 8);
            printf("restore -> #%u\n", r ? r->id : 0);
        } else if (!strcmp(cmd, "DATA") && sscanf(line, "%*s %u %63s", &x, a) == 2 && (s = session_find(x))) {
            printf("data #%u -> %d\n", x, session_feed(s, a, strlen(a)));
            session_touch(s, s->last_seen + strlen(a));
        } else if (!strcmp(cmd, "EXPIRE") && sscanf(line, "%*s %u %u", &x, &y) == 2) {
            printf("expired %d\n", session_expire_idle(x, y));
        } else if (!strcmp(cmd, "DUMP")) {
            for (uint32_t id = 1000; id < 1100; id++)
                if ((s = session_find(id)))
                    session_dump(s, stdout);
        }
    }
    return 0;
}
