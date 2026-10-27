# 10 · Bare-metal firmware: peripherals, register structs and a backdoor (Cortex-M4)

| | |
|---|---|
| Open | `kestrel-lock-fw.bin.i64`, one IDA instance. Needs the **ARM** decompiler. |
| Binary | Raw flash image, 1.9 KB, Cortex-M4 Thumb-2, LLVM clang `-Os`, loaded at `0x08000000`. 23 functions. |
| You'll try | firmware without headers or symbols, recognising a memory map, recovering and applying MMIO register structures, naming drivers, auditing how a lock decides to open |
| Agent time | 5–7 minutes |

## The program

The lock MCU firmware of the Kestrel SmartLock K3: a keypad on GPIOA, the motor H-bridge enable on GPIOB pin 5 with PWM from TIM3 channel 1, a service console on USART1 at 115200 baud, a watchdog, and the PIN hash in the last flash sector (`0x0807F000`). The main loop scans the keypad, checks the PIN, drives the motor, and polls the console for service commands.

This exercise is static on purpose: no emulation, just reading code that pokes hardware registers.

## How the image was loaded

The shipped database already has the setup an analyst does in the first minute with a raw image:

* ARM, little-endian, loaded at `0x08000000`, Thumb mode (T bit set for flash);
* segments: `FLASH`, `CONFIG` (`0x0807F000`, the writable config sector), `SRAM` (`0x20000000`), `PERIPH` (`0x40000000`–`0x40080000`), `SCS` (`0xE000E000`);
* the vector table typed as pointers, its handlers created as functions, `Reset_Handler` named.

IDA itself named `NMI_handler`, `HardFault_handler` and `SysTick_handler` from the vector table. Everything else is `sub_…`. If you want to redo the loading from scratch: open `kestrel-lock-fw.bin` as a binary file, processor ARM little-endian, loading address `0x08000000`, then run `tools/load_firmware.py` (File ▸ Script file).

## Before you start

1. Open `kestrel-lock-fw.bin.i64`, close other IDA instances, open the Assist panel (**Ctrl+Shift+K**), new conversation.
2. Have a look at `Reset_Handler`, then `sub_8000188` (`main`), then the raw MMIO stores in `sub_800060A`: things like `unk_40020418 = 32` and `v4[13] = v3` on `&unk_40000400`.

## Prompts

```
This is the raw flash image of the lock's MCU (Cortex-M4, loaded at 0x08000000, vector table at the start). Identify the peripherals it drives and what each is used for, create register structures for them and apply them, and name the functions. As part of our security audit, list every condition under which the firmware drives the lock motor open, and flag any weakness in how the PIN check is enforced.
```

The wording is deliberate. "Find a way to open the lock without the PIN" asks for the same analysis, but some models refuse at the final write-up with that phrasing, after having done all the work.

Follow-up:

```
Draw the memory map with the peripherals you found and which functions touch them.
```

## What to expect

1. `get_segments`, then `get_xrefs` on the peripheral base addresses. IDA resolved the `movw`/`movt` pairs, so there are write references such as `STR R0, [R1,#(unk_40011004 - 0x40011000)]` in `sub_8000420`.
2. It recognises the STM32F4-style map:

   | Peripheral | Base | Used for |
   |---|---|---|
   | RCC | `0x40023800` | `AHB1ENR` +0x30, `APB1ENR` +0x40, `APB2ENR` +0x44 |
   | GPIOA | `0x40020000` | keypad rows and columns, USART pins (AF7) |
   | GPIOB | `0x40020400` | motor enable through `BSRR` +0x18 |
   | USART1 | `0x40011000` | `SR`, `DR`, `BRR`, `CR1` |
   | TIM3 | `0x40000400` | PWM: `PSC` +0x28, `ARR` +0x2C, `CCR1` +0x34 |
   | FLASH | `0x40023C00` | `KEYR` unlock sequence `0x45670123` / `0xCDEF89AB` |
   | IWDG | `0x40003000` | `KR = 0xAAAA`, the watchdog kick |
   | SysTick | `0xE000E010` | 1 ms tick |

3. `define_local_struct` for `GPIO_TypeDef`, `USART_TypeDef`, `TIM_TypeDef`, `FLASH_TypeDef` and so on, then `create_data` with the struct type at each base. The decompiler then shows `USART1->DR = c`, `TIM3->CCR1 = …`, `GPIOB->BSRR = 1 << 5`.
4. It names the drivers: `uart_putc`, `uart_puts`, `motor_drive`, `motor_open`, `keypad_scan`, `check_user_pin`, `flash_write_word` and friends.
5. **The finding:** the service-command handler (`sub_800044A`) accepts `SVC 0451` over the UART, prints `svc: service unlock`, resets the failure counter and calls `motor_open()`. The lock opens without a PIN, and the keypad lockout is cleared too. An unauthenticated `CLOSE` command is the second finding.
6. Bonus: `motor_open` decompiles as `sub_800060A(1)` with the duration argument dropped. The agent can fix the prototype to `motor_drive(int open, uint32_t ms)`.

Reference run (Claude Opus): 50 tool calls, 6 minutes. All 8 peripherals typed (it removed IDA's bogus arrays over the peripheral segment first), 19 functions renamed, a `lock_config_t` applied at `0x0807F000`, the backdoor commented in the database as a hard-coded auth bypass, plus three real defects: the keypad rows on PA0–PA3 are never configured as outputs, TIM3 CH1 is not routed to a pin, and the PIN change writes flash without erasing.

## Check your results

| Address | Function | | Address | Function |
|---|---|---|---|---|
| `0x08000120` | `Reset_Handler` | | `0x080003F6` | `motor_open` |
| `0x08000188` | `main` | | `0x08000420` | `uart_putc` |
| `0x08000206` | `clocks_init` | | `0x08000432` | `uart_getc_nb` |
| `0x0800023A` | `uart_init` | | `0x0800044A` | **`service_command`** |
| `0x08000268` | `motor_init` | | `0x08000534` | `str_eq` |
| `0x08000294` | `uart_puts` | | `0x0800055C` | `str_starts` |
| `0x080002B0` | `service_poll` | | `0x0800057C` | `flash_write_word` |
| `0x080002F8` | `keypad_scan` | | `0x080005B8` | `pin_hash` (FNV-1a ^ 0x4B33) |
| `0x08000364` | `delay_ms` | | `0x080005EE` | `motor_close` |
| `0x080003A0` | `check_user_pin` | | `0x0800060A` | `motor_drive(open, ms)` |

Config sector at `0x0807F000`: `magic 'KCFG'`, `pin_hash`, `u16 max_attempts`, `u16 open_ms`, `char serial[16]`.

`ground-truth/kestrel-lock-fw.elf` is the unstripped ELF the raw image was produced from: load it in a second IDA if you want to compare side by side. `src/regs.h` has the register definitions the firmware was written against.

## If it gets stuck

* Don't ask for emulation here. 32-bit ARM function emulation isn't supported by Assist's emulator, and MMIO recovery by emulation needs a backend with MMIO exits.
* `find_range_stores` does not resolve Thumb `movw`/`movt` base registers: every row comes back as "possible". Steer the agent to `get_xrefs` on the peripheral addresses instead.
* If the agent creates code at an odd address, remind it that Thumb mode is set per segment (`set_segment_register_range`).
* "Use get_xrefs on 0x40011000, 0x40020400, 0x40000400 and 0x40023C00 to find the peripheral drivers."
* "Decompile sub_800044A and explain every command it accepts."

## Files

* `kestrel-lock-fw.bin`, `kestrel-lock-fw.bin.i64`
* `src/`: `firmware.c`, `startup.c`, `regs.h`, `link.ld`.
* `tools/load_firmware.py`: the loading script, for reloading the raw image yourself.
* `ground-truth/`: `kestrel-lock-fw.elf` (unstripped), `symbols.txt`.
