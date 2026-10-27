# 06 · Constraint solving: a keygen with Z3, proved by emulation

| | |
|---|---|
| Open | `kestrel-keycheck.i64`, one IDA instance |
| Binary | Linux x86-64 ELF, statically linked (musl), non-PIE, stripped |
| You'll try | turning a decompiled check into constraints, solving them with Assist's Z3 tools, proving a branch unreachable, verifying by emulation |
| Agent time | 2–4 minutes |

## The program

`kestrel-keycheck` is the offline license validator of the Kestrel Fleet Manager: `kestrel-keycheck <user> <key>`, where the key is four 32-bit hex words (`XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX`). It hashes the user name (FNV-1a over the lower-cased name), checks four equations over the key words, and then decides the tier: STANDARD, ENTERPRISE, or a LIFETIME tier that the vendor never issues. The strings `License accepted for %s: %s tier` and `License rejected.` make the entry point easy to find.

## Before you start

1. Open `kestrel-keycheck.i64`, close other IDA instances, open the Assist panel (**Ctrl+Shift+K**), new conversation.
2. Put the cursor on `sub_4005D0` (the license check) or on `main` (`0x400220`).
3. Check that the solver is available: `solve_constraint` and `check_reachability` should be listed in the tool picker or in the permissions dialog. They appear only when Assist could load IDA's bundled Z3 library.

## Prompts

```
Find a license key that unlocks the ENTERPRISE tier for user "hexrays". Translate the key check into constraints and solve them with Z3, then prove the key works by emulating the license check and by running the whole program in the emulator. Finally: can any key ever reach the LIFETIME tier? Prove your answer.
```

Follow-ups:

```
Show me the smallest patch that would make every key ENTERPRISE - as a preview only, don't apply it.
```

```
Generate keys for users alice, kenan and ilfak and verify each one by emulation.
```

## What to expect

1. The agent decompiles `main`, then `sub_4005D0`: `parse_key` (`0x400470`) splits the key into four words, `user_hash` (`0x400400`) is FNV-1a over the lower-cased name, and `check_words` (`0x400510`) holds the equations.
2. It computes `h = fnv1a("hexrays") = 0x962F79BF`, by emulating `user_hash` or in its head. Both are fine.
3. `solve_constraint` with `w0..w3` as 32-bit bit-vectors and five SMT-LIB assertions (the four equations plus the ENTERPRISE marker). The result is `sat` within a second, with a model.
4. `emulate_function(verify_license, [{"utf8": "hexrays"}, {"utf8": "<key>"}])` returns **2** (ENTERPRISE).
5. `emulate_process` with `arguments: ["hexrays", "<key>"]` prints `License accepted for hexrays: ENTERPRISE tier` and the feature line.
6. `check_reachability` with the equations, the ENTERPRISE marker and the LIFETIME condition reports that the path is **not reachable**: the conditions contradict each other. (`solve_constraint` answering `unsat` is an equally valid proof.)

The model writes the SMT itself, which is exactly why the key is then run through the real check: a transcription error cannot survive emulation.

Reference run (Claude Opus): 39 turns, 3 minutes. Besides the Z3 proof it gave a parity argument for why LIFETIME is impossible: the fourth equation and the LIFETIME condition together require `h == ~h`, which would make `2h` equal to an odd number.

## Check your results

The check in `check_words`, all arithmetic modulo 2³², with `h = fnv1a(lower(user))`:

```
E1  (w0 * 0x9E3779B1) ^ rotl(w1, 13)                == h ^ 0xC0FFEE11
E2  ((w1 + 3*w2) ^ (w3 >> 7)) & 0xFFFF              == (rotl(h, 7) + 0x1337) & 0xFFFF
E3  rotl(w2 ^ w0, 5) - w3                           == h * 0x2545F491
E4  (w3 ^ 0xA5A5A5A5) * 0x01000193 + w0             == ~h
ENTERPRISE: (w0 + w1 + w2 + w3) >> 20               == 0x4B4      (otherwise STANDARD)
LIFETIME:   (w3 ^ 0xA5A5A5A5) * 0x01000193          == h - w0     (contradicts E4: unreachable)
```

GCC folded the LIFETIME test into `return (… == h - w0) + 2;`. It is still in the binary; Z3 proves it dead.

Many keys are valid. Any key the agent produces is correct if emulating the check returns 2. Known-good ones (`ground-truth/keys.txt` has more):

| User | Tier | Key |
|---|---|---|
| hexrays | STANDARD | `3B09FC10-05F63D52-19663A6F-918FCEB5` |
| hexrays | ENTERPRISE | `5B008D23-BCEA6265-91EEF451-A1662D2A` |
| alice | ENTERPRISE | `D4E86A33-ADA8F02B-1F9D685D-A917DC02` |

| Address | Function |
|---|---|
| `0x400220` | `main` |
| `0x400400` | `user_hash` |
| `0x400470` | `parse_key` |
| `0x400510` | `check_words` |
| `0x4005D0` | `verify_license` |

`tools/keygen_z3.py` is a reference keygen in z3py (`pip install z3-solver`).

## If it gets stuck

* A Z3 `parse error` is usually fixed by the agent on its own; the tool's error message says what is wrong.
* `emulate_process` takes `arguments` as argv[1..], without the program name.
* "Write the four equations from sub_400510 as SMT-LIB over bv32 w0..w3 with h = 0x962F79BF and call solve_constraint."
* "Use check_reachability with the LIFETIME condition added to the valid-key equations."

## Files

* `kestrel-keycheck`, `kestrel-keycheck.i64`
* `src/`: `license.c` has the equations in C.
* `tools/keygen_z3.py`: reference keygen.
* `ground-truth/`: `keys.txt`, `symbols.txt`.
