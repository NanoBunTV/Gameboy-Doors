# DOORS for Game Boy: builds build/gbc/doors.gbc (GBC only) and build/dmg/doors.gb (DMG).
# Each target has its own folder because the linker names the .map/.noi/.sym files after the ROM's base
# name, and BGB, SameBoy and Emulicious load the .sym next to the ROM.
# Run from the project root with GNU make (devkitPro MSYS2's make works), or use build.ps1. See Docs/BUILDING.md.
#
# All paths are relative: the project folder has a space in it, which make can't handle in absolute paths.

GBDK    ?= tools/gbdk
LCC     := $(GBDK)/bin/lcc
PYTHON  ?= tools/venv/Scripts/python.exe
HUGE    ?= tools/hugedriver
UGE2SRC ?= tools/hugetracker/uge2source.exe
ROMNAME := doors
TITLE   := DOORS

SRCDIR  := src
OUTDIR  := build
GEN     := $(OUTDIR)/gen

# Assets: each Tiled tileset, Tiled room and sprite sheet becomes a generated .c/.h pair in build/gen
TILESETS := $(wildcard assets/tilesets/*.tsx)
ROOMS    := $(wildcard assets/rooms/*.tmx)
SPRITES  := $(wildcard assets/sprites/*.json)
IMAGES   := $(wildcard assets/title/*.gbc.png) $(wildcard assets/scares/*.gbc.png)
SONGS    := $(wildcard assets/music/*.uge)
GEN_C := $(patsubst assets/tilesets/%.tsx,$(GEN)/tileset_%.c,$(TILESETS)) \
         $(patsubst assets/rooms/%.tmx,$(GEN)/room_%.c,$(ROOMS)) \
         $(GEN)/room_templates.c \
         $(patsubst assets/sprites/%.json,$(GEN)/sprite_%.c,$(SPRITES)) \
         $(GEN)/font.c \
         $(patsubst %.gbc.png,$(GEN)/image_%.c,$(notdir $(IMAGES)))
GEN_H := $(GEN_C:.c=.h)
# Songs: uge2source writes only a .c (the declarations are in src/audio.c)
GEN_C += $(patsubst assets/music/%.uge,$(GEN)/music_%.c,$(SONGS))
ASSET_TOOL := scripts/assets.py

# Shared sources plus the target's own platform folder
SHARED_C := $(wildcard $(SRCDIR)/*.c)
CGB_C    := $(SHARED_C) $(wildcard $(SRCDIR)/platform/cgb/*.c)
DMG_C    := $(SHARED_C) $(wildcard $(SRCDIR)/platform/dmg/*.c)

CFLAGS  := -Wa-l -I$(SRCDIR) -I$(GEN) -I$(HUGE)/include -Wf--max-allocs-per-node50000

# make AUTOTEST=1: the player walks a fixed route by itself (input.c), for scripts/bgb-check.ps1.
# Built into build/gbc-autotest and build/dmg-autotest so it never mixes with the real ROMs.
ifdef AUTOTEST
VARIANT := -autotest
CFLAGS  += -DAUTOTEST -Itests
TEST_C  := $(wildcard tests/*.c)   # the recorded run (scripts/bot.py)
endif
CGB_DIR := $(OUTDIR)/gbc$(VARIANT)
DMG_DIR := $(OUTDIR)/dmg$(VARIANT)

CGB_O := $(patsubst $(SRCDIR)/%.c,$(CGB_DIR)/obj/%.o,$(CGB_C)) $(patsubst $(GEN)/%.c,$(CGB_DIR)/obj/gen/%.o,$(GEN_C)) \
         $(patsubst tests/%.c,$(CGB_DIR)/obj/tests/%.o,$(TEST_C))
DMG_O := $(patsubst $(SRCDIR)/%.c,$(DMG_DIR)/obj/%.o,$(DMG_C)) $(patsubst $(GEN)/%.c,$(DMG_DIR)/obj/gen/%.o,$(GEN_C)) \
         $(patsubst tests/%.c,$(DMG_DIR)/obj/tests/%.o,$(TEST_C))
CGB_ROM := $(CGB_DIR)/$(ROMNAME).gbc
DMG_ROM := $(DMG_DIR)/$(ROMNAME).gb

HEADERS := $(wildcard $(SRCDIR)/*.h) $(GEN_H)
# MBC5 + RAM + battery, 4 RAM banks (32 KB), ROM size picked automatically, .sym file for emulator debuggers.
# -Wb-ext=.rel: bankpack (autobanking) otherwise overwrites the .o files with banks filled in, and the next
# link that doesn't recompile everything fails in ihxcheck without a message (found 2026-10-17).
# hUGEDriver: the release's prebuilt library for GBDK (Docs/BUILDING.md).
LDFLAGS := -autobank -Wb-ext=.rel -Wl-m -Wl-j -Wm-yt0x1B -Wm-ya4 -Wm-yoA -Wm-yn"$(TITLE)" -Wm-yS            -Wl-l$(HUGE)/gbdk/hUGEDriver.lib

.PHONY: all gbc dmg assets clean

all: gbc dmg
gbc: $(CGB_ROM)
dmg: $(DMG_ROM)
assets: $(GEN_C)

# GBC only: CGB flag 0xC0
$(CGB_ROM): $(CGB_O)
	$(LCC) $(LDFLAGS) -Wm-yC -o $@ $^

# DMG: CGB flag 0x00 (runs on a GBC in compatibility mode)
$(DMG_ROM): $(DMG_O)
	$(LCC) $(LDFLAGS) -o $@ $^

# --- generated asset sources (one run writes both the .c and the .h)
$(GEN)/tileset_%.c $(GEN)/tileset_%.h: assets/tilesets/%.tsx assets/tilesets/%.gbc.png assets/tilesets/%.dmg.png $(ASSET_TOOL)
	$(PYTHON) $(ASSET_TOOL) tileset $< $(GEN)

$(GEN)/room_%.c $(GEN)/room_%.h: assets/rooms/%.tmx $(TILESETS) $(ASSET_TOOL)
	$(PYTHON) $(ASSET_TOOL) room $< $(GEN)

$(GEN)/sprite_%.c $(GEN)/sprite_%.h: assets/sprites/%.json assets/sprites/%.gbc.png assets/sprites/%.dmg.png $(ASSET_TOOL)
	$(PYTHON) $(ASSET_TOOL) sprite $< $(GEN)

# Every room template in one table for the generator (src/run.c)
$(GEN)/room_templates.c $(GEN)/room_templates.h: $(ROOMS) $(TILESETS) $(ASSET_TOOL)
	$(PYTHON) $(ASSET_TOOL) roomlist $(GEN) $(ROOMS)

$(GEN)/image_%.c $(GEN)/image_%.h: assets/title/%.gbc.png assets/title/%.dmg.png $(ASSET_TOOL)
	$(PYTHON) $(ASSET_TOOL) image $< $(GEN)

$(GEN)/image_%.c $(GEN)/image_%.h: assets/scares/%.gbc.png assets/scares/%.dmg.png $(ASSET_TOOL)
	$(PYTHON) $(ASSET_TOOL) image $< $(GEN)

# hUGETracker songs -> C, banked (-b 255: autobank picks the bank)
$(GEN)/music_%.c: assets/music/%.uge $(ASSET_TOOL)
	@mkdir -p $(GEN)/raw
	$(UGE2SRC) $< -b 255 music_$* $(GEN)/raw/music_$*.c
	$(PYTHON) $(ASSET_TOOL) songfix $(GEN)/raw/music_$*.c $@

$(GEN)/font.c $(GEN)/font.h: assets/fonts/font.json assets/fonts/font.gbc.png assets/fonts/font.dmg.png $(ASSET_TOOL)
	$(PYTHON) $(ASSET_TOOL) font $< $(GEN)

# --- compiling (every object depends on every header: simple, and the project is small)
$(CGB_DIR)/obj/%.o: $(SRCDIR)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(LCC) $(CFLAGS) -DCGB -c -o $@ $<

$(DMG_DIR)/obj/%.o: $(SRCDIR)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(LCC) $(CFLAGS) -DDMG -c -o $@ $<

$(CGB_DIR)/obj/gen/%.o: $(GEN)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(LCC) $(CFLAGS) -DCGB -c -o $@ $<

$(DMG_DIR)/obj/gen/%.o: $(GEN)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(LCC) $(CFLAGS) -DDMG -c -o $@ $<

$(CGB_DIR)/obj/tests/%.o: tests/%.c $(HEADERS) $(wildcard tests/*.h)
	@mkdir -p $(dir $@)
	$(LCC) $(CFLAGS) -DCGB -c -o $@ $<

$(DMG_DIR)/obj/tests/%.o: tests/%.c $(HEADERS) $(wildcard tests/*.h)
	@mkdir -p $(dir $@)
	$(LCC) $(CFLAGS) -DDMG -c -o $@ $<

clean:
	rm -rf $(OUTDIR)/gbc $(OUTDIR)/dmg $(OUTDIR)/gbc-autotest $(OUTDIR)/dmg-autotest $(GEN)
