#pragma once
#include <stddef.h>
#include <stdint.h>
int vm_run(const uint8_t *code, size_t code_len, const char *input);
