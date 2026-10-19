// The cartridge RAM: 32 KB, four 8 KB banks at 0xA000-0xBFFF (MBC5, battery-backed).
//   Bank 0: the save (save.c). Switched in only while save.c reads or writes it.
//   Bank 1: play memory, buffers moved out of WRAM (2026-10-19: the GBC's WRAM was nearly full). main.c
//           enables SRAM and switches bank 1 in at boot, and it stays in: save.c switches to bank 0 and back.
// Nothing an interrupt reads may live here (it could run while bank 0 is in). Unlike WRAM, it isn't zeroed at
// boot (it holds the last session's leftovers): everything here is set before it's read.
// A power cut while it's enabled can only hit the bank switched in: bank 1, not the save.
#ifndef SRAM_H
#define SRAM_H

#define SRAM_SAVE_BANK 0
#define SRAM_PLAY_BANK 1

// Bank 1's layout (fixed addresses: `__at`, so the tests still find each by name in the .sym)
#define SRAM_RUN 0xA000u   // run.c's seven per-room arrays, RUN_ROOMS each (707 bytes)
#define SRAM_DIM 0xA300u   // platform_cgb.c's dim cache (448 bytes)

#endif
