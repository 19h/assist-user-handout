/* libkproto: unlock challenge formats. Loaded by lockctl with dlopen(). */
#include <stdio.h>

#include "kestrel_export.h"
#include "kproto.h"

static uint32_t user_nonce(const char *user)
{
    uint32_t h = 0x811C9DC5u;

    for (const unsigned char *p = (const unsigned char *)user; *p; ++p)
        h = (h ^ *p) * 0x01000193u;
    return h;
}

static size_t build_challenge_v1(const char *user, unsigned door, char *out, size_t cap)
{
    return (size_t)snprintf(out, cap, "KLK1:%s:%u", user, door);
}

static size_t build_challenge_v2(const char *user, unsigned door, char *out, size_t cap)
{
    uint32_t nonce = user_nonce(user) ^ (door * 0x9E3779B9u);
    return (size_t)snprintf(out, cap, "KLK2|%s|%u|%08x", user, door, nonce);
}

static const struct kproto_ops k_v1 = {1, "kestrel-unlock/1", build_challenge_v1};
static const struct kproto_ops k_v2 = {2, "kestrel-unlock/2", build_challenge_v2};

KESTREL_API const struct kproto_ops *kproto_v1_entry(void)
{
    return &k_v1;
}

KESTREL_API const struct kproto_ops *kproto_v2_entry(void)
{
    return &k_v2;
}
