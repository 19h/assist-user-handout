#!/usr/bin/env python3
"""Writes site.kcfg, the sample Kestrel site configuration (ground truth in
expected/site.json)."""
import json
import os
import struct

ENTRIES = [
    (5, "gateway.ip", bytes([10, 20, 0, 1])),
    (1, "gateway.port", 7443),
    (2, "site.name", "Hex-Rays HQ, Liege"),
    (2, "site.admin", "facility@kestrel.example"),
    (1, "doors.count", 24),
    (3, "doors.auto_relock", True),
    (1, "doors.relock_seconds", 8),
    (2, "ble.pairing_code", "KS-PAIR-7741"),
    (4, "ble.irk", bytes.fromhex("8f3c2a91d05e7b4466f1a0c3b2d95e17")),
    (3, "telemetry.enabled", False),
]


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


out = bytearray(b"KCFG" + bytes([2, 0]) + struct.pack("<H", len(ENTRIES)))
truth = {}
for typ, key, val in ENTRIES:
    out += bytes([typ, len(key)]) + key.encode()
    if typ == 1:
        out += struct.pack("<i", val); truth[key] = val
    elif typ == 2:
        raw = val.encode()
        out += struct.pack("<H", len(raw)) + bytes(c ^ ((0x5C + i) & 0xFF) for i, c in enumerate(raw)); truth[key] = val
    elif typ == 3:
        out += bytes([1 if val else 0]); truth[key] = val
    elif typ == 4:
        out += struct.pack("<H", len(val)) + val; truth[key] = val.hex()
    elif typ == 5:
        out += val; truth[key] = ".".join(map(str, val))
out += struct.pack("<H", crc16(out))
here = os.path.dirname(os.path.abspath(__file__))
open(os.path.join(here, "..", "site.kcfg"), "wb").write(out)
json.dump(truth, open(os.path.join(here, "..", "expected", "site.json"), "w"), indent=2)
print("site.kcfg: %d bytes" % len(out))
