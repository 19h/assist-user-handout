# 13 · An external agent drives IDA through Assist (MCP)

| | |
|---|---|
| Open | `kestrel-cfgtool.i64` in IDA, plus **Claude Code** (or another MCP client) in this folder |
| Binary | Linux x86-64 ELF (PIE), Debian GCC 12 / glibc, `-O2`, stripped |
| You'll try | Assist as an MCP server: the same tools and permission checks, used by an agent outside IDA, reversing a file format and writing a parser from the result |
| Agent time | 3–5 minutes |

## The program

`kestrel-cfgtool` reads and dumps KCFG files, the binary site configuration of a Kestrel installation. `site.kcfg` is a real sample (255 bytes): the configuration of the installation at "Hex-Rays HQ, Liège". `ground-truth/dump.txt` is what the tool prints for it.

The format has a trap: string values are obfuscated with a running XOR. A parser that misses it prints garbage for `site.name`.

## Before you start

1. **Start IDA first.** Open `kestrel-cfgtool.i64` and open the Assist panel once (**Ctrl+Shift+K**). Until the panel has been opened, the instance tells MCP clients it has no database.
2. Register Assist's MCP bridge with your client. The bridge is the `assist` program in the plugin's `bin/` folder, run with `--mcp`. The easiest way: Assist ▸ Settings ▸ Sharing & network ▸ Mesh MCP has a **Copy** line for Claude Code, Codex, Grok and the Antigravity CLI with the absolute path filled in, plus an `mcpServers` entry for any other client. For Claude Code, on macOS or Linux, that line is:
   ```
   claude mcp add ida -- "$HOME/.idapro/plugins/assist/bin/assist" --mcp
   ```
   This folder also carries a project-scoped `.mcp.json` with the same server, so `claude` started here finds it without any registration. It assumes the plugin lives in `~/.idapro`; on Windows edit the path to `%APPDATA%\Hex-Rays\IDA Pro\plugins\assist\bin\assist.exe`.
3. In a terminal: `cd 13-external-agent && claude`. Approve the `ida` server when Claude Code asks about the project's MCP config, and check it with `/mcp`.
4. On Assist's side, the first call from a new client shows an **MCP connection request** dialog in IDA (Deny, Decide later, Allow). Allow it. Your decision is remembered.
5. Close other IDA instances, or expect the agent to pick the database with `use_database kestrel-cfgtool`.

## Prompt (in Claude Code)

```
Use the IDA tools from the ida MCP server to reverse engineer how kestrel-cfgtool parses its binary configuration format. Then write kcfg.py in this folder that parses site.kcfg and prints it as JSON, and verify your output against what the format implies. Don't run kestrel-cfgtool itself - it's a Linux binary.
```

The same task works from the Assist panel, if you want to compare: "Reverse engineer how this tool parses its configuration format, then write a Python parser for site.kcfg and check it against the binary."

## What to expect

1. Claude Code calls `list_functions`, `search_strings` and `decompile` through the bridge. The parser is `0x1360` and the CRC is `0x1310` (CRC16-CCITT, polynomial `0x1021`, initial value `0xFFFF`).
2. It derives the format:
   * header: `KCFG`, version byte 2, flags, u16 entry count;
   * entries: u8 type, u8 key length, key, value;
   * value types: 1 = i32, 2 = string (u16 length + bytes, obfuscated with `(0x5C + i) & 0xFF`), 3 = bool, 4 = blob (u16 length + bytes), 5 = IPv4;
   * trailer: CRC16 over everything before it.
3. It writes `kcfg.py`, asks you for permission to run `python3 kcfg.py`, validates the CRC, and prints the JSON.

Reference run (the Assist-panel variant, Claude Opus): 25 tool calls, 3 min 41 s. The parser's output matched the tool's own `dump` byte for byte, which the agent checked by running the real parser in the emulator, and it fuzzed the error paths with broken copies of the file (return codes -1, -2, -3 and -5). It also added a 172-byte `kcfg_record` struct to the database, with names and prototypes.

What you don't see: MCP calls don't appear in the Assist panel's transcript. Their trail is Edit ▸ Undo (one "IDA Assist: …" step per edit) and the provenance of any memory they write. The bridge is local and owner-only; nothing is exposed on the network unless you open the optional network endpoint yourself.

## Check your results

`ground-truth/site.json`:

```json
{"gateway.ip": "10.20.0.1", "gateway.port": 7443, "site.name": "Hex-Rays HQ, Liege",
 "site.admin": "facility@kestrel.example", "doors.count": 24, "doors.auto_relock": true,
 "doors.relock_seconds": 8, "ble.pairing_code": "KS-PAIR-7741",
 "ble.irk": "8f3c2a91d05e7b4466f1a0c3b2d95e17", "telemetry.enabled": false}
```

| Function | Offset |
|---|---|
| `kcfg_crc16` | `0x1310` |
| `kcfg_parse` | `0x1360` |

`tools/mksample.py` is the generator that wrote `site.kcfg`; `src/kcfg.c` is the parser.

## If it gets stuck

* If IDA wasn't running when Claude Code started, the tool list comes from a built-in manifest and calls return "not reachable, safe to retry". No restart needed: once IDA is up and the panel has been opened, the next call reconnects. With the connection request still unanswered, the first call returns "not approved yet" until you click Allow.
* Several databases open? "Call use_database kestrel-cfgtool first."
* "Decompile 0x1360 and 0x1310 in IDA and describe the record layout."
* "String values are XOR-obfuscated in kcfg_parse; look at the loop after the memcpy."

## Files

* `kestrel-cfgtool`, `kestrel-cfgtool.i64`, `site.kcfg`
* `.mcp.json`, `CLAUDE.md`: project configuration and notes for Claude Code.
* `src/`: `kcfg.c`, `kcfg.h`, `main.c`.
* `tools/mksample.py`: generates `site.kcfg`.
* `ground-truth/`: `site.json`, `dump.txt`, `symbols.txt`.
