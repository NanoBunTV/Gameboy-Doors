"""Phase 9a's items in PyBoy: loot, the HUD's item row, the lights, vitamins, lock tools, the crucifix, the pause.

    tools/venv/Scripts/python.exe scripts/items-test.py build/gbc-debug/doors.gbc
    tools/venv/Scripts/python.exe scripts/items-test.py build/dmg-debug/doors.gb

Needs a DEBUG build (game_warp and the inventory, which the script pokes). Each part from a fresh boot:
  hud      three rows: the item row has the gold's coin; the icons are in the shape glyphs' tiles outside the
           Library and the shapes are back in it
  loot     a seed (poked after the plan, so only the loot changes) whose rooms 1-8 hold gold, an item and a
           bandage; every drawer's take matches scripts/plan.py's copy of run_loot; an item that doesn't fit
           stays and is taken later; a bandage heals 25; loot on a key spot is drawn and taken
  light    the lighter: B lights it (the player's light source is the lighter's), it burns fuel, Select puts it
           out, and burnt out it's gone; the flashlight's cone
  vitamins B: one fewer, +10 HP, faster for 10 s
  locks    a locked door with no key: a lockpick opens it, then the skeleton key (5 uses, one spent)
  crucifix Eyes: no damage, the crucifix spent, he's gone and kept away; Rush: alive, he's gone, and kept away
  pause    Start: the panel, the player and the clock stand still, Start again: back
Screenshots go to build/screens/items-<console>-*.png. Exits non-zero on any failure.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bot import Game, Bot, J, GAME_PLAYING, OBJ_EXIT, OBJ_DRAWER, OBJ_KEYSPOT, RF_LOCKED, RF_KEY_DRAWER, \
    RF_KEY_FIXED, RF_KEY_TAKEN, RF_EXIT_OPEN, RF2_DARK, RF2_EYES, ENT_IDLE, DIRS  # noqa: E402
import plan  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
LIBRARY = 50
HELD_NONE, HELD_LIGHTER, HELD_FLASHLIGHT, HELD_VITAMINS = 0, 1, 2, 3
LIGHT_GLOW, LIGHT_LIGHTER, LIGHT_FLASH = 0, 1, 2
LIGHTER_FUEL, FLASH_CHARGE = 3600, 7200
ENTITY_RUSH = 1
FONT_ALT_TILE = 248   # build/gen/font.h
CHARS = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,!?-:'/_~@*+{}#>=|%[&$()^]"
ICON_GOLD = '^'
KIND_NORMAL = 0

rom = Path(sys.argv[1])
tag = "gbc" if rom.suffix == ".gbc" else "dmg"
failures = []
plan.load(ROOT / "build" / "gen" / "room_templates.c")


def check(cond, what):
    print(("  ok   " if cond else "  FAIL ") + what)
    if not cond:
        failures.append(what)


def boot():
    g = Game(rom)
    b = Bot(g)
    b.wait_playing()
    for _ in range(60):
        g.tick()
    b.tried_fake = b.met_dupe = True
    return g, b


def warp(g, n, calm=True):
    """Enters room n. calm: Screech, or a Rush the room rolled, sent off (the seed is the title's timing, so which
    rooms roll one changes from build to build); the crucifix part wants its Rush"""
    g.mem[g.syms["_game_warp"]] = n
    for _ in range(90):
        quiet(g) if calm else g.tick()


def shot(g, name):
    g.tick(0, render=True)
    g.pyboy.screen.image.save(ROOT / "build" / "screens" / f"items-{tag}-{name}.png")


def quiet(g, mask=0):
    """A frame with Screech and Rush kept away"""
    g.tick(mask)
    if g.u8("_screech_state"):
        g.mem[g.syms["_screech_state"]] = 0
    if g.u8("_entity_state") != ENT_IDLE or g.u8("_entity_queued"):
        g.mem[g.syms["_entity_state"]] = ENT_IDLE
        g.mem[g.syms["_entity_queued"]] = 0


def poke(g, name, v, off=0):
    g.mem[g.syms[name] + off] = v & 0xFF


def poke16(g, name, v):
    g.mem[g.syms[name]] = v & 0xFF
    g.mem[g.syms[name] + 1] = (v >> 8) & 0xFF


def win_row(g, y):
    return "".join(CHARS[t - 192] if 192 <= t < 192 + len(CHARS) else "?"
                   for t in (g.mem[0x9C00 + y * 32 + i] for i in range(20))).rstrip()


def hud_text(g):
    return win_row(g, 1).strip()


def press_until(g, button, cond, tries=8, f=quiet):
    """Taps a button until cond() holds (a tap can fall on a frame the game skipped reading the joypad)"""
    for _ in range(tries):
        f(g, J[button])
        f(g, 0)
        f(g, 0)
        if cond():
            return True
    return False


def inventory(g):
    return dict(gold=g.u16("_inv_gold"), picks=g.u8("_inv_lockpicks"), vit=g.u8("_inv_vitamins"),
                skel=g.u8("_inv_skeleton"), cruc=g.u8("_inv_crucifix"), lighter=g.u8("_inv_lighter"),
                flash=g.u8("_inv_flashlight"), hp=g.u8("_run_hp"), charge=g.u16("_inv_charge"))


ITEM_KEY = {1: "picks", 2: "vit", 3: "lighter", 4: "flash", 5: "cruc", 6: "skel"}
ITEM_FULL = {1: ("_inv_lockpicks", 3), 2: ("_inv_vitamins", 3), 3: ("_inv_lighter", 1),
             4: ("_inv_flashlight", 1), 5: ("_inv_crucifix", 1), 6: ("_inv_skeleton", 5)}


def face(b, obj):
    cellpos, d = b.approach(obj)
    b.walk_to(cellpos)
    for _ in range(3):
        b.step(J[d])
    b.step(0)


def drawers(g):
    return [o for o in g.objects() if o[0] == OBJ_DRAWER]


def loose_spots(g):
    return [o for o in g.objects() if o[0] == OBJ_KEYSPOT and not (o[5] & 1)]


# ------------------------------------------------------------------------------------------ the HUD
print(f"{rom}: hud")
g, b = boot()
for _ in range(10):
    g.tick(J["select"])   # Select held: the HUD
check(g.mem[0x9C00 + 64 + 16] == FONT_ALT_TILE + 6 and win_row(g, 2).endswith("^  0"),
      f"the item row: the coin and the gold ({win_row(g, 2)!r})")
shot(g, "hud")
icons = bytes(g.mem[0x8F80 + i] for i in range(128))   # tiles 248-255 (bg.c bg_tile_addr)
warp(g, LIBRARY)
shapes = bytes(g.mem[0x8F80 + i] for i in range(128))
warp(g, LIBRARY + 1)
again = bytes(g.mem[0x8F80 + i] for i in range(128))
check(icons != shapes and again == icons, "the shape glyphs are the icons outside the Library and shapes in it")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ loot
print(f"{rom}: loot")
g, b = boot()
warp(g, 12)   # plans rooms 0-13
ROOMS = range(1, 9)
tpl = {n: g.u8("_run_template", n) for n in ROOMS}
fl = {n: g.u8("_run_flags", n) for n in ROOMS}
normal = [n for n in ROOMS if plan.info[tpl[n]][0] == KIND_NORMAL]


def want(seed):
    """The seed's loot in rooms 1-8: drawers (room, slot, loot) and loose spots (room, k, loot)"""
    ds, ls = [], []
    for n in normal:
        for d in range(plan.info[tpl[n]][4]):
            ds.append((n, d, plan.loot(seed, n, d, 0, 0)))
        key_k = g.u8("_run_key_slot", n) if (fl[n] & (RF_LOCKED | RF_KEY_DRAWER | RF_KEY_FIXED)) == RF_LOCKED else -1
        for k in range(min(2, plan.info[tpl[n]][5])):
            if k != key_k:
                ls.append((n, k, plan.loot(seed, n, k, 1, 0)))
    return ds, ls


seed = None
for s in range(1, 20000):
    ds, ls = want(s)
    if any(l[1] for _, _, l in ds) and any(l[3] for _, _, l in ds) and any(l[0] for _, _, l in ds) and \
            any(l[0] for _, _, l in ls):
        seed = s
        break
check(seed is not None, f"a seed with gold, an item, a bandage in drawers and gold on a key spot ({seed})")
poke16(g, "_run_seed", seed)
ds, ls = want(seed)
left_tested = taken_loose = 0
for n in normal:
    warp(g, n)
    for d, o in enumerate(drawers(g)):
        l = next(x[2] for x in ds if x[0] == n and x[1] == d)
        gold, item, battery, bandage = l
        if bandage:
            poke(g, "_run_hp", 50)
        if item and not left_tested:
            name, full = ITEM_FULL[item]
            poke(g, name, full)
        before = inventory(g)
        face(b, o)
        press_until(g, "a", lambda: g.u8("_run_drawers", n) & (1 << d))
        for _ in range(4):
            quiet(g)
        after = inventory(g)
        if g.u8("_run_timothy"):
            print(f"  note: Timothy in room {n} drawer {d}")
            continue
        exp = dict(before)
        exp["gold"] += gold
        if bandage:
            exp["hp"] = 75
        if item and left_tested:
            exp[ITEM_KEY[item]] += 1 if item not in (6,) else 5
            if item in (3, 4):
                exp[ITEM_KEY[item]] = 1
        ok = all(after[k] == exp[k] for k in ("gold", "picks", "vit", "skel", "cruc", "lighter", "flash", "hp"))
        check(ok, f"room {n} drawer {d}: {l} -> gold {before['gold']}->{after['gold']}, hp {before['hp']}->"
                  f"{after['hp']}" + (f", item {item}" if item else ""))
        if item and not left_tested:
            left_tested = 1
            check(g.u8("_run_drawers", n) & (1 << d) and after[ITEM_KEY[item]] == before[ITEM_KEY[item]],
                  f"a full bag: item {item} stays in the drawer")
            poke(g, name, 0)
            press_until(g, "a", lambda: g.u8(name) > 0)
            check(g.u8(name) > 0, f"... and A again takes it later ({g.u8(name)})")
            for _ in range(200):   # the messages
                quiet(g)
    for k, o in enumerate(loose_spots(g)[:2]):
        x = [y for y in ls if y[0] == n and y[1] == k]
        if not x or not x[0][2][0]:
            continue
        gold = x[0][2][0]
        if not taken_loose:
            shot(g, "loose")
        before = g.u16("_inv_gold")
        face(b, o)
        press_until(g, "a", lambda: g.u8("_run_drawers", n) & (0x80 >> k))
        check(g.u16("_inv_gold") == before + gold and g.u8("_run_drawers", n) & (0x80 >> k),
              f"room {n} key spot {k}: {gold} gold lying there taken ({before} -> {g.u16('_inv_gold')})")
        taken_loose += 1
check(taken_loose > 0, "loot on a key spot was taken")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ the lights
print(f"{rom}: light")
g, b = boot()
poke(g, "_inv_lighter", 1)
poke16(g, "_inv_fuel", LIGHTER_FUEL)
poke(g, "_inv_held", HELD_LIGHTER)
press_until(g, "b", lambda: g.u8("_inv_lit"))
for _ in range(10):
    quiet(g)
check(g.u8("_inv_lit") and g.u8("_light_src_shape") == LIGHT_LIGHTER, f"B lights the lighter (shape "
      f"{g.u8('_light_src_shape')})")
f0 = g.u16("_inv_fuel")
for _ in range(60):
    quiet(g)
check(0 < f0 - g.u16("_inv_fuel") <= 62, f"it burns fuel ({f0} -> {g.u16('_inv_fuel')})")
press_until(g, "select", lambda: g.u8("_inv_held") != HELD_LIGHTER)
for _ in range(10):
    quiet(g)
check(not g.u8("_inv_lit") and g.u8("_light_src_shape") == LIGHT_GLOW, "Select away: it goes out")
press_until(g, "select", lambda: g.u8("_inv_held") == HELD_LIGHTER)
press_until(g, "b", lambda: g.u8("_inv_lit"))
poke16(g, "_inv_fuel", 3)
for _ in range(20):
    quiet(g)
check(not g.u8("_inv_lighter") and not g.u8("_inv_lit") and g.u8("_inv_held") == HELD_NONE,
      "burnt out: the lighter's gone")
poke(g, "_inv_flashlight", 1)
poke16(g, "_inv_charge", FLASH_CHARGE)
press_until(g, "select", lambda: g.u8("_inv_held") == HELD_FLASHLIGHT)
press_until(g, "b", lambda: g.u8("_inv_lit"))
for _ in range(10):
    quiet(g)
sh = g.u8("_light_src_shape")
check(LIGHT_FLASH <= sh < LIGHT_FLASH + 8 and sh - LIGHT_FLASH == g.u8("_player_facing"),
      f"the flashlight: the cone the way you face (shape {sh})")
shot(g, "flashlight")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ vitamins
print(f"{rom}: vitamins")
g, b = boot()
x0, y0 = g.s16("_player_x"), g.s16("_player_y")


def run_for(frames, d):
    poke16(g, "_player_x", x0)
    poke16(g, "_player_y", y0)
    for _ in range(frames):
        quiet(g, J[d])
    return abs(g.s16("_player_x") - x0) + abs(g.s16("_player_y") - y0)


w, h, solid = g.grid()
cx, cy = g.cell()
d = max(DIRS.items(), key=lambda kv: sum(1 for i in range(1, 6) if 0 <= cx + kv[0][0] * i < w and
                                          0 <= cy + kv[0][1] * i < h and not solid[cy + kv[0][1] * i][cx + kv[0][0] * i]))[1]
walk = run_for(24, d)
poke(g, "_inv_vitamins", 2)
poke(g, "_inv_held", HELD_VITAMINS)
poke(g, "_run_hp", 50)
press_until(g, "b", lambda: g.u8("_inv_vitamins") == 1)
check(g.u8("_inv_vitamins") == 1 and g.u8("_run_hp") == 60 and g.u16("_inv_vit_t") > 0,
      f"B: one taken, +10 HP ({g.u8('_run_hp')})")
fast = run_for(24, d)
check(fast > walk * 1.15, f"faster for a while ({walk} -> {fast} px in 24 frames, walking {d})")
press_until(g, "b", lambda: False, tries=2)
check(g.u8("_inv_vitamins") == 1, "a second dose while one runs does nothing")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ lock tools
print(f"{rom}: locks")
g, b = boot()
warp(g, 40)
locked = [n for n in range(2, 40) if g.u8("_run_flags", n) & RF_LOCKED and
          plan.kind_at(n) == 0 and not g.u8("_run_flags2", n) & RF2_DARK]
check(len(locked) >= 2, f"locked rooms {locked[:2]}")
for i, n in enumerate(locked[:2]):
    warp(g, n)
    poke(g, "_run_key_door", 0)
    poke(g, "_inv_lockpicks", 1 if i == 0 else 0)
    poke(g, "_inv_skeleton", 0 if i == 0 else 5)
    exit_ = [o for o in g.objects() if o[0] == OBJ_EXIT][g.u8("_run_exit", n)]
    face(b, exit_)
    press_until(g, "a", lambda: g.flags() & RF_EXIT_OPEN)
    if i == 0:
        check(g.flags() & RF_EXIT_OPEN and g.u8("_inv_lockpicks") == 0 and g.flags() & RF_KEY_TAKEN,
              f"room {n}: a lockpick opens it, and its key is gone")
    else:
        check(g.flags() & RF_EXIT_OPEN and g.u8("_inv_skeleton") == 4, f"room {n}: the skeleton key opens it "
              f"({g.u8('_inv_skeleton')} uses left; HUD {hud_text(g)!r})")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ the crucifix
print(f"{rom}: crucifix")
g, b = boot()
warp(g, 20)
n = next(n for n in range(10, 19) if plan.kind_at(n) == 0 and not g.u8("_run_flags2", n) & RF2_DARK)
poke(g, "_run_flags2", g.u8("_run_flags2", n) | RF2_EYES, n)
poke(g, "_inv_crucifix", 1)
hp = g.u8("_run_hp")
warp(g, n)
ex, ey = g.s16("_eyes_x"), g.s16("_eyes_y")
for _ in range(120):
    px, py = g.s16("_player_x") + 8, g.s16("_player_y") + 8
    dx, dy = ex - px, ey - py
    # eyes.c octant: the direction he's in
    ax, ay = abs(dx), abs(dy)
    if ay * 5 < ax * 2:
        f = 6 if dx > 0 else 2
    elif ax * 5 < ay * 2:
        f = 0 if dy > 0 else 4
    else:
        f = (7 if dy > 0 else 5) if dx > 0 else (1 if dy > 0 else 3)
    poke(g, "_player_facing", f)
    quiet(g)
check(g.u8("_run_hp") == hp and not g.u8("_inv_crucifix") and not g.u8("_eyes_here") and
      g.u16("_ward_eyes") > g.u16("_run_seconds"), f"Eyes: no harm, the crucifix spent, he's gone (HP {hp} -> "
      f"{g.u8('_run_hp')}, HUD {hud_text(g)!r})")
g.pyboy.stop(save=False)

g, b = boot()
pl = plan.plan(g.u16("_run_seed"), 30)   # rooms not entered yet (he comes only into a room entered the first time)
rooms = [n for n in range(5, 29) if plan.kind_at(n) == 0 and not pl.f2[n] & RF2_DARK and
         plan.info[pl.t[n]][6] > 0]
poke(g, "_inv_crucifix", 1)
poke(g, "_entity_queued", ENTITY_RUSH)
warp(g, rooms[0], calm=False)
fired = g.u8("_entity_state") != ENT_IDLE
for _ in range(900):
    g.tick(0)
    if g.state() != GAME_PLAYING or (fired and g.u8("_entity_state") == ENT_IDLE):
        break
check(fired and g.state() == GAME_PLAYING and not g.u8("_inv_crucifix") and
      g.u16("_ward_rush") > g.u16("_run_seconds"), f"Rush in room {rooms[0]}: alive in the open, the crucifix "
      f"spent (fired {fired}, state {g.state()}, death {g.u8('_game_death')}, crucifix {g.u8('_inv_crucifix')}, "
      f"ward {g.u16('_ward_rush')} at {g.u16('_run_seconds')})")
poke(g, "_entity_queued", ENTITY_RUSH)
warp(g, rooms[1], calm=False)
check(g.u8("_entity_state") == ENT_IDLE and g.u8("_entity_queued") == ENTITY_RUSH,
      "kept away a minute: the next spawn waits")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ the pause
print(f"{rom}: pause")
g, b = boot()
poke(g, "_inv_lockpicks", 2)
press_until(g, "start", lambda: g.u8("_hud_panel") > 0, f=lambda g, m: g.tick(m))
for _ in range(4):
    g.tick(0)
check("PAUSED" in win_row(g, 2) and any("LOCKPICKS" in win_row(g, y) and "X2" in win_row(g, y)
                                        for y in range(3, 14)), f"Start: the panel ({win_row(g, 2)!r})")
shot(g, "pause")
pos, t = g.feet(), g.u16("_run_seconds")
for _ in range(150):
    g.tick(J["right"])
check(g.feet() == pos and g.u16("_run_seconds") == t, "paused: the player and the clock stand still")
press_until(g, "start", lambda: g.u8("_hud_panel") == 0, f=lambda g, m: g.tick(m))
for _ in range(30):
    g.tick(J["right"])
check(g.feet() != pos and g.state() == GAME_PLAYING, "Start again: playing")
g.pyboy.stop(save=False)

print("all passed" if not failures else f"{len(failures)} failed")
sys.exit(1 if failures else 0)
