/* kestrel-cfgtool - inspect Kestrel site configuration files.
 *   kestrel-cfgtool dump <site.kcfg>
 *   kestrel-cfgtool get <site.kcfg> <key>  */
#include <stdio.h>
#include <string.h>

#include "kcfg.h"

int main(int argc, char **argv)
{
    static uint8_t buf[8192];
    static struct kcfg_entry entries[64];

    if (argc < 3 || (strcmp(argv[1], "dump") && strcmp(argv[1], "get")) || (!strcmp(argv[1], "get") && argc < 4)) {
        fprintf(stderr, "usage: %s dump <file> | get <file> <key>\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[2], "rb");
    if (!f) {
        perror(argv[2]);
        return 1;
    }
    size_t len = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    int n = kcfg_parse(buf, len, entries, 64);
    if (n < 0) {
        fprintf(stderr, "kestrel-cfgtool: invalid config (error %d)\n", n);
        return 1;
    }
    for (int i = 0; i < n; i++)
        if (!strcmp(argv[1], "dump") || !strcmp(entries[i].key, argv[3]))
            kcfg_print(&entries[i]);
    return 0;
}
