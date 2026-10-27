/* Local control channel commands (one command per line). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gatewayd.h"

static uint32_t g_doors;   /* bit n = door n unlocked */
static uint32_t g_uptime;
static int g_authed;

typedef int (*cmd_fn)(struct config *c, char *arg);

static int cmd_status(struct config *c, char *arg)
{
    (void)arg;
    printf("gateway %s up %us doors=%08x auth=%d\n", config_get(c, "gateway", "device_id", "?"), g_uptime, g_doors, g_authed);
    return 0;
}

static int cmd_login(struct config *c, char *arg)
{
    char user[32] = "", pass[64] = "";
    if (sscanf(arg, "%31s %63s", user, pass) != 2)
        return -1;
    g_authed = !strcmp(user, config_get(c, "admin", "user", "admin")) &&
               !strcmp(pass, config_get(c, "admin", "password", "kestrel"));
    log_msg("login %s: %s", user, g_authed ? "ok" : "denied");
    puts(g_authed ? "login ok" : "login denied");
    return g_authed ? 0 : 1;
}

static int cmd_unlock(struct config *c, char *arg)
{
    (void)c;
    int door = atoi(arg);
    if (!g_authed) {
        puts("unlock: not authorised");
        return 1;
    }
    if (door < 0 || door > 31)
        return -1;
    g_doors |= 1u << door;
    log_msg("door %d unlocked", door);
    printf("door %d unlocked\n", door);
    return 0;
}

static int cmd_pair(struct config *c, char *arg)
{
    uint32_t nonce = (uint32_t)strtoul(arg, NULL, 16);
    uint32_t code = pairing_code(config_get(c, "gateway", "device_secret", ""), nonce);
    printf("pairing code for nonce %08x: %06u\n", nonce, code);
    return 0;
}

static int cmd_telemetry(struct config *c, char *arg)
{
    (void)arg;
    int rc = telemetry_flush(c, g_uptime, g_doors);
    puts(rc == 0 ? "telemetry queued" : "telemetry failed");
    return rc;
}

static int cmd_set(struct config *c, char *arg)
{
    char sec[24], key[32], val[96];
    if (!g_authed || sscanf(arg, "%23[^.].%31s %95s", sec, key, val) != 3)
        return 1;
    log_msg("config %s.%s changed", sec, key);
    return 0;
}

static int cmd_tick(struct config *c, char *arg)
{
    (void)c;
    g_uptime += (uint32_t)atoi(arg);
    return 0;
}

static const struct {
    const char *name;
    cmd_fn fn;
    int needs_auth;
} k_commands[] = {
    {"status", cmd_status, 0},
    {"login", cmd_login, 0},
    {"unlock", cmd_unlock, 1},
    {"pair", cmd_pair, 0},
    {"telemetry", cmd_telemetry, 0},
    {"set", cmd_set, 1},
    {"tick", cmd_tick, 0},
};

int dispatch_command(struct config *c, const char *line)
{
    char buf[256], *arg;
    snprintf(buf, sizeof(buf), "%s", line);
    buf[strcspn(buf, "\r\n")] = '\0';
    arg = strchr(buf, ' ');
    if (arg)
        *arg++ = '\0';
    else
        arg = buf + strlen(buf);

    for (size_t i = 0; i < sizeof(k_commands) / sizeof(k_commands[0]); i++)
        if (!strcmp(buf, k_commands[i].name))
            return k_commands[i].fn(c, arg);
    printf("unknown command: %s\n", buf);
    return -1;
}
