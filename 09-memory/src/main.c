/*
 * kestrel-fwpack - Kestrel Lockworks firmware container tool.
 *   kestrel-fwpack inspect <file.kfwp>
 *   kestrel-fwpack demo <out.kfwp>      (writes a sample container)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kfwp.h"

#ifndef BUILD_TAG
#define BUILD_TAG "dev"
#endif

static const char k_banner[] = "kestrel-fwpack 3.12 (" BUILD_TAG ")";

static size_t read_file(const char *path, uint8_t *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

static int write_demo(const char *path)
{
    static const uint8_t boot[] = "KESTREL-BOOT\x00\x10\x20\x30 stage1 loader v7";
    static const uint8_t app[] = "lock application image: motor driver + keypad + ble stack";
    const char *names[] = {"boot", "app"};
    const uint8_t *blobs[] = {boot, app};
    const size_t sizes[] = {sizeof(boot), sizeof(app)};
    const uint32_t loads[] = {0x08000000u, 0x08004000u};
    uint8_t out[1024];

    size_t n = kfwp_pack(out, sizeof(out), 0x4B530107u, 1791763200ull, 1, names, blobs, sizes, loads, 2);
    FILE *f = fopen(path, "wb");
    if (!f || !n)
        return 1;
    fwrite(out, 1, n, f);
    fclose(f);
    printf("wrote %zu bytes to %s\n", n, path);
    return 0;
}

int main(int argc, char **argv)
{
    static uint8_t image[65536];

    puts(k_banner);
    if (argc == 3 && strcmp(argv[1], "inspect") == 0) {
        size_t n = read_file(argv[2], image, sizeof(image));
        if (!n) {
            perror(argv[2]);
            return 1;
        }
        return kfwp_inspect(image, n) == 0 ? 0 : 1;
    }
    if (argc == 3 && strcmp(argv[1], "demo") == 0)
        return write_demo(argv[2]);

    fprintf(stderr, "usage: %s inspect <file.kfwp> | demo <out.kfwp>\n", argv[0]);
    return 2;
}
