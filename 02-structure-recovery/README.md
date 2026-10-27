# 02 · Structure recovery: from `*(a1 + 84)` to `s->door_mask`

| | |
|---|---|
| Open | `kestrel-sessiond.i64`, one IDA instance |
| Binary | Linux x86-64 ELF (PIE), Debian GCC 12 / glibc 2.36, `-O2`, stripped |
| You'll try | struct recovery, enum recovery, type propagation, checking that new prototypes don't break callers |
| Agent time | 4–6 minutes |

## The program

`kestrel-sessiond` is the gateway's session daemon. Everything revolves around one `0x90`-byte session object (magic `KSES`): a state machine (idle, hello sent, challenged, authenticated, locked out, closed), an event callback, a nested peer object and a nested cipher object. The daemon reads commands from stdin (`HELLO`, `AUTH`, `KEY`, `DATA`, `DOORS`, `FAILOVER`, `RESTORE`, `DUMP`, `EXPIRE`); `ground-truth/replay.txt` is a sample input and `replay-output.txt` what it prints.

Stripped and compiled with `-O2`, every function touches the session through raw offsets. That is the view you want Assist to fix.

## Before you start

1. Open `kestrel-sessiond.i64` and close other IDA instances. Open the Assist panel (**Ctrl+Shift+K**), new conversation.
2. Look at the problem first. Decompile `sub_1AD0`:
   ```c
   if ( *(_DWORD *)(a1 + 8) != 3 ) return 0xFFFFFFFFLL;
   v2 = *(_DWORD *)(a1 + 12);
   if ( (v2 & 2) == 0 && a2 > 0xFF ) return 4294967294LL;
   *(_DWORD *)(a1 + 84) |= a2;
   ...
   v3 = *(void (__fastcall **)(...))(a1 + 112);  v3(a1, 5, *(_QWORD *)(a1 + 120));
   ```
3. Then `sub_1770`: `calloc(1, 0x90)` and the magic `0x5345534B`.
4. Optional, instead of typing a prompt: right-click inside `sub_1770` and pick **IDA Assist ▸ Recover structure**. The action runs in its own lane with read-only permissions and proposes a layout.

## Prompts

```
sub_1770 allocates the daemon's central session object and almost every other function works on it through raw offsets. Start with recover_struct_layout on sub_1770, then complete the layout from all the code that touches it, define it as a local type named kst_session (with proper types for the nested pointers), and apply it everywhere it is used. The state field is an enum - recover that too. Then show me sub_1AD0 before and after.
```

The prompt names the tool on purpose. Without that hint the agent tends to rebuild the struct from decompilation alone; the result is just as correct, but you miss the per-field evidence table that `recover_struct_layout` produces.

Follow-ups:

```
Draw the struct layout.
```

```
Name the functions that operate on kst_session based on what they do.
```

## What to expect

1. `recover_struct_layout(target_function=0x1770, propagate_into_callees=true, verify=true)` returns about **21 field candidates** gathered from 3 callers (`main`, `sub_1D50`, `sub_1DD0`) and 4 callees (`sub_19B0`, `sub_1BC0`, `sub_1E80`, `sub_1B30`). Each field carries a confidence and the accesses that justify it. The first candidate contains some noise from the nested objects (something like `field_0[16]` or `field_2E`); that is normal, the tool proposes and the agent decides.
2. The agent reconciles the candidate with the remaining users of the object. `sub_1E80` is the gold mine: its format strings name the fields (`user=%s flags=%#x doors=%#x`, `rtt=%ums proto=v%u retries=%u lock_level=%u failed=%u`, `rx=%zu/%zu cipher=%s nonce=%08x`).
3. `define_local_struct` for `kst_session`, plus `kst_peer` and `kst_cipher` for the nested objects.
4. `kst_session *` is applied to the return of `sub_1770` and to the first parameter of every session function (`set_function_prototype` or `apply_c_signature`). `verify_prototype_convergence` re-decompiles every caller with the new type and shows what changed; if it asks for approval to apply, approve it (one undo step).
5. `create_enum` for the state (`sub_1CD0` is a state-to-name switch) and `apply_enum` on the comparisons.
6. The new `sub_1AD0` reads `if (s->state != KST_AUTHENTICATED) … s->door_mask |= mask; … s->on_event(s, 5, s->event_ctx);`.

Reference run (Claude Opus): about 50 tool calls, 4½ minutes. It created `kst_session`, `kst_peer`, `kst_cipher`, an event-callback type, `kst_state`, `kst_event` and `kst_flags`, set prototypes on all 15 session functions (the nine session locals in `main` became `kst_session *` automatically), fixed a `__spoils` list on `sub_1CD0` so `session_dump` decompiles correctly, and noted unprompted that the auth response is computable from the printed nonce.

Two compiler quirks the agent has to see through: GCC stores `id` and `state` with one 8-byte write and both timestamps with one 16-byte `xmm` store, and writes to `peer->port` and `cipher->key` show up as candidate fields of the outer object.

## Check your results

The original definition:

```c
struct kst_session {            /* sizeof == 0x90 */
    uint32_t magic;             /* 0x00  'KSES' 0x5345534B */
    uint32_t id;                /* 0x04  from 1000 up */
    enum kst_state state;       /* 0x08 */
    uint32_t flags;             /* 0x0c  1 ENCRYPTED, 2 ADMIN, 4 REMOTE, 8 AUDIT */
    char user[32];              /* 0x10 */
    uint64_t created_at;        /* 0x30 */
    uint64_t last_seen;         /* 0x38 */
    struct kst_peer *peer;      /* 0x40  { char addr[46]; uint16_t port; uint32_t rtt_ms; } */
    struct kst_cipher *cipher;  /* 0x48  { uint8_t key[32]; uint8_t iv[12]; uint64_t counter; } */
    uint32_t failed_attempts;   /* 0x50 */
    uint32_t door_mask;         /* 0x54 */
    uint8_t *rx_buf;            /* 0x58 */
    size_t rx_len;              /* 0x60 */
    size_t rx_cap;              /* 0x68 */
    kst_event_fn on_event;      /* 0x70  void (*)(struct kst_session *, int event, void *ctx) */
    void *event_ctx;            /* 0x78 */
    struct kst_session *next;   /* 0x80  global list */
    uint16_t proto_version;     /* 0x88 */
    uint8_t retries;            /* 0x8a */
    uint8_t lock_level;         /* 0x8b */
    uint32_t nonce;             /* 0x8c */
};
enum kst_state { KST_IDLE, KST_HELLO_SENT, KST_CHALLENGED, KST_AUTHENTICATED, KST_LOCKED_OUT, KST_CLOSED };
```

Function map (`ground-truth/symbols.txt`):

| Address | Function | | Address | Function |
|---|---|---|---|---|
| `0x1120` | `main` | | `0x1BC0` | `session_touch` |
| `0x1730` | `on_session_event` | | `0x1C10` | `session_find` |
| `0x1770` | `session_new` | | `0x1C40` | `session_expire_idle` |
| `0x1860` | `session_free` | | `0x1CD0` | `session_state_name` |
| `0x1910` | `session_feed` | | `0x1D50` | `session_clone_for_failover` |
| `0x19B0` | `session_send_hello` | | `0x1DD0` | `session_restore` |
| `0x19F0` | `session_authenticate` | | `0x1E80` | `session_dump` |
| `0x1AD0` | `session_grant_doors` | | | |
| `0x1B30` | `session_enable_cipher` | | | |

Every type and prototype change is a database edit: look for `IDA Assist: define_local_struct` and friends in Edit ▸ Undo.

## If it gets stuck

* "Call recover_struct_layout on 0x1770 with propagate_into_callees and verify enabled."
* "Use the format strings in sub_1E80 to name the fields."

## Files

* `kestrel-sessiond`, `kestrel-sessiond.i64`
* `src/`: `session.h` holds the real struct and enum definitions.
* `ground-truth/`: `symbols.txt`, `replay.txt` (sample input), `replay-output.txt`.
