#ifndef PROTECT_H
#define PROTECT_H

/*
 * Anti-disassembly: an opaque jz/jnz pair that always lands on the label,
 * followed by a 0xE8 byte. A linear-sweep or fall-through disassembler decodes
 * the 0xE8 as `call rel32`, swallowing the first four bytes of the real code.
 */
#define ANTI_DISASM()                     \
    __asm__ volatile("jz 1f\n\t"          \
                     "jnz 1f\n\t"         \
                     ".byte 0xE8\n\t"     \
                     "1:\n\t" ::: "cc")

/* Fatal-error exit that the toolchain knows is noreturn but the binary hides:
 * it leaves via a raw exit_group syscall and has no ret instruction. */
__attribute__((noreturn)) void die(int code);

#endif
