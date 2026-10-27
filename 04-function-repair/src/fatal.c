#include <unistd.h>

#include "protect.h"

static const char k_fatal[] = "kestrel-licensed: fatal license error\n";

__attribute__((noreturn, noinline)) void die(int code)
{
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "0"(1L), "D"(2L), "S"(k_fatal), "d"(sizeof(k_fatal) - 1) : "rcx", "r11", "memory");
    __asm__ volatile("syscall" : : "a"(231L), "D"((long)code) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
