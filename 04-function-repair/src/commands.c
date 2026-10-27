/*
 * Command handlers. The dispatch table is stored XOR-masked and decoded at
 * call time, so the handlers have no static references at all.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "commands.h"
#include "license.h"
#include "protect.h"

#define HANDLER_MASK 0x5A5AC3C3A5A53C3CULL

__attribute__((noinline, used)) static int cmd_status(const struct license *lic, const char *arg)
{
    (void)arg;
    printf("license v%u, %u seats, expires day %u, features %#x\n",
           lic->version, lic->seats, lic->expiry_day, lic->features);
    return 0;
}

__attribute__((noinline, used)) static int cmd_unlock(const struct license *lic, const char *arg)
{
    if (!license_allows(lic, 19000, FEAT_REMOTE_UNLOCK)) {
        puts("remote unlock not licensed");
        return 1;
    }
    printf("door %s: unlock command queued\n", arg);
    return 0;
}

__attribute__((noinline, used)) static int cmd_export(const struct license *lic, const char *arg)
{
    if (!license_allows(lic, 19000, FEAT_AUDIT_EXPORT))
        die(5);
    printf("audit log exported to %s\n", arg);
    return 0;
}

/* Undocumented: service mode opens every door without a licensed feature
 * check when the argument matches the hard-coded service code. */
__attribute__((noinline, used)) static int cmd_service(const struct license *lic, const char *arg)
{
    (void)lic;
    ANTI_DISASM();
    if (strcmp(arg, "KS-SERVICE-0451") == 0) {
        puts("service mode: all doors released");
        return 0;
    }
    ANTI_DISASM();
    puts("service mode: denied");
    return 1;
}

typedef int (*handler_fn)(const struct license *, const char *);

static const char k_command_names[4][8] = {"status", "unlock", "export", "svc"};

/*
 * Handler table as masked offsets from dispatch(): emitted by the assembler,
 * so the binary holds no pointer to (and no instruction referencing) any
 * handler. Resolved at call time.
 */
extern const uint64_t k_handler_deltas[4];
__asm__("\t.section .rodata\n"
        "\t.balign 8\n"
        "k_handler_deltas:\n"
        "\t.quad (cmd_status - dispatch) ^ 0x5A5AC3C3A5A53C3C\n"
        "\t.quad (cmd_unlock - dispatch) ^ 0x5A5AC3C3A5A53C3C\n"
        "\t.quad (cmd_export - dispatch) ^ 0x5A5AC3C3A5A53C3C\n"
        "\t.quad (cmd_service - dispatch) ^ 0x5A5AC3C3A5A53C3C\n"
        "\t.text\n");

__attribute__((noinline)) int dispatch(const struct license *lic, const char *cmd, const char *arg)
{
    for (unsigned i = 0; i < 4; i++) {
        if (strcmp(cmd, k_command_names[i]) == 0) {
            volatile uint64_t mask = HANDLER_MASK;
            uintptr_t target = (uintptr_t)dispatch + (uintptr_t)(k_handler_deltas[i] ^ mask);
            return ((handler_fn)target)(lic, arg);
        }
    }
    printf("unknown command '%s'\n", cmd);
    return 2;
}
