"""Phase 9b in PyBoy: the save, Knobs, the lift shop and Jeff's Shop.

    tools/venv/Scripts/python.exe scripts/shop-test.py build/gbc-debug/doors.gbc
    tools/venv/Scripts/python.exe scripts/shop-test.py build/dmg-debug/doors.gb

Needs a DEBUG build (game_warp; the script pokes the Knobs, gold and inventory). Each part from a fresh boot
(the cartridge RAM handed in: blank, or what an earlier part saved):
  knobs    a death pays a Knob per door reached and one per 10 gold; the Guiding Light lists them; the title shows
           the balance; it's in SRAM after a power cycle; a damaged save starts fresh
  lift     the lift comes after the title; an offer you can't afford is refused; picked offers are bought only on
           BUY (the Knobs spent, saved, the items in the bag: two lockpicks, a flashlight at half charge); Start
           leaves buying nothing; the song ending leaves buying nothing (owner)
  jeff     room 55 is Jeff's Shop: his music, his eyes, the run's four items on the counter (scripts/plan.py);
           too little gold is refused, B buys nothing, a purchase takes the gold and empties its place, a full bag
           is refused before the gold is taken, the music stops at the next door
  erase    Select + B held on the title, then A: the save is empty
Screenshots go to build/screens/shop-<console>-*.png. Exits non-zero on any failure.
"""
import io
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bot import Game, Bot, J, GAME_PLAYING, GAME_TITLE, GAME_LIFT, OBJ_EXIT  # noqa: E402
import plan  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
GAME_DEAD = 3
DEATH_RUSH = 1
SHOP = 55
OBJ_STOCK = 19
MUSIC_ELEVATOR, MUSIC_JEFF = 8, 9
SONG_FRAMES = 1954                     # lift.c
PRICE = {1: 12, 2: 15, 3: 20, 4: 25, 5: 70, 6: 50}   # shop.c, by LOOT_*
INV = {1: "_inv_lockpicks", 2: "_inv_vitamins", 3: "_inv_lighter", 4: "_inv_flashlight", 5: "_inv_crucifix",
       6: "_inv_skeleton"}
CHARS = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,!?-:'/_~@*+{}#>=|%[&$()^]"
SAVE = 0                               # SRAM offset 0 (0xA000); save.c: "DOOR", version, knobs (2), runs, escapes, best door, ...
KNOBS_AT, RUNS_AT, BEST_AT = 5, 7, 11

rom = Path(sys.argv[1])
tag = "gbc" if rom.suffix == ".gbc" else "dmg"
failures = []
plan.load(ROOT / "build" / "gen" / "room_templates.c")
# The shop's counter metatiles (build/gen/tileset_shop.h)
mt = {}
for line in (ROOT / "build" / "gen" / "tileset_shop.h").read_text().splitlines():
    p = line.split()
    if len(p) == 3 and p[0] == "#define" and p[1].startswith("MT_SHOP_COUNTER"):
        mt[p[1]] = int(p[2])
COUNTER_MT = {1: mt["MT_SHOP_COUNTER_LOCKPICK"], 2: mt["MT_SHOP_COUNTER_VITAMINS"], 3: mt["MT_SHOP_COUNTER_LIGHTER"],
              4: mt["MT_SHOP_COUNTER_FLASHLIGHT"], 5: mt["MT_SHOP_COUNTER_CRUCIFIX"], 6: mt["MT_SHOP_COUNTER_SKELETON"]}


def check(cond, what):
    print(("  ok   " if cond else "  FAIL ") + what)
    if not cond:
        failures.append(what)


def poke(g, name, v, off=0):
    g.mem[g.syms[name] + off] = v & 0xFF


def poke16(g, name, v):
    poke(g, name, v)
    poke(g, name, v >> 8, 1)


def bg_row(g, y):
    return "".join(CHARS[t - 192] if 192 <= t < 192 + len(CHARS) else "?"
                   for t in (g.mem[0x9800 + y * 32 + i] for i in range(20))).rstrip()


def win_row(g, y):
    return "".join(CHARS[t - 192] if 192 <= t < 192 + len(CHARS) else "?"
                   for t in (g.mem[0x9C00 + y * 32 + i] for i in range(20))).rstrip()


def shot(g, name):
    g.tick(0, render=True)
    g.pyboy.screen.image.save(ROOT / "build" / "screens" / f"shop-{tag}-{name}.png")


def ram_of(g):
    """Stops the emulator, keeping the cartridge RAM (a power cycle)"""
    buf = io.BytesIO()
    g.pyboy.stop(save=True, ram_file=buf)
    return buf.getvalue()


def sram(ram, off, n=1):
    return int.from_bytes(ram[off:off + n], "little")


def until(g, cond, limit=600, mask=0):
    for _ in range(limit):
        if cond():
            return True
        g.tick(mask)
    return cond()


def tap(g, button, cond, tries=8):
    """Taps a button until cond() holds (DMG: a tap can fall on a frame the game skipped reading the joypad)"""
    for _ in range(tries):
        g.tick(J[button])
        g.tick(0)
        g.tick(0)
        if cond():
            return True
    return False


def to_lift(g):
    """From power-on through the title into the lift, its panel drawn"""
    until(g, lambda: bg_row(g, 13).strip() == "PRESS START", 600)   # the title is up (it blinks)
    tap(g, "start", lambda: g.state() == GAME_LIFT)
    until(g, lambda: g.u8("_platform_fade") == 0, 200)


def to_room0(g):
    until(g, lambda: g.state() == GAME_PLAYING and g.room() == 0, 900)
    for _ in range(30):
        g.tick(0)


# ------------------------------------------------------------------------------------------ Knobs
print(f"{rom}: knobs")
g = Game(rom)
to_lift(g)
check(g.state() == GAME_LIFT and g.u8("_music_id") == MUSIC_ELEVATOR, "the lift follows the title, Elevator Jam")
check("PRE-RUN SHOP" in bg_row(g, 9) and bg_row(g, 9).endswith("|0"), f"the shop's balance: {bg_row(g, 9)!r}")
tap(g, "start", lambda: g.state() != GAME_LIFT)
to_room0(g)
check(g.u16("_save_knobs") == 0, "a fresh save: no Knobs")
# Reach room 7 (warps count as reaching it), carry 95 gold, and die there
poke(g, "_game_warp", 7)
until(g, lambda: g.room() == 7 and g.state() == GAME_PLAYING, 200)
poke16(g, "_inv_gold", 95)
poke(g, "_game_death", DEATH_RUSH)
poke(g, "_game_state", GAME_DEAD)
until(g, lambda: g.u16("_save_knobs") != 0, 60)
check(g.u16("_save_knobs") == 7 + 9, f"the death paid 7 doors + 9 for 95 gold ({g.u16('_save_knobs')})")
for i in range(900):   # A skips the typing; the Knobs lines come before the menu
    g.tick(J["a"] if i % 20 == 0 and i > 200 else 0)
    if bg_row(g, 15).strip().startswith(">"):
        break
def knob_row(label, paid):
    right = "|" + str(paid)
    return label.ljust(18 - len(right)) + right


rows = [bg_row(g, y).strip() for y in (11, 12, 13)]
check(rows == [knob_row("DOORS 7", 7), knob_row("GOLD 95", 9), knob_row("NOW YOU HAVE", 16)],
      f"the Guiding Light lists them: {rows}")
shot(g, "death")
# Title: down to TITLE, A
tap(g, "down", lambda: bg_row(g, 16).strip().startswith(">"))
tap(g, "a", lambda: g.state() == GAME_TITLE, tries=4)
until(g, lambda: g.u8("_platform_fade") == 0, 200)
for _ in range(10):
    g.tick(0)
check(bg_row(g, 17).startswith(" |16"), f"the title shows the Knobs: {bg_row(g, 17)!r}")
shot(g, "title")
ram = ram_of(g)
check(ram[SAVE:SAVE + 4] == b"DOOR" and sram(ram, SAVE + KNOBS_AT, 2) == 16 and sram(ram, SAVE + RUNS_AT, 2) == 1
      and ram[SAVE + BEST_AT] == 7, "SRAM: the Knobs, one run, best door 7")
# A power cycle keeps them; a damaged save starts fresh
g = Game(rom, ram)
to_lift(g)
check(g.u16("_save_knobs") == 16 and bg_row(g, 9).endswith("|16"), "after a power cycle: 16 Knobs")
g.pyboy.stop(save=False)
bad = bytearray(ram)
bad[SAVE + KNOBS_AT] ^= 0x40
g = Game(rom, bytes(bad))
to_lift(g)
check(g.u16("_save_knobs") == 0, "a damaged save starts fresh")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ the lift shop
print(f"{rom}: lift")
g = Game(rom, ram)
to_lift(g)
# 16 Knobs: lockpicks (50) can't be had
tap(g, "a", lambda: "NOT ENOUGH" in bg_row(g, 10))
check("NOT ENOUGH KNOBS." in bg_row(g, 10) and not g.u8("_lift_picked"), "an offer you can't afford is refused")
poke16(g, "_save_knobs", 400)
tap(g, "a", lambda: g.u8("_lift_picked") & 1)                       # two lockpicks
tap(g, "down", lambda: g.u8("_lift_cursor") == 1)
tap(g, "down", lambda: g.u8("_lift_cursor") == 2)
tap(g, "a", lambda: g.u8("_lift_picked") & 4)                       # the flashlight
check(g.u8("_lift_picked") == 5 and bg_row(g, 11)[1] == "|" and bg_row(g, 13)[1] == "|", "two offers picked, marked")
check(bg_row(g, 16).endswith("|150"), f"BUY shows the total: {bg_row(g, 16)!r}")
shot(g, "lift")
tap(g, "down", lambda: g.u8("_lift_cursor") == 3)
tap(g, "down", lambda: g.u8("_lift_cursor") == 4)
tap(g, "a", lambda: g.state() != GAME_LIFT)
to_room0(g)
check(g.u16("_save_knobs") == 250, f"BUY spent 150 Knobs ({g.u16('_save_knobs')})")
check(g.u8("_inv_lockpicks") == 2 and g.u8("_inv_flashlight") == 1 and g.u16("_inv_charge") == 3600,
      "the bag: two lockpicks and a flashlight at half charge")
ram2 = ram_of(g)
check(sram(ram2, SAVE + KNOBS_AT, 2) == 250, "the spend is saved")
# Start leaves buying nothing
g = Game(rom, ram2)
to_lift(g)
tap(g, "a", lambda: g.u8("_lift_picked"))
tap(g, "start", lambda: g.state() != GAME_LIFT)
to_room0(g)
check(g.u16("_save_knobs") == 250 and g.u8("_inv_lockpicks") == 0, "Start: nothing bought, nothing spent")
g.pyboy.stop(save=False)
# The song ending leaves buying nothing that wasn't confirmed (owner)
g = Game(rom, ram2)
to_lift(g)
tap(g, "a", lambda: g.u8("_lift_picked"))
left = until(g, lambda: g.state() != GAME_LIFT, SONG_FRAMES + 100)
check(left, "the lift leaves by itself when Elevator Jam ends")
to_room0(g)
check(g.u16("_save_knobs") == 250 and g.u8("_inv_lockpicks") == 0, "... buying nothing")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ Jeff's Shop
print(f"{rom}: jeff")
g = Game(rom)
b = Bot(g)
b.wait_playing()
poke(g, "_game_warp", SHOP)
until(g, lambda: g.room() == SHOP and g.state() == GAME_PLAYING, 200)
for _ in range(30):
    g.tick(0)
seed = g.u16("_run_seed")
want = plan.shop_items(seed)
stock = [o for o in g.objects() if o[0] == OBJ_STOCK]
w = g.u8("_room_w")
on_counter = [g.mem[g.syms["_room_map"] + o[2] * w + o[1]] for o in stock]
check(on_counter == [COUNTER_MT[i] for i in want], f"the counter holds the run's four ({want})")
check(g.u8("_music_id") == MUSIC_JEFF, "Jeff's Shop Jingle")
check(any(g.mem[0xFE00 + i * 4] for i in range(40)) and g.u8("_shop_in"), "his eyes are drawn")
shot(g, "jeff")


def at_stock(k):
    o = stock[k]
    cellpos, face = b.approach((o[0], o[1], o[2] + 1, 1, 1, 0))   # its front: the desk under the counter
    b.walk_to(cellpos)
    for _ in range(3):
        b.step(J[face])
    b.step(0)


def ask(k):
    at_stock(k)
    return tap(g, "a", lambda: g.u8("_hud_panel") > 0)


item = want[0]
check(ask(0) and win_row(g, 2).endswith("^" + str(PRICE[item])), f"A: the price panel ({win_row(g, 2)!r})")
shot(g, "price")
tap(g, "a", lambda: g.u8("_hud_panel") == 0)
until(g, lambda: g.u8("_hud_msg_timer"), 20)
check(win_row(g, 1).strip() == "NOT ENOUGH GOLD." and g.u8(INV[item]) == 0, "no gold: refused")
poke16(g, "_inv_gold", 200)
ask(0)
tap(g, "b", lambda: g.u8("_hud_panel") == 0)
check(g.u16("_inv_gold") == 200 and g.u8(INV[item]) == 0, "B: nothing bought")
ask(0)
tap(g, "a", lambda: g.u8("_hud_panel") == 0)
for _ in range(5):
    g.tick(0)
check(g.u16("_inv_gold") == 200 - PRICE[item] and g.u8(INV[item]) > 0, f"bought item {item} for {PRICE[item]} gold")
check(g.mem[g.syms["_room_map"] + stock[0][2] * w + stock[0][1]] == mt["MT_SHOP_COUNTER"]
      and g.mem[g.syms["_run_drawers"] + SHOP] & 1, "its place on the counter is empty")
# A full bag is refused before the gold is taken
item = want[1]
poke(g, INV[item], {1: 3, 2: 3, 6: 5}.get(item, 1))
gold = g.u16("_inv_gold")
ask(1)
tap(g, "a", lambda: g.u8("_hud_panel") == 0)
until(g, lambda: g.u8("_hud_msg_timer"), 20)
check(win_row(g, 1).strip() == "I CAN'T CARRY MORE." and g.u16("_inv_gold") == gold, "a full bag: refused, gold kept")
# Out through his door: the music stops
exit_obj = next(o for o in g.objects() if o[0] == OBJ_EXIT)
cellpos, face = b.approach(exit_obj)
b.walk_to(cellpos)
for _ in range(3):
    b.step(J[face])
tap(g, "a", lambda: g.mem[g.syms["_run_flags"] + SHOP] & 0x08)
b.walk_to((exit_obj[1], exit_obj[2] + 1))
until(g, lambda: g.room() == SHOP + 1 and g.state() == GAME_PLAYING, 200, J["up"])
check(g.room() == SHOP + 1 and g.u8("_music_id") == 0, "through door 56: the music stops")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ erase
print(f"{rom}: erase")
g = Game(rom, ram2)
until(g, lambda: bg_row(g, 13).strip() == "PRESS START", 600)
for _ in range(200):
    g.tick(J["select"] | J["b"])
check(bg_row(g, 13).strip() == "ERASE SAVE?", f"Select + B held: {bg_row(g, 13)!r}")
tap(g, "a", lambda: g.u16("_save_knobs") == 0)
for _ in range(5):
    g.tick(0)
check(g.u16("_save_knobs") == 0 and bg_row(g, 17).startswith(" |0"), "A: the save is erased")
ram3 = ram_of(g)
check(sram(ram3, SAVE + KNOBS_AT, 2) == 0 and sram(ram3, SAVE + RUNS_AT, 2) == 0, "SRAM: empty")

if failures:
    print(f"{len(failures)} FAILED")
    sys.exit(1)
print("all passed")
