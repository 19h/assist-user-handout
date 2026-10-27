# 07 · Several databases at once: one question across four modules

| | |
|---|---|
| Open | **four IDA instances**: `lockctl.i64`, `libkstore.so.i64`, `libkcrypto.so.i64`, `libkproto.so.i64` |
| Binaries | x86-64 Linux, Debian GCC 12 / glibc, `-O2`. `lockctl` is a stripped PIE executable. The libraries export only their API (`-fvisibility=hidden`, stripped). |
| You'll try | multi-module reversing, following data across databases, emulating each library inside its own database, running the dynamically linked program with its real libraries, annotating across databases |
| Agent time | 4–7 minutes |

## The program

`lockctl <user> <door> <token>` is the door controller CLI: it prints `ACCESS GRANTED` only for a valid unlock token. The work is spread over four modules:

* `lockctl` itself, where everything is inlined into `main`;
* `libkstore.so` (linked, `DT_NEEDED`): per-door secrets, stored encrypted;
* `libkcrypto.so` (linked): the MAC and hex helpers;
* `libkproto.so`: a plugin that `lockctl` loads at runtime with `dlopen("libkproto.so")` and `dlsym("kproto_v2_entry")`. It is in no import table.

The token pipeline: open the store, fetch the door's secret, build a challenge string through the plugin, MAC it with the secret, hex-encode the 8-byte result, compare with the token. `ground-truth/runs.txt` shows the program accepting the right token and rejecting a wrong one.

Also in the folder: `lockctl-static` (the same code, statically linked with the plugin built in) and `sysroot/` (Debian's `ld-linux-x86-64.so.2` and `libc.so.6`), both for running the program in the emulator.

## Before you start

Every IDA instance running Assist on your machine joins a local mesh, and the agent can run any tool in any linked database.

1. Open four IDA instances, one per database. Use the same Assist build in all four.
2. **Open the Assist panel once in each instance** (**Ctrl+Shift+K**). Until the panel has been opened, that instance tells the mesh it has no database. The databases are pre-analysed, so there is nothing to wait for.
3. The panel badge in each instance should read **Linked 4**. Click it to see the Mesh Nodes dialog.
4. Close every other IDA instance, or untick it in the Mesh Nodes dialog. The list of linked databases goes into every prompt.
5. Type the prompt in the **lockctl** instance.
6. Permissions: reads across linked databases never prompt. Writes (renames, comments) run in the peer if that peer has *Modify this IDA database* on, which is the default; they do not show a dialog there, and you undo them in that peer's Edit ▸ Undo. Set *Run Python* to **Ask** if you want to see the approval card when the agent stitches the token together.

The libraries were rebased when the databases were made, so addresses don't collide across the four databases:

| Database | Image base |
|---|---|
| `lockctl` | `0x0` |
| `libkstore.so` | `0x10000000` |
| `libkcrypto.so` | `0x20000000` |
| `libkproto.so` | `0x30000000` |

(If you ever rebuild a library database from the `.so`, rebase it with Edit ▸ Segments ▸ Rebase program.)

## Prompts (in the lockctl instance)

```
lockctl prints ACCESS GRANTED only for a valid unlock token. For our audit report, work out end to end - across all the linked databases - how lockctl validates tokens, and compute the token it expects for user alice on door 7 as a test vector. Compute each piece by emulating the real library code in the database it lives in, then confirm the test vector by running lockctl in the emulator together with its real libraries.
```

Follow-ups:

```
Name the internal functions you identified in each library and add a comment at each lockctl call site saying which library function it reaches and what it returns.
```

```
Which module would I have to patch to accept any token, and where?
```

## What to expect

1. In `lockctl` the agent decompiles `main` (`0x10F0`): `kstore_open`, `kstore_door_secret(ks, door, secret)`, `dlopen("libkproto.so")` and `dlsym("kproto_v2_entry")`, `ops->build_challenge(user, door, buf, 128)`, `kc_mac64(secret, buf, n)`, `kc_hex` on the 8 little-endian bytes, `strcasecmp` against the token.
2. `get_imports` lists the `kstore_*` and `kc_*` functions. `get_exports` with `scope: "mesh"` shows which database exports what, including `kproto_v2_entry`.
3. `decompile` with `target: "libkstore.so"`, `"libkcrypto.so"` and `"libkproto.so"`. A routed result says *"This ran in the database named below."*
4. **libkstore**, with `target`: `emulate_function(kstore_open)` returns the handle `0x10002020`; `emulate_function(kstore_door_secret, [0x10002020, 7, {"zero_bytes": 16}])` writes `2a 6c 1e 5a e9 29 bd 7c 14 c3 db eb 8d 01 6a c9`.
5. **libkproto**: `kproto_v2_entry` returns an ops table whose slot 2 is the v2 challenge builder (`0x30001140`). The format is `KLK2|%s|%u|%08x` with `nonce = fnv1a(user) ^ (door * 0x9E3779B9)`, giving `KLK2|alice|7|d4a647e8`. The agent will usually compute this in Python; see *If it gets stuck* for why.
6. **libkcrypto**: `emulate_function(kc_mac64, [{"bytes": "2a6c…6ac9"}, {"utf8": "KLK2|alice|7|d4a647e8"}, 21])` returns `0x414b33b3656612c0`. The token is its little-endian hex: **`c0126665b3334b41`**.
7. **The proof:** `emulate_process` on `lockctl` with the real dynamic loader and libraries:
   * `path: "lockctl"`, `options: {personality: "linux", guest_path: "/opt/kestrel/lockctl", arguments: ["alice", "7", "c0126665b3334b41"]}`
   * `dependencies`:

     | `path` | `guest_path` |
     |---|---|
     | `sysroot/ld-linux-x86-64.so.2` | `/lib64/ld-linux-x86-64.so.2` |
     | `sysroot/libc.so.6` | `/lib/x86_64-linux-gnu/libc.so.6` |
     | `libkstore.so` | `/opt/kestrel/libkstore.so` |
     | `libkcrypto.so` | `/opt/kestrel/libkcrypto.so` |
     | `libkproto.so` | `/opt/kestrel/libkproto.so` |

   Then `run` and `read_output`: **`ACCESS GRANTED: door 7 released for alice`**. That is glibc's loader resolving three libraries and `dlopen`'ing the fourth, inside the emulator. The simpler fallback is `emulate_process` on `lockctl-static` with the same arguments.

Reference run: from `lockctl` alone the agent mapped the whole pipeline in 16 turns and produced a "look here next" table per library; the cross-database part adds the per-library emulation and the final run. Extra findings it made along the way: the `$ORIGIN` plugin search path can be hijacked, the challenge builder's returned length is not checked against the 128-byte buffer, and the token comparison is not constant-time.

## Check your results

```
user=alice door=7
secret    = 2a6c1e5ae929bd7c14c3dbeb8d016ac9      sha256("kestrel-door-7")[:16], stored XOR-encrypted in libkstore
challenge = KLK2|alice|7|d4a647e8                 libkproto v2; the v1 format "KLK1:%s:%u" is a decoy
mac64     = 0x414b33b3656612c0                    SipHash-2-4 with custom initialisation constants, libkcrypto
token     = c0126665b3334b41
```

* `kc_mac64` is SipHash-2-4 (rotations 13/16/21/17/32) initialised with `0x6b657374726c6f63`, `0x6b6d61632d763221`, `0x646f6f726c6f636b` and `0x746f6b656e2d3634` ("kestrloc", "kmac-v2!", "doorlock", "token-64").
* `kc_mac64_legacy` (FNV) and `kproto_v1_entry` are exported but unused: decoys.
* A second test vector (user `hexrays`, door 3) is in `ground-truth/token-hexrays-3.txt`.

Function map, as file offsets (add the database's image base):

| Module | Function | Offset |
|---|---|---|
| lockctl | `main` | `0x10F0` |
| libkstore | `kstore_open` | `0x1100` |
| libkstore | `kstore_door_secret` | `0x1110` |
| libkcrypto | `kc_mac64` | `0x1100` |
| libkcrypto | `kc_mac64_legacy` | `0x1360` |
| libkcrypto | `kc_hex` | `0x13D0` |
| libkproto | `build_challenge_v1` | `0x1110` |
| libkproto | `build_challenge_v2` | `0x1140` |
| libkproto | `kproto_v2_entry` | `0x11C0` |

## If it gets stuck

* **Emulating the challenge builder in libkproto fails** with *"modeled string is not completely backed"*. The emulator's model of `snprintf` reads its format string from a read-only page it hasn't touched yet. Let the agent compute the challenge in Python (it is one format string), or pass `initial_memory` for the page at `0x30002000`; with the page seeded the call returns 21 and the nonce `0xd4a647e8` is visible in `r9`.
* `emulate_process` takes `arguments` as argv[1..]. Dependency paths are relative to the database's folder, so keep `sysroot/` next to the `.i64`.
* If the agent asks *which* database to use, answer with the binary name, for instance "libkcrypto.so".
* "Use get_exports with scope mesh to find which database exports kc_mac64 and kproto_v2_entry."
* "Emulate kstore_door_secret in libkstore.so (target) with handle 0x10002020, door 7 and a 16-byte buffer."
* "Emulate kc_mac64 in libkcrypto.so with the secret bytes and the challenge string."
* "Run lockctl with emulate_process: dependencies sysroot/ld-linux-x86-64.so.2 → /lib64/ld-linux-x86-64.so.2, sysroot/libc.so.6 → /lib/x86_64-linux-gnu/libc.so.6, and the three libraries under /opt/kestrel, guest_path /opt/kestrel/lockctl."

## Files

* `lockctl`, `libkstore.so`, `libkcrypto.so`, `libkproto.so` and their `.i64` databases
* `lockctl-static`: the same program, statically linked (musl), for a simpler emulator run
* `sysroot/`: the glibc loader and libc the emulator needs for the dynamic run
* `src/`: all four modules' sources. `libkstore/kstore_table.h` (the encrypted door secrets) was generated by `tools/gen_kstore.py`.
* `tools/mktoken.c`: computes a token by linking the real library sources natively.
* `ground-truth/`: `runs.txt`, `token-alice-7.txt`, `token-hexrays-3.txt`, `symbols.txt`.
