#!/usr/bin/env python3
"""Reference KFWP parser/verifier (ground truth for 09-memory; also the shape of
the answer to the follow-up prompt "write me a Python verifier/packer")."""
import struct
import sys
import zlib


def rotl32(v, n):
    return ((v << n) | (v >> (32 - n))) & 0xFFFFFFFF


def checksum(image, total, device_id):
    a, b = 0x4B53, 0
    for i in range(0x10, total):
        if 0x20 <= i < 0x40:
            continue
        a = (a + image[i]) % 65521
        b = (b + a) % 65521
    return ((b << 16) | a) ^ rotl32(device_id, 8)


def crypt(data, device_id, build_time):
    return bytes(c ^ ((device_id >> (8 * (i % 4))) & 0xFF) ^ ((build_time >> (8 * (i % 8))) & 0xFF) ^ ((i * 0x1D) & 0xFF)
                 for i, c in enumerate(data))


def main(path):
    img = open(path, "rb").read()
    magic, vmaj, vmin, hsz, total, csum, dev, nsec, flags, btime = struct.unpack_from("<4sBBHIIIHHQ", img, 0)
    bcd = lambda v: (v >> 4) * 10 + (v & 15)
    calc = checksum(img, total, dev)
    print("reference: %s v%d.%02d device %08x sections %d flags %#x checksum stored %08x computed %08x -> %s"
          % (magic.decode(), bcd(vmaj), bcd(vmin), dev, nsec, flags, csum, calc, "OK" if calc == csum else "MISMATCH"))
    for i in range(nsec):
        name, off, size, load, crc = struct.unpack_from("<8sIIII", img, 0x40 + 0x18 * i)
        plain = img[off:off + size]
        if flags & 1:
            plain = crypt(plain, dev, btime)
        print("  %-8s crc %08x %s  %r" % (name.rstrip(b"\0").decode(), crc,
                                         "OK" if zlib.crc32(plain) == crc else "BAD", plain[:24]))


if __name__ == "__main__":
    main(sys.argv[1])
