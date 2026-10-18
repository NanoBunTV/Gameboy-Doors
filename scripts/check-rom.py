"""Checks a Game Boy ROM header the way the boot ROM and the cartridge hardware will see it.

Usage: tools/venv/Scripts/python.exe scripts/check-rom.py build/doors.gbc [build/doors.gb ...]
Exits 1 if any ROM fails. A wrong logo or header checksum locks a real console up at the boot logo.
"""
import sys
from pathlib import Path

NINTENDO_LOGO = bytes.fromhex(
    "CEED6666CC0D000B03730083000C000D0008111F8889000EDCCC6EE6DDDDD999"
    "BBBB67636E0EECCCDDDC999FBBB9333E"
)
CART_TYPES = {0x00: "ROM only", 0x19: "MBC5", 0x1A: "MBC5+RAM", 0x1B: "MBC5+RAM+BATTERY"}
RAM_SIZES = {0: 0, 2: 8, 3: 32, 4: 128, 5: 64}  # KB
CGB_FLAGS = {0x00: "DMG", 0x80: "GBC-enhanced (dual)", 0xC0: "GBC only"}


def check(path: Path) -> bool:
    rom = path.read_bytes()
    problems = []
    if len(rom) < 0x8000 or len(rom) % 0x4000:
        problems.append(f"size {len(rom)} is not a whole number of 16 KB banks (min 32 KB)")
    if rom[0x104:0x134] != NINTENDO_LOGO:
        problems.append("Nintendo logo does not match: the boot ROM will lock up")

    title = rom[0x134:0x143].split(b"\0")[0].decode("ascii", "replace")
    cgb, cart, rom_code, ram_code = rom[0x143], rom[0x147], rom[0x148], rom[0x149]

    header_sum = 0
    for b in rom[0x134:0x14D]:
        header_sum = (header_sum - b - 1) & 0xFF
    if header_sum != rom[0x14D]:
        problems.append(f"header checksum {rom[0x14D]:02X}, expected {header_sum:02X}: the boot ROM will lock up")

    global_sum = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
    stored_global = (rom[0x14E] << 8) | rom[0x14F]
    if global_sum != stored_global:
        problems.append(f"global checksum {stored_global:04X}, expected {global_sum:04X} (not checked by hardware)")

    if (32 << rom_code) * 1024 != len(rom):
        problems.append(f"header ROM size code {rom_code} says {32 << rom_code} KB, file is {len(rom) // 1024} KB")

    print(f"{path}: \"{title}\", {CGB_FLAGS.get(cgb, f'CGB flag {cgb:02X}?')}, "
          f"{CART_TYPES.get(cart, f'cart type {cart:02X}?')}, ROM {len(rom) // 1024} KB, "
          f"RAM {RAM_SIZES.get(ram_code, '?')} KB")
    for p in problems:
        print(f"  FAIL: {p}")
    return not problems


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    results = [check(Path(a)) for a in sys.argv[1:]]
    sys.exit(0 if all(results) else 1)
