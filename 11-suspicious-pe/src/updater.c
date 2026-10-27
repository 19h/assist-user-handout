/*
 * KestrelUpdate.exe - simulated suspicious sample for the RE demo.
 *
 * Behaves like a typical updater implant *in structure only*: an RC4-encrypted
 * configuration, a host fingerprint and a check-in loop. It never opens a
 * socket, writes the registry or persists: it decrypts its configuration and
 * prints what it "would" do (dry run).
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "config_blob.h"

struct rc4_state {
    unsigned char s[256];
    unsigned char i, j;
};

__attribute__((noinline)) static void rc4_init(struct rc4_state *st, const unsigned char *key, size_t keylen)
{
    for (int i = 0; i < 256; i++)
        st->s[i] = (unsigned char)i;
    unsigned char j = 0;
    for (int i = 0; i < 256; i++) {
        j = (unsigned char)(j + st->s[i] + key[i % keylen]);
        unsigned char t = st->s[i];
        st->s[i] = st->s[j];
        st->s[j] = t;
    }
    st->i = st->j = 0;
}

__attribute__((noinline)) static void rc4_crypt(struct rc4_state *st, unsigned char *buf, size_t len)
{
    for (size_t n = 0; n < len; n++) {
        st->i++;
        st->j = (unsigned char)(st->j + st->s[st->i]);
        unsigned char t = st->s[st->i];
        st->s[st->i] = st->s[st->j];
        st->s[st->j] = t;
        buf[n] ^= st->s[(unsigned char)(st->s[st->i] + st->s[st->j])];
    }
}

/* Decrypts the embedded configuration into out (at least sizeof(k_cfg_blob)). */
__attribute__((noinline)) size_t decrypt_config(char *out)
{
    unsigned char key[sizeof(k_cfg_key_x)];
    struct rc4_state st;

    for (size_t i = 0; i < sizeof(key); i++)
        key[i] = k_cfg_key_x[i] ^ 0x5A;
    memcpy(out, k_cfg_blob, sizeof(k_cfg_blob));
    rc4_init(&st, key, sizeof(key));
    rc4_crypt(&st, (unsigned char *)out, sizeof(k_cfg_blob));
    memset(key, 0, sizeof(key));
    return sizeof(k_cfg_blob);
}

/* Minimal "json" field lookup: copies the string or number after "name": */
__attribute__((noinline)) static int cfg_field(const char *cfg, const char *name, char *out, size_t cap)
{
    char pat[32];
    snprintf(pat, sizeof(pat), "\"%s\":", name);
    const char *p = strstr(cfg, pat);
    if (!p)
        return 0;
    p += strlen(pat);
    if (*p == '"')
        p++;
    size_t n = 0;
    while (*p && *p != '"' && *p != ',' && *p != '}' && n + 1 < cap)
        out[n++] = *p++;
    out[n] = '\0';
    return 1;
}

__attribute__((noinline)) static unsigned host_fingerprint(void)
{
    char name[MAX_COMPUTERNAME_LENGTH + 1] = "unknown";
    DWORD len = sizeof(name);
    unsigned h = 0x811C9DC5u;

    GetComputerNameA(name, &len);
    for (const char *p = name; *p; p++)
        h = (h ^ (unsigned char)*p) * 0x01000193u;
    return h ^ GetCurrentProcessId();
}

int main(void)
{
    char cfg[sizeof(k_cfg_blob)];
    char c2[128], sleep_s[16], campaign[32];

    decrypt_config(cfg);
    cfg_field(cfg, "c2", c2, sizeof(c2));
    cfg_field(cfg, "sleep", sleep_s, sizeof(sleep_s));
    cfg_field(cfg, "campaign", campaign, sizeof(campaign));

    printf("KestrelUpdate 4.2 (dry run)\n");
    printf("host id %08x, campaign %s\n", host_fingerprint(), campaign);
    printf("would check in to %s every %s s - no network activity in this build\n", c2, sleep_s);
    SecureZeroMemory(cfg, sizeof(cfg));
    return 0;
}
