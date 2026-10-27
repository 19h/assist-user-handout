#ifndef KPROTO_H
#define KPROTO_H
#include <stddef.h>
#include <stdint.h>

/* libkproto - unlock protocol plugin, loaded at runtime via dlopen(). */
struct kproto_ops {
    uint32_t abi;
    const char *name;
    size_t (*build_challenge)(const char *user, unsigned door, char *out, size_t cap);
};

typedef const struct kproto_ops *(*kproto_entry_fn)(void);

const struct kproto_ops *kproto_v1_entry(void);
const struct kproto_ops *kproto_v2_entry(void);

#endif
