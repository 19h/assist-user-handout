# 08 · Patch diff: find the security fix, explain the bug, prove it

| | |
|---|---|
| Open | **two IDA instances**: `kestrel-bled-1.0.i64` and `kestrel-bled-1.1.i64` |
| Binaries | x86-64 Linux ELF (PIE), Debian GCC 12 / glibc, `-O2 -fno-stack-protector`, stripped. 1.0 is vulnerable; 1.1 is the "security release". |
| You'll try | diffing two versions across linked databases, vulnerability analysis, building a regression-test input, verifying it by emulation in each version |
| Agent time | 3–5 minutes |

## The program

`kestrel-bled` is the BLE gateway daemon. It parses TLV packets of the form `B1 | type | seq16 | {tag len value}*` and dispatches a message structure to a completion callback. The vendor shipped 1.1 with "security fixes" and no details. Both versions are here, along with noise changes that have nothing to do with security (a new TLV, a reworded log line).

What actually changed:

| | 1.0.3 | 1.1.0 |
|---|---|---|
| Name TLV (`0x09`) length check | `int8_t room = (int8_t)(len - 1); if (room >= 32) reject;` so lengths `0x81`–`0xFF` wrap negative and pass | `if (len == 0 \|\| len > 31) reject;` |
| New TLV `0x0A` (tx-power) | no | yes, noise |
| Reject log line | `bled: rejected packet %u (%s)` | `bled[%u]: packet rejected: %s`, noise |
| Message struct | `name[32]` at +4, `on_complete` function pointer at +0x28 | gains a `tx_power` byte, noise |

`memcpy(msg->name, p, len)` with `len >= 0x81` overwrites `on_complete`, and `parse_packet` then calls `if (m->on_complete) m->on_complete(m)`: a control-flow hijack.

`ground-truth/packets.txt` has a benign packet and the triggering one (`B1 01 07 00 09 81` followed by 0x81 bytes of `A`, 135 bytes in total). `ground-truth/runs.txt` shows both versions on the benign packet, 1.1 rejecting the bad one, and an AddressSanitizer build of 1.0 reporting the 129-byte stack write.

## Before you start

1. Open both databases in two IDA instances and **open the Assist panel once in each** (**Ctrl+Shift+K**). The badge reads **Linked 2**.
2. Close or mute every other IDA instance.
3. Type the prompt in the **1.0** instance.

## Prompts

```
kestrel-bled 1.1 is a security release of 1.0 and both are linked. Diff the two databases, find the security fix, and explain the bug it fixes in 1.0 and its impact. Then construct a minimal regression-test packet for the fix and confirm by emulating the packet parser in each database that 1.0 mishandles it while 1.1 rejects it.
```

Some models decline to construct the triggering input themselves even after analysing the bug. If yours does, use the "triage a fuzzer crash" version, which is the everyday workflow anyway:

```
Our fuzzer found this crashing input for kestrel-bled 1.0: b10107000981414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141414141
Both 1.0 and the 1.1 security release are linked. Find the root cause in 1.0, show me the fix in 1.1, and confirm by emulating the packet parser in each database with this input.
```

Follow-up:

```
Write this up as a short advisory: affected versions, root cause, impact, regression test, and the fix.
```

## What to expect

1. `list_functions` or `search_strings` with `scope: "mesh"`: the log strings anchor the parser in both databases, and `device name too long` is the tell.
2. `decompile 0x12E0` locally (1.0) and with `target: "kestrel-bled-1.1"`. The agent compares the name-TLV branch: a signed 8-bit comparison versus an unsigned bound.
3. It explains the overflow: `name[32]` at +4, 4 bytes of padding, `on_complete` at +0x28, called at the end of `parse_packet`.
4. **1.0:** `emulate_function(0x12E0, [{"bytes": "<packet>"}, 135, {"zero_bytes": 256}])` ends with a **memory fetch fault at PC `0x4141414141414141`**. That is the hijacked call.
5. **1.1** (`target`): the same call with **`call_stubs: [{"address": "0x12C0", "return": 0}]`** returns `0xffffffa6`, which is -90 (`-EMSGSIZE`). See *If it gets stuck* for why the stub is needed.
6. The benign packet returns 0 in both.

Reference runs (Claude Opus, single database): 28 turns with the regression-test wording, 34 with the fuzzer wording. One run built its own 135-byte packet with a recognisable `0x4242424242424242` marker instead of `A`s and showed `rax = 0x4242424242424242` at the `call rax`; it also checked the boundaries (`0x20` calls the real handler, `0x21` and `0x80` are rejected, `0x81` and `0xFF` hijack).

## Check your results

* The bug: the name TLV length is checked as a signed 8-bit value, so lengths `0x81`–`0xFF` pass.
* `name[32]` overflows into the `on_complete` pointer at +0x28, which `parse_packet` calls.
* Test packet: `B1 01 07 00 09 81` + `"A" × 0x81` (135 bytes).
* 1.0: fault at PC `0x4141414141414141`. 1.1: returns -90.

| Function | Offset (both versions) |
|---|---|
| `main` | `0x1080` |
| `on_message` | `0x11C0` |
| `log_reject` | `0x12C0` |
| `parse_packet` | `0x12E0` |

The sources are in `src/`; `tools/mkpoc.py` prints the packets.

## If it gets stuck

* **In 1.1 the rejection path logs through `fprintf`**, and the emulator's model of `fprintf` cannot read its format string from a read-only page it hasn't touched yet (*"modeled string is not completely backed"*). Stub the logging helper: `call_stubs: [{"address": "0x12C0", "return": 0}]`. Stubbing the `fprintf` symbol itself does not help, because the runtime model takes precedence.
* Make sure the agent targets the right database. If it asks, answer "kestrel-bled-1.1".
* "Decompile 0x12E0 in both databases and compare the handling of tag 0x09."
* "Emulate parse_packet in 1.0 with the packet bytes, length 135 and a 256-byte zeroed message buffer."
* "In 1.1 stub the logging helper 0x12C0 to return 0 and emulate the same packet."

## Files

* `kestrel-bled-1.0`, `kestrel-bled-1.1` and their `.i64` databases
* `src/`: the sources of both versions; `parse.c` carries both checks, selected by the `BLED_VERSION_MINOR` macro.
* `tools/mkpoc.py`: prints the benign and the triggering packet.
* `ground-truth/`: `packets.txt`, `runs.txt`, `symbols.txt`.
