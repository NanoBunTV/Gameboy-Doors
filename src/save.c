#pragma bank 255
#include <gb/gb.h>
#include <stddef.h>
#include <string.h>
#include "save.h"
#include "sram.h"
#include "run.h"
#include "items.h"
#include "death.h"
#include "hud.h"

// SRAM bank 0 at 0xA000 (sram.h). Version 1 (Phase 9b). The statistics are kept from the first save on, for Phase 11's
// statistics screen (owner: recorded now so it has history).
#define SAVE_VERSION 1
typedef struct {
    uint8_t magic[4];                      // "DOOR"
    uint8_t version;
    uint16_t knobs;
    uint16_t runs, escapes;
    uint8_t best_door;                     // the furthest door reached in any run
    uint32_t knobs_earned;                 // every Knob ever paid
    uint16_t deaths[SAVE_DEATH_CAUSES];    // by DEATH_* (index 0 unused)
    uint16_t check;                        // over everything above
} save_t;
#define SRAM ((save_t *)0xA000u)

uint16_t save_knobs;
uint8_t knobs_doors, knobs_escape, knobs_gold;

static uint16_t checksum(const save_t *s) {
    const uint8_t *p = (const uint8_t *)s;
    uint16_t sum = 0x5AA5u;
    for (uint8_t i = 0; i < offsetof(save_t, check); i++)
        sum = (sum << 1 | sum >> 15) ^ p[i];
    return sum;
}

// SRAM stays enabled from boot with the play bank in (sram.h): the save's bank is switched in only while it's used
static void open_sram(void) {
    SWITCH_RAM(SRAM_SAVE_BANK);
}

// Seals the save and switches the play bank back (a power cut mid-write leaves a bad checksum: the next boot
// starts fresh)
static void close_sram(void) {
    SRAM->check = checksum(SRAM);
    SWITCH_RAM(SRAM_PLAY_BANK);
}

static void fresh(void) {
    memset(SRAM, 0, sizeof(save_t));
    memcpy(SRAM->magic, "DOOR", 4);
    SRAM->version = SAVE_VERSION;
}

void save_load(void) BANKED {
    open_sram();
    if (memcmp(SRAM->magic, "DOOR", 4) || SRAM->version != SAVE_VERSION || SRAM->check != checksum(SRAM)) {
        fresh();
        close_sram();
        open_sram();
    }
    save_knobs = SRAM->knobs;
    SWITCH_RAM(SRAM_PLAY_BANK);
}

void save_run_end(uint8_t cause) BANKED {
    // Minecraft docs 12: a Knob per door reached (the high-water mark: walking back doesn't cost one), 50 for
    // escaping, one per 10 gold still carried. Read before the bag is emptied (the next run's items_reset).
    knobs_doors = run_max_room;
    knobs_escape = cause == DEATH_NONE ? KNOBS_ESCAPE : 0;
    knobs_gold = (uint8_t)(inv_gold / KNOBS_GOLD_EACH);
    uint16_t pay = (uint16_t)knobs_doors + knobs_escape + knobs_gold;
    save_knobs = save_knobs > KNOBS_MAX - pay ? KNOBS_MAX : save_knobs + pay;
    open_sram();
    SRAM->knobs = save_knobs;
    SRAM->knobs_earned += pay;
    if (SRAM->runs != 0xFFFFu)
        SRAM->runs++;
    if (cause == DEATH_NONE) {
        if (SRAM->escapes != 0xFFFFu)
            SRAM->escapes++;
    } else if (cause < SAVE_DEATH_CAUSES && SRAM->deaths[cause] != 0xFFFFu) {
        SRAM->deaths[cause]++;
    }
    if (run_max_room > SRAM->best_door)
        SRAM->best_door = run_max_room;
    close_sram();
}

void save_spend(uint16_t knobs) BANKED {
    save_knobs = knobs > save_knobs ? 0 : save_knobs - knobs;
    open_sram();
    SRAM->knobs = save_knobs;
    close_sram();
}

void save_erase(void) BANKED {
    open_sram();
    fresh();
    close_sram();
    save_knobs = 0;
}

void save_num(char *last, uint16_t v) BANKED {
    do {
        *last-- = '0' + v % 10;
        v /= 10;
    } while (v);
}

// "DOORS 37       |37": the label (and its count), the knob and what it paid at the right
static void knobs_row(uint8_t x, uint8_t y, const char *label, uint16_t count, uint16_t paid) {
    char row[19];
    memset(row, ' ', 18);
    row[18] = 0;
    uint8_t n = 0;
    while (label[n]) {
        row[n] = label[n];
        n++;
    }
    if (count != 0xFFFFu) {
        uint16_t c = count;
        do {
            n++;
            c /= 10;
        } while (c);
        save_num(row + n, count);
    }
    save_num(row + 17, paid);
    uint8_t k = 17;
    while (row[k] != ' ')
        k--;
    row[k] = KNOB_CHAR;
    text_print_bg(x, y, row);
}

uint8_t save_print_knobs(uint8_t x, uint8_t y) BANKED {
    uint8_t y0 = y;
    knobs_row(x, y++, "DOORS", knobs_doors, knobs_doors);
    if (knobs_escape)
        knobs_row(x, y++, "ESCAPED", 0xFFFFu, knobs_escape);
    knobs_row(x, y++, "GOLD", inv_gold, knobs_gold);
    knobs_row(x, y++, "NOW YOU HAVE", 0xFFFFu, save_knobs);
    return y - y0;
}
