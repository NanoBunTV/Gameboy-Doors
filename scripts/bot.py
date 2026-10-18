"""Plays the game in PyBoy like a player would, and records the buttons for AUTOTEST builds.

    tools/venv/Scripts/python.exe scripts/bot.py build/dmg-autotest/doors.gb --rooms 25 --record

The ROM must be an AUTOTEST build (fixed seed) with an empty recording (tests/autotest_input.c as
written by --reset, or replay finished). The bot reads the game's state from WRAM by symbol (room map,
objects, run flags, player), walks with a breadth-first search over metatiles, opens drawers until it has
the key, unlocks and opens doors, and walks through them. At room 3 it goes back to room 2 and returns, to
test walking back through a door. When Rush or Ambush is announced it drops what it's doing and hides in
the nearest closet or bed it isn't banned from, stays until he's gone, and during Ambush's pauses steps out
and straight back in so Hide doesn't throw it out.

--record writes tests/autotest_input.c: the buttons for every input_update() call, run-length coded.
AUTOTEST builds replay it, so BGB (which can't press buttons) plays the same run under its hardware checks.
--check replays an AUTOTEST ROM built with that recording and confirms it reaches the same room at the
same input frame (the game is deterministic in its input).
"""
import argparse
import sys
from collections import deque
import heapq
from pathlib import Path

from pyboy import PyBoy

ROOT = Path(__file__).resolve().parent.parent

J = {"right": 0x01, "left": 0x02, "up": 0x04, "down": 0x08, "a": 0x10, "b": 0x20, "select": 0x40, "start": 0x80}
OBJ_ENTRY, OBJ_EXIT, OBJ_DRAWER, OBJ_KEYSPOT, OBJ_CLOSET, OBJ_BED = 1, 2, 3, 4, 6, 7
RF_LOCKED, RF_KEY_DRAWER, RF_KEY_TAKEN, RF_EXIT_OPEN, RF_FINAL, RF_KEY_FIXED = 1, 2, 4, 8, 16, 32
RF2_DARK, RF2_EYES, RF2_DUPE, RF2_DUPE_HIT, RF2_HALT = 1, 2, 8, 0x10, 0x40
GAME_TITLE, GAME_PLAYING, GAME_TRANSITION, GAME_DEAD, GAME_HALT = 0, 1, 2, 3, 4
WRONG_DOOR_FROM = 12  # the first Dupe room from here: the bot opens a wrong door on purpose (40 HP)
ENT_IDLE, ENT_WAIT, ENT_WARN, ENT_MOVE, ENT_PAUSE = 0, 1, 2, 3, 4
REHIDE_AFTER = 540   # frames hidden before stepping out and back in (Hide throws you out at 720)


# player.h DIR_* (clockwise from down) as button masks, and a 4-way step's DIR_*
DIR_MASK = [0x08, 0x0A, 0x02, 0x06, 0x04, 0x05, 0x01, 0x09]
STEP_DIR = {(0, 1): 0, (-1, 0): 2, (0, -1): 4, (1, 0): 6}
EYES_PENALTY = 8     # a step toward Eyes costs as much as this many steps (scripts/bot.py routes round him)


def octant(dx, dy):
    """src/eyes.c's octant(): which DIR_* a vector points in"""
    ax, ay = abs(dx), abs(dy)
    if ay * 5 < ax * 2:
        return 6 if dx > 0 else 2
    if ax * 5 < ay * 2:
        return 0 if dy > 0 else 4
    if dx > 0:
        return 7 if dy > 0 else 5
    return 1 if dy > 0 else 3


class Danger(Exception):
    """Rush or Ambush was announced while the bot was doing something else"""
FEET_X, FEET_Y = 8, 12
# Facing used for interacting toward a neighbour cell
DIRS = {(0, -1): "up", (0, 1): "down", (-1, 0): "left", (1, 0): "right"}


class Game:
    def __init__(self, rom: Path):
        self.syms = {}
        for line in rom.with_suffix(".sym").read_text().splitlines():
            p = line.split()
            if len(p) == 2 and ":" in p[0]:
                self.syms[p[1]] = int(p[0].split(":")[1], 16)
        self.pyboy = PyBoy(str(rom), window="null", cgb=rom.suffix == ".gbc", sound_emulated=False)
        self.pyboy.set_emulation_speed(0)
        self.mem = self.pyboy.memory
        self.held = 0
        self.record = []      # buttons per input_update() call
        self.frames = 0
        # Recorded at the moment the game reads the joypad (a hook on input_update), not by comparing
        # input_frame between ticks: a frame boundary can fall between the read and the count, which put
        # the recording one input out (found 2026-10-18)
        self.pyboy.hook_register(0, self.syms["_input_update"], lambda _: self.record.append(self.held), None)

    def u8(self, name, off=0):
        return self.mem[self.syms[name] + off]

    def s16(self, name):
        v = self.u8(name) | (self.u8(name, 1) << 8)
        return v - 0x10000 if v & 0x8000 else v

    def u16(self, name):
        return self.u8(name) | (self.u8(name, 1) << 8)

    def set_buttons(self, mask):
        for name, bit in J.items():
            if (mask & bit) and not (self.held & bit):
                self.pyboy.button_press(name)
            elif not (mask & bit) and (self.held & bit):
                self.pyboy.button_release(name)
        self.held = mask

    def tick(self, mask=0, render=False):
        """One frame with these buttons held. Records them for each input_update() call it caused."""
        self.set_buttons(mask)
        self.pyboy.tick(1, render)
        self.frames += 1

    # --- state
    def state(self):
        return self.u8("_game_state")

    def room(self):
        return self.u8("_run_room")

    def feet(self):
        return self.s16("_player_x") + FEET_X, self.s16("_player_y") + FEET_Y

    def cell(self):
        fx, fy = self.feet()
        return fx // 16, fy // 16

    def grid(self):
        w, h = self.u8("_room_w"), self.u8("_room_h")
        base = self.syms["_room_map"]
        flags = self.syms["_mt_flags"]
        solid = [[bool(self.mem[flags + self.mem[base + y * w + x]] & 1) for x in range(w)] for y in range(h)]
        return w, h, solid

    def objects(self):
        n = self.u8("_room_obj_count")
        base = self.syms["_room_objs"]
        return [tuple(self.mem[base + i * 6 + k] for k in range(6)) for i in range(n)]

    def flags(self):
        return self.u8("_run_flags", self.room())

    def entity(self):
        return self.u8("_entity_state")

    def hidden(self):
        return self.u8("_hide_hidden")


def bfs(solid, w, h, start, goals):
    """Shortest 4-way path over walkable cells from start to any goal cell (goals may be walkable or not;
    the path ends on the last walkable cell before... no: goals here are walkable cells)."""
    prev = {start: None}
    q = deque([start])
    while q:
        c = q.popleft()
        if c in goals:
            path = []
            while c is not None:
                path.append(c)
                c = prev[c]
            return path[::-1]
        x, y = c
        for dx, dy in DIRS:
            n = (x + dx, y + dy)
            if 0 <= n[0] < w and 0 <= n[1] < h and n not in prev and not solid[n[1]][n[0]]:
                prev[n] = c
                q.append(n)
    return None


def cheapest(solid, w, h, start, goal, eyes):
    """Like bfs, but a step that walks toward Eyes (eyes = his middle in px, or None) costs EYES_PENALTY:
    facing him hurts, so the bot goes round instead of straight at him"""
    if eyes is None:
        return bfs(solid, w, h, start, {goal})
    dist, prev = {start: 0}, {start: None}
    q = [(0, start)]
    while q:
        d, c = heapq.heappop(q)
        if c == goal:
            path = []
            while c is not None:
                path.append(c)
                c = prev[c]
            return path[::-1]
        if d > dist[c]:
            continue
        for (dx, dy) in DIRS:
            n = (c[0] + dx, c[1] + dy)
            if not (0 <= n[0] < w and 0 <= n[1] < h) or solid[n[1]][n[0]]:
                continue
            toward = octant(eyes[0] - (n[0] * 16 + 8), eyes[1] - (n[1] * 16 + 8 - 4)) == STEP_DIR[(dx, dy)]
            nd = d + (EYES_PENALTY if toward else 1)
            if nd < dist.get(n, 1 << 30):
                dist[n], prev[n] = nd, c
                heapq.heappush(q, (nd, n))
    return None


class Bot:
    def __init__(self, game: Game, verbose=False):
        self.g = game
        self.verbose = verbose
        self.backtrack_done = False
        self.hides = 0
        self.entity_shots = False
        self.lit_rooms = set()
        self.tried_fake = False    # pressed A at a numberless wrong door ("IT'S FAKE.")
        self.met_dupe = False      # opened a wrong door with Dupe behind it
        self.halts = 0
        self.door_shots = None   # filename prefix: screenshot each exit door when walking up to it

    def log(self, *a):
        if self.verbose:
            print(f"[frame {self.g.frames} room {self.g.room()}]", *a)

    def face_screech(self):
        """Screech is waiting: walk a step his way and stand still facing him until he's gone"""
        m = DIR_MASK[self.g.u8("_screech_dir")]
        self.log("facing Screech")
        for _ in range(3):
            self.g.tick(m)
        for _ in range(40):
            if self.g.u8("_screech_state") != 1:
                break
            self.g.tick(0)

    def eyes(self):
        return (self.g.s16("_eyes_x"), self.g.s16("_eyes_y")) if self.g.u8("_eyes_here") else None

    def step(self, mask=0):
        """One frame of the bot's own actions; stops them when something is coming"""
        if self.g.u8("_screech_state") == 1 and self.g.state() == GAME_PLAYING and not self.g.hidden():
            self.face_screech()
        self.g.tick(mask)
        if self.g.state() == GAME_DEAD:
            raise RuntimeError(f"died (cause {self.g.u8('_game_death')})")
        if self.g.entity() != ENT_IDLE and not self.g.hidden() and self.g.state() == GAME_PLAYING:
            raise Danger()

    def press_a_until(self, cond, tries=8):
        """Taps A until cond() holds (a tap can fall on a frame the game skipped reading the joypad)"""
        for _ in range(tries):
            self.g.tick(J["a"])
            self.g.tick(0)
            if cond():
                return True
        return False

    def hide(self):
        """Hides until Rush / Ambush is gone"""
        self.hides += 1
        objs = self.g.objects()
        banned = self.g.u8("_run_banned", self.g.room())
        spots = [o for o in objs if o[0] in (OBJ_CLOSET, OBJ_BED)]
        free = [o for i, o in enumerate(spots) if not banned & (1 << i)]
        if not free:
            raise RuntimeError("something is coming and there's nowhere to hide")
        w, h, solid = self.g.grid()
        here = self.g.cell()

        def distance(o):
            cellpos, _ = self.approach(o)
            return len(bfs(solid, w, h, here, {cellpos}) or [0] * 999)
        spot = min(free, key=distance)
        cellpos, face = self.approach(spot)
        self.log("hiding in", spot, "entity state", self.g.entity())
        if not self.walk_to(cellpos, check=False):
            raise RuntimeError(f"stuck walking to hiding spot {spot}")
        for _ in range(3):
            self.g.tick(J[face])
        self.g.tick(0)
        if not self.press_a_until(lambda: self.g.hidden(), tries=3):
            raise RuntimeError(f"pressed A at {spot} but didn't hide")
        while self.g.entity() != ENT_IDLE:
            if self.g.state() == GAME_DEAD:
                raise RuntimeError(f"died while hidden (cause {self.g.u8('_game_death')})")
            if not self.g.hidden():
                raise RuntimeError("thrown out of hiding")
            # Ambush waiting at the rear: step out and back in, which restarts Hide's clock
            if self.g.u16("_hide_frames") > REHIDE_AFTER and self.g.entity() in (ENT_PAUSE, ENT_WAIT, ENT_WARN):
                self.log("out and back in")
                if not self.press_a_until(lambda: not self.g.hidden()) or                         not self.press_a_until(lambda: self.g.hidden()):
                    raise RuntimeError("couldn't get back into hiding")
                continue
            shot = self.entity_shots and self.g.frames % 6 == 0 and self.g.entity() != ENT_WAIT
            self.g.tick(0, render=shot)
            if shot:
                self.g.pyboy.screen.image.save(ROOT / "build" / "screens" /
                                               f"ent-{self.hides:02d}-{self.g.frames:06d}.png")
        if not self.press_a_until(lambda: not self.g.hidden()):   # out
            raise RuntimeError("couldn't get out of hiding")
        self.log("out of hiding")

    def wait_playing(self, limit=600):
        for _ in range(limit):
            if self.g.state() == GAME_PLAYING:
                return
            if self.g.state() == GAME_TITLE:
                self.g.tick(J["start"])
                self.g.tick(0)
            else:
                self.g.tick(0)
        raise RuntimeError("game never got to PLAYING")

    def walk_to(self, target, limit=900, check=True):
        """Walks the feet to the centre of a walkable cell along a BFS path. Returns False if stuck."""
        room = self.g.room()
        stuck, last = 0, None
        for _ in range(limit):
            if self.g.state() != GAME_PLAYING or self.g.room() != room:
                return True  # went through a door
            w, h, solid = self.g.grid()
            here = self.g.cell()
            path = cheapest(solid, w, h, here, target, self.eyes())
            if path is None:
                raise RuntimeError(f"no path from {here} to {target}")
            fx, fy = self.g.feet()
            nxt = path[1] if len(path) > 1 else path[0]
            tx, ty = nxt[0] * 16 + 8, nxt[1] * 16 + 8
            if len(path) == 1 and abs(fx - tx) <= 1 and abs(fy - ty) <= 1:
                self.g.tick(0)
                return True
            mask = 0
            # Line up across the direction of travel first, then go
            if nxt[0] != here[0]:
                if abs(fy - ty) > 2:
                    mask |= J["down"] if fy < ty else J["up"]
                else:
                    mask |= J["right"] if fx < tx else J["left"]
            elif nxt[1] != here[1]:
                if abs(fx - tx) > 2:
                    mask |= J["right"] if fx < tx else J["left"]
                else:
                    mask |= J["down"] if fy < ty else J["up"]
            else:
                if abs(fx - tx) > 1:
                    mask |= J["right"] if fx < tx else J["left"]
                if abs(fy - ty) > 1:
                    mask |= J["down"] if fy < ty else J["up"]
            if check:
                self.step(mask)
            else:
                self.g.tick(mask)
            pos = self.g.feet()
            stuck = stuck + 1 if pos == last else 0
            last = pos
            if stuck > 90:
                return False
        return False

    def approach(self, obj):
        """A walkable cell next to the object, and the direction to face it from there."""
        w, h, solid = self.g.grid()
        t, ox, oy, ow, oh, _ = obj
        options = []
        for y in range(oy, oy + oh):
            for x in range(ox, ox + ow):
                for (dx, dy), name in DIRS.items():
                    sx, sy = x - dx, y - dy
                    if 0 <= sx < w and 0 <= sy < h and not solid[sy][sx] and not (ox <= sx < ox + ow and oy <= sy < oy + oh):
                        options.append(((sx, sy), name))
        here = self.g.cell()
        best = None
        for cellpos, face in options:
            p = bfs(solid, w, h, here, {cellpos})
            if p is not None and (best is None or len(p) < best[0]):
                best = (len(p), cellpos, face)
        if best is None:
            raise RuntimeError(f"can't reach object {obj}")
        return best[1], best[2]

    def use(self, obj):
        cellpos, face = self.approach(obj)
        self.log("use", obj, "from", cellpos, face)
        if not self.walk_to(cellpos):
            raise RuntimeError(f"stuck walking to {cellpos} for {obj}")
        for _ in range(3):
            self.step(J[face])      # turn to face it (walking into it is harmless)
        self.step(0)
        if self.door_shots and obj[0] == 2:   # OBJ_EXIT
            self.g.tick(0, render=True)
            self.g.pyboy.screen.image.save(ROOT / "build" / "screens" /
                                           f"{self.door_shots}-door-{self.g.room() + 1:03d}.png")
        if obj[0] == OBJ_EXIT and not (self.g.flags() & RF_LOCKED and self.g.u8("_run_key_door") != self.g.room() + 1):
            # a door that will open: tap until it has (one tap can land on a lag frame)
            self.press_a_until(lambda: self.g.flags() & RF_EXIT_OPEN)
            # the flag is set before the door is redrawn, and on DMG that work can run past the frame end
            self.g.tick(0)
            self.g.tick(0)
            if self.g.entity() != ENT_IDLE and not self.g.hidden():
                raise Danger()
            return
        self.step(J["a"])
        self.step(0)

    def exits(self, objs=None):
        """The room's exits in object order, and this run's real one"""
        ex = [o for o in (objs or self.g.objects()) if o[0] == OBJ_EXIT]
        return ex, ex[self.g.u8("_run_exit", self.g.room())]

    def try_wrong_door(self, obj):
        """Presses A at a wrong door: 'IT'S FAKE.' without Dupe; with him, his face, 40 HP, and back"""
        cellpos, face = self.approach(obj)
        self.log("wrong door", obj)
        if not self.walk_to(cellpos):
            raise RuntimeError(f"stuck walking to the wrong door {obj}")
        for _ in range(3):
            self.step(J[face])
        self.step(0)
        before = self.g.u8("_run_flags2", self.g.room())
        dupe = before & RF2_DUPE
        # Dupe: tap until he fires (a tap can land on a frame the game didn't read). No Dupe: one tap.
        self.press_a_until(lambda: self.g.u8("_run_flags2", self.g.room()) != before, tries=6 if dupe else 1)
        for _ in range(600):   # Dupe: the door opens, his face, the room again
            if self.g.state() == GAME_PLAYING:
                break
            self.g.tick(0)
        if self.g.state() == GAME_DEAD:
            raise RuntimeError("Dupe killed the bot")
        return self.g.u8("_run_flags2", self.g.room()) != before

    def play_halt(self):
        """Halt's hallway: walk the way the game says (halt_dir) until out at the far end"""
        self.halts += 1
        hp = self.g.u8("_run_hp")
        while self.g.state() in (GAME_HALT, GAME_TRANSITION):
            if self.g.state() == GAME_HALT:
                self.g.tick(J["right"] if self.g.u8("_halt_dir") == 1 else J["left"])
            else:
                self.g.tick(0)
        if self.g.state() == GAME_DEAD:
            raise RuntimeError("Halt killed the bot")
        self.log("out of Halt's hallway, HP", hp, "->", self.g.u8("_run_hp"))

    def go_through(self, obj):
        """Walks into an open door until the room changes."""
        room = self.g.room()
        w, h, solid = self.g.grid()
        t, ox, oy, ow, oh, _ = obj
        cells = [(x, y) for y in range(oy, oy + oh) for x in range(ox, ox + ow) if not solid[y][x]]
        here = self.g.cell()
        best = min(cells, key=lambda c: len(bfs(solid, w, h, here, {c}) or [0] * 999))
        self.log("go through", obj, "via", best)
        self.walk_to(best)
        for _ in range(240):
            if self.g.room() != room:
                break
            if self.g.state() == GAME_HALT or (self.g.state() == GAME_TRANSITION and
                                               (self.g.u8("_run_flags2", room) & (RF2_HALT | 0x80)) == RF2_HALT):
                self.play_halt()
                continue
            # keep pushing outward through the door
            wall = obj[5]
            self.step({1: J["up"], 2: J["down"], 3: J["left"], 4: J["right"]}[wall])
        if self.g.room() == room:
            raise RuntimeError(f"walked into {obj} but stayed in room {room}")
        self.wait_playing()

    def play_room(self):
        """Plays the room until it goes through the exit; hides whenever something comes"""
        while True:
            try:
                if self.g.entity() != ENT_IDLE and not self.g.hidden():
                    self.hide()
                return self.play_room_once()
            except Danger:
                self.hide()

    def play_room_once(self):
        objs = self.g.objects()
        exits, exit_ = self.exits(objs)
        entry = next((o for o in objs if o[0] == OBJ_ENTRY), None)
        n = self.g.room()
        f2 = self.g.u8("_run_flags2", n)

        # Dupe rooms: once, a numberless wrong door (refuses) and once, from room WRONG_DOOR_FROM, Dupe himself
        wrong = [o for o in exits if o != exit_]
        if wrong and not (f2 & RF2_DUPE) and not self.tried_fake:
            self.tried_fake = True
            if self.try_wrong_door(wrong[0]):
                raise RuntimeError("a numberless wrong door did something")
        if wrong and (f2 & RF2_DUPE) and n >= WRONG_DOOR_FROM and not self.met_dupe:
            self.met_dupe = True
            hp = self.g.u8("_run_hp")
            if not self.try_wrong_door(wrong[0]):
                raise RuntimeError("Dupe's door didn't fire")
            self.log("Dupe: HP", hp, "->", self.g.u8("_run_hp"))

        # Debug and AUTOTEST builds: B cycles the light (glow, lighter, flashlight). Each new dark room gets
        # the next one, so the recorded run lights rooms all three ways.
        if self.g.u8("_run_flags2", n) & RF2_DARK and "_game_light_tool" in self.g.syms and n not in self.lit_rooms:
            self.lit_rooms.add(n)
            self.g.tick(J["b"])
            self.g.tick(0)

        if n == 3 and not self.backtrack_done and entry:
            self.backtrack_done = True
            self.log("backtracking test")
            self.go_through(entry)          # to room 2, arriving at its exit
            if self.g.room() != 2:
                raise RuntimeError("backtrack went to the wrong room")
            self.go_through(self.exits()[1])
            return

        for attempt in range(12):
            f = self.g.flags()
            if f & RF_EXIT_OPEN:
                self.go_through(exit_)
                return
            if f & RF_FINAL:
                return "final"
            if (f & RF_LOCKED) and self.g.u8("_run_key_door") != n + 1:
                if f & RF_KEY_DRAWER:
                    opened = self.g.u8("_run_drawers", n)
                    drawers = [o for o in objs if o[0] == OBJ_DRAWER]
                    # open drawers in order until the key turns up (like a player searching)
                    todo = [d for i, d in enumerate(drawers) if not opened & (1 << i)]
                    self.use(todo[0])
                else:
                    spots = [o for o in objs if o[0] == OBJ_KEYSPOT]
                    if f & RF_KEY_FIXED:
                        spot = next(o for o in spots if o[5] & 1)
                    else:
                        spot = [o for o in spots if not o[5] & 1][self.g.u8("_run_key_slot", n)]
                    self.use(spot)
                continue
            self.use(exit_)
        raise RuntimeError("couldn't get the exit open")


def die_and_retry(g: Game, bot: Bot):
    """Plays on until Rush or Ambush comes, stands still to be caught, then gets through the Guiding Light
    (A skips the typing, A again picks Try again) into a new run"""
    for _ in range(40):
        if g.entity() != ENT_IDLE:
            break
        # play the room without hiding: Danger only interrupts, so finish rooms until something comes
        try:
            bot.play_room_once()
        except Danger:
            break
    bot.log("standing still to be caught")
    for _ in range(4000):
        g.tick(0)
        if g.state() == GAME_DEAD:
            break
    else:
        raise RuntimeError("nothing came to kill the player")
    cause = g.u8("_game_death")
    for i in range(4000):
        g.tick(J["a"] if i % 10 == 0 else 0)
        if g.state() == GAME_PLAYING and g.room() == 0:
            break
    else:
        raise RuntimeError("never got from the death screen into a new run")
    print(f"died (cause {cause}), went through the Guiding Light and started a new run")


def write_recording(record, path: Path):
    pairs = []
    for b in record:
        if pairs and pairs[-1][0] == b and pairs[-1][1] < 255:
            pairs[-1][1] += 1
        else:
            pairs.append([b, 1])
    data = [v for p in pairs for v in p]
    lines = [", ".join(f"0x{v:02X}" for v in data[i:i + 16]) + "," for i in range(0, len(data), 16)]
    path.write_text(
        "// Recorded by scripts/bot.py. Do not edit; re-record with scripts/bot.py --record.\n"
        "#pragma bank 255\n#include \"autotest_input.h\"\nBANKREF(autotest_input)\n\n"
        "const uint8_t autotest_input[] = {\n    " + "\n    ".join(lines) + "\n};\n"
        f"const uint16_t autotest_input_len = {len(data)};\n", encoding="utf-8")
    return len(data)


def reset_recording(path: Path):
    path.write_text(
        "// Placeholder until scripts/bot.py records a run: AUTOTEST builds read the joypad.\n"
        "#pragma bank 255\n#include \"autotest_input.h\"\nBANKREF(autotest_input)\n\n"
        "const uint8_t autotest_input[] = { 0, 0 };\nconst uint16_t autotest_input_len = 0;\n", encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom", type=Path, nargs="?")
    ap.add_argument("--rooms", type=int, default=20, help="play until this room number")
    ap.add_argument("--record", action="store_true", help="write tests/autotest_input.c")
    ap.add_argument("--reset", action="store_true", help="write an empty tests/autotest_input.c and stop")
    ap.add_argument("--check", type=int, metavar="ROOM", help="replay only: expect to reach ROOM")
    ap.add_argument("--shots", action="store_true", help="screenshot each room on entering it")
    ap.add_argument("--die-at-end", action="store_true",
                    help="after the last room, stand still until something kills the player, then Try again "
                         "(so the recording covers the jumpscare and the Guiding Light)")
    ap.add_argument("--entity-shots", action="store_true",
                    help="screenshot every 6th frame while hidden (build/screens/ent-*.png)")
    ap.add_argument("--door-shots", metavar="PREFIX",
                    help="screenshot each exit door before opening it (build/screens/PREFIX-door-NNN.png)")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
    rec_path = ROOT / "tests" / "autotest_input.c"
    if args.reset:
        reset_recording(rec_path)
        print(f"wrote empty {rec_path}")
        return

    g = Game(args.rom)
    if args.check is not None:
        # Replay: no buttons from us; the ROM plays its recording
        for _ in range(200000):
            g.pyboy.tick(1, False)
            g.frames += 1
            if g.room() >= args.check and g.state() == GAME_PLAYING:
                print(f"replay reached room {g.room()} at input frame {g.u16('_input_frame')} "
                      f"(emulator frame {g.frames})")
                g.pyboy.stop(save=False)
                return
        g.pyboy.stop(save=False)
        sys.exit(f"replay never reached room {args.check} (stuck in room {g.room()})")

    bot = Bot(g, args.verbose)
    bot.entity_shots = args.entity_shots
    bot.door_shots = args.door_shots
    for _ in range(60):
        g.tick(0)
    bot.wait_playing()
    visited = []
    try:
        while g.room() < args.rooms:
            visited.append(g.room())
            if args.shots:
                for i in range(20):   # let the HUD slide in
                    g.tick(0, render=(i == 19))
                g.pyboy.screen.image.save(ROOT / "build" / "screens" / f"bot-room-{g.room():03d}.png")
            if bot.play_room() == "final":
                break
    except RuntimeError as e:
        g.tick(0, render=True)
        g.pyboy.screen.image.save(ROOT / "build" / "screens" / "bot-fail.png")
        print(f"BOT FAILED in room {g.room()} (template {g.u8('_run_template', g.room())}): {e}")
        print("screen saved to build/screens/bot-fail.png")
        g.pyboy.stop(save=False)
        sys.exit(1)
    if args.die_at_end:
        try:
            die_and_retry(g, bot)
        except RuntimeError as e:
            print(f"BOT FAILED: {e}")
            g.pyboy.stop(save=False)
            sys.exit(1)
    g.pyboy.screen.image.save(ROOT / "build" / "screens" / "bot-end.png")
    templates = [g.u8("_run_template", i) for i in range(g.room() + 1)]
    locked = sum(1 for i in range(g.room() + 1) if g.u8("_run_flags", i) & RF_LOCKED)
    spawns = sum(1 for i in range(g.room() + 1) if g.u8("_run_flags", i) & 0x40)
    print(f"reached room {g.room()} in {g.frames} frames ({len(g.record)} input frames); "
          f"{locked} locked doors; {spawns} spawn rolls; hid {bot.hides} times; HP {g.u8('_run_hp')}; "
          f"Halt {bot.halts}, Dupe {'met' if bot.met_dupe else 'not met'}, fake door {'tried' if bot.tried_fake else 'not tried'}; "
          f"templates {templates}")
    if args.record:
        n = write_recording(g.record, rec_path)
        print(f"wrote {rec_path} ({n} bytes)")
    g.pyboy.stop(save=False)


if __name__ == "__main__":
    main()
