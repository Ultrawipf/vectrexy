#!/usr/bin/env python3
"""Produce a Vectrex BIOS image whose startup logo border is not dashed.

Why
---
The startup screen draws its border with the BIOS's pattern-vector routine, which
pulses the beam on and off through the VIA shift register. On a phosphor screen
that reads as a dashed rectangle; on a laser projector every gap is exaggerated,
and stitching the dashes back together after the fact distorts nearby geometry
(text especially). Patching the pattern at the source avoids all of that.

What the stock BIOS does
------------------------
Disassembled from data/bios/System.bin (md5 ab082fa8c8e632dd68589a8c7741388f).
All three BIOS images shipped in data/bios are byte-identical in this region.

    $F02A  ANDB #$03            ; index 0..3
    $F02C  LDX  #$F0FD          ; dash-pattern table
    $F02F  LDB  B,X
    $F031  STB  $C829           ; Vec_Pattern = table[i]
    $F033  LDB  #$02            ; border pass count -> the DOUBLE border
    $F035  STB  $C824
    ...
    $F058  JSR  $F308           ; loop body: position
    $F05B  LDA  #$03
    $F05D  JSR  $F434           ; Draw_Pat_VL_a - draws the patterned rectangle
    $F060  DEC  $C824
    $F063  BNE  $F058           ; ...again -> second rectangle

    $F0FD  E0 38 0E 03          ; the four dash patterns (a set bit lights the beam)
    $F06F  LDA #$CC / STA $C829 ; a further dashed pattern, set after the logo loop

The logo also pulses. Two 12-byte Print_List_hw blocks hold the same string at
two different heights, and the boot loop alternates between them on bit 5 of
the frame counter:

    $F046  LDA  $C826
    $F049  LDU  #$F10C          ; block A
    $F04C  BITA #$20
    $F04E  BEQ  $F052
    $F050  LEAU 12,U            ; ...or block B
    $F052  JSR  $F385           ; Print_List_hw

    $F10C  F1 60 27 CF "VECTREX" 80   height -15, width 96, y 39, x -49
    $F118  F3 60 26 CF "VECTREX" 80   height -13, width 96, y 38, x -49

On a phosphor screen that reads as the logo breathing. A laser bridge that
accumulates more than one Vectrex redraw per output frame instead shows both
sizes at once, i.e. doubled text. Forcing the BEQ to an unconditional BRA
pins the logo to the taller block.

The patches
-----------
  solid          pattern bytes -> $FF, so the beam stays lit: continuous lines
  single-border  additionally LDB #$02 -> #$01, drawing one rectangle not two
  no-border      additionally NOP out the JSR, drawing no rectangle at all
  --keep-pulse   leave the logo alternating (it is pinned by default)

Only the startup logo is affected; Mine Storm and everything after it are
untouched (verified by comparing rendered gameplay frames).

Usage
-----
    python tools/patch_bios_border.py                       # solid + single border
    python tools/patch_bios_border.py --variant no-border
    python tools/patch_bios_border.py --input data/bios/System.bin --output out.bin
"""
import argparse
import hashlib
from pathlib import Path

ROM_BASE = 0xE000
ROM_SIZE = 8192

PATTERN_TABLE = 0xF0FD      # 4 pattern bytes
LATE_PATTERN_IMM = 0xF070   # operand of LDA #$CC
BORDER_COUNT_IMM = 0xF034   # operand of LDB #$02
BORDER_DRAW_JSR = 0xF05D    # JSR $F434, 3 bytes
LOGO_SIZE_BEQ = 0xF04E      # BEQ that picks between the two logo blocks

EXPECTED = {
    PATTERN_TABLE: bytes([0xE0, 0x38, 0x0E, 0x03]),
    LATE_PATTERN_IMM - 1: bytes([0x86, 0xCC]),
    BORDER_COUNT_IMM - 1: bytes([0xC6, 0x02]),
    BORDER_DRAW_JSR: bytes([0xBD, 0xF4, 0x34]),
    LOGO_SIZE_BEQ: bytes([0x27, 0x02]),
}

VARIANTS = ("solid", "single-border", "no-border")


def _offset(address):
    return address - ROM_BASE


def patch(data: bytearray, variant: str, pin_logo_size: bool = True) -> bytearray:
    for address, expected in EXPECTED.items():
        start = _offset(address)
        actual = bytes(data[start:start + len(expected)])
        if actual != expected:
            raise SystemExit(
                f"Unexpected bytes at ${address:04X}: got {actual.hex(' ')}, "
                f"expected {expected.hex(' ')}. This is not the standard Vectrex BIOS, "
                f"so patching it blindly would corrupt it.")

    # Every pattern fully lit -> continuous lines rather than dashes.
    for i in range(4):
        data[_offset(PATTERN_TABLE) + i] = 0xFF
    data[_offset(LATE_PATTERN_IMM)] = 0xFF

    if variant == "single-border":
        data[_offset(BORDER_COUNT_IMM)] = 0x01
    elif variant == "no-border":
        for i in range(3):
            data[_offset(BORDER_DRAW_JSR) + i] = 0x12  # NOP

    if pin_logo_size:
        # BEQ -> BRA: always take the first Print_List_hw block, so the logo stops
        # alternating between its two heights.
        data[_offset(LOGO_SIZE_BEQ)] = 0x20

    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input", type=Path, default=Path("data/bios/System.bin"))
    parser.add_argument("--output", type=Path, default=Path("data/bios/LaserBoot.bin"))
    parser.add_argument("--variant", choices=VARIANTS, default="single-border",
                        help="solid: keep both rectangles, just undash them. "
                             "single-border (default): one rectangle. "
                             "no-border: none at all.")
    parser.add_argument("--keep-pulse", action="store_true",
                        help="Leave the startup logo alternating between its two heights. By "
                             "default it is pinned to the taller one, because a laser bridge that "
                             "accumulates several redraws per frame shows both at once.")
    args = parser.parse_args()

    original = args.input.read_bytes()
    if len(original) != ROM_SIZE:
        raise SystemExit(f"{args.input} is {len(original)} bytes, expected {ROM_SIZE}")

    data = patch(bytearray(original), args.variant, pin_logo_size=not args.keep_pulse)
    args.output.write_bytes(bytes(data))

    changed = sum(1 for a, b in zip(original, data) if a != b)
    print(f"{args.output}: variant={args.variant}, {changed} bytes changed, "
          f"md5={hashlib.md5(bytes(data)).hexdigest()}")


if __name__ == "__main__":
    main()
