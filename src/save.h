// The save (Phase 9b, owner 2026-10-19): Knobs and statistics in the cartridge's battery-backed SRAM, written
// only between runs (a run's end, the lift shop). A save that doesn't check out (a new cartridge, a battery that
// died, a power cut mid-write) starts fresh. SRAM is enabled only while it's read or written (DEBUGGING.md).
#ifndef SAVE_H
#define SAVE_H

#include <gb/gb.h>
#include <stdint.h>

#define KNOBS_MAX 65535u
#define KNOBS_ESCAPE 50          // Minecraft docs 12: escaping the Hotel
#define KNOBS_GOLD_EACH 10       // ... and one per 10 gold still carried
#define SAVE_DEATH_CAUSES 12     // DEATH_NONE .. DEATH_ESCAPE (death.h): deaths by what killed you

extern uint16_t save_knobs;      // the balance (kept in WRAM: the title and the lift show it)

// What the run just paid (save_run_end), for the death screen and the results
extern uint8_t knobs_doors;      // doors reached (each pays one)
extern uint8_t knobs_escape;     // KNOBS_ESCAPE or 0
extern uint8_t knobs_gold;       // one per KNOBS_GOLD_EACH gold carried

// At boot: reads the save (or starts a fresh one) into save_knobs
void save_load(void) BANKED;

// A run ended: pays its Knobs (doors reached, escaping, gold carried: read before anything empties the bag) and
// counts it in the statistics. `cause`: DEATH_* (death.h), DEATH_NONE when escaped.
void save_run_end(uint8_t cause) BANKED;

// The lift shop: `knobs` spent
void save_spend(uint16_t knobs) BANKED;

// The title's erase: everything back to nothing
void save_erase(void) BANKED;

// The Knobs line as printed: v right-aligned so its last digit is at `last` (no leading zeros)
void save_num(char *last, uint16_t v) BANKED;

// The death screen's and the results' lines (18 wide at tile (x, y)): what each source paid, then the balance
// ("NOW YOU HAVE"). Returns the rows used.
uint8_t save_print_knobs(uint8_t x, uint8_t y) BANKED;

#define KNOB_CHAR '|'   // the knob glyph (font.json)

#endif
