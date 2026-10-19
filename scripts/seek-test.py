"""Seek's chase (rooms 30-35, Phase 7) in PyBoy: the rise, the chase, being caught, fire, the wall eyes.

    tools/venv/Scripts/python.exe scripts/seek-test.py build/gbc-debug/doors.gbc
    tools/venv/Scripts/python.exe scripts/seek-test.py build/dmg-debug/doors.gb

Needs a DEBUG or AUTOTEST build (game_warp). Each run boots afresh and warps:
  eyes    rooms 25-30: Seek's eyes on the walls, 1 more each room, 6 in the start hallway
  chase   room 30: the bot walks up the start hallway; Seek must rise (the player held still), then it runs
          the chase through each room's real door (they open by themselves) round the fire to room 36: Seek
          gone, the music stopped, the speed back to walking, door 36 shut behind
  caught  room 30: up the hallway until he rises, then stand still: death cause 9 (his face)
  behind  room 30: once he's up, round him: when he goes on into the next room, being behind him kills
  fire    room 35 (no chase): walk into the fire: death cause 10
Screenshots go to build/screens/seek-<console>-*.png. Exits non-zero on any failure.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bot import (Game, Bot, J, GAME_PLAYING, GAME_DEAD, OBJ_ENTRY, SEEK_WAIT, SEEK_RISE, SEEK_CHASE,  # noqa: E402
                 SEEK_OFF, bfs)

START, END = 30, 35
DEATH_SEEK, DEATH_FIRE = 9, 10
ROOT = Path(__file__).resolve().parent.parent

rom = Path(sys.argv[1])
tag = "gbc" if rom.suffix == ".gbc" else "dmg"
failures = []


def check(cond, what):
    print(("  ok   " if cond else "  FAIL ") + what)
    if not cond:
        failures.append(what)


def boot(room):
    g = Game(rom)
    b = Bot(g)
    b.wait_playing()
    for _ in range(60):
        g.tick()
    warp(g, room)
    return g, b


def warp(g, room):
    g.mem[g.syms["_game_warp"]] = room
    for _ in range(120):
        g.tick()


def shot(g, name):
    g.tick(0, render=True)
    g.pyboy.screen.image.save(ROOT / "build" / "screens" / f"seek-{tag}-{name}.png")


def up_to_rise(g, b):
    """Walks toward the start hallway's exit until Seek rises; returns frames walked"""
    for i in range(1200):
        g.tick(J["up"])
        if g.u8("_seek_state") == SEEK_RISE:
            return i
    return None


def wait_dead(g, limit):
    for i in range(limit):
        if g.state() == GAME_DEAD:
            return i
        g.tick(0)
    return None


# ------------------------------------------------------------------------------------------ the wall eyes
print(f"{rom}: eyes")
g, b = boot(START - 5)
counts = []
for n in range(START - 5, START + 1):
    if n != START - 5:
        warp(g, n)
    counts.append(g.u8("_walleyes_count"))
    if n == START - 1:
        shot(g, "eyes-29")
check(counts == [1, 2, 3, 4, 5, 6], f"eyes per room 25-30: {counts} (want 1-6)")
warp(g, START - 7)
check(g.u8("_walleyes_count") == 0, "none further back (room 23)")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ the chase
print(f"{rom}: chase")
g, b = boot(START)
check(g.room() == START and g.u8("_seek_state") == SEEK_WAIT, f"in the start hallway, Seek waiting (room {g.room()})")
walked = up_to_rise(g, b)
check(walked is not None, f"he rises once the player is well along ({walked} frames walking up)")
y0 = g.s16("_player_y")
for i in range(200):
    g.tick(J["up"])
    if i == 150:
        shot(g, "rise")
    if g.u8("_seek_state") == SEEK_CHASE:
        break
check(g.s16("_player_y") == y0, "the player can't move while he rises")
check(g.u8("_seek_state") == SEEK_CHASE and g.u8("_player_fast") == 1, "then the chase: the player runs faster")
b.chasing = True
rooms = [g.room()]
try:
    while g.u8("_room_template_info", g.u8("_run_template", g.room()) * 7) >= 5:
        exit_ = b.exits()[1]
        cellpos, _ = b.approach(exit_)
        if g.room() == START + 2:
            b.walk_to(cellpos, limit=45)
            shot(g, "fourway")   # on the way: the real door's glow ahead
        b.walk_to(cellpos)
        if g.room() == END:
            shot(g, "end")
        for _ in range(30):
            if g.flags() & 8:
                break
            b.step(0)
        check(g.flags() & 8, f"room {g.room()}: the real door opened by itself")
        b.go_through(exit_)
        rooms.append(g.room())
except RuntimeError as e:
    check(False, f"the chase: {e}")
check(rooms == list(range(START, END + 2)), f"through rooms {rooms}")
check(g.u8("_seek_state") == SEEK_OFF and g.u8("_player_fast") == 0, "out: Seek gone, walking speed again")
entry = next(o for o in g.objects() if o[0] == OBJ_ENTRY)
w, h, solid = g.grid()
check(all(solid[y][x] for y in range(entry[2], entry[2] + entry[4]) for x in range(entry[1], entry[1] + entry[3])),
      "door 36 is shut behind you")
shot(g, "safe")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ caught
print(f"{rom}: caught")
g, b = boot(START)
up_to_rise(g, b)
n = wait_dead(g, 600)
check(n is not None and g.u8("_game_death") == DEATH_SEEK, f"standing still after he rises: caught "
      f"(cause {g.u8('_game_death')}, {n} frames)")
for _ in range(40):
    g.tick(0)
shot(g, "scare")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ behind him
print(f"{rom}: behind")
g, b = boot(START)
up_to_rise(g, b)
for _ in range(400):
    if g.u8("_seek_state") == SEEK_CHASE:
        break
    g.tick(0)
# Round him while he holds still a moment: two columns to the left (out of his 24 px reach, clear of the
# hallway's tables against the wall) and down past him. He runs on up the hallway and through its door;
# with the player left in a room behind him, he has them (Minecraft docs 08: "a room behind him").
b.chasing = True
passed = False
for _ in range(600):
    if g.state() != GAME_PLAYING:
        break
    if g.cell()[0] > 2:
        g.tick(J["left"])
    else:
        passed = passed or g.feet()[1] > g.s16("_seek_y") + 24
        g.tick(J["down"] if g.cell()[1] < g.u8("_room_h") - 3 else 0)
check(passed, "got past him, out of reach")
check(g.state() == GAME_DEAD and g.u8("_game_death") == DEATH_SEEK and g.room() == START and
      g.u8("_seek_room") == START + 1, f"he went on into room {g.u8('_seek_room')}, leaving the player behind him: "
      f"caught (cause {g.u8('_game_death')})")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ fire
print(f"{rom}: fire")
g, b = boot(END)
check(g.u8("_seek_state") == SEEK_OFF, "the end hallway on its own: no chase")
w, h = g.u8("_room_w"), g.u8("_room_h")
base, flags = g.syms["_room_map"], g.syms["_mt_flags"]
fire = [(x, y) for y in range(h) for x in range(w) if g.mem[flags + g.mem[base + y * w + x]] & 4]
check(len(fire) > 0, f"{len(fire)} fire metatiles")
_, _, solid = g.grid()
# The floor cell beside the nearest fire, then step into it
here = g.cell()
best = None
for fx, fy in fire:
    for dx, dy, key in ((0, 1, "up"), (0, -1, "down"), (1, 0, "left"), (-1, 0, "right")):
        c = (fx + dx, fy + dy)
        if 0 <= c[0] < w and 0 <= c[1] < h and not solid[c[1]][c[0]]:
            p = bfs(solid, w, h, here, {c})
            if p is not None and (best is None or len(p) < best[0]):
                best = (len(p), c, key)
b.walk_to(best[1])
shot(g, "fire")
for _ in range(60):
    if g.state() == GAME_DEAD:
        break
    g.tick(J[best[2]])
check(g.state() == GAME_DEAD and g.u8("_game_death") == DEATH_FIRE, f"into the fire: dead (cause {g.u8('_game_death')})")
g.pyboy.stop(save=False)

print("all passed" if not failures else f"{len(failures)} FAILED")
sys.exit(1 if failures else 0)
