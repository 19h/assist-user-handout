# 03 · C++ class recovery without RTTI (arm64 Mach-O)

| | |
|---|---|
| Open | `kestrel-transport.i64`, one IDA instance. Needs the **arm64** decompiler. |
| Binary | macOS arm64 Mach-O, Apple clang++ C++17, `-O2 -fno-rtti -fno-exceptions -fvisibility=hidden`, stripped, ad-hoc signed |
| You'll try | the C++ object model, recovering an interface from its call sites, vtable mapping, applying class types, naming methods |
| Agent time | 6–8 minutes (long; you can stop halfway) |

## The program

`kestrel-transport` is a fleet link test tool. An abstract `Transport` interface has three implementations, `TcpTransport`, `BleTransport` and `SerialTransport`, plus a `LoggingTransport` decorator that wraps another transport and logs every call. A factory parses `tcp://`, `ble://` and `serial://` URLs; `-v` wraps the result in the logging decorator. The tool then performs a short handshake and sends a few frames.

It runs natively on Apple silicon if you want to see it work (`ground-truth/runs.txt` has the output):

```
./kestrel-transport tcp://gw.kestrel.example:7443
./kestrel-transport ble://c0:ff:ee:00:00:07 -v
```

No RTTI, no symbols: the class structure has to come from how the code calls through the vtables.

## Before you start

1. Open `kestrel-transport.i64`, close other IDA instances, open the Assist panel (**Ctrl+Shift+K**), new conversation.
2. Look at a virtual call first, in `start` (that is `main`) or in the frame-sending helper: `(*(void (__fastcall **)(...))(*(_QWORD *)v + 0x18))(...)`.

## Prompt

```
This is a stripped C++ binary built without RTTI. Start with recover_interface_layout, then recover the class hierarchy behind its virtual calls: find the vtables, work out what each slot does, map every slot to its implementation for each class, define the interface and object types, and name the classes and methods. Draw me the hierarchy.
```

## What to expect

1. `recover_interface_layout(interface_name="Transport")` clusters the indirect calls by object and slot: **7 slots** (offsets `+0x8` to `+0x38`) from **18 call sites**, each with its callers. The call targets stay unresolved at this point, because the objects are constructed in another translation unit. That is the realistic case.
2. The agent locates the **five vtables** in `__const` at `0x100004000`, `0x100004050`, `0x1000040A0`, `0x1000040F0` and `0x100004140` (10 pointers each), through `get_xrefs` from the constructors or by reading `__const` with `get_bytes`.
3. It maps slot to method per class from the strings each method prints (`[tcp] connect`, `[ble] notify`, `[serial] …`, `[log] send(%zu) = %ld`) and from `name()` returning `"tcp"`, `"ble"` or `"serial"`.
4. `define_local_struct` for `Transport_vtbl`, `Transport { vtbl*, state, tx, rx, errors }` and the derived objects (`TcpTransport` adds `host[64]`, `port`, `fd`, `nodelay`, and so on), then `set_type` on the vtables and `rename` on the methods. The reference run gave the methods Itanium-mangled names, so IDA itself displays `TcpTransport::send(...)`.
5. `render_visual` draws the hierarchy: `Transport` with Tcp, Ble and Serial below it, and `LoggingTransport` holding a `Transport*`.

`recover_interface_layout` can also propose an annotation plan. It is approval-gated and a dry run by default; approve it if the agent proposes it.

Reference run (Claude Opus): about 90 tool calls, 7½ minutes. All five vtables were read and every slot mapped, 40 methods were named, and the factory was decoded (default port 7443). It also reported a real latent bug: the 256-byte frame buffer in the sending helper is capped by `mtu() - 3`, and TCP's `mtu()` returns 1400. If the run is long for your taste, stop after the slot table and the vtable mapping and leave the naming for a follow-up.

## Check your results

Itanium ABI vtable layout, after the offset-to-top and the (null) RTTI pointer:

| Slot offset | Method |
|---|---|
| `+0x00` | `~T()` complete |
| `+0x08` | `~T()` deleting |
| `+0x10` | `name()` |
| `+0x18` | `open()` |
| `+0x20` | `send(data, len)` |
| `+0x28` | `recv(buf, cap)` |
| `+0x30` | `close()` |
| `+0x38` | `mtu()` |

The default `mtu()` is 244. TCP overrides it with 1400, BLE returns `att_mtu - 3`, and the logging decorator forwards to the inner transport.

| Class | Vtable | Notable methods |
|---|---|---|
| `TcpTransport` | `0x100004000` | ctor `0x100000AF4`, `open` `0x100000C90`, `send` `0x100000CF0` |
| `BleTransport` | `0x100004050` | ctor `0x100000DA8`, `open` `0x100000E80`, `send` `0x100000EF0`, `mtu` `0x100000F70` |
| `SerialTransport` | `0x1000040A0` | ctor `0x100000FC0`, `open` `0x100001028` |
| `LoggingTransport` | `0x1000040F0` | ctor `0x1000010BC`, `open` `0x10000118C`, `send` `0x100001200` |
| `Transport` (abstract) | `0x100004140` | `mtu` `0x100001308` |

The full demangled list is in `ground-truth/symbols.txt`; the class definitions are in `src/transport.hpp`.

Clang merges or devirtualises some tiny methods (`SerialTransport::close()` and the base destructor, for instance). The agent should go by vtable contents, not by guesswork.

## If it gets stuck

* "Read the five vtables in __const (10 pointers each, starting at 0x100004000) and map each slot to the function it points to."
* "Use the printf strings inside each method to tell the classes apart."

## Files

* `kestrel-transport`, `kestrel-transport.i64`
* `src/`: `transport.hpp` (the interface), `transports.cpp` (the three implementations and the decorator), `factory.cpp`, `session.cpp`.
* `ground-truth/`: `symbols.txt` (demangled), `runs.txt`.
