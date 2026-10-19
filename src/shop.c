#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "shop.h"
#include "run.h"
#include "room.h"
#include "bg.h"
#include "hud.h"
#include "input.h"
#include "audio.h"
#include "items.h"
#include "save.h"
#include "sprites.h"
#include "room_templates.h"
#include "tileset_shop.h"
#include "sprite_jeff.h"

#define SAY_FRAMES  120
#define BLINK_EVERY 170    // frames between his blinks ...
#define BLINK_LONG  8      // ... and how long one lasts
#define LOOK_EVERY  7      // every 7th blink he glances aside for a while instead
#define LOOK_LONG   50

// By LOOT_* - 1 (Minecraft config shop.prices)
static const uint8_t price[6] = { 12, 15, 20, 25, 70, 50 };
static const char icon[6] = { ICON_LOCKPICK, ICON_VITAMINS, ICON_LIGHTER, ICON_FLASHLIGHT, ICON_CRUCIFIX, ICON_SKELETON };
static const char *const name[6] = { "LOCKPICK", "VITAMINS", "LIGHTER", "FLASHLIGHT", "CRUCIFIX", "SKELETON KEY" };
static const uint8_t counter_mt[6] = { MT_SHOP_COUNTER_LOCKPICK, MT_SHOP_COUNTER_VITAMINS, MT_SHOP_COUNTER_LIGHTER,
                                       MT_SHOP_COUNTER_FLASHLIGHT, MT_SHOP_COUNTER_CRUCIFIX, MT_SHOP_COUNTER_SKELETON };

uint8_t shop_in;   // the room is Jeff's Shop (tests read it)
static const room_obj_t *stock[SHOP_STOCK];
static uint8_t stock_item[SHOP_STOCK];
static int16_t jeff_x, jeff_y;
static uint8_t eye_t, blinks;
static spr_sheet_t jeff_sheet;
static char msg[21];

void shop_room_loaded(void) BANKED {
    shop_in = run_room == RUN_SHOP_ROOM && run_kind_at(RUN_SHOP_ROOM) == ROOM_KIND_SHOP;
    if (!shop_in)
        return;
    uint8_t k = 0;
    for (uint8_t i = 0; i < room_obj_count; i++) {
        const room_obj_t *o = &room_objs[i];
        if (o->type == OBJ_STOCK && k < SHOP_STOCK) {
            stock[k] = o;
            stock_item[k] = run_shop_item(k);
            if (!(run_drawers[RUN_SHOP_ROOM] & (1 << k)))
                room_map[room_row[o->y] + o->x] = counter_mt[stock_item[k] - 1];
            k++;
        } else if (o->type == OBJ_JEFF) {
            jeff_x = ((int16_t)o->x << 4) + (o->param & 15);
            jeff_y = ((int16_t)o->y << 4) + (o->param >> 4);
        }
    }
}

void shop_enter(void) BANKED {
    if (shop_in)
        music_play(MUSIC_JEFF);
}

void shop_exit(void) BANKED {
    music_play(MUSIC_NONE);
}

void shop_need_sprites(void) BANKED {
    if (shop_in)
        spr_need(&jeff_sheet, BANK(sprite_jeff), &sprite_jeff);
}

void shop_draw(void) BANKED {
    if (!shop_in)
        return;
    // As the Minecraft pack's eyes: still most of the time, a blink now and then, sometimes a look aside
    uint8_t frame = SPR_JEFF_EYES;
    if (++eye_t >= BLINK_EVERY) {
        uint8_t look = blinks == LOOK_EVERY - 1, d = eye_t - BLINK_EVERY;
        if (d < (look ? LOOK_LONG : BLINK_LONG)) {
            frame = look ? SPR_JEFF_LOOK : SPR_JEFF_BLINK;
        } else {
            eye_t = 0;
            blinks = look ? 0 : blinks + 1;
        }
    }
    spr_draw(&jeff_sheet, frame, jeff_x - cam_x, jeff_y - cam_y, 0);
}

static void row_blank(char *row) {
    memset(row, ' ', 20);
    row[20] = 0;
}

// The gold icon and a number right-aligned to column 19
static void gold_right(char *row, uint16_t v) {
    save_num(row + 19, v);
    uint8_t k = 19;
    while (row[k] != ' ')
        k--;
    row[k] = ICON_GOLD;
}

static char *put(char *p, const char *s) {
    while (*s)
        *p++ = *s++;
    return p;
}

// The price panel: the item and its price, your gold, A: buy / B: no. 1 if A.
static uint8_t ask(uint8_t item) {
    char row[21];
    row_blank(row);
    row[1] = icon[item - 1];
    put(row + 3, name[item - 1]);
    gold_right(row, price[item - 1]);
    text_print_win(0, 2, row);
    row_blank(row);
    put(row + 3, "YOU HAVE");
    gold_right(row, inv_gold);
    text_print_win(0, 3, row);
    row_blank(row);
    put(row + 1, "A: BUY");
    put(row + 14, "B: NO");
    text_print_win(0, 4, row);
    hud_panel_set(3);
    HIDE_SPRITES;   // they'd draw over the panel
    sfx_play(SFX_SELECT);
    do {
        vsync();
        bg_flush();
        bg_idle();
        audio_frame();
        input_update();
    } while (!(input_pressed & (J_A | J_B)));
    hud_panel_set(0);
    SHOW_SPRITES;
    return (input_pressed & J_A) != 0;
}

uint8_t shop_use_at(uint8_t mx, uint8_t my) BANKED {
    if (!shop_in)
        return 0;
    for (uint8_t k = 0; k < SHOP_STOCK; k++) {
        // On the counter or its front (the desk under it: the counter's top is out of reach)
        const room_obj_t *o = stock[k];
        if (o->x != mx || (o->y != my && o->y + 1 != my))
            continue;
        if (run_drawers[RUN_SHOP_ROOM] & (1 << k))
            return 0;   // sold: an empty counter
        uint8_t item = stock_item[k];
        if (!ask(item))
            return 1;
        // Minecraft docs 12: the carry limit is checked before the gold is taken
        if (inv_gold < price[item - 1]) {
            hud_message("NOT ENOUGH GOLD.", SAY_FRAMES);
            sfx_play(SFX_LOCKED);
        } else if (!items_buy(item)) {
            hud_message("I CAN'T CARRY MORE.", SAY_FRAMES);
            sfx_play(SFX_LOCKED);
        } else {
            inv_gold -= price[item - 1];
            run_drawers[RUN_SHOP_ROOM] |= 1 << k;
            room_set_metatile(o->x, o->y, MT_SHOP_COUNTER);
            *put(put(msg, "BOUGHT "), name[item - 1]) = 0;
            hud_message(msg, SAY_FRAMES);
        }
        return 1;
    }
    return 0;
}
