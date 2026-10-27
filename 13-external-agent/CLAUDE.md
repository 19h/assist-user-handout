# Kestrel site configuration format (KCFG)

This folder is an exercise in using IDA Assist from an external agent over MCP.

- The `ida` MCP server (see `.mcp.json`) is IDA Assist's MCP bridge. It talks to
  the IDA instance that has `kestrel-cfgtool.i64` open; use its tools
  (decompile, xrefs, strings, get_bytes, emulate_function, ...) to analyse the
  binary. If several databases are linked, select `kestrel-cfgtool` with
  `use_database`.
- `kestrel-cfgtool` is a Linux x86-64 ELF: do not try to execute it here.
- `site.kcfg` is a real configuration file in that format.
- Put new files (e.g. `kcfg.py`) in this folder. Python 3 is available.
