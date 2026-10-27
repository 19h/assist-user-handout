#!/usr/bin/env python3
"""Builds a valid kestrel-licensed license blob (hex) - ground truth for the demo."""
import struct


def rotr(v, n):
    return ((v >> n) | (v << (32 - n))) & 0xFFFFFFFF


def mac(data):
    m = 0x6B657374
    for b in data[:16]:
        m = (rotr(m ^ b, 5) * 0x01000193) & 0xFFFFFFFF
    return m


body = b"KLIC" + struct.pack("<HHII", 3, 25, 20000, 0x07)
print((body + struct.pack("<I", mac(body))).hex())
