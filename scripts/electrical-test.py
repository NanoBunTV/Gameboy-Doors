"""Room 100 and the ending (Phase 8b) in PyBoy.

    tools/venv/Scripts/python.exe scripts/electrical-test.py build/gbc-debug/doors.gbc
    tools/venv/Scripts/python.exe scripts/electrical-test.py build/dmg-debug/doors.gb

Needs a DEBUG build (game_warp). Each part from a fresh boot:
  room    room 99 has one door, on top; room 100 is the warehouse: ten fuses on shelves, the key on a key spot,
          Figure waiting in his cage and not moving before the lever
  puzzle  (Figure kept away) the lever: the gates go but the lift's two, door 100 seals, the sliding door
          parts; all ten fuses, the key, the breaker door, the box (the tenth seals the breaker room), three
          rounds of the breaker; the power: the lift opens, the player runs, Figure out of his door 3 s later;
          into the lift; the ending and the results; Start back to the title
  figure  after the lever, walking: he hears you and comes; sneaking (B): never heard
  escape  standing still after the power: Figure catches you (cause DEATH_ESCAPE)
  stack   how deep the stack went during the puzzle run, from a canary pattern under it (GBC WRAM is tight)
Screenshots go to build/screens/room100-<console>-*.png. Exits non-zero on any failure.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bot import Game, Bot, J, GAME_PLAYING, GAME_DEAD, OBJ_ENTRY, OBJ_EXIT, OBJ_KEYSPOT  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
GAME_TITLE, GAME_WON = 0, 5
KIND_ELECTRICAL = 10
OBJ_FIGURE, OBJ_LEVER, OBJ_BREAKER, OBJ_BDOOR, OBJ_FDOOR, OBJ_LIFT, OBJ_LIGHT, OBJ_SAFE = 9, 12, 13, 14, 15, 16, 17, 18
MTF_GATE, MTF_SLIDE = 0x40, 0x80
ELEC_WAIT, ELEC_OPENING, ELEC_SEARCH, ELEC_SEALED, ELEC_POWER, ELEC_ESCAPE, ELEC_WON = range(7)
FIG_OFF, FIG_CHASE = 0, 4
DEATH_ESCAPE = 11
FUSES = 10
CANARY = 0xA5

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
    b.tried_fake = b.met_dupe = True
    return g, b


def warp(g, n):
    g.mem[g.syms["_game_warp"]] = n
    for _ in range(90):
        g.tick()


def shot(g, name):
    g.tick(0, render=True)
    g.pyboy.screen.image.save(ROOT / "build" / "screens" / f"room100-{tag}-{name}.png")


def kind(g, n):
    return g.u8("_room_template_info", g.u8("_run_template", n) * 7)


def mt_names():
    names = {}
    for line in (ROOT / "build" / "gen" / "tileset_electrical.h").read_text().splitlines():
        p = line.split()
        if len(p) == 3 and p[0] == "#define" and p[1].startswith("MT_ELECTRICAL_"):
            names[p[1][len("MT_ELECTRICAL_"):].lower()] = int(p[2])
    return names


MT = mt_names()


def cells(g):
    return g.u8("_room_w") * g.u8("_room_h")


def mt_at(g, x, y):
    return g.mem[g.syms["_room_map"] + y * g.u8("_room_w") + x]


def fuses(g):
    base = g.syms["_room_map"] + 2 * cells(g) + 512   # electrical.c's scratch: fuse_x, then fuse_y
    return [(g.mem[base + i], g.mem[base + FUSES + i]) for i in range(FUSES)]


def obj(g, t):
    return next(o for o in g.objects() if o[0] == t)


def quiet(g, mask=0):
    """A frame with Figure kept away (the puzzle is tested on its own)"""
    g.tick(mask)
    if g.u8("_figure_state") != FIG_OFF:
        g.mem[g.syms["_figure_state"]] = FIG_OFF


def walk(b, cell, keep_off=True):
    g = b.g
    if keep_off and g.u8("_figure_state") != FIG_OFF:
        g.mem[g.syms["_figure_state"]] = FIG_OFF
    ok = b.walk_to(cell, check=False)
    if keep_off and g.u8("_figure_state") != FIG_OFF:
        g.mem[g.syms["_figure_state"]] = FIG_OFF
    return ok


def use_up(b, x, y, keep_off=True):
    """Stand below (x, y), face up, press A"""
    g = b.g
    walk(b, (x, y + 1), keep_off)
    for _ in range(3):
        (quiet if keep_off else lambda g, m: g.tick(m))(g, J["up"])
    (quiet if keep_off else lambda g, m: g.tick(m))(g, 0)
    for _ in range(2):
        (quiet if keep_off else lambda g, m: g.tick(m))(g, J["a"])
        (quiet if keep_off else lambda g, m: g.tick(m))(g, 0)


def to_room100(g):
    warp(g, 99)
    warp(g, 100)


def press(g, button, frames=2):
    for _ in range(frames):
        quiet(g, J[button])
    quiet(g, 0)
    quiet(g, 0)


# ------------------------------------------------------------------------------------------ the room
print(f"{rom}: room")
g, b = boot()
warp(g, 99)
ex = [o for o in g.objects() if o[0] == OBJ_EXIT]
check(len(ex) == 1 and ex[0][5] == 1, f"room 99 has one door, on top ({[o[5] for o in ex]})")
warp(g, 100)
check(g.room() == 100 and kind(g, 100) == KIND_ELECTRICAL, f"room 100 is the warehouse (kind {kind(g, 100)})")
check(g.u8("_electrical_state") == ELEC_WAIT, "the lever not pulled yet")
fs = fuses(g)
check(len(set(fs)) == FUSES and all(mt_at(g, x, y) == MT["shelf_fuse"] for x, y in fs), f"ten fuses on shelves {fs}")
spots = [o for o in g.objects() if o[0] == OBJ_KEYSPOT and not o[5] & 1]
check(g.u8("_elec_key_slot") < len(spots), f"the key on key spot {g.u8('_elec_key_slot')} of {len(spots)}")
fx0, fy0 = g.s16("_figure_x"), g.s16("_figure_y")
for _ in range(120):
    g.tick(0)
check(g.u8("_figure_state") != FIG_OFF and (g.s16("_figure_x"), g.s16("_figure_y")) == (fx0, fy0),
      "Figure waits in his cage until the lever")
shot(g, "start")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ the puzzle
print(f"{rom}: puzzle")
g, b = boot()
# The stack canary: everything between the end of the data and a little under the stack pointer
data_end = g.syms["s__HEAP"]   # after _DATA and _INITIALIZED
sp = g.pyboy.register_file.SP
for a in range(data_end, sp - 16):
    g.mem[a] = CANARY
to_room100(g)
lever = obj(g, OBJ_LEVER)
use_up(b, lever[1], lever[2] + 1)
for _ in range(10):   # (its frame is a long one)
    if g.u8("_electrical_state") != ELEC_WAIT:
        break
    quiet(g, 0)
check(g.u8("_electrical_state") in (ELEC_OPENING, ELEC_SEARCH), f"the lever pulled (state {g.u8('_electrical_state')})")
lift = obj(g, OBJ_LIFT)
w, h = g.u8("_room_w"), g.u8("_room_h")
gates = [(x, y) for y in range(h) for x in range(w) if g.mem[g.syms["_mt_flags"] + mt_at(g, x, y)] & MTF_GATE]
check(sorted(gates) == [(lift[1], lift[2] + 2), (lift[1] + 1, lift[2] + 2)], f"the gates gone but the lift's {gates}")
entry = obj(g, OBJ_ENTRY)
check(mt_at(g, entry[1], entry[2]) == MT["wall_top"], "door 100 sealed")
for _ in range(400):
    quiet(g, 0)
    if g.u8("_electrical_state") == ELEC_SEARCH:
        break
slides = [(x, y) for y in range(h) for x in range(w) if g.mem[g.syms["_mt_flags"] + mt_at(g, x, y)] & MTF_SLIDE]
check(g.u8("_electrical_state") == ELEC_SEARCH and not slides, f"the sliding door open ({len(slides)} left)")
shot(g, "lever")
try:
    for i, (x, y) in enumerate(fs):
        use_up(b, x, y)
        if i == 4:
            shot(g, "fuse")
    check(g.u8("_electrical_carried") == FUSES and g.u16("_elec_taken") == 0x3FF,
          f"all ten fuses taken (carried {g.u8('_electrical_carried')})")
    key = spots[g.u8("_elec_key_slot")]
    walk(b, (key[1], key[2]))
    press(g, "a")
    check(g.u8("_elec_key_taken") == 1, "the key")
    bd = obj(g, OBJ_BDOOR)
    use_up(b, bd[1], bd[2] + 1)
    check(g.u8("_elec_key_taken") == 2 and mt_at(g, bd[1], bd[2]) == MT["copper_open_top"], "the breaker door unlocked")
    box = obj(g, OBJ_BREAKER)
    use_up(b, box[1], box[2] + 1)
    check(g.u8("_electrical_inserted") == FUSES and g.u8("_electrical_state") == ELEC_SEALED and
          mt_at(g, bd[1], bd[2]) == MT["copper_top"], "the tenth fuse: the breaker room shut")
    for _ in range(3):
        quiet(g, J["up"])
    for _ in range(8):   # until the panel is up (the DMG can drop the press)
        if g.u8("_hud_panel"):
            break
        quiet(g, J["a"])
        quiet(g, 0)
    check(g.u8("_hud_panel") != 0, "the breaker's panel open")
    for _ in range(4):
        quiet(g, 0)
    shot(g, "breaker")
    for r in range(3):
        ans = g.u16("_elec_answer")
        for k in range(10):
            if bool(g.u16("_elec_switch") & (1 << k)) == bool(ans & (1 << k)):
                continue
            # Each press held until it has done its job: a panel redraw can take the DMG a few frames
            for _ in range(12):
                c = g.u8("_elec_cursor")
                if c == k:
                    break
                button = "down" if c // 5 != k // 5 else "right" if c < k else "left"
                for _ in range(8):
                    quiet(g, J[button])
                    quiet(g, 0)
                    if g.u8("_elec_cursor") != c:
                        break
            before = g.u16("_elec_switch")
            for _ in range(8):
                quiet(g, J["a"])
                quiet(g, 0)
                if g.u16("_elec_switch") != before:
                    break
        check(g.u16("_elec_switch") == ans, f"round {r + 1}: the switches match")
        for _ in range(240):
            quiet(g, 0)
            if g.u8("_elec_round") > r or g.u8("_electrical_state") == ELEC_POWER:
                break
        quiet(g, 0)   # a frame boundary can fall in the middle of the game's frame
        quiet(g, 0)
    check(g.u8("_electrical_state") == ELEC_POWER and g.u8("_player_fast") and
          mt_at(g, lift[1], lift[2]) == MT["lift_open_tl"], f"the power's back: the lift open, running (state "
          f"{g.u8('_electrical_state')}, fast {g.u8('_player_fast')}, lift {mt_at(g, lift[1], lift[2])})")
    shot(g, "power")
    for _ in range(200):
        g.tick(0)
        if g.u8("_electrical_state") == ELEC_ESCAPE:
            break
    check(g.u8("_electrical_state") == ELEC_ESCAPE and g.u8("_figure_state") == FIG_CHASE, "Figure out of his door")
    g.mem[g.syms["_figure_state"]] = FIG_OFF
    walk(b, (lift[1], lift[2] + 3))
    for _ in range(60):
        quiet(g, J["up"])
        if g.state() == GAME_WON or g.state() != GAME_PLAYING:
            break
    check(g.u8("_electrical_state") == ELEC_WON, f"into the lift (state {g.u8('_electrical_state')}, game {g.state()})")
    for _ in range(600):
        g.tick(0)
    shot(g, "results")
    for _ in range(20):
        g.tick(J["start"])
        g.tick(0)
        if g.state() == GAME_TITLE:
            break
    for _ in range(60):
        g.tick(0)
    check(g.state() == GAME_TITLE, "Start: back to the title")
except RuntimeError as e:
    check(False, f"puzzle: {e}")
low = next(a for a in range(data_end, sp) if g.mem[a] != CANARY)
check(low - data_end >= 32, f"stack: at most {0xE000 - low} bytes deep, {low - data_end} bytes never touched "
                            f"(data ends at {data_end:04X})")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ Figure
print(f"{rom}: figure")
g, b = boot()
to_room100(g)
lever = obj(g, OBJ_LEVER)
use_up(b, lever[1], lever[2] + 1, keep_off=False)
# He comes at once to where the lever was pulled: sneak away from it (B held), down and east
for i in range(360):
    g.tick(J["b"] | (J["down"] if i < 50 else J["right"]))
    if g.state() != GAME_PLAYING:
        break
heard = False
check(g.state() == GAME_PLAYING and not g.u8("_figure_hunting"), f"sneaking away: alive, not hunted "
      f"(state {g.state()}, hunting {g.u8('_figure_hunting')})")
for i in range(200):
    g.tick(J["right"] if (i // 40) % 2 == 0 else J["left"])   # walking
    if g.u8("_figure_hunting"):
        heard = True
        break
check(heard, "walking: he heard and comes")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ the escape
print(f"{rom}: escape")
g, b = boot()
to_room100(g)
lever = obj(g, OBJ_LEVER)
cause = None
try:
    use_up(b, lever[1], lever[2] + 1)
    for _ in range(400):
        quiet(g, 0)
        if g.u8("_electrical_state") == ELEC_SEARCH:
            break
    # Skip the search (the puzzle part plays it): the breaker door open, at the box, the last round one
    # switch from solved
    w = g.u8("_room_w")
    bd, box = obj(g, OBJ_BDOOR), obj(g, OBJ_BREAKER)
    g.mem[g.syms["_room_map"] + bd[2] * w + bd[1]] = MT["copper_open_top"]
    g.mem[g.syms["_room_map"] + (bd[2] + 1) * w + bd[1]] = MT["copper_open_bottom"]
    g.mem[g.syms["_elec_key_taken"]] = 2
    walk(b, (box[1], box[2] + 2))
    g.mem[g.syms["_electrical_state"]] = ELEC_SEALED
    g.mem[g.syms["_elec_round"]] = 2
    ans = g.u16("_elec_answer")
    g.mem[g.syms["_elec_switch"]] = (ans ^ 1) & 0xFF
    g.mem[g.syms["_elec_switch"] + 1] = ans >> 8
    use_up(b, box[1], box[2] + 1)   # opens the panel (cursor on switch 1) ...
    press(g, "a")                   # ... and flips switch 1 to the answer
    for _ in range(240):
        quiet(g, 0)
        if g.u8("_electrical_state") == ELEC_POWER:
            break
    check(g.u8("_electrical_state") == ELEC_POWER, "the power (the last round solved)")
    for _ in range(600):   # stand there
        g.tick(0)
        if g.state() == GAME_DEAD:
            cause = g.u8("_game_death")
            break
    check(cause == DEATH_ESCAPE, f"standing still: Figure catches you (cause {cause})")
except RuntimeError as e:
    check(False, f"escape: {e}")
g.pyboy.stop(save=False)
print("all passed" if not failures else f"{len(failures)} failed")
sys.exit(1 if failures else 0)
