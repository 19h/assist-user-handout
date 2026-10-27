/* KVM: the tiny bytecode VM Kestrel uses to hide its license/password checks. */
#include <stddef.h>
#include <stdint.h>

#include "vm.h"

enum {
    OP_HALT = 0x3C, OP_LDI = 0x91, OP_LDB = 0x5E, OP_LEN = 0xC2, OP_ADD = 0x17, OP_SUB = 0x2A,
    OP_XOR = 0x88, OP_MULI = 0x4D, OP_ROL8 = 0xE3, OP_ANDI = 0x76, OP_CMPI = 0xB9, OP_JNZ = 0x05,
    OP_JMP = 0xD0, OP_FAIL = 0x6F, OP_OK = 0xA4,
};

static uint32_t imm32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

__attribute__((noinline)) int vm_run(const uint8_t *code, size_t code_len, const char *input)
{
    uint32_t r[8] = {0};
    size_t pc = 0;
    int z = 0, steps = 0;

    while (pc < code_len && steps++ < 100000) {
        uint8_t op = code[pc++];
        switch (op) {
        case OP_HALT: return -1;
        case OP_LDI:  r[code[pc] & 7] = imm32(code + pc + 1); pc += 5; break;
        case OP_LDB:  r[code[pc] & 7] = (uint8_t)input[code[pc + 1]]; pc += 2; break;
        case OP_LEN: {
            uint32_t n = 0;
            while (input[n]) n++;
            r[code[pc] & 7] = n; pc += 1; break;
        }
        case OP_ADD:  r[code[pc] & 7] += r[code[pc + 1] & 7]; pc += 2; break;
        case OP_SUB:  r[code[pc] & 7] -= r[code[pc + 1] & 7]; pc += 2; break;
        case OP_XOR:  r[code[pc] & 7] ^= r[code[pc + 1] & 7]; pc += 2; break;
        case OP_MULI: r[code[pc] & 7] *= imm32(code + pc + 1); pc += 5; break;
        case OP_ROL8: {
            uint32_t v = r[code[pc] & 7] & 0xFF, n = code[pc + 1] & 7;
            r[code[pc] & 7] = ((v << n) | (v >> (8 - n))) & 0xFF; pc += 2; break;
        }
        case OP_ANDI: r[code[pc] & 7] &= imm32(code + pc + 1); pc += 5; break;
        case OP_CMPI: z = r[code[pc] & 7] == imm32(code + pc + 1); pc += 5; break;
        case OP_JNZ: {
            int16_t rel = (int16_t)(code[pc] | code[pc + 1] << 8);
            pc += 2;
            if (!z) pc += rel;
            break;
        }
        case OP_JMP: {
            int16_t rel = (int16_t)(code[pc] | code[pc + 1] << 8);
            pc += 2 + rel;
            break;
        }
        case OP_FAIL: return 0;
        case OP_OK:   return 1;
        default:      return -2;
        }
    }
    return -3;
}
