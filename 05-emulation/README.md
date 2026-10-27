# 05 · Emulation: hidden strings, one function, then the whole program

| | |
|---|---|
| Open | `kestrel-provision.i64`, one IDA instance |
| Binary | Linux x86-64 ELF, statically linked (musl), non-PIE, stripped. 76 functions, 6 of them named. |
| You'll try | string deobfuscation by emulation, emulating a single function with arguments you choose, running the whole program in the emulator, annotating the database with the results |
| Agent time | 3–5 minutes |

## The program

`kestrel-provision` is the factory provisioning tool: `kestrel-provision <serial KS-XXXX-XXXX> <day>` prints a banner, the audit log path, the service endpoint, and the lock's unlock PIN for that day. The PIN comes from a 256-entry S-box plus rotations, the kind of thing nobody wants to reimplement by hand.

Every string in the binary is encrypted. A decoder expands each one into a stack buffer right before use (a 64-bit linear congruential generator keyed by the string id), and the buffer is wiped straight after. Static string extraction finds nothing. There is also a factory master override serial that releases all doors.

`ground-truth/host-run.txt` shows the output for serial `KS-0042-1337` on day 19000, and `host-override.txt` what the master code does.

## Before you start

1. Open `kestrel-provision.i64`, close other IDA instances, open the Assist panel (**Ctrl+Shift+K**), new conversation.
2. Put the cursor on `main` (`0x400220`, the program entry) and look at the pseudocode: everything is `sub_…` and there are no readable strings.
3. Permissions: the defaults are fine. With *Modify this IDA database* on, comments are applied without a prompt.

## Prompts

```
This binary hides all of its strings. Recover every hidden string by emulating the decoder - don't reimplement it - and add a comment at each place a string is used. Then tell me which unlock PIN it prints for serial KS-0042-1337 on day 19000, and prove it by running the whole program in the emulator.
```

For a shorter run, add: *decode with emulate_function on the decoder, one call per string id*.

Follow-ups, pick one or two:

```
What happens if I pass the master code you found as the serial? Show me by running it.
```

```
Rename the decoder, the PIN function and the callers to something meaningful.
```

## What to expect

1. The agent decompiles `main`, sees `sub_400420(n)` everywhere, and decompiles it: that is `say()`, which decodes into a stack buffer, prints it and wipes it.
2. It identifies `sub_4004C0` as the decoder (64-bit LCG with multiplier `0x5851F42D4C957F2D` and increment `0x14057B7EF767814F`, 8-byte stores).
3. It decodes each string with `emulate_function(0x4004C0, [id, {"zero_bytes": 64}])`, one call per id 0–7. The decoded text appears in the result's `memory_written` section. It may try `extract_strings` on the callers first; see *If it gets stuck*.
4. `add_comment` at each call site.
5. `emulate_function(0x400580, [{"utf8": "KS-0042-1337"}, 19000])` returns **576785** after about 190 instructions.
6. `emulate_process`: `open` with `path: "kestrel-provision"` and `options: {personality: "linux", arguments: ["KS-0042-1337", "19000"]}`, then `run`, then `read_output`. The captured stdout ends with `UNLOCK PIN for KS-0042-1337 (day 19000): 576785`. That is a static Linux ELF, loader and libc startup included, running inside IDA's emulator.

Reference run (Claude Opus): about 48 turns, 3 minutes. All 8 strings decoded, comments placed, the PIN cross-checked by both emulation routes, and the master serial flagged as a high-impact finding.

## Check your results

| id | Decoded string | Used in |
|---|---|---|
| 0 | `Kestrel Lockworks :: factory provisioning tool v3.2` | `main` |
| 1 | `usage: kestrel-provision <serial KS-XXXX-XXXX> <day>` | `main` (argc < 3) |
| 2 | `error: serial must look like KS-0000-0000` | `main` |
| 3 | `UNLOCK PIN for %s (day %u): %06u\n` | `print_pin` (`0x400470`) |
| 4 | `7F3A-99C1-D00D`, the factory master override code | `is_master_override` (`0x4003E0`) |
| 5 | `audit log: /var/kestrel/provision.log` | `print_service_info` (`0x400450`) |
| 6 | `service endpoint: provisioning.kestrel.example:8443` | `print_service_info` |
| 7 | `debug: factory master override accepted, all doors released` | `main` (override path) |

* PIN for `KS-0042-1337` on day 19000: **576785**.
* `kestrel-provision 7F3A-99C1-D00D 0` prints the banner, then `debug: factory master override accepted, all doors released`.

| Address | Function |
|---|---|
| `0x400220` | `main` |
| `0x4003E0` | `is_master_override` |
| `0x400420` | `say` |
| `0x400450` | `print_service_info` |
| `0x400470` | `print_pin` |
| `0x4004C0` | `kstr_decode` |
| `0x400530` | `kstr_wipe` |
| `0x400580` | `derive_unlock_pin` |
| `0x400630` | `serial_is_valid` |

## If it gets stuck

* `emulate_process` takes `options.arguments` as argv[1..], **without** the program name. If the emulated program prints `error: serial must look like…`, say: "The arguments list excludes argv[0]; rerun."
* `extract_strings` has a fixed instruction budget. On the callers it runs into musl's `puts`, which is unnamed code in a static binary, and stops with an invalid-instruction fault. The strings decoded before the fault are still reported, but the tool marks the result `success: false`. Don't read that as a failure; the decoder-level `emulate_function` path is the clean one.
* "Use emulate_function on the decoder sub_4004C0 with each string id 0–7 and a 64-byte zero buffer."
* "Don't reimplement derive_unlock_pin; call emulate_function on 0x400580 with the serial as utf8 and day 19000."

## Files

* `kestrel-provision`, `kestrel-provision.i64`
* `src/`: sources. `kstr_table.h` (the encrypted strings) and `pin_table.h` (the S-box) were generated by `tools/gen_tables.py`.
* `ground-truth/`: `symbols.txt`, program output for the normal run and the override.
