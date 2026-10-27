/* Ring-buffer logger: lines are kept in memory and flushed in batches. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "gatewayd.h"

#define LOG_LINES 64
#define LOG_LINE_LEN 160

static char g_ring[LOG_LINES][LOG_LINE_LEN];
static unsigned g_head, g_count;
static char g_path[128] = "gatewayd.log";

void log_init(const char *path)
{
    snprintf(g_path, sizeof(g_path), "%s", path);
    g_head = g_count = 0;
}

void log_msg(const char *fmt, ...)
{
    char *slot = g_ring[(g_head + g_count) % LOG_LINES];
    int n = snprintf(slot, LOG_LINE_LEN, "[%ld] ", (long)time(NULL));
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(slot + n, LOG_LINE_LEN - (size_t)n, fmt, ap);
    va_end(ap);

    if (g_count < LOG_LINES)
        g_count++;
    else
        g_head = (g_head + 1) % LOG_LINES;
    if (g_count >= LOG_LINES / 2)
        log_flush();
}

void log_flush(void)
{
    FILE *f = fopen(g_path, "a");
    if (!f)
        return;
    for (unsigned i = 0; i < g_count; i++)
        fprintf(f, "%s\n", g_ring[(g_head + i) % LOG_LINES]);
    fclose(f);
    g_head = g_count = 0;
}
