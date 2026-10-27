/* Minimal INI parser: [section] key = value, '#' comments. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "gatewayd.h"

static void set_entry(struct config *c, const char *sec, const char *key, const char *val)
{
    for (int i = 0; i < c->n; i++) {
        if (!strcmp(c->e[i].section, sec) && !strcmp(c->e[i].key, key)) {
            snprintf(c->e[i].value, sizeof(c->e[i].value), "%s", val);
            return;
        }
    }
    if (c->n == CFG_MAX)
        return;
    struct cfg_entry *e = &c->e[c->n++];
    snprintf(e->section, sizeof(e->section), "%s", sec);
    snprintf(e->key, sizeof(e->key), "%s", key);
    snprintf(e->value, sizeof(e->value), "%s", val);
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return s;
}

void config_defaults(struct config *c)
{
    memset(c, 0, sizeof(*c));
    set_entry(c, "gateway", "device_id", "KGW-000000");
    set_entry(c, "gateway", "device_secret", "kestrel-factory-secret");
    set_entry(c, "admin", "user", "admin");
    set_entry(c, "admin", "password", "kestrel");
    set_entry(c, "telemetry", "outbox", "telemetry.out");
    set_entry(c, "telemetry", "endpoint", "https://telemetry.kestrel.example/v2/ingest");
    set_entry(c, "telemetry", "interval", "300");
    set_entry(c, "log", "path", "gatewayd.log");
}

int config_load(struct config *c, const char *path)
{
    char line[256], section[24] = "global";
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line);
        if (!*s || *s == '#' || *s == ';')
            continue;
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (end) {
                *end = '\0';
                snprintf(section, sizeof(section), "%s", trim(s + 1));
            }
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq)
            continue;
        *eq = '\0';
        set_entry(c, section, trim(s), trim(eq + 1));
    }
    fclose(f);
    return 0;
}

const char *config_get(const struct config *c, const char *section, const char *key, const char *dflt)
{
    for (int i = 0; i < c->n; i++)
        if (!strcmp(c->e[i].section, section) && !strcmp(c->e[i].key, key))
            return c->e[i].value;
    return dflt;
}
