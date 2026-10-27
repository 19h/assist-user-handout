/* kestrel-vm - unlocks the Kestrel installer's maintenance mode.  usage: kestrel-vm <password> */
#include <stdio.h>

#include "program.h"
#include "vm.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <maintenance password>\n", argv[0]);
        return 2;
    }
    int rc = vm_run(k_check_program, sizeof(k_check_program), argv[1]);
    if (rc == 1) {
        puts("Maintenance mode unlocked. Welcome, engineer.");
        return 0;
    }
    puts(rc == 0 ? "Wrong password." : "VM error.");
    return 1;
}
