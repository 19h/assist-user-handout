"""Loading script for the raw Cortex-M4 image kestrel-lock-fw.bin.

The shipped database already has this applied. To redo the loading yourself:
open kestrel-lock-fw.bin in IDA as a binary file, processor ARM little-endian,
loading address 0x08000000, then run this script (File > Script file...).

It performs only the steps an analyst does in the first minute with a raw
firmware image:
  * Thumb mode for flash, segment named FLASH,
  * SRAM / peripheral / system-control / config-sector segments (uninitialised),
  * vector table typed as pointers, handlers created as functions,
  * Reset_Handler named (it is entry 1 of the vector table).
Everything else (naming, peripherals, structs) is left for the exercise."""
import ida_auto
import ida_bytes
import ida_funcs
import ida_kernwin
import ida_name
import ida_offset
import ida_segment
import idc

FLASH = 0x08000000
VECTORS = 70


def add_seg(start, end, name, sclass, perm):
    seg = ida_segment.segment_t()
    seg.start_ea = start
    seg.end_ea = end
    seg.bitness = 1  # 32-bit
    seg.perm = perm
    ida_segment.add_segm_ex(seg, name, sclass, ida_segment.ADDSEG_NOSREG | ida_segment.ADDSEG_SPARSE)


def prep():
    seg = ida_segment.getseg(FLASH)
    ida_segment.set_segm_name(seg, "FLASH")
    ida_segment.set_segm_class(seg, "CODE")
    seg.perm = ida_segment.SEGPERM_READ | ida_segment.SEGPERM_EXEC
    seg.update()
    idc.split_sreg_range(seg.start_ea, "T", 1, idc.SR_user)

    add_seg(0x0807F000, 0x08080000, "CONFIG", "DATA", ida_segment.SEGPERM_READ | ida_segment.SEGPERM_WRITE)  # flash config sector, rewritten at runtime
    add_seg(0x20000000, 0x20018000, "SRAM", "DATA", ida_segment.SEGPERM_READ | ida_segment.SEGPERM_WRITE)
    add_seg(0x40000000, 0x40080000, "PERIPH", "IO", ida_segment.SEGPERM_READ | ida_segment.SEGPERM_WRITE)
    add_seg(0xE000E000, 0xE000F000, "SCS", "IO", ida_segment.SEGPERM_READ | ida_segment.SEGPERM_WRITE)

    ida_name.set_name(FLASH, "vector_table")
    for i in range(VECTORS):
        ea = FLASH + 4 * i
        ida_bytes.create_dword(ea, 4)
        value = ida_bytes.get_dword(ea)
        if value == 0:
            continue
        ida_offset.op_plain_offset(ea, 0, 0)
        if i == 0:
            continue
        target = value & ~1
        idc.split_sreg_range(target, "T", 1, idc.SR_user)
        idc.create_insn(target)
        ida_funcs.add_func(target)
    reset = ida_bytes.get_dword(FLASH + 4) & ~1
    ida_name.set_name(reset, "Reset_Handler")
    idc.set_inf_attr(idc.INF_START_EA, reset)
    ida_auto.auto_wait()
    ida_kernwin.msg("[prep] cortex-m layout applied; reset handler at %#x\n" % reset)


if __name__ == "__main__":
    prep()
