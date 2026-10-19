"""Rooms 88-100 (Phase 8a) in PyBoy: the Courtyard, the Greenhouse, Snares, Rush in the dark, the plan.

    tools/venv/Scripts/python.exe scripts/garden-test.py build/gbc-debug/doors.gbc
    tools/venv/Scripts/python.exe scripts/garden-test.py build/dmg-debug/doors.gb

Needs a DEBUG build (game_warp, which the script pokes). Each part from a fresh boot:
  plan    scripts/plan.py against the ROM's plan of rooms 0-100 (templates, locks, rolls, the real exit)
  rules   88 has one door, on top; 89 the Courtyard is lit with nothing planned and rain streaks on screen;
          90-98 are dark with 3 Snares on floor cells in the room's snare rectangles, their four-ways Dupe;
          99 has no Snares and nothing planned; no Halt in 88-99
  snare   onto a Snare: 20 HP, held 5 s (pushing doesn't move you), then 4 s it can't catch you again; at
          15 HP it leaves 10
  rush    an Ambush queued into a new Greenhouse room comes as a Rush, in the dark, and the bot hides from him
  walk    the bot plays 89 to 100 (Snares are walls to it); room 100 is a hotel room again
Screenshots go to build/screens/garden-<console>-*.png. Exits non-zero on any failure.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bot import Game, Bot, J, GAME_PLAYING, GAME_DEAD, OBJ_EXIT, RF_LOCKED, RF_FINAL, RF2_DARK, RF2_DUPE, \
    RF2_EYES, RF2_HALT, ENT_IDLE, ENT_WARN  # noqa: E402
import plan  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OBJ_SNARE = 11
MTF_SNARE = 0x10
KIND_COURTYARD, KIND_GREENHOUSE = 8, 9
RF_SPAWN = 0x40
ENTITY_RUSH, ENTITY_AMBUSH = 1, 2
ROOT_FRAMES, GRACE_FRAMES = 300, 240

rom = Path(sys.argv[1])
tag = "gbc" if rom.suffix == ".gbc" else "dmg"
failures = []


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
    b.tried_fake = b.met_dupe = True   # the bot just takes the real doors here
    return g, b


def warp(g, n):
    g.mem[g.syms["_game_warp"]] = n
    for _ in range(90):
        g.tick()


def shot(g, name):
    g.tick(0, render=True)
    g.pyboy.screen.image.save(ROOT / "build" / "screens" / f"garden-{tag}-{name}.png")


def quiet(g, mask=0):
    """A frame with Screech kept away: a rooted player can't turn to face him, and his bite would muddle the
    Snare's numbers (that he can bite you then is the Greenhouse working as meant)"""
    g.tick(mask)
    if g.u8("_screech_state"):
        g.mem[g.syms["_screech_state"]] = 0


def kind(g, n):
    return g.u8("_room_template_info", g.u8("_run_template", n) * 7)


def snares(g):
    return [(g.u8("_snare_x", i), g.u8("_snare_y", i)) for i in range(3) if g.u8("_snare_x", i) != 0xFF]


def oam_sprites(g):
    """Hardware sprites on screen (shadow OAM at 0xC000)"""
    return sum(1 for i in range(40) if 0 < g.mem[0xC000 + i * 4] < 160)


# ------------------------------------------------------------------------------------------ the plan
print(f"{rom}: plan")
g, b = boot()
warp(g, 99)   # plans rooms 0-100
plan.load(ROOT / "build" / "gen" / "room_templates.c")
p = plan.plan(g.u16("_run_seed"), 100)
bad = []
for n in range(101):
    rom_state = (g.u8("_run_template", n), g.u8("_run_flags", n) & ~0x0C, g.u8("_run_flags2", n) & ~0xB4,
                 g.u8("_run_exit", n), g.u8("_run_key_slot", n))
    py_state = (p.t[n], p.flags[n], p.f2[n], p.exit[n], p.slot[n])
    if rom_state != py_state:
        bad.append((n, rom_state, py_state))
check(not bad, f"plan.py matches the ROM for rooms 0-100 (seed {g.u16('_run_seed'):04X}){'' if not bad else ': ' + str(bad[:3])}")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ the rules
print(f"{rom}: rules")
g, b = boot()
warp(g, 88)
ex = [o for o in g.objects() if o[0] == OBJ_EXIT]
check(len(ex) == 1 and ex[0][5] == 1, f"room 88 has one door, on top ({[o[5] for o in ex]})")
warp(g, 89)
check(kind(g, 89) == KIND_COURTYARD, "room 89 is the Courtyard")
f, f2 = g.u8("_run_flags", 89), g.u8("_run_flags2", 89)
check(not g.u8("_light_dark") and not f & (RF_SPAWN | RF_LOCKED) and not f2, f"the Courtyard: lit, nothing planned "
      f"(flags {f:02X} {f2:02X})")
for _ in range(20):
    g.tick()
check(oam_sprites(g) >= 6, f"rain streaks over the Courtyard ({oam_sprites(g)} sprites on screen)")
shot(g, "courtyard")
halts = [n for n in range(88, 100) if g.u8("_run_flags2", n) & RF2_HALT]
check(not halts, f"no Halt in rooms 88-99 {halts}")
for n in range(90, 100):
    warp(g, n)
    f2 = g.u8("_run_flags2", n)
    multi = sum(1 for o in g.objects() if o[0] == OBJ_EXIT) > 1
    s = snares(g)
    w = g.u8("_room_w")
    cells = set()
    for o in g.objects():
        if o[0] == OBJ_SNARE:
            cells |= {(x, y) for y in range(o[2], o[2] + o[4]) for x in range(o[1], o[1] + o[3])}
    on_snare_mt = all(g.mem[g.syms["_mt_flags"] + g.mem[g.syms["_room_map"] + y * w + x]] & MTF_SNARE for x, y in s)
    check(kind(g, n) == KIND_GREENHOUSE and g.u8("_light_dark") and f2 & RF2_DARK, f"room {n}: the Greenhouse, dark")
    if n < 99:
        check(len(set(s)) == 3 and set(s) <= cells and on_snare_mt, f"room {n}: 3 Snares in its rectangles {s}")
        check(not multi or f2 & RF2_DUPE, f"room {n}: {'a four-way, Dupe' if multi else 'a hall'}")
    else:
        f = g.u8("_run_flags", n)
        check(not s and not f2 & (RF2_DUPE | RF2_EYES) and not f & RF_SPAWN, f"room 99: safe (no Snares, Dupe, Eyes, "
              f"spawn; {s} {f:02X} {f2:02X})")
    if n == 90:
        shot(g, "greenhouse")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ Snares
print(f"{rom}: snare")
g, b = boot()
warp(g, 90)
w, h, solid = g.grid()
sx, sy = snares(g)[0]
side = next(((sx + dx, sy + dy), d) for (dx, dy), d in (((0, 1), "up"), ((0, -1), "down"), ((1, 0), "left"), ((-1, 0), "right"))
            if not solid[sy + dy][sx + dx])
b.walk_to(side[0], check=False)
hp = g.u8("_run_hp")
for i in range(40):
    quiet(g, J[side[1]])
    if g.u16("_snare_root"):
        break
check(g.u16("_snare_root") > 0, f"stepping onto the Snare at {(sx, sy)} catches you")
check(g.u8("_run_hp") == hp - 20, f"it costs 20 HP ({hp} -> {g.u8('_run_hp')})")
shot(g, "caught")
pos = g.feet()
for _ in range(120):
    quiet(g, J[side[1]] | J["a"])
check(g.feet() == pos and g.u16("_snare_root") > 0, "held: pushing and A do nothing")
for _ in range(ROOT_FRAMES):
    quiet(g, 0)
    if not g.u16("_snare_root"):
        break
check(not g.u16("_snare_root") and g.u8("_snare_grace") > 0, "let go after 5 s, then the grace")
hp = g.u8("_run_hp")
for _ in range(GRACE_FRAMES - 40):
    quiet(g, 0)
check(not g.u16("_snare_root") and g.u8("_run_hp") == hp, "standing on it during the grace: not caught again")
back = {"up": "down", "down": "up", "left": "right", "right": "left"}[side[1]]
for _ in range(14):   # a step off it (not back out through the door behind)
    quiet(g, J[back])
for _ in range(600):   # (DMG drops frames: wait on the game's own count, not the emulator's)
    if not g.u8("_snare_grace"):
        break
    quiet(g, 0)
g.mem[g.syms["_run_hp"]] = 15
for i in range(80):
    quiet(g, J[side[1]])
    if g.u16("_snare_root"):
        break
check(g.u16("_snare_root") > 0 and g.u8("_run_hp") == 10, f"at 15 HP it leaves 10 ({g.u8('_run_hp')})")
check(g.state() == GAME_PLAYING, "it never kills")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ Rush in the dark
print(f"{rom}: rush")
g, b = boot()
warp(g, 92)
g.mem[g.syms["_entity_queued"]] = ENTITY_AMBUSH
warp(g, 93)
check(g.u8("_entity_state") != ENT_IDLE and g.u8("_entity_kind") == ENTITY_RUSH and g.u8("_light_dark"),
      f"a queued Ambush comes into the dark Greenhouse as a Rush (state {g.u8('_entity_state')}, "
      f"kind {g.u8('_entity_kind')})")
try:
    b.hide()
    check(g.state() == GAME_PLAYING, "hid from him")
except RuntimeError as e:
    check(False, f"hid from him: {e}")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ walking through
print(f"{rom}: walk")
g, b = boot()
warp(g, 89)
try:
    while g.room() < 100:
        n = g.room()
        shot(g, f"{n:03d}")
        b.play_room()
        if g.state() == GAME_DEAD:
            break
    check(g.room() == 100 and g.state() == GAME_PLAYING, f"walked from 89 to room {g.room()} (HP {g.u8('_run_hp')}, "
                                                         f"hid {b.hides} times)")
except RuntimeError as e:
    check(False, f"walking 89-100: {e} (room {g.room()})")
if g.room() == 100:
    check(kind(g, 100) == 0 and g.flags() & RF_FINAL, "room 100: a hotel room, its door shut for now")
    shot(g, "100")
g.pyboy.stop(save=False)

print("all passed" if not failures else f"{len(failures)} failed")
sys.exit(1 if failures else 0)
