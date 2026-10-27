/*
 * kestrel-gatewayd 2.7.1 - Kestrel Lockworks site gateway daemon.
 * usage: kestrel-gatewayd [-c gatewayd.ini]   (control commands on stdin)
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "gatewayd.h"

int main(int argc, char **argv)
{
    static struct config cfg;
    const char *cfg_path = "/etc/kestrel/gatewayd.ini";
    int opt;

    while ((opt = getopt(argc, argv, "c:")) != -1)
        if (opt == 'c')
            cfg_path = optarg;

    config_defaults(&cfg);
    if (config_load(&cfg, cfg_path) != 0)
        fprintf(stderr, "gatewayd: %s not found, using factory defaults\n", cfg_path);
    log_init(config_get(&cfg, "log", "path", "gatewayd.log"));
    log_msg("kestrel-gatewayd 2.7.1 starting as %s", config_get(&cfg, "gateway", "device_id", "?"));

    char line[256];
    while (fgets(line, sizeof(line), stdin))
        dispatch_command(&cfg, line);

    log_flush();
    return 0;
}
