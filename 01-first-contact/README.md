# 01 · First contact: triage a stripped binary

| | |
|---|---|
| Open | `kestrel-gatewayd.i64`, one IDA instance |
| Binary | Linux x86-64 ELF (PIE), Debian GCC 12.2 / glibc 2.36, `-O2`, stripped. zlib 1.2.13 is linked in statically. 112 functions, 28 imports. |
| You'll try | triage, toolchain and library identification, recognising algorithms from their constants, a security review, bulk renaming |
| Agent time | 3–6 minutes |

## The program

`kestrel-gatewayd` is the site gateway daemon of the Kestrel smart-lock system. It reads an INI configuration from `/etc/kestrel/gatewayd.ini` (factory defaults if the file is missing), keeps a ring-buffer log, and serves a small command channel on stdin: `status`, `login`, `unlock`, `pair`, `set`, `tick` and `telemetry`. Telemetry leaves as JSON, compressed with zlib, encrypted with XTEA, checksummed with CRC-32 and base64-encoded, bound for `https://telemetry.kestrel.example/v2/ingest`. Pairing codes are derived with SHA-256.

It is a Linux binary, so it won't run on macOS or Windows. `ground-truth/session-output.txt` shows a sample session: a login with the default credentials, door 3 unlocked, and pairing code `567682` for nonce `1a2b3c4d`.

Planted for you to find: a fleet-wide hard-coded encryption key, default admin credentials, a default device secret that makes pairing codes predictable, and commands that skip the login check.

## Before you start

1. Open `kestrel-gatewayd.i64`. Close other IDA instances, or they will show up in the agent's context as linked databases.
2. Open the Assist panel (**Ctrl+Shift+K**) and start a new conversation.
3. The first time Assist sees a binary it may offer **Triage this binary**. Clicking it runs the built-in `/fulltriage`, which is option A below.

## Prompts

Option A, the built-in triage. It runs a fixed set of checks (toolchain, libraries, debug info, imports, strings, function ranking and more) and saves its findings to Assist's memory for this binary:

```
/fulltriage
```

Option B, free-form. It tells a better story and lets you watch the agent choose its own tools:

```
We were just handed this binary from a customer site. Triage it: what is it, what does it do, which toolchain built it, which third-party code and algorithms are embedded, and what are the security-relevant findings? Start with a five-line executive summary, then give me a map of the 10 most important functions with the names you propose, each backed by evidence.
```

Follow-up:

```
Good. Apply those names, and add a one-line comment at the top of each function saying what it does.
```

## What to expect

1. **Orientation.** `get_binary_type`, `detect_toolchain` (GNU gcc), `detect_libraries` (zlib 1.2.13, from the `deflate 1.2.13 Copyright 1995-2022 Jean-loup Gailly and Mark Adler` banner, plus glibc), `get_imports`, and `search_strings` (the telemetry endpoint, the INI path, the command names, the `KTLM` magic).
2. **Finding the hubs.** `rank_functions` or `get_call_graph`, then `decompile` on the command dispatcher, the telemetry pipeline, the crypto and the config code.
3. **Algorithm recognition from constants.**

   | Algorithm | Evidence |
   |---|---|
   | SHA-256 | round constants `0x428a2f98…`, H0 = `0x6a09e667` |
   | CRC-32 | polynomial `0xEDB88320`, table built at runtime |
   | XTEA | delta `0x9E3779B9`, 32 cycles, `key[sum & 3]`, `key[(sum >> 11) & 3]` |
   | base64 | the alphabet string |
   | zlib | `compress2` at level 9 |

4. **Security findings**, the payoff:
   * a hard-coded fleet-wide XTEA key `{0x4B455354, 0x52454C21, 0x464C4545, 0x54323032}` ("KESTREL!FLEET2026"), the same on every gateway;
   * default admin credentials `admin` / `kestrel` in the config defaults, used whenever the INI file is missing;
   * a default `device_secret` of `kestrel-factory-secret`, which feeds the pairing code (SHA-256 of secret and nonce, modulo 10⁶), so pairing codes can be computed offline;
   * `set` requires a login, but `pair` and `telemetry` do not;
   * XTEA in ECB mode, and a CRC used where a MAC belongs.

The follow-up renames functions and adds comments. Each rename shows as a diff pill, and each is its own step in Edit ▸ Undo.

Reference run (option B, Claude Opus): 45 tool calls, about 6 minutes. For a shorter run, ask for the executive summary first and the function map as a follow-up.

## Check your results

Function map. `ground-truth/symbols-all.txt` has all 56 names from the unstripped build, zlib included.

| Address | Function | | Address | Function |
|---|---|---|---|---|
| `0x11B0` | `main` | | `0x1E50` | `crc32_ieee` (runtime table) |
| `0x13C0` | `set_entry` (config) | | `0x1A40` / `0x1F10` | `sha256_block` / `sha256` |
| `0x14F0` | `trim` | | `0x23C0` | `xtea_encrypt_blocks` (ECB) |
| `0x1560` | `config_defaults` (credentials, secret, endpoint) | | `0x2450` | `base64_encode` |
| `0x1660` / `0x17B0` | `config_load` / `config_get` | | `0x2540` | `telemetry_flush` (JSON → zlib → XTEA → CRC → base64) |
| `0x1830` / `0x1910` / `0x1870` | `log_init` / `log_msg` / `log_flush` | | `0x2700` | `pairing_code` (SHA-256 → 6 digits) |
| `0x2AE0` | `dispatch_command` | | `0x2900` / `0x2880` / `0x2A80` | `cmd_login` / `cmd_unlock` / `cmd_pair` |
| `0x2A30` / `0x2840` / `0x27C0` / `0x27A0` | `cmd_status` / `cmd_telemetry` / `cmd_set` / `cmd_tick` | | the rest | zlib (`compress2`, `deflate*`, `adler32`, `crc32`, `_tr_*`, `get_crc_table`, …) |

The command table (`{name, handler, needs_auth}`) lives in `.data.rel.ro`. The sources in `src/` are the final word.

Don't expect FLIRT names for zlib: IDA has no signatures for this particular static build. The agent should recognise zlib from its banner and name the rest from evidence, which is the more interesting outcome anyway.

## If it gets stuck

* "Use detect_libraries and tell me which code is zlib so we can ignore it."
* "Decompile the function that references the string 'telemetry: %zu bytes queued' and explain the pipeline."
* Triage findings end up in memory. Open `/memory` to see them; they will be recalled in your next conversation on this binary.

## Files

* `kestrel-gatewayd`, `kestrel-gatewayd.i64`: the binary and its database (plain auto-analysis, nothing renamed).
* `src/`: the C sources.
* `ground-truth/`: `symbols-all.txt` (names from the unstripped build), `session.txt` and `session-output.txt` (a sample session and its output).
