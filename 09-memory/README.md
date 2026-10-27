# 09 · Memory: the same program twice, and one of them remembers

| | |
|---|---|
| Open | **two IDA instances**: `without-memory/kestrel-fwpack.i64` and `with-memory/kestrel-fwpack.i64` |
| Binaries | x86-64 Linux ELF (PIE), Debian GCC 12 / glibc, `-O2`, stripped. **Identical code**; only an embedded build tag differs (`build 4711` / `build 4712`). |
| You'll try | Assist's persistent memory: importing a colleague's notes, automatic recall, provenance, building on prior findings |
| Agent time | 1–3 minutes per instance |

## The program

`kestrel-fwpack` packs and inspects KFWP firmware containers: a 0x40-byte header (magic, BCD version, sizes, a header checksum, device id, section count, flags, build time, a signature), a section table, and encrypted sections with a CRC each. `sample.kfwp` next to each binary is a container from the update server; `ground-truth/inspect.txt` shows what the tool prints for it.

The question you will ask is deceptively simple: how is the header checksum computed, and which bytes does it cover? GCC turned the "skip the signature" logic into an index trick that takes real reasoning to untangle.

## Why two binaries

Assist keeps what it learns **per binary**, keyed by the file's hash, in your Assist profile rather than in the `.i64`. Two byte-identical files would share one memory store, so the two builds differ by a build tag. The notes in `memory/` were written for the `with-memory` build and will only import into that one.

## Before you start

1. Open the two databases in two IDA instances, `without-memory` on the left and `with-memory` on the right. Close every other IDA instance.
2. **Mute the peer in each instance:** click the Linked badge, open the Mesh Nodes dialog and untick the other node. Do it in both instances; muting applies only where you set it. Otherwise the "no memory" agent could read the other database over the mesh.
3. In the **with-memory** instance, import the colleague's notes: type `/memory` to open the knowledge browser, then More ▸ **Import proposals…** and choose `memory/seed.jsonl`. Six notes arrive as proposals for review; **accept** each one. They are marked as foreign, imported knowledge, which is the "a colleague sent me their findings" story. The notes are written in plain language (`memory/facts.json` is the readable version): the container format, the checksum algorithm, the section encryption, pitfalls, a handover note, and one note anchored to the checksum function.
4. Leave the **without-memory** instance alone: no imports, nothing to recall.
5. Settings ▸ Advanced ▸ Persistent memory: *Allow memory recall* and *Include memory in prompts* should be on (they are by default).

## Prompts

Paste into both instances, left one first:

```
How is the checksum in the firmware container header computed, and which bytes does it cover?
```

For an almost instant "it just knows" answer in the with-memory instance:

```
From what we already know about this binary: how is the firmware container header checksum computed and which bytes does it cover? Short answer, no need to re-verify.
```

Follow-up, in the with-memory instance:

```
Write me a Python script that verifies a firmware container - header checksum, decryption and every section CRC - and run it against sample.kfwp.
```

## What to expect

* **without-memory:** the agent investigates. It finds the string `header checksum mismatch (stored %08x, computed %08x)`, decompiles the verifier `sub_1540`, then the checksum `sub_1410`, and has to see through the compiler's trick: the loop runs an index from -16, reads `a1 + v6 + 32`, and only sums when the index is above `0x1F` unsigned. That means "0x10..0x1F and 0x40..end". Expect 15–25 tool calls and 2–3 minutes. The reference run (21 tool calls, 2 min 11 s) got it right, and as a bonus found a real bug: the section bounds check `offset + size` wraps in 32 bits, allowing an out-of-bounds read after a forged checksum. That bug is genuinely in `container.c`.
* **with-memory:** the recalled notes are in the context of the very first turn. The agent goes straight to `sub_1410`, cites the notes, confirms them against the code with a single emulation, and answers. The reference run took 8 tool calls and 76 seconds, with the remark *"Your stored notes on this checksum match the code exactly."* It also said which claim it could not check (the sample file is not in the database).
* Memory is injected as attributed claims to verify, not as truth. The agent uses it to skip the search and go straight to confirmation: the same correct answer, about half the time and a third of the tool calls.
* The follow-up builds on the remembered layout and writes a verifier. `tools/kfwp.py` is the reference: `checksum stored 8995847e computed 8995847e -> OK`, both section CRCs OK.

Try this too: ask the same question a second time in a **new conversation** in the without-memory instance. Assist remembers its own findings as well, so the second run should already be shorter. `/memory` shows what it kept.

## Check your results

```
checksum(image, total, device_id):
    a, b = 0x4B53, 0
    for i in 0x10 .. total-1:
        if 0x20 <= i < 0x40: continue        # the signature is excluded
        a = (a + image[i]) % 65521
        b = (b + a) % 65521
    return ((b << 16) | a) ^ rotl32(device_id, 8)
```

An Adler-32 variant seeded with `0x4B53` instead of 1, over `0x10..end` minus the signature, XORed with the rotated device id. For `sample.kfwp` (device `0x4B530107`, two encrypted sections `boot` and `app`) the checksum is `0x8995847E`.

| Function | Offset (both builds) |
|---|---|
| `main` | `0x1110` |
| `kfwp_checksum` | `0x1410` |
| `kfwp_crc32` | `0x14A0` |
| `kfwp_crypt_section` | `0x14F0` |
| `kfwp_verify` | `0x1540` |
| `kfwp_inspect` | `0x1730` |
| `kfwp_pack` | `0x1840` |

## If it gets stuck

* If the left agent queries the right database, the peers aren't muted (step 2).
* If the import is refused, you picked the `without-memory` database: the notes are bound to the `with-memory` binary's hash.
* To get a clean "no memory" run again later, open `/memory` in that instance and review what was saved, or ask Assist to forget what it remembered about this binary. Alternatively use the other build: the code is identical.
* Without the import you can still try memory by hand: right-click in `sub_1410` and use **Add local memory**, or add global notes from `memory/facts.json`.

## Files

* `with-memory/`, `without-memory/`: one build each, its database, and `sample.kfwp`.
* `memory/seed.jsonl`: the importable notes; `memory/facts.json`: the same notes, readable.
* `src/`: sources (`checksum.c` is the answer).
* `tools/kfwp.py`: reference parser and verifier.
* `ground-truth/`: `inspect.txt`, `symbols.txt`.
