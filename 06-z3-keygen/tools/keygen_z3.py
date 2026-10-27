#!/usr/bin/env python3
"""Reference keygen / ground truth for 06-z3-keygen (z3py).

Usage: keygen_z3.py [user ...]   -> prints one STANDARD and one ENTERPRISE key
per user and checks that LIFETIME is unreachable (UNSAT).
"""
import sys

import z3


def fnv1a(user):
    h = 0x811C9DC5
    for c in user.lower().encode():
        h ^= c
        h = (h * 0x01000193) & 0xFFFFFFFF
    return h


def base(h):
    w = [z3.BitVec("w%d" % i, 32) for i in range(4)]
    H = z3.BitVecVal(h, 32)
    s = z3.Solver()
    s.add((w[0] * 0x9E3779B1) ^ z3.RotateLeft(w[1], 13) == H ^ 0xC0FFEE11)
    s.add((((w[1] + 3 * w[2]) ^ z3.LShR(w[3], 7)) & 0xFFFF) == ((z3.RotateLeft(H, 7) + 0x1337) & 0xFFFF))
    s.add(z3.RotateLeft(w[2] ^ w[0], 5) - w[3] == H * 0x2545F491)
    s.add(((w[3] ^ 0xA5A5A5A5) * 0x01000193 + w[0]) == ~H)
    return s, w


def key(model, w):
    return "-".join("%08X" % model[x].as_long() for x in w)


def main(users):
    for user in users:
        h = fnv1a(user)
        s, w = base(h)
        s.push()
        s.add(z3.LShR(w[0] + w[1] + w[2] + w[3], 20) != 0x4B4)
        assert s.check() == z3.sat, user
        std = key(s.model(), w)
        s.pop()
        s.push()
        s.add(z3.LShR(w[0] + w[1] + w[2] + w[3], 20) == 0x4B4)
        ent_ok = s.check() == z3.sat
        ent = key(s.model(), w) if ent_ok else "-"
        s.add((w[3] ^ 0xA5A5A5A5) * 0x01000193 == z3.BitVecVal(h, 32) - w[0])
        life = s.check()
        s.pop()
        print("%-10s h=%08x STANDARD=%s ENTERPRISE=%s LIFETIME=%s" % (user, h, std, ent, life))


if __name__ == "__main__":
    main(sys.argv[1:] or ["hexrays", "kenan", "alice", "ilfak", "admin", "hex-rays", "company-week"])
