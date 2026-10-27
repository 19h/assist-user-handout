# 04 · Function repair: anti-disassembly, a hidden handler, a missing noreturn

| | |
|---|---|
| Open | `kestrel-licensed.i64`, one IDA instance |
| Binary | Linux x86-64 ELF (PIE), Debian GCC 12 / glibc, `-O2 -fno-stack-protector`, stripped |
| You'll try | diagnosing broken functions, undoing anti-disassembly tricks, patching, finding code that has no references, noreturn analysis |
| Agent time | 4–7 minutes |

## The program

`kestrel-licensed` is a licensed command runner: `kestrel-licensed <license-hex> <command> [arg]`, with the commands `status`, `unlock`, `export` and `svc`. The license blob starts with the magic `KLIC` and carries a 16-byte rolling MAC, a seat count, an expiry day and a feature mask; `ground-truth/license.hex` is a valid one and `tools/mklicense.py` builds it. `ground-truth/runs.txt` shows what each command prints.

It was built with a few "protections", all in `src/protect.h`: opaque `jz`/`jnz` pairs followed by a junk `E8` byte, command handlers reached only through an XOR-masked offset table, and a `die()` that leaves through a raw `exit_group` system call. IDA's auto-analysis falls for all of them. Here is the damage in the shipped database:

| Where | What IDA made of it |
|---|---|
| `0x12B0` (the license parser) | **Not a function.** Mis-decoded as `call near ptr 13FE9601h`; the `jz`/`jnz` pairs jump into the middle of that bogus call. Nothing to decompile. |
| `0x1340` (the feature check) | Truncated. Decompiles to `JUMPOUT(0x1353)` / `JUMPOUT(0x134A)`. |
| `0x14C0` (the `svc` handler) | **Not a function.** No references at all, and mis-decoded. |
| `0x1510` (`die`) | Not marked noreturn. It ends in a syscall with no `ret`. |
| `0x1360` (`dispatch`) | Decompiles, but calls `sub_1360 + (qword_2060[i] ^ 0x5A5AC3C3A5A53C3C)`: the handler table is masked. |

There are six junk sites, at `0x12B4`, `0x12DC`, `0x12FE`, `0x1349`, `0x14C8` and `0x14DD` (the `E8` after `74 03 75 01`).

## Before you start

1. Open `kestrel-licensed.i64`, close other IDA instances, open the Assist panel (**Ctrl+Shift+K**), new conversation.
2. Look at the damage: jump to `0x12B0` (red, no function), then decompile `sub_1340` (two `JUMPOUT`s).
3. Optional: in `sub_1340`, right-click and pick **IDA Assist ▸ Repair ▸ Repair this function**. The entry appears because diagnostics fire on that function. It shows a before/after card with the diagnostics, the actions taken and the undo label.

## Prompts

```
This binary was built with anti-disassembly tricks and IDA's auto-analysis got several functions wrong. Find every damaged function - mis-decoded bytes, code that is not inside any function, functions that never return - repair them with the minimal changes, and show me each one decompiling cleanly afterwards. Then tell me what the command handler that IDA couldn't see does.
```

For a faster run, append: *the junk pattern is 74 03 75 01 E8; NOP the E8 bytes*.

Follow-ups:

```
Decode the masked handler table in sub_1360 and tell me which command maps to which handler.
```

```
Undo everything you changed.
```

The last one shows Assist undoing only its own edits, leaving yours alone.

## What to expect

1. `diagnose_function` flags undefined register reads and unmapped offsets in `sub_1340`.
2. `find_bytes "74 03 75 01 E8"` returns 6 matches, and the agent explains the opaque-predicate trick.
3. Two possible repair paths:
   * **Patch:** `undefine_range` over `0x12B0–0x1360` and `0x14C0–0x1510`, then `nop_range` on the six `E8` bytes (shown as a patch preview, `e8` to `90`), then `create_function` at `0x12B0`, `0x1340` and `0x14C0`. `revert_patched_bytes` undoes the patch.
   * **No patch:** `create_code` to decode around the junk bytes and `edit_xref` to remove the bogus flow. The reference run took this path and patched nothing.
4. `set_function_flags(0x1510, FUNC_NORET)`.
5. `reanalyze_range`, then `decompile`. All three functions are clean:
   * the license parser: magic `KLIC` (`1128877131`), a 16-byte rolling MAC (`ror 5`, `* 0x01000193`), field copies, `if (version != 3) die();`
   * the feature check: `if (expiry < today) die(); return (features & mask) != 0;`
   * the `svc` handler: `if (strcmp(arg, "KS-SERVICE-0451") == 0) puts("service mode: all doors released");`. **A backdoor, and nothing checks the license first.**

Each edit is one entry in Edit ▸ Undo ("IDA Assist: …"). After every edit Assist re-runs the diagnostics, so the model sees whether its fix worked.

Reference run (Claude Opus): 52 tool calls, about 7 minutes. It repaired all three functions without patching a byte, decoded the masked table, found the backdoor, and added that the license check in front of it is forgeable (an unkeyed checksum).

## Check your results

| Address | Function |
|---|---|
| `0x12B0` | `check_license_blob` |
| `0x1340` | `license_allows` |
| `0x1360` | `dispatch` |
| `0x1400` / `0x1430` / `0x1480` | `cmd_status` / `cmd_unlock` / `cmd_export` |
| `0x14C0` | `cmd_service` |
| `0x1510` | `die` |

The handler table at `0x2060` holds deltas from `dispatch` (`0x1360`), masked with `0x5A5AC3C3A5A53C3C`; the command names sit at `0x2080`, 8 bytes each:

| Command | Handler |
|---|---|
| `status` | `0x1400` |
| `unlock` | `0x1430` |
| `export` | `0x1480` |
| `svc` | `0x14C0` |

`svc KS-SERVICE-0451` prints `service mode: all doors released`. The full list is in `ground-truth/symbols.txt`; the sources are in `src/`.

## If it gets stuck

* Decompiling right after `create_function` can fail with *"positive sp value has been found"*. One `reanalyze_range` fixes it; the agent usually retries by itself.
* `create_function` at `0x14C0` can fail on the first attempt, before reanalysis. It succeeds afterwards.
* "Search for the byte pattern 74 03 75 01 E8, NOP the E8 byte of every match, then recreate the functions at 0x12B0, 0x1340 and 0x14C0 and reanalyze."
* "sub_1510 never returns: mark it noreturn."

## Files

* `kestrel-licensed`, `kestrel-licensed.i64` (the damaged database, as IDA produced it)
* `src/`: sources, including `protect.h` with the anti-disassembly macros.
* `tools/mklicense.py`: builds a valid license blob.
* `ground-truth/`: `symbols.txt`, `license.hex`, `runs.txt`.
