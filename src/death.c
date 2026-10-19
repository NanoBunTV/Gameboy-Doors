#pragma bank 255
#include <gb/gb.h>
#include "death.h"
#include "game.h"
#include "hud.h"
#include "bg.h"
#include "room.h"
#include "image.h"
#include "input.h"
#include "audio.h"
#include "sprites.h"
#include "platform.h"
#include "font.h"
#include "save.h"
#include "image_scare_rush.h"
#include "image_scare_ambush.h"
#include "image_scare_screech.h"
#include "image_scare_dupe.h"
#include "image_scare_figure.h"
#include "image_scare_seek.h"

#define SCARE_FRAMES 60
#define TYPE_FRAMES  3       // per character
#define TEXT_X       1
#define TEXT_Y       2
#define MENU_Y       15
#define KNOBS_Y      11      // the Knobs the run paid (Phase 9b), under the hint

uint16_t death_seed;

// Guiding Light hints (DESIGN.md): what killed you and how to survive it. At most 18 characters a line.
static const char *const lines_rush[] = {
    "YOU DIED TO RUSH.", "", "THE LIGHTS FLICKER", "BEFORE HE COMES.", "", "HIDE IN A CLOSET",
    "OR UNDER A BED", "UNTIL HE PASSES.", 0 };
static const char *const lines_ambush[] = {
    "YOU DIED TO AMBUSH", "", "HE COMES BACK MORE", "THAN ONCE.", "", "STAY HIDDEN UNTIL",
    "IT'S QUIET, BUT", "NOT FOR TOO LONG.", 0 };
static const char *const lines_hide[] = {
    "SOMETHING DIDN'T", "WANT YOU IN THERE.", "", "DON'T STAY HIDDEN", "FOR TOO LONG.", "",
    "LISTEN FOR YOUR", "HEARTBEAT.", 0 };
static const char *const lines_screech[] = {
    "SCREECH GOT YOU.", "", "IN THE DARK, IF", "YOU HEAR A PSST,", "", "TURN TO FACE HIM",
    "BEFORE HE BITES.", 0 };
static const char *const lines_eyes[] = {
    "YOU DIED TO EYES.", "", "DON'T LOOK AT HIM.", "", "WALK PAST WITH", "YOUR EYES AWAY,",
    "THEN LEAVE.", 0 };
static const char *const lines_dupe[] = {
    "DUPE GOT YOU.", "", "COUNT THE DOORS.", "", "A WRONG DOOR LIES", "ABOUT ITS NUMBER.", "",
    "OPEN THE NEXT ONE.", 0 };
static const char *const lines_figure[] = {
    "FIGURE GOT YOU.", "", "HE CAN'T SEE YOU,", "BUT HE HEARS YOU.", "", "HOLD B TO SNEAK,", "OR HIDE IN A",
    "CLOSET.", 0 };
static const char *const lines_seek[] = {
    "SEEK GOT YOU.", "", "HE'S SLOWER THAN", "YOU: KEEP RUNNING.", "", "THE RIGHT DOOR", "HAS A NUMBER AND",
    "A BLUE LIGHT.", 0 };
static const char *const lines_fire[] = {
    "THE FIRE GOT YOU.", "", "SEEK IS SLOWER", "THAN YOU.", "", "TAKE THE TIME TO", "GO AROUND THE",
    "FIRE.", 0 };
static const char *const lines_escape[] = {
    "FIGURE GOT YOU.", "", "THE LIFT HAS POWER", "NOW: RUN FOR IT.", "", "HE'S SLOWER THAN", "YOU. DON'T STOP,",
    "DON'T HIDE.", 0 };
static const char *const lines_halt[] = {
    "YOU DIED TO HALT.", "", "WHEN THE SCREEN", "SAYS TURN AROUND,", "WALK THE OTHER WAY", "", "KEEP GOING AND",
    "YOU'LL GET OUT.", 0 };

#ifdef CGB
// The font palette made blue (colour 0 the text, 2 the background)
static const palette_color_t guiding_palette[] = {
    RGB(28, 31, 31), RGB(14, 22, 31), RGB(3, 9, 24), RGB(0, 2, 10) };
#endif

static uint16_t frames;

static void wait_input(void) {
    game_wait_frames(1);
    input_update();
    frames++;
}

void death_jumpscare(uint8_t cause) BANKED {
    spr_begin();
    spr_end();
    hud_hide_now();
    platform_set_dim(0, TINT_NONE);
    platform_set_fade(FADE_BLACK);
    game_wait_frames(1);
    bg_set_scroll(0, 0);
    if (cause == DEATH_AMBUSH)
        image_show(BANK(image_scare_ambush), &image_scare_ambush, 0, 0);
    else if (cause == DEATH_SCREECH)
        image_show(BANK(image_scare_screech), &image_scare_screech, 0, 0);
    else if (cause == DEATH_DUPE)
        image_show(BANK(image_scare_dupe), &image_scare_dupe, 0, 0);
    else if (cause == DEATH_FIGURE || cause == DEATH_ESCAPE)
        image_show(BANK(image_scare_figure), &image_scare_figure, 0, 0);
    else if (cause == DEATH_SEEK || cause == DEATH_FIRE)   // his eye (owner's frame), both ways
        image_show(BANK(image_scare_seek), &image_scare_seek, 0, 0);
    else
        image_show(BANK(image_scare_rush), &image_scare_rush, 0, 0);
    platform_set_fade(0);
    sfx_play(SFX_SCREAM);
    for (uint8_t i = 0; i < SCARE_FRAMES; i++) {
        static const int8_t shake[8] = { 3, -2, 1, -3, 2, -1, 3, -2 };
        bg_set_scroll(shake[i & 7], shake[(i + 3) & 7]);
        game_wait_frames(1);
    }
    bg_set_scroll(0, 0);
    game_fade(0, FADE_BLACK);
}

// Types the hint one character at a time; A or Start shows the rest at once
static void type_lines(const char *const *lines) {
    uint8_t fast = 0;
    char one[2] = { 0, 0 };
    for (uint8_t i = 0; lines[i]; i++) {
        uint16_t addr = 0x9800u + ((uint16_t)(TEXT_Y + i) << 5) + TEXT_X;
        for (const char *c = lines[i]; *c; c++) {
            one[0] = *c;
            text_queue(addr++, one);
            if (fast)
                continue;
            // Silent: a click per character clashed with the Guiding Light song (owner, 2026-10-18)
            for (uint8_t f = 0; f < TYPE_FRAMES; f++) {
                wait_input();
                if (input_pressed & (J_A | J_START))
                    fast = 1;
            }
        }
        if (!fast)
            for (uint8_t f = 0; f < 10; f++)
                wait_input();
    }
}

static uint8_t menu(void) {
    uint8_t choice = DEATH_TRY_AGAIN;
    text_queue(0x9800u + (MENU_Y << 5) + 6, "TRY AGAIN");
    text_queue(0x9800u + ((MENU_Y + 1) << 5) + 6, "TITLE");
    while (1) {
        text_queue(0x9800u + (MENU_Y << 5) + 4, choice == DEATH_TRY_AGAIN ? ">" : " ");
        text_queue(0x9800u + ((MENU_Y + 1) << 5) + 4, choice == DEATH_TITLE ? ">" : " ");
        wait_input();
        if (input_pressed & (J_UP | J_DOWN | J_SELECT)) {
            choice ^= 1;
            sfx_play(SFX_SELECT);
        }
        if (input_pressed & (J_A | J_START))
            return choice;
    }
}

uint8_t death_run(uint8_t cause) BANKED {
    game_state = GAME_DEAD;
    save_run_end(cause);   // the Knobs, and the statistics (before anything empties the bag)
    sfx_rumble(0, RUMBLE_RUSH);
    music_play(MUSIC_NONE);
    platform_set_dim(0, TINT_NONE);
    hud_hide_now();
    spr_begin();
    spr_end();
    frames = 0;

    if (cause == DEATH_RUSH || cause == DEATH_AMBUSH || cause == DEATH_SCREECH || cause == DEATH_DUPE ||
        cause == DEATH_FIGURE || cause == DEATH_SEEK || cause == DEATH_FIRE || cause == DEATH_ESCAPE)
        death_jumpscare(cause);
    else
        game_fade(platform_fade, FADE_BLACK);

    // The Guiding Light
    screen_fill(FONT_FIRST_TILE, PAL_TEXT);   // the font's first character is the space
#ifdef CGB
    platform_set_bkg_palettes(PAL_TEXT, guiding_palette, 1);
#endif
    bg_set_scroll(0, 0);
    music_play(MUSIC_GUIDING);
    game_fade(FADE_BLACK, 0);
    static const char *const *const lines[] = { lines_rush, lines_rush, lines_ambush, lines_hide, lines_screech,
                                                lines_eyes, lines_dupe, lines_halt, lines_figure, lines_seek,
                                                lines_fire, lines_escape };
    type_lines(lines[cause]);
    save_print_knobs(TEXT_X, KNOBS_Y);
    uint8_t choice = menu();
    sfx_play(SFX_SELECT);
    game_fade(0, FADE_BLACK);
    music_play(MUSIC_NONE);
    hud_font_palette();
#ifdef AUTOTEST
    death_seed = AUTOTEST_SEED;
#else
    death_seed = frames ^ ((uint16_t)DIV_REG << 8) ^ DIV_REG;
#endif
    return choice;
}
