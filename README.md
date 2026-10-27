# IDA Assist hands-on kit: the Kestrel Lockworks audit

Thirteen small reverse-engineering exercises for trying out **IDA Assist**, the AI agent built into IDA. They are all pieces of one fictional product line, the **Kestrel Lockworks** smart-lock ecosystem: a gateway daemon, a session daemon, a licensed command runner, a provisioning tool, a license checker, a door controller with its libraries and plugin, a BLE daemon in two versions, a firmware packer, the lock's own firmware, a Windows "updater", a bytecode-protected installer, and a configuration tool. Imagine you have been handed the whole ecosystem for a security audit.

Every binary was written for this kit and built from the sources in its folder. Nothing here is real malware, nothing talks to the network, and every domain is `*.example`. The backdoors, secrets and bugs are planted on purpose, so you have something to find and a ground truth to check against.

## What you need

* IDA 9.5 with the Hex-Rays decompiler. x86-64 covers most exercises; 03 needs the arm64 decompiler and 10 the ARM one.
* IDA Assist, installed and connected to a model provider. Send one message in the panel to make sure it works.
* For 06: the constraint-solver tools, which Assist enables automatically when it can load IDA's bundled Z3.
* For 07 and 08: several IDA instances on the same machine. Assist links them automatically.
* For 13: Claude Code or another MCP client.

Unpack the kit anywhere. Assist resolves file paths (the executable it runs in the emulator, the libraries, sample files) relative to the folder of the open database, so the location doesn't matter. Keep each folder intact: 07 needs `sysroot/` next to its databases and 09 needs `sample.kfwp`.

## Quick start

1. Open an exercise's `.i64` in IDA. The databases are plain IDA auto-analysis with nothing renamed or typed, so opening the binary itself gives you the same starting point, except for 07 (rebased libraries) and 10 (raw firmware), whose READMEs describe the loading.
2. Open the Assist panel with **Ctrl+Shift+K** and start a new conversation.
3. Paste the prompt from the folder's `README.md`. Watch the agent read the database, call tools, maybe ask for an approval, and report.
4. Compare the result with the README's *Check your results* section and the `ground-truth/` folder. The original sources are in `src/`.
5. Try the follow-ups, or your own questions.

Take a copy of the kit before you start if you want to replay an exercise from its clean state. Every change Assist makes is also its own step in Edit ▸ Undo, labelled "IDA Assist: …".

## The exercises

| # | Folder | What you'll do | Binary | IDA instances | Agent time |
|---|---|---|---|---|---|
| 01 | [`01-first-contact`](01-first-contact/README.md) | Triage an unknown stripped binary: toolchain, libraries, algorithms, security findings, function map | x86-64 Linux ELF, zlib inside | 1 | 3–6 min |
| 02 | [`02-structure-recovery`](02-structure-recovery/README.md) | Recover a 0x90-byte struct and its enums from every function that touches it, apply them everywhere | x86-64 Linux ELF | 1 | 4–6 min |
| 03 | [`03-cpp-class-recovery`](03-cpp-class-recovery/README.md) | Rebuild a C++ class hierarchy without RTTI: interface slots, vtables, method names | **arm64 Mach-O** | 1 | 6–8 min |
| 04 | [`04-function-repair`](04-function-repair/README.md) | Repair functions broken by anti-disassembly tricks, find a handler IDA couldn't see | x86-64 Linux ELF | 1 | 4–7 min |
| 05 | [`05-emulation`](05-emulation/README.md) | Decode hidden strings by emulation, emulate one function, run the whole program | x86-64 static ELF | 1 | 3–5 min |
| 06 | [`06-z3-keygen`](06-z3-keygen/README.md) | Turn a key check into constraints, solve with Z3, prove a tier unreachable, verify by emulation | x86-64 static ELF | 1 | 2–4 min |
| 07 | [`07-mesh-modules`](07-mesh-modules/README.md) | One question across an executable, two libraries and a plugin, then run the real program with its libraries | x86-64 ELF ×4 | **4** | 4–7 min |
| 08 | [`08-patch-diff`](08-patch-diff/README.md) | Diff a security release, explain the bug, build a regression-test packet, prove it in both versions | x86-64 ELF ×2 | **2** | 3–5 min |
| 09 | [`09-memory`](09-memory/README.md) | Same question, two instances: one has a colleague's notes in memory, the other has to dig | x86-64 ELF ×2 | **2** | 1–3 min each |
| 10 | [`10-firmware`](10-firmware/README.md) | Raw Cortex-M4 firmware: peripherals, register structs, drivers, a UART backdoor | **ARM Thumb raw image** | 1 | 5–7 min |
| 11 | [`11-suspicious-pe`](11-suspicious-pe/README.md) | Malware-style triage of a benign sample: decrypt its config by emulation, IOCs, a YARA rule | **Windows x64 PE** | 1 | 3–5 min |
| 12 | [`12-vm-crackme`](12-vm-crackme/README.md) | Reverse a custom bytecode VM, write its disassembler, recover the password, confirm by emulation | x86-64 Linux ELF | 1 | 4–7 min |
| 13 | [`13-external-agent`](13-external-agent/README.md) | Claude Code reverses a file format through IDA over MCP and writes a parser | x86-64 Linux ELF | 1 + an MCP client | 3–5 min |

A suggested path: start with **01, 02, 04, 05 and 06**, one database each, each showing a different side of Assist (triage, types, repair, emulation, solving). Then **07 and 08**, which link several IDA instances. **09** is about memory. **13** shows an external agent using IDA through Assist. **03, 10, 11 and 12** are extras: other architectures and formats, and the hardest one (12).

## What a run looks like

* **Assist is an agent.** Each prompt starts a loop: the model gets your message plus context (the function under your cursor with its pseudocode, a quick emulation of it, decompiler diagnostics, what Assist remembers about this binary, which databases are linked), calls IDA tools, reads the results, and calls more until it can answer. A typical exercise takes 10–60 tool calls and 2–8 minutes. The panel shows every call and result; result cards are clickable and jump to the address.
* **Results vary.** The same prompt takes a different path on every run and with every model: different tool choices, different proposed names, sometimes a different depth. The ground truth tells you whether the answer is right, not whether the wording matches. The reference numbers in the READMEs come from runs with Claude Opus; other models need more or fewer steps.
* **Steering is part of the workflow.** If the agent heads the wrong way, say so: "use X", "look at Y first", "that's not what I meant". Each README has steering prompts under *If it gets stuck*.
* **Approval cards.** Some tools wait for your approval: running Python, writing files, running shell commands, and anything you set to Ask. "Allow for this session" stops the card from coming back.
* **Everything is undoable.** Renames, types, comments, patches: each one is an Edit ▸ Undo step. You can also tell Assist to undo its own changes, and it leaves yours alone.
* **Prompt wording matters.** Some models refuse to "exploit" or "bypass" things, sometimes at the write-up stage after the analysis is done. The prompts in this kit use audit, regression-test and test-vector wording, which asks for the same work. 08 has a fuzzer-crash variant, and 10 explains its wording.
* **Mentions.** `@name` in a prompt pulls in a function's prototype, callers and callees; `#0x…` pulls in an address with its type, value and cross-references.

## Settings worth checking

Assist ▸ Settings ▸ Permissions:

| Capability | Suggested | Why |
|---|---|---|
| Modify this IDA database | **On** (default) | renames, types, comments, repairs. In 07 and 08 this applies in each linked instance separately. |
| Read files | **On** (default) | the emulator loads the executable and libraries next to the database |
| Read files outside the project | Off | not needed |
| Run Python | **Ask** | you'll see an approval card when the agent runs a script (07, 12) |
| Write files | Off, or Ask for 11 and 12 | only if you want the YARA rule or the disassembler written to disk |
| Run shell commands | Off | not needed |

Settings ▸ Advanced ▸ Persistent memory: recall and "include memory in prompts" on (the defaults). Mesh: "Keep linked when chat is closed" on (the default).

## Several databases at once (07, 08, 09)

Every IDA instance on your machine that runs Assist joins a local, owner-only mesh. The panel badge shows **Linked N**, and clicking it opens the Mesh Nodes dialog. The agent sees the list of linked databases in every prompt and can run any tool in a specific one (`target`) or fan a read-only tool out to all of them (`scope: "mesh"`). A routed call runs in the other instance, with that instance's bytes and permissions, and its result says which database it ran in.

Three rules:

* **Open the Assist panel once in every instance**, including the ones you won't type in. Until then, that instance tells the mesh it has no database.
* **Open only the databases of the current exercise.** The roster goes into every prompt, so other open databases leak into the agent's context. Close them, or untick them in the Mesh Nodes dialog.
* Reads across linked databases never prompt. Writes run in the peer if its *Modify this IDA database* is on, and show up in that peer's Edit ▸ Undo.

## Memory (09, and a little in 01)

Assist keeps what it learns about a binary in your Assist profile, keyed by the file's hash, not in the `.i64`: open any database of the same file and the knowledge is there. Facts are recalled into later conversations, with their provenance (who wrote them, verified or not), and the agent treats them as claims to confirm, not as truth. `/memory` opens the browser, where you can also import a colleague's notes (09 ships a set), export yours, and review what the agent saved. Triage (01) saves its findings; you can also ask the agent to remember or forget something, or right-click in a function and add a note by hand.

## An external agent (13)

Assist is also an MCP server. The bridge ships with the plugin (`bin/assist --mcp` in the plugin folder) and gives any MCP client, such as Claude Code, the same tools, the same permission gate and the same linked databases as the panel, minus the panel's automatic context and memory recall. Settings ▸ Sharing & network ▸ Mesh MCP has copy-paste lines for the common clients. The first connection from a new client is approved in IDA with an **MCP connection request** dialog.

## Starting over

* Don't save the database when you close IDA, or restore it from your copy of the kit.
* To rebuild a database from scratch, delete the `.i64` and open the binary; auto-analysis gives you the same starting point. 07's libraries need rebasing (bases in its README) and 10 has a loading script.
* Memory lives outside the databases. `/memory` shows what Assist kept for a binary; ask it to forget, or review the entries there.

## Things that can trip you up

Observed with the Assist build these exercises were written against (October 2026); some may be fixed by the time you read this.

* `emulate_process` takes `options.arguments` as argv[1..], without the program name. If an emulated program prints its usage line, that's why.
* Emulating a function that calls `snprintf` or `fprintf` can fail with *"modeled string is not completely backed"* when the format string lives on a page the emulator hasn't touched yet. Stub the logging helper with `call_stubs` (08) or seed the page with `initial_memory` (07). Stubbing the libc symbol itself does not help.
* `extract_strings` has a fixed instruction budget and reports `success: false` when it hits a fault later, even though it already decoded strings (05). Emulating the decoder function directly is cleaner.
* `find_range_stores` doesn't resolve Thumb `movw`/`movt` base registers (10). Use `get_xrefs` on the peripheral addresses.
* 32-bit ARM function emulation isn't supported (10). That exercise is static.
* Decompiling right after `create_function` can fail with "positive sp value has been found" (04). One `reanalyze_range` fixes it; the agent usually retries by itself.

## What's in each folder

```
NN-name/
  <binary>            the program, exactly as analysed
  <binary>.i64        its IDA 9.5 database: plain auto-analysis, nothing renamed
  README.md           the program, the prompts, what to expect, how to check
  src/                the sources it was built from
  ground-truth/       real symbol names, program output, keys, packets, configs
  tools/              reference scripts where useful (keygen, disassembler, generators)
```

Toolchains, for the record: Debian 12 GCC 12.2 / glibc 2.36 for the Linux ELFs, musl-cross GCC for the static ones, mingw-w64 GCC 16.2 for the PE, Apple clang for the Mach-O, LLVM clang and lld for the firmware.
