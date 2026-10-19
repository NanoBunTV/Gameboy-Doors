// Jeff's Shop, room 55 (Phase 9b, owner 2026-10-19, after the Minecraft version's docs 12): a lit, safe room;
// Jeff is two eyes in the dark doorway behind his counter, and four of the six items lie on it, one of each, for
// gold (lockpick 12, vitamins 15, lighter 20, flashlight 25, skeleton key 50, crucifix 70). A on one shows its
// price; A again buys it (the carry limits are checked before the gold is taken), B doesn't. His jingle plays
// while you're there.
#ifndef SHOP_H
#define SHOP_H

#include <gb/gb.h>
#include <stdint.h>

void shop_room_loaded(void) BANKED;   // the stock not yet bought onto the counter (the map is fresh from ROM)
void shop_enter(void) BANKED;         // the music
void shop_exit(void) BANKED;
void shop_need_sprites(void) BANKED;
void shop_draw(void) BANKED;          // his eyes, blinking and glancing about

// A at metatile (mx, my): 1 if it was a place on the counter
uint8_t shop_use_at(uint8_t mx, uint8_t my) BANKED;

#endif
